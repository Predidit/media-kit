// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "include/media_kit_video/video_output.h"
#include "include/media_kit_video/texture_gl.h"
#include "include/media_kit_video/texture_sw.h"

#include <epoxy/egl.h>
#include <epoxy/glx.h>
#include <gdk/gdkwayland.h>
#include <gdk/gdkx.h>

struct _VideoOutput {
  GObject parent_instance;
  TextureGL* texture_gl;
  GdkGLContext* gdk_gl_context;
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;
  gboolean using_flutter_surface;  /* TRUE if using Flutter's surface instead of pbuffer */
  guint8* pixel_buffer;
  TextureSW* texture_sw;
  GMutex mutex; /* Only used in S/W rendering. */
  mpv_handle* handle;
  mpv_render_context* render_context;
  gint64 width;
  gint64 height;
  VideoOutputConfiguration configuration;
  TextureUpdateCallback texture_update_callback;
  gpointer texture_update_callback_context;
  FlTextureRegistrar* texture_registrar;
  gboolean destroyed;
};

G_DEFINE_TYPE(VideoOutput, video_output, G_TYPE_OBJECT)

static void video_output_dispose(GObject* object) {
  VideoOutput* self = VIDEO_OUTPUT(object);
  self->destroyed = TRUE;
  
  g_print("media_kit: VideoOutput: dispose called.\n");
  
  // Make sure that no more callbacks are invoked from mpv.
  if (self->render_context) {
    g_print("media_kit: VideoOutput: Clearing mpv render context callbacks.\n");
    mpv_render_context_set_update_callback(self->render_context, NULL, NULL);
  }

  // H/W
  if (self->texture_gl) {
    g_print("media_kit: VideoOutput: Unregistering GL texture.\n");
    fl_texture_registrar_unregister_texture(self->texture_registrar,
                                            FL_TEXTURE(self->texture_gl));
    
    // Save Flutter's current context before cleanup
    EGLContext flutter_context = eglGetCurrentContext();
    EGLSurface flutter_draw_surface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface flutter_read_surface = eglGetCurrentSurface(EGL_READ);
    
    g_print("media_kit: VideoOutput: Saved Flutter context=%p, surfaces: draw=%p, read=%p\n",
            flutter_context, flutter_draw_surface, flutter_read_surface);
    
    // Free mpv_render_context with our own EGL context
    if (self->render_context != NULL) {
      if (self->egl_context != EGL_NO_CONTEXT) {
        g_print("media_kit: VideoOutput: Switching to mpv context for cleanup.\n");
        eglMakeCurrent(self->egl_display, self->egl_surface, self->egl_surface, self->egl_context);
      }
      g_print("media_kit: VideoOutput: Freeing mpv_render_context.\n");
      mpv_render_context_free(self->render_context);
      self->render_context = NULL;
      
      // Restore Flutter's context
      if (flutter_context != EGL_NO_CONTEXT) {
        g_print("media_kit: VideoOutput: Restoring Flutter context after cleanup.\n");
        eglMakeCurrent(self->egl_display, flutter_draw_surface, flutter_read_surface, flutter_context);
      }
    }
    
    // Clean up EGL resources
    if (self->egl_context != EGL_NO_CONTEXT) {
      g_print("media_kit: VideoOutput: Destroying EGL context %p.\n", self->egl_context);
      eglDestroyContext(self->egl_display, self->egl_context);
      self->egl_context = EGL_NO_CONTEXT;
    }
    if (self->egl_surface != EGL_NO_SURFACE) {
      g_print("media_kit: VideoOutput: Destroying EGL surface %p.\n", self->egl_surface);
      eglDestroySurface(self->egl_display, self->egl_surface);
      self->egl_surface = EGL_NO_SURFACE;
    }
    
    g_print("media_kit: VideoOutput: Unreferencing texture_gl.\n");
    g_object_unref(self->texture_gl);
    if (self->gdk_gl_context != NULL) {
      g_object_unref(self->gdk_gl_context);
    }
  }
  // S/W
  if (self->texture_sw) {
    g_print("media_kit: VideoOutput: Cleaning up S/W rendering resources.\n");
    fl_texture_registrar_unregister_texture(self->texture_registrar,
                                            FL_TEXTURE(self->texture_sw));
    g_free(self->pixel_buffer);
    g_object_unref(self->texture_sw);
    if (self->render_context != NULL) {
      mpv_render_context_free(self->render_context);
      self->render_context = NULL;
    }
  }
  
  g_mutex_clear(&self->mutex);
  G_OBJECT_CLASS(video_output_parent_class)->dispose(object);
}

static void video_output_class_init(VideoOutputClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = video_output_dispose;
}

static void video_output_init(VideoOutput* self) {
  self->texture_gl = NULL;
  self->gdk_gl_context = NULL;
  self->egl_display = EGL_NO_DISPLAY;
  self->egl_context = EGL_NO_CONTEXT;
  self->egl_surface = EGL_NO_SURFACE;
  self->using_flutter_surface = FALSE;
  self->texture_sw = NULL;
  self->pixel_buffer = NULL;
  self->handle = NULL;
  self->render_context = NULL;
  self->width = 0;
  self->height = 0;
  self->configuration = VideoOutputConfiguration{};
  self->texture_update_callback = NULL;
  self->texture_update_callback_context = NULL;
  self->texture_registrar = NULL;
  self->destroyed = FALSE;
  g_mutex_init(&self->mutex);
}

VideoOutput* video_output_new(FlTextureRegistrar* texture_registrar,
                              FlView* view,
                              gint64 handle,
                              VideoOutputConfiguration configuration) {
  VideoOutput* self = VIDEO_OUTPUT(g_object_new(video_output_get_type(), NULL));
  self->texture_registrar = texture_registrar;
  self->handle = (mpv_handle*)handle;
  self->width = configuration.width;
  self->height = configuration.height;
  self->configuration = configuration;
#ifndef MPV_RENDER_API_TYPE_SW
  // MPV_RENDER_API_TYPE_SW must be available for S/W rendering.
  if (!self->configuration.enable_hardware_acceleration) {
    g_printerr("media_kit: VideoOutput: S/W rendering is not supported.\n");
  }
  self->configuration.enable_hardware_acceleration = TRUE;
#endif
  mpv_set_option_string(self->handle, "video-sync", "audio");
  // Causes frame drops with `pulse` audio output. (SlotSun/dart_simple_live#42)
  // mpv_set_option_string(self->handle, "video-timing-offset", "0");
  gboolean hardware_acceleration_supported = FALSE;
  if (self->configuration.enable_hardware_acceleration) {
    g_print("media_kit: VideoOutput: Attempting H/W acceleration initialization.\n");
    // Get Flutter's current EGL display and context to share resources
    self->egl_display = eglGetCurrentDisplay();
    EGLContext flutter_context = eglGetCurrentContext();
    EGLSurface flutter_draw_surface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface flutter_read_surface = eglGetCurrentSurface(EGL_READ);
    
    g_print("media_kit: VideoOutput: Flutter EGL display=%p, context=%p\n", 
            self->egl_display, flutter_context);
    g_print("media_kit: VideoOutput: Flutter surfaces: draw=%p, read=%p\n",
            flutter_draw_surface, flutter_read_surface);
    
    if (self->egl_display != EGL_NO_DISPLAY) {
      g_print("media_kit: VideoOutput: EGL display is valid.\n");
      // Bind EGL API - Flutter uses OpenGL ES on Linux, not desktop OpenGL
      eglBindAPI(EGL_OPENGL_ES_API);
      g_print("media_kit: VideoOutput: Bound EGL_OPENGL_ES_API.\n");
      
      EGLConfig config = NULL;
      EGLint num_configs = 0;
      
      // First, let's query what configs are actually available
      EGLint total_configs = 0;
      if (eglGetConfigs(self->egl_display, NULL, 0, &total_configs)) {
        g_print("media_kit: VideoOutput: Total EGL configs available: %d\n", total_configs);
      }
      
      // Try to get Flutter's config first
      EGLContext flutter_context_for_query = eglGetCurrentContext();
      if (flutter_context_for_query != EGL_NO_CONTEXT) {
        EGLint config_id = 0;
        if (eglQueryContext(self->egl_display, flutter_context_for_query, EGL_CONFIG_ID, &config_id)) {
          g_print("media_kit: VideoOutput: Flutter's config ID: %d\n", config_id);
          
          // Get Flutter's config and check its properties
          EGLint flutter_config_attribs[] = { EGL_CONFIG_ID, config_id, EGL_NONE };
          EGLConfig flutter_config;
          EGLint flutter_num_configs = 0;
          if (eglChooseConfig(self->egl_display, flutter_config_attribs, &flutter_config, 1, &flutter_num_configs) && flutter_num_configs > 0) {
            g_print("media_kit: VideoOutput: Successfully retrieved Flutter's config.\n");
            
            EGLint surface_type = 0, renderable_type = 0;
            eglGetConfigAttrib(self->egl_display, flutter_config, EGL_SURFACE_TYPE, &surface_type);
            eglGetConfigAttrib(self->egl_display, flutter_config, EGL_RENDERABLE_TYPE, &renderable_type);
            
            g_print("media_kit: VideoOutput: Flutter config - surface_type=0x%x, renderable_type=0x%x\n", 
                    surface_type, renderable_type);
            g_print("media_kit: VideoOutput: Flutter config supports: %s%s%s | %s%s%s\n",
                    (surface_type & EGL_WINDOW_BIT) ? "WINDOW " : "",
                    (surface_type & EGL_PBUFFER_BIT) ? "PBUFFER " : "",
                    (surface_type & EGL_PIXMAP_BIT) ? "PIXMAP " : "",
                    (renderable_type & EGL_OPENGL_ES2_BIT) ? "ES2 " : "",
                    (renderable_type & EGL_OPENGL_ES3_BIT) ? "ES3 " : "",
                    (renderable_type & EGL_OPENGL_BIT) ? "GL " : "");
            
            // Use Flutter's config directly if possible
            config = flutter_config;
            num_configs = 1;
          }
        }
      }
      
      // Choose an EGL config. Prefer configs that support both WINDOW and
      // PBUFFER so that the config is compatible with Flutter's surface and
      // also allows creating an offscreen pbuffer. If that fails, try a
      // PBUFFER-only config. We will query the chosen config's surface type
      // and only create a pbuffer if supported.
      if (num_configs == 0) {
        g_print("media_kit: VideoOutput: Choosing EGL config with pbuffer support.\n");

        // Attempts: Try OpenGL ES 3 first, then ES 2, with WINDOW|PBUFFER or PBUFFER only
        EGLint try_configs[][5] = {
            // OpenGL ES 3 with WINDOW|PBUFFER
            {EGL_SURFACE_TYPE, (EGLint)(EGL_WINDOW_BIT | EGL_PBUFFER_BIT), EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE},
            // OpenGL ES 3 with PBUFFER only
            {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE},
            // OpenGL ES 2 with WINDOW|PBUFFER
            {EGL_SURFACE_TYPE, (EGLint)(EGL_WINDOW_BIT | EGL_PBUFFER_BIT), EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE},
            // OpenGL ES 2 with PBUFFER only
            {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE},
        };

        for (int attempt = 0; attempt < (int)(sizeof(try_configs) / sizeof(try_configs[0])); ++attempt) {
          EGLint config_attribs[] = {
              try_configs[attempt][0], try_configs[attempt][1],
              try_configs[attempt][2], try_configs[attempt][3],
              EGL_RED_SIZE, 8,
              EGL_GREEN_SIZE, 8,
              EGL_BLUE_SIZE, 8,
              EGL_ALPHA_SIZE, 8,
              EGL_NONE,
          };
          
          g_print("media_kit: VideoOutput: Attempt %d: surface_type=0x%x, renderable_type=0x%x\n",
                  attempt, try_configs[attempt][1], try_configs[attempt][3]);
          
          if (eglChooseConfig(self->egl_display, config_attribs, &config, 1, &num_configs) && num_configs > 0) {
            const char* api_name = (try_configs[attempt][3] == EGL_OPENGL_ES3_BIT) ? "OpenGL ES 3" : "OpenGL ES 2";
            const char* surface_name = (try_configs[attempt][1] & EGL_WINDOW_BIT) ? "WINDOW|PBUFFER" : "PBUFFER";
            g_print("media_kit: VideoOutput: eglChooseConfig succeeded on attempt %d (%s, %s, num_configs=%d).\n", 
                    attempt, api_name, surface_name, num_configs);
            break;
          } else {
            g_print("media_kit: VideoOutput: eglChooseConfig attempt %d failed (returned=%d, num_configs=%d, eglGetError=0x%x).\n", 
                    attempt, (eglChooseConfig(self->egl_display, config_attribs, &config, 1, &num_configs) ? 1 : 0), 
                    num_configs, eglGetError());
            num_configs = 0;
          }
        }
      }

      if (num_configs > 0 && config != NULL) {
        EGLint surface_type = 0;
        if (!eglGetConfigAttrib(self->egl_display, config, EGL_SURFACE_TYPE, &surface_type)) {
          g_printerr("media_kit: VideoOutput: eglGetConfigAttrib(EGL_SURFACE_TYPE) failed (eglGetError=0x%x).\n", eglGetError());
        } else {
          g_print("media_kit: VideoOutput: Chosen EGL surface type mask = 0x%x\n", surface_type);
        }

        // Create a pbuffer surface only if supported by the chosen config.
        // If pbuffer is not supported, we'll use Flutter's surface directly.
        if ((surface_type & EGL_PBUFFER_BIT) == 0) {
          g_print("media_kit: VideoOutput: Chosen config does not support PBUFFER. Will use Flutter's surface.\n");
          self->egl_surface = flutter_draw_surface;  // Use Flutter's surface
          self->using_flutter_surface = TRUE;
        } else {
          EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
          self->egl_surface = eglCreatePbufferSurface(self->egl_display, config, pbuffer_attribs);
          if (self->egl_surface != EGL_NO_SURFACE) {
            g_print("media_kit: VideoOutput: Pbuffer surface created successfully: %p\n", self->egl_surface);
            self->using_flutter_surface = FALSE;
          } else {
            g_printerr("media_kit: VideoOutput: Failed to create pbuffer surface (Error: 0x%x). Using Flutter's surface.\n", eglGetError());
            self->egl_surface = flutter_draw_surface;  // Fallback to Flutter's surface
            self->using_flutter_surface = TRUE;
          }
        }
        
        // Create our own EGL context that shares with Flutter's context
        // OpenGL ES requires specifying the context version
        EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,  // Start with ES 2, will try ES 3 if needed
            EGL_NONE,
        };
        
        self->egl_context = eglCreateContext(self->egl_display, config, 
                                             flutter_context, context_attribs);
        
        // If ES 2 context creation failed and we chose an ES 3 config, try ES 3 context
        if (self->egl_context == EGL_NO_CONTEXT) {
          EGLint surface_type_check = 0;
          eglGetConfigAttrib(self->egl_display, config, EGL_RENDERABLE_TYPE, &surface_type_check);
          if (surface_type_check & EGL_OPENGL_ES3_BIT) {
            g_print("media_kit: VideoOutput: Trying OpenGL ES 3 context.\n");
            context_attribs[1] = 3;
            self->egl_context = eglCreateContext(self->egl_display, config, 
                                                 flutter_context, context_attribs);
          }
        }
        
        if (self->egl_context != EGL_NO_CONTEXT) {
          g_print("media_kit: VideoOutput: EGL context created successfully: %p (sharing with %p)\n", 
                  self->egl_context, flutter_context);
          // Make our context current for initialization
          if (eglMakeCurrent(self->egl_display, self->egl_surface, self->egl_surface, self->egl_context)) {
            g_print("media_kit: VideoOutput: Successfully made mpv EGL context current.\n");
            // Now create texture and initialize mpv
            self->texture_gl = texture_gl_new(self);
            
            if (fl_texture_registrar_register_texture(
                    texture_registrar, FL_TEXTURE(self->texture_gl))) {
              g_print("media_kit: VideoOutput: Texture registered successfully.\n");
              // Initialize mpv with our EGL context
              mpv_opengl_init_params gl_init_params{
                  [](auto, auto name) {
                    return (void*)eglGetProcAddress(name);
                  },
                  NULL,
              };
              
              mpv_render_param params[] = {
                  {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
                  {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, (void*)&gl_init_params},
                  {MPV_RENDER_PARAM_INVALID, (void*)0},
                  {MPV_RENDER_PARAM_INVALID, (void*)0},
              };
              
              // VAAPI acceleration requires passing X11/Wayland display.
              GdkDisplay* display = gdk_display_get_default();
              if (GDK_IS_WAYLAND_DISPLAY(display)) {
                g_print("media_kit: VideoOutput: Using Wayland display.\n");
                params[2].type = MPV_RENDER_PARAM_WL_DISPLAY;
                params[2].data = gdk_wayland_display_get_wl_display(display);
              } else if (GDK_IS_X11_DISPLAY(display)) {
                g_print("media_kit: VideoOutput: Using X11 display.\n");
                params[2].type = MPV_RENDER_PARAM_X11_DISPLAY;
                params[2].data = gdk_x11_display_get_xdisplay(display);
              }
              
              if (mpv_render_context_create(&self->render_context, self->handle, params) == 0) {
                g_print("media_kit: VideoOutput: mpv_render_context created successfully.\n");
                mpv_render_context_set_update_callback(
                    self->render_context,
                    [](void* data) {
                      VideoOutput* self = (VideoOutput*)data;
                      if (self->destroyed) {
                        return;
                      }
                      fl_texture_registrar_mark_texture_frame_available(
                          self->texture_registrar, FL_TEXTURE(self->texture_gl));
                    },
                    self);
                hardware_acceleration_supported = TRUE;
                g_print("media_kit: VideoOutput: H/W acceleration initialized successfully.\n");
              } else {
                g_printerr("media_kit: VideoOutput: Failed to create mpv_render_context.\n");
              }
            } else {
              g_printerr("media_kit: VideoOutput: Failed to register texture.\n");
            }
            
            // Restore Flutter's context
            if (flutter_context != EGL_NO_CONTEXT) {
              g_print("media_kit: VideoOutput: Restoring Flutter EGL context.\n");
              if (eglMakeCurrent(self->egl_display, flutter_draw_surface, flutter_read_surface, flutter_context)) {
                g_print("media_kit: VideoOutput: Flutter EGL context restored successfully.\n");
              } else {
                g_printerr("media_kit: VideoOutput: Failed to restore Flutter EGL context. Error: 0x%x\n", eglGetError());
              }
            }
          } else {
            g_printerr("media_kit: VideoOutput: Failed to make mpv EGL context current. Error: 0x%x\n", eglGetError());
          }
        } else {
          g_printerr("media_kit: VideoOutput: Failed to create EGL context. Error: 0x%x\n", eglGetError());
        }
      } else {
        g_printerr("media_kit: VideoOutput: No valid EGL config found.\n");
      }
    } else {
      g_printerr("media_kit: VideoOutput: EGL display is invalid.\n");
    }
  }
#ifdef MPV_RENDER_API_TYPE_SW
  if (!hardware_acceleration_supported) {
    g_print("media_kit: VideoOutput: H/W acceleration failed, falling back to S/W rendering.\n");
    // H/W rendering failed somewhere down the line. Fallback to S/W
    // rendering.
    self->pixel_buffer = g_new0(guint8, SW_RENDERING_PIXEL_BUFFER_SIZE);
    self->texture_gl = NULL;
    self->gdk_gl_context = NULL;
    self->texture_sw = texture_sw_new(self);
    if (fl_texture_registrar_register_texture(texture_registrar,
                                              FL_TEXTURE(self->texture_sw))) {
      mpv_render_param params[] = {
          {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_SW},
          {MPV_RENDER_PARAM_INVALID, (void*)0},
      };
      if (mpv_render_context_create(&self->render_context, self->handle,
                                    params) == 0) {
        mpv_render_context_set_update_callback(
            self->render_context,
            [](void* data) {
              // Usage on single-thread is not a concern with pixel buffers
              // unlike OpenGL. So, I'd like to render on a separate thread
              // for slowing the UI thread as little as possible. It's a pity
              // that software rendering is feeling faster than hardware
              // rendering due to fucked-up GTK.
              gdk_threads_add_idle(
                  [](gpointer data) -> gboolean {
                    VideoOutput* self = (VideoOutput*)data;
                    if (self->destroyed) {
                      return FALSE;
                    }
                    g_mutex_lock(&self->mutex);
                    gint64 width = video_output_get_width(self);
                    gint64 height = video_output_get_height(self);
                    if (width > 0 && height > 0) {
                      gint32 size[]{(gint32)width, (gint32)height};
                      gint32 pitch = 4 * (gint32)width;
                      mpv_render_param params[]{
                          {MPV_RENDER_PARAM_SW_SIZE, size},
                          {MPV_RENDER_PARAM_SW_FORMAT, (void*)"rgb0"},
                          {MPV_RENDER_PARAM_SW_STRIDE, &pitch},
                          {MPV_RENDER_PARAM_SW_POINTER, self->pixel_buffer},
                          {MPV_RENDER_PARAM_INVALID, (void*)0},
                      };
                      mpv_render_context_render(self->render_context, params);
                      fl_texture_registrar_mark_texture_frame_available(
                          self->texture_registrar,
                          FL_TEXTURE(self->texture_sw));
                    }
                    g_mutex_unlock(&self->mutex);
                    return FALSE;
                  },
                  data);
            },
            self);
        g_print("media_kit: VideoOutput: Using S/W rendering.\n");
      }
    }
  }
#endif
  return self;
}

void video_output_set_texture_update_callback(
    VideoOutput* self,
    TextureUpdateCallback texture_update_callback,
    gpointer texture_update_callback_context) {
  self->texture_update_callback = texture_update_callback;
  self->texture_update_callback_context = texture_update_callback_context;
  // Notify initial dimensions as (1, 1) if |width| & |height| are 0 i.e.
  // texture & video frame size is based on playing file's resolution. This
  // will make sure that `Texture` widget on Flutter's widget tree is actually
  // mounted & |fl_texture_registrar_mark_texture_frame_available| actually
  // invokes the |TextureGL| or |TextureSW| callbacks. Otherwise it will be a
  // never ending deadlock where no video frames are ever rendered.
  gint64 texture_id = video_output_get_texture_id(self);
  if (self->width == 0 || self->height == 0) {
    self->texture_update_callback(texture_id, 1, 1,
                                  self->texture_update_callback_context);
  } else {
    self->texture_update_callback(texture_id, self->width, self->height,
                                  self->texture_update_callback_context);
  }
}

void video_output_set_size(VideoOutput* self, gint64 width, gint64 height) {
  // Ideally, a mutex should be used here & |video_output_get_width| +
  // |video_output_get_height|. However, that is throwing everything into a
  // deadlock. Flutter itself seems to have some synchronization mechanism in
  // rendering & platform channels AFAIK.

  // H/W
  if (self->texture_gl) {
    self->width = width;
    self->height = height;
  }
  // S/W
  if (self->texture_sw) {
    self->width = CLAMP(width, 0, SW_RENDERING_MAX_WIDTH);
    self->height = CLAMP(height, 0, SW_RENDERING_MAX_HEIGHT);
  }
}

mpv_render_context* video_output_get_render_context(VideoOutput* self) {
  return self->render_context;
}

GdkGLContext* video_output_get_gdk_gl_context(VideoOutput* self) {
  return self->gdk_gl_context;
}

EGLDisplay video_output_get_egl_display(VideoOutput* self) {
  return self->egl_display;
}

EGLContext video_output_get_egl_context(VideoOutput* self) {
  return self->egl_context;
}

EGLSurface video_output_get_egl_surface(VideoOutput* self) {
  return self->egl_surface;
}

gboolean video_output_is_using_flutter_surface(VideoOutput* self) {
  return self->using_flutter_surface;
}

guint8* video_output_get_pixel_buffer(VideoOutput* self) {
  return self->pixel_buffer;
}

gint64 video_output_get_width(VideoOutput* self) {
  // Fixed width.
  if (self->width) {
    return self->width;
  }

  // Video resolution dependent width.
  gint64 width = 0;
  gint64 height = 0;

  mpv_node params;
  mpv_get_property(self->handle, "video-out-params", MPV_FORMAT_NODE, &params);

  int64_t dw = 0, dh = 0, rotate = 0;
  if (params.format == MPV_FORMAT_NODE_MAP) {
    for (int32_t i = 0; i < params.u.list->num; i++) {
      char* key = params.u.list->keys[i];
      auto value = params.u.list->values[i];
      if (value.format == MPV_FORMAT_INT64) {
        if (strcmp(key, "dw") == 0) {
          dw = value.u.int64;
        }
        if (strcmp(key, "dh") == 0) {
          dh = value.u.int64;
        }
        if (strcmp(key, "rotate") == 0) {
          rotate = value.u.int64;
        }
      }
    }
    mpv_free_node_contents(&params);
  }

  width = rotate == 0 || rotate == 180 ? dw : dh;
  height = rotate == 0 || rotate == 180 ? dh : dw;

  if (self->texture_sw != NULL) {
    // Make sure |width| & |height| fit between |SW_RENDERING_MAX_WIDTH| &
    // |SW_RENDERING_MAX_HEIGHT| while maintaining aspect ratio.
    if (width >= SW_RENDERING_MAX_WIDTH) {
      return SW_RENDERING_MAX_WIDTH;
    }
    if (height >= SW_RENDERING_MAX_HEIGHT) {
      return width / height * SW_RENDERING_MAX_HEIGHT;
    }
  }

  return width;
}

gint64 video_output_get_height(VideoOutput* self) {
  // Fixed height.
  if (self->width) {
    return self->height;
  }

  // Video resolution dependent height.
  gint64 width = 0;
  gint64 height = 0;

  mpv_node params;
  mpv_get_property(self->handle, "video-out-params", MPV_FORMAT_NODE, &params);

  int64_t dw = 0, dh = 0, rotate = 0;
  if (params.format == MPV_FORMAT_NODE_MAP) {
    for (int32_t i = 0; i < params.u.list->num; i++) {
      char* key = params.u.list->keys[i];
      auto value = params.u.list->values[i];
      if (value.format == MPV_FORMAT_INT64) {
        if (strcmp(key, "dw") == 0) {
          dw = value.u.int64;
        }
        if (strcmp(key, "dh") == 0) {
          dh = value.u.int64;
        }
        if (strcmp(key, "rotate") == 0) {
          rotate = value.u.int64;
        }
      }
    }
    mpv_free_node_contents(&params);
  }

  width = rotate == 0 || rotate == 180 ? dw : dh;
  height = rotate == 0 || rotate == 180 ? dh : dw;

  if (self->texture_sw != NULL) {
    // Make sure |width| & |height| fit between |SW_RENDERING_MAX_WIDTH| &
    // |SW_RENDERING_MAX_HEIGHT| while maintaining aspect ratio.
    if (height >= SW_RENDERING_MAX_HEIGHT) {
      return SW_RENDERING_MAX_HEIGHT;
    }
    if (width >= SW_RENDERING_MAX_WIDTH) {
      return height / width * SW_RENDERING_MAX_WIDTH;
    }
  }

  return height;
}

gint64 video_output_get_texture_id(VideoOutput* self) {
  // H/W
  if (self->texture_gl) {
    return (gint64)self->texture_gl;
  }
  // S/W
  if (self->texture_sw) {
    return (gint64)self->texture_sw;
  }
  g_assert_not_reached();
  return -1;
}

void video_output_notify_texture_update(VideoOutput* self) {
  gint64 id = video_output_get_texture_id(self);
  gint64 width = video_output_get_width(self);
  gint64 height = video_output_get_height(self);
  gpointer context = self->texture_update_callback_context;
  if (self->texture_update_callback != NULL) {
    self->texture_update_callback(id, width, height, context);
  }
}
