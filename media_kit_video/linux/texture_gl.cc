// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "include/media_kit_video/texture_gl.h"
#include "include/media_kit_video/gl_render_thread.h"

#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <atomic>

// Number of buffers for double buffering
#define NUM_BUFFERS 2

// EGLImage extension function pointers
typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay dpy, EGLImageKHR image);
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);

// EGL_KHR_fence_sync extension function pointers (available in EGL 1.4+)
typedef EGLSyncKHR (*PFNEGLCREATESYNCKHRPROC)(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
typedef EGLBoolean (*PFNEGLDESTROYSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync);
typedef EGLint (*PFNEGLCLIENTWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);

// Define the extension functions
#ifndef eglCreateImageKHR
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
#endif
#ifndef eglDestroyImageKHR
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
#endif
#ifndef glEGLImageTargetTexture2DOES
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;
#endif

// EGL_KHR_fence_sync extension functions
static PFNEGLCREATESYNCKHRPROC _eglCreateSyncKHR = NULL;
static PFNEGLDESTROYSYNCKHRPROC _eglDestroySyncKHR = NULL;
static PFNEGLCLIENTWAITSYNCKHRPROC _eglClientWaitSyncKHR = NULL;

static void init_egl_extensions() {
  static gboolean initialized = FALSE;
  if (!initialized) {
    // EGLImage extensions
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    // EGL_KHR_fence_sync extensions (EGL 1.4+, required by Flutter)
    _eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    _eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    _eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    
    initialized = TRUE;
  }
}

// Buffer structure for double buffering
typedef struct {
  guint32 fbo;              // FBO for mpv rendering
  guint32 texture;          // Texture attached to FBO
  EGLImageKHR egl_image;    // EGLImage for sharing between contexts
  EGLSyncKHR sync;          // EGLSyncKHR for synchronization (no glFinish needed)
  gboolean ready;           // Whether this buffer has valid content
} RenderBuffer;

struct _TextureGL {
  FlTextureGL parent_instance;
  guint32 flutter_texture;       // Flutter's texture (single, rebinds to current front buffer)
  RenderBuffer buffers[NUM_BUFFERS];  // Double buffer array
  std::atomic<int> front_index;  // Index of the front buffer (for Flutter to read)
  int back_index;                // Index of the back buffer (for mpv to render)
  guint32 current_width;
  guint32 current_height;
  gboolean needs_flutter_texture_update;  // Flag to rebind Flutter texture
  gboolean initialization_posted;  // Flag to avoid duplicate initialization
  GMutex swap_mutex;             // Mutex for buffer swap synchronization
  VideoOutput* video_output;
};

G_DEFINE_TYPE(TextureGL, texture_gl, fl_texture_gl_get_type())

static void texture_gl_init(TextureGL* self) {
  self->flutter_texture = 0;
  for (int i = 0; i < NUM_BUFFERS; i++) {
    self->buffers[i].fbo = 0;
    self->buffers[i].texture = 0;
    self->buffers[i].egl_image = EGL_NO_IMAGE_KHR;
    self->buffers[i].sync = EGL_NO_SYNC_KHR;
    self->buffers[i].ready = FALSE;
  }
  self->front_index.store(0);
  self->back_index = 1;
  self->current_width = 1;
  self->current_height = 1;
  self->needs_flutter_texture_update = FALSE;
  self->initialization_posted = FALSE;
  g_mutex_init(&self->swap_mutex);
  self->video_output = NULL;
}

static void texture_gl_dispose(GObject* object) {
  TextureGL* self = TEXTURE_GL(object);
  VideoOutput* video_output = self->video_output;
  GLRenderThread* gl_thread = video_output_get_gl_render_thread(video_output);
  
  // Clean up Flutter's texture (in Flutter's context)
  if (self->flutter_texture != 0) {
    glDeleteTextures(1, &self->flutter_texture);
    self->flutter_texture = 0;
  }
  
  // Clean up double buffer resources in dedicated GL thread
  if (video_output != NULL && gl_thread != NULL) {
    gl_thread->PostAndWait([self, video_output]() {
      EGLDisplay egl_display = video_output_get_egl_display(video_output);
      EGLContext egl_context = video_output_get_egl_context(video_output);
      
      // Clean up all buffers
      for (int i = 0; i < NUM_BUFFERS; i++) {
        RenderBuffer* buf = &self->buffers[i];
        
        // Clean up EGLSyncKHR
        if (buf->sync != EGL_NO_SYNC_KHR) {
          _eglDestroySyncKHR(egl_display, buf->sync);
          buf->sync = EGL_NO_SYNC_KHR;
        }
        
        // Clean up EGLImage
        if (buf->egl_image != EGL_NO_IMAGE_KHR) {
          eglDestroyImageKHR(egl_display, buf->egl_image);
          buf->egl_image = EGL_NO_IMAGE_KHR;
        }
      }
      
      // Clean up mpv's OpenGL resources (in mpv's isolated context)
      if (egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
        
        for (int i = 0; i < NUM_BUFFERS; i++) {
          RenderBuffer* buf = &self->buffers[i];
          
          if (buf->texture != 0) {
            glDeleteTextures(1, &buf->texture);
            buf->texture = 0;
          }
          if (buf->fbo != 0) {
            glDeleteFramebuffers(1, &buf->fbo);
            buf->fbo = 0;
          }
          buf->ready = FALSE;
        }
      }
    });
  }
  
  g_mutex_clear(&self->swap_mutex);
  self->current_width = 1;
  self->current_height = 1;
  self->video_output = NULL;
  G_OBJECT_CLASS(texture_gl_parent_class)->dispose(object);
}

static void texture_gl_class_init(TextureGLClass* klass) {
  FL_TEXTURE_GL_CLASS(klass)->populate = texture_gl_populate_texture;
  G_OBJECT_CLASS(klass)->dispose = texture_gl_dispose;
}

TextureGL* texture_gl_new(VideoOutput* video_output) {
  init_egl_extensions();
  TextureGL* self = TEXTURE_GL(g_object_new(texture_gl_get_type(), NULL));
  self->video_output = video_output;
  return self;
}

void texture_gl_check_and_resize(TextureGL* self, gint64 required_width, gint64 required_height) {
  VideoOutput* video_output = self->video_output;
  
  if (required_width < 1 || required_height < 1) {
    return;
  }
  
  gboolean first_frame = self->buffers[0].fbo == 0 || self->buffers[1].fbo == 0;
  gboolean resize = self->current_width != required_width ||
                    self->current_height != required_height;
  
  if (!first_frame && !resize) {
    return;  // No resize needed
  }
  
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  EGLContext egl_context = video_output_get_egl_context(video_output);
  
  // This function is called from the dedicated rendering thread
  // So we can directly perform OpenGL operations
  
  // Switch to mpv's isolated context
  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
  
  // Lock to prevent concurrent access during resize
  g_mutex_lock(&self->swap_mutex);
  
  // Free previous resources for both buffers
  for (int i = 0; i < NUM_BUFFERS; i++) {
    RenderBuffer* buf = &self->buffers[i];
    
    if (!first_frame) {
      // Wait for any pending sync before destroying resources
      if (buf->sync != EGL_NO_SYNC_KHR) {
        _eglClientWaitSyncKHR(egl_display, buf->sync, 0, EGL_FOREVER_KHR);
        _eglDestroySyncKHR(egl_display, buf->sync);
        buf->sync = EGL_NO_SYNC_KHR;
      }
      
      if (buf->egl_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(egl_display, buf->egl_image);
        buf->egl_image = EGL_NO_IMAGE_KHR;
      }
      
      glDeleteTextures(1, &buf->texture);
      glDeleteFramebuffers(1, &buf->fbo);
    }
    
    // Create FBO and texture for this buffer
    glGenFramebuffers(1, &buf->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
    
    glGenTextures(1, &buf->texture);
    glBindTexture(GL_TEXTURE_2D, buf->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, required_width, required_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    // Attach texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, buf->texture, 0);
    
    // Create EGLImage from texture
    EGLint egl_image_attribs[] = { EGL_NONE };
    buf->egl_image = eglCreateImageKHR(
        egl_display,
        egl_context,
        EGL_GL_TEXTURE_2D_KHR,
        (EGLClientBuffer)(guintptr)buf->texture,
        egl_image_attribs);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    buf->ready = FALSE;
    buf->sync = EGL_NO_SYNC_KHR;
  }
  
  // Flush to ensure textures are ready
  glFlush();
  
  // Reset buffer indices
  self->front_index.store(0);
  self->back_index = 1;
  
  // Mark that Flutter texture needs update
  self->current_width = required_width;
  self->current_height = required_height;
  self->needs_flutter_texture_update = TRUE;
  
  g_mutex_unlock(&self->swap_mutex);
}

void texture_gl_render(TextureGL* self) {
  VideoOutput* video_output = self->video_output;
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  EGLContext egl_context = video_output_get_egl_context(video_output);
  mpv_render_context* render_context = video_output_get_render_context(video_output);
  
  if (!render_context) {
    return;
  }
  
  // Get back buffer for rendering
  int back_idx = self->back_index;
  RenderBuffer* back_buf = &self->buffers[back_idx];
  
  if (back_buf->fbo == 0) {
    return;
  }
  
  // Check if back buffer is still being used by Flutter (non-blocking)
  // The sync here was created by Flutter when it started reading this buffer
  if (back_buf->sync != EGL_NO_SYNC_KHR) {
    EGLint result = _eglClientWaitSyncKHR(egl_display, back_buf->sync, 0, 0);
    if (result == EGL_TIMEOUT_EXPIRED_KHR) {
      // Back buffer still in use by Flutter, skip this frame
      // The previous front buffer will continue to be displayed
      return;
    }
    // Flutter finished using this buffer, destroy the sync
    _eglDestroySyncKHR(egl_display, back_buf->sync);
    back_buf->sync = EGL_NO_SYNC_KHR;
  }
  
  gint32 required_width = self->current_width;
  gint32 required_height = self->current_height;
  
  // Switch to mpv's isolated context for rendering
  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
  
  // Bind back buffer's FBO
  glBindFramebuffer(GL_FRAMEBUFFER, back_buf->fbo);
  
  // Render mpv frame to back buffer's texture
  mpv_opengl_fbo fbo{(gint32)back_buf->fbo, required_width, required_height, 0};
  int flip_y = 0;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };
  mpv_render_context_render(render_context, params);
  
  // Unbind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // Flush to ensure rendering commands are submitted
  glFlush();
  
  // Mark back buffer as ready (no sync needed here - Flutter will create one when reading)
  back_buf->ready = TRUE;
}

void texture_gl_swap_buffers(TextureGL* self) {
  // This is called from the GL render thread after rendering is complete
  
  g_mutex_lock(&self->swap_mutex);
  
  int back_idx = self->back_index;
  RenderBuffer* back_buf = &self->buffers[back_idx];
  
  // Only swap if back buffer has valid content
  if (back_buf->ready) {
    // Atomically swap front and back indices
    int old_front = self->front_index.load();
    self->front_index.store(back_idx);
    self->back_index = old_front;
    
    // Mark that Flutter texture needs to rebind to new front buffer
    self->needs_flutter_texture_update = TRUE;
  }
  
  g_mutex_unlock(&self->swap_mutex);
}

gboolean texture_gl_populate_texture(FlTextureGL* texture,
                                     guint32* target,
                                     guint32* name,
                                     guint32* width,
                                     guint32* height,
                                     GError** error) {
  TextureGL* self = TEXTURE_GL(texture);
  VideoOutput* video_output = self->video_output;
  GLRenderThread* gl_thread = video_output_get_gl_render_thread(video_output);
  
  // Asynchronously trigger initialization on first call (non-blocking)
  if (!self->initialization_posted && self->buffers[0].fbo == 0) {
    gint64 required_width = video_output_get_width(video_output);
    gint64 required_height = video_output_get_height(video_output);
    
    if (required_width > 0 && required_height > 0 && gl_thread) {
      self->initialization_posted = TRUE;
      video_output_notify_render(video_output);
    }
  }
  
  // Lock to safely access front buffer and its sync
  g_mutex_lock(&self->swap_mutex);
  
  // Get current front buffer
  int front_idx = self->front_index.load();
  RenderBuffer* front_buf = &self->buffers[front_idx];
  
  // Update Flutter's texture from EGLImage if resize happened or buffer swapped
  if (self->needs_flutter_texture_update && front_buf->egl_image != EGL_NO_IMAGE_KHR) {
    EGLDisplay egl_display = video_output_get_egl_display(video_output);
    
    // Destroy any old sync on this buffer (from previous Flutter read cycle)
    if (front_buf->sync != EGL_NO_SYNC_KHR) {
      _eglDestroySyncKHR(egl_display, front_buf->sync);
      front_buf->sync = EGL_NO_SYNC_KHR;
    }
    
    // Free previous Flutter texture
    if (self->flutter_texture != 0) {
      glDeleteTextures(1, &self->flutter_texture);
    }
    
    // Create Flutter's texture from current front buffer's EGLImage
    glGenTextures(1, &self->flutter_texture);
    glBindTexture(GL_TEXTURE_2D, self->flutter_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, front_buf->egl_image);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Create sync fence to mark that Flutter is now using this buffer
    // mpv will check this sync before writing to this buffer again
    front_buf->sync = _eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
    
    self->needs_flutter_texture_update = FALSE;
    
    // Notify Flutter about dimension change (unlock before callback to avoid deadlock)
    g_mutex_unlock(&self->swap_mutex);
    video_output_notify_texture_update(video_output);
    
    *target = GL_TEXTURE_2D;
    *name = self->flutter_texture;
    *width = self->current_width;
    *height = self->current_height;
    
    return TRUE;
  }
  
  g_mutex_unlock(&self->swap_mutex);
  
  *target = GL_TEXTURE_2D;
  *name = self->flutter_texture;
  *width = self->current_width;
  *height = self->current_height;
  
  if (self->flutter_texture == 0) {
    // First frame not yet available - create dummy texture in Flutter's context
    glGenTextures(1, &self->flutter_texture);
    glBindTexture(GL_TEXTURE_2D, self->flutter_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    *name = self->flutter_texture;
    *width = 1;
    *height = 1;
  }
  
  return TRUE;
}
