// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "include/media_kit_video/texture_gl.h"

#include <epoxy/gl.h>
#include <epoxy/egl.h>

struct _TextureGL {
  FlTextureGL parent_instance;
  guint32 name;
  guint32 fbo;
  guint32 current_width;
  guint32 current_height;
  VideoOutput* video_output;
  EGLSync render_sync; /* EGL sync object for frame synchronization */
};

G_DEFINE_TYPE(TextureGL, texture_gl, fl_texture_gl_get_type())

static void texture_gl_init(TextureGL* self) {
  self->name = 0;
  self->fbo = 0;
  self->current_width = 1;
  self->current_height = 1;
  self->video_output = NULL;
  self->render_sync = EGL_NO_SYNC_KHR;
}

static void texture_gl_dispose(GObject* object) {
  TextureGL* self = TEXTURE_GL(object);
  
  // Clean up OpenGL resources only if we have a valid video_output and EGL context
  if (self->video_output != NULL && !video_output_is_destroyed(self->video_output)) {
    EGLDisplay egl_display = video_output_get_egl_display(self->video_output);
    EGLContext mpv_context = video_output_get_egl_context(self->video_output);
    EGLSurface mpv_surface = video_output_get_egl_surface(self->video_output);
    
    if (mpv_context != EGL_NO_CONTEXT && egl_display != EGL_NO_DISPLAY && mpv_surface != EGL_NO_SURFACE) {
      // Save current context
      EGLContext current_context = eglGetCurrentContext();
      EGLSurface current_draw = eglGetCurrentSurface(EGL_DRAW);
      EGLSurface current_read = eglGetCurrentSurface(EGL_READ);
      
      // Try to make our context current for cleanup
      if (eglMakeCurrent(egl_display, mpv_surface, mpv_surface, mpv_context) == EGL_TRUE) {
        // Clean up sync object
        if (self->render_sync != EGL_NO_SYNC_KHR) {
          eglDestroySyncKHR(egl_display, self->render_sync);
          self->render_sync = EGL_NO_SYNC_KHR;
        }
        
        if (self->name != 0) {
          glDeleteTextures(1, &self->name);
          self->name = 0;
        }
        if (self->fbo != 0) {
          glDeleteFramebuffers(1, &self->fbo);
          self->fbo = 0;
        }
        
        // Restore previous context
        if (current_context != EGL_NO_CONTEXT) {
          eglMakeCurrent(egl_display, current_draw, current_read, current_context);
        }
      } else {
        // If we can't make context current, just reset the handles
        g_warning("media_kit: Failed to make EGL context current during texture cleanup");
        self->render_sync = EGL_NO_SYNC_KHR;
        self->name = 0;
        self->fbo = 0;
      }
    } else {
      // EGL context is invalid, just reset the handles
      self->render_sync = EGL_NO_SYNC_KHR;
      self->name = 0;
      self->fbo = 0;
    }
  } else {
    // VideoOutput is destroyed or invalid, just reset the handles
    self->render_sync = EGL_NO_SYNC_KHR;
    self->name = 0;
    self->fbo = 0;
  }
  
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
  TextureGL* self = TEXTURE_GL(g_object_new(texture_gl_get_type(), NULL));
  self->video_output = video_output;
  return self;
}

gboolean texture_gl_populate_texture(FlTextureGL* texture,
                                     guint32* target,
                                     guint32* name,
                                     guint32* width,
                                     guint32* height,
                                     GError** error) {
  TextureGL* self = TEXTURE_GL(texture);
  VideoOutput* video_output = self->video_output;
  
  // Safety check: ensure video_output is valid
  if (video_output == NULL) {
    *target = GL_TEXTURE_2D;
    *name = 0;
    *width = 1;
    *height = 1;
    return TRUE;
  }
  
  // Lock to prevent concurrent access during texture updates
  video_output_lock(video_output);
  
  // Check if VideoOutput is being destroyed
  if (video_output_is_destroyed(video_output)) {
    video_output_unlock(video_output);
    *target = GL_TEXTURE_2D;
    *name = self->name ? self->name : 0;
    *width = self->current_width;
    *height = self->current_height;
    return TRUE;
  }
  
  // Get current EGL state to restore later
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  EGLContext flutter_context = eglGetCurrentContext();
  EGLSurface flutter_draw_surface = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface flutter_read_surface = eglGetCurrentSurface(EGL_READ);
  
  // Switch to mpv's EGL context for rendering
  EGLContext mpv_context = video_output_get_egl_context(video_output);
  EGLSurface mpv_surface = video_output_get_egl_surface(video_output);
  
  gboolean context_switched = FALSE;
  gboolean should_switch_context = (flutter_context != mpv_context);
  
  if (mpv_context != EGL_NO_CONTEXT && egl_display != EGL_NO_DISPLAY && mpv_surface != EGL_NO_SURFACE) {
    if (should_switch_context) {
      if (eglMakeCurrent(egl_display, mpv_surface, mpv_surface, mpv_context) == EGL_TRUE) {
        context_switched = TRUE;
      } else {
        // eglMakeCurrent failed, log error and skip rendering
        EGLint egl_error = eglGetError();
        g_warning("media_kit: eglMakeCurrent failed with error 0x%x", egl_error);
        video_output_unlock(video_output);
        *target = GL_TEXTURE_2D;
        *name = self->name ? self->name : 0;
        *width = self->current_width;
        *height = self->current_height;
        return TRUE;
      }
    } else {
      // Already in the correct context
      context_switched = FALSE;
    }
  }
  
  gint32 required_width = (guint32)video_output_get_width(video_output);
  gint32 required_height = (guint32)video_output_get_height(video_output);
  
  if (required_width > 0 && required_height > 0) {
    gboolean first_frame = self->name == 0 || self->fbo == 0;
    gboolean resize = self->current_width != required_width ||
                      self->current_height != required_height;
    if (first_frame || resize) {
      // Free previous texture & FBO
      if (!first_frame) {
        glDeleteTextures(1, &self->name);
        glDeleteFramebuffers(1, &self->fbo);
      }
      // Create new texture & FBO
      glGenFramebuffers(1, &self->fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, self->fbo);
      glGenTextures(1, &self->name);
      glBindTexture(GL_TEXTURE_2D, self->name);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, required_width, required_height,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      // Attach the texture to the FBO
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, self->name, 0);
      self->current_width = required_width;
      self->current_height = required_height;
      // Unbind FBO immediately after creation
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // Notify Flutter about the change in texture's dimensions
      video_output_notify_texture_update(video_output);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, self->fbo);
    
    mpv_render_context* render_context =
        video_output_get_render_context(video_output);
    
    // Check if render_context is still valid
    if (render_context != NULL) {
      // Wait for previous frame sync before rendering new frame
      if (self->render_sync != EGL_NO_SYNC_KHR) {
        EGLint wait_result = eglClientWaitSyncKHR(egl_display, self->render_sync, 
                                                   EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 
                                                   16666666); // ~16ms timeout (60fps)
        if (wait_result == EGL_FALSE || wait_result == EGL_TIMEOUT_EXPIRED_KHR) {
          g_warning("media_kit: Previous frame sync timeout or failed");
        }
        eglDestroySyncKHR(egl_display, self->render_sync);
        self->render_sync = EGL_NO_SYNC_KHR;
      }
      
      // Render the frame
      mpv_opengl_fbo fbo{(gint32)self->fbo, required_width, required_height, 0};
      int flip_y = 0;
      mpv_render_param params[] = {
          {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
          {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
          {MPV_RENDER_PARAM_INVALID, NULL},
      };
      mpv_render_context_render(render_context, params);
      
      // Create sync object to ensure rendering completes before Flutter uses the texture
      // This prevents flickering when Flutter's compositor reads from the texture
      // while mpv is still rendering to it
      glFlush();
      self->render_sync = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
      if (self->render_sync == EGL_NO_SYNC_KHR) {
        // Fallback to glFinish if sync creation fails
        glFinish();
      }
    }
    
    // Unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  
  // Restore Flutter's EGL context only if we successfully switched
  if (context_switched && flutter_context != EGL_NO_CONTEXT && egl_display != EGL_NO_DISPLAY) {
    if (eglMakeCurrent(egl_display, flutter_draw_surface, flutter_read_surface, flutter_context) != EGL_TRUE) {
      // Log error but continue - we've already finished our rendering
      EGLint egl_error = eglGetError();
      g_warning("media_kit: Failed to restore Flutter EGL context with error 0x%x", egl_error);
    }
  }
  
  *target = GL_TEXTURE_2D;
  *name = self->name;
  *width = self->current_width;
  *height = self->current_height;
  
  if (self->name == 0 && self->fbo == 0) {
    // This means that required_width > 0 && required_height > 0 code-path
    // hasn't been executed yet (because first frame isn't available yet).
    // Just creating a dummy texture; prevent Flutter from complaining.
    glGenTextures(1, &self->name);
    glBindTexture(GL_TEXTURE_2D, self->name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    *name = self->name;
    *width = 1;
    *height = 1;
  }
  
  // Unlock before returning
  video_output_unlock(video_output);
  
  return TRUE;
}
