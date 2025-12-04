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

// Number of buffers for triple buffering
#define NUM_BUFFERS 3

// EGLImage extension function pointers
typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay dpy, EGLImageKHR image);
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);

// EGL_KHR_fence_sync extension function pointers
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
    
    // EGL_KHR_fence_sync extensions
    _eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    _eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    _eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    
    initialized = TRUE;
  }
}

// Buffer structure for triple buffering with sequence numbers
typedef struct {
  guint32 fbo;              // FBO for mpv rendering
  guint32 texture;          // Texture attached to FBO
  EGLImageKHR egl_image;    // EGLImage for sharing between contexts
  EGLSyncKHR render_sync;   // Sync created after mpv render completes
  std::atomic<guint64> seq; // Sequence number of the frame in this buffer (0 = empty)
} RenderBuffer;

struct _TextureGL {
  FlTextureGL parent_instance;
  guint32 flutter_textures[NUM_BUFFERS];  // Flutter's textures, one per buffer
  gboolean flutter_textures_valid[NUM_BUFFERS];  // Whether Flutter textures are valid (need recreate after resize)
  RenderBuffer buffers[NUM_BUFFERS];  // Triple buffer array
  std::atomic<guint64> producer_seq;  // Next sequence number to assign (producer side)
  std::atomic<guint64> display_seq;   // Seq of buffer currently being displayed (protects from producer overwrite)
  guint64 consumer_seq;               // Last consumed sequence number (consumer side, main thread only)
  int write_index;                    // Index of buffer mpv is currently writing to (GL thread only)
  guint32 current_width;
  guint32 current_height;
  gboolean buffers_initialized;  // Flag to check if buffers are created
  gboolean initialization_posted;  // Flag to avoid duplicate initialization
  std::atomic<gboolean> resizing;  // Flag to indicate resize in progress
  GMutex resize_mutex;            // Mutex for resize synchronization only
  VideoOutput* video_output;
};

G_DEFINE_TYPE(TextureGL, texture_gl, fl_texture_gl_get_type())

static void texture_gl_init(TextureGL* self) {
  for (int i = 0; i < NUM_BUFFERS; i++) {
    self->flutter_textures[i] = 0;
    self->flutter_textures_valid[i] = FALSE;
    self->buffers[i].fbo = 0;
    self->buffers[i].texture = 0;
    self->buffers[i].egl_image = EGL_NO_IMAGE_KHR;
    self->buffers[i].render_sync = EGL_NO_SYNC_KHR;
    self->buffers[i].seq.store(0);  // 0 means empty/no frame
  }
  self->producer_seq.store(1);  // Start from 1 (0 means empty)
  self->display_seq.store(0);   // No buffer being displayed yet
  self->consumer_seq = 0;       // Consumer hasn't seen any frame yet
  self->write_index = 0;
  self->current_width = 1;
  self->current_height = 1;
  self->buffers_initialized = FALSE;
  self->initialization_posted = FALSE;
  self->resizing.store(FALSE);
  g_mutex_init(&self->resize_mutex);
  self->video_output = NULL;
}

static void texture_gl_dispose(GObject* object) {
  TextureGL* self = TEXTURE_GL(object);
  VideoOutput* video_output = self->video_output;
  GLRenderThread* gl_thread = video_output_get_gl_render_thread(video_output);
  
  // Clean up Flutter's textures
  for (int i = 0; i < NUM_BUFFERS; i++) {
    if (self->flutter_textures[i] != 0) {
      glDeleteTextures(1, &self->flutter_textures[i]);
      self->flutter_textures[i] = 0;
    }
  }
  
  // Clean up triple buffer resources in dedicated GL thread
  if (video_output != NULL && gl_thread != NULL) {
    gl_thread->PostAndWait([self, video_output]() {
      EGLDisplay egl_display = video_output_get_egl_display(video_output);
      EGLContext egl_context = video_output_get_egl_context(video_output);
      
      // Clean up all buffers
      for (int i = 0; i < NUM_BUFFERS; i++) {
        RenderBuffer* buf = &self->buffers[i];
        
        // Clean up EGLSyncKHR
        if (buf->render_sync != EGL_NO_SYNC_KHR) {
          _eglDestroySyncKHR(egl_display, buf->render_sync);
          buf->render_sync = EGL_NO_SYNC_KHR;
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
        }
      }
    });
  }
  
  g_mutex_clear(&self->resize_mutex);
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

/// This function is called from the dedicated rendering thread
/// So we can directly perform OpenGL operations
void texture_gl_check_and_resize(TextureGL* self, gint64 required_width, gint64 required_height) {
  VideoOutput* video_output = self->video_output;
  
  if (required_width < 1 || required_height < 1) {
    return;
  }
  
  gboolean first_frame = !self->buffers_initialized;
  gboolean resize = self->current_width != (guint32)required_width ||
                    self->current_height != (guint32)required_height;
  
  if (!first_frame && !resize) {
    return;  // No resize needed
  }
  
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  EGLContext egl_context = video_output_get_egl_context(video_output);
  
  // Switch to mpv's isolated context
  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
  
  // Mark as resizing and lock to prevent concurrent access
  self->resizing.store(TRUE, std::memory_order_release);
  g_mutex_lock(&self->resize_mutex);
  
  // Free previous resources for all buffers
  for (int i = 0; i < NUM_BUFFERS; i++) {
    RenderBuffer* buf = &self->buffers[i];
    
    if (!first_frame) {
      // Wait for any pending sync before destroying resources
      if (buf->render_sync != EGL_NO_SYNC_KHR) {
        _eglClientWaitSyncKHR(egl_display, buf->render_sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
        _eglDestroySyncKHR(egl_display, buf->render_sync);
        buf->render_sync = EGL_NO_SYNC_KHR;
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
    
    buf->render_sync = EGL_NO_SYNC_KHR;
  }
  
  // Flush to ensure textures are ready
  glFlush();
  
  // Reset sequence numbers for triple buffering
  for (int i = 0; i < NUM_BUFFERS; i++) {
    self->buffers[i].seq.store(0, std::memory_order_release);  // Mark all buffers as empty
  }
  self->producer_seq.store(1, std::memory_order_release);  // Reset producer sequence
  self->display_seq.store(0, std::memory_order_release);   // Reset display sequence
  self->consumer_seq = 0;  // Reset consumer sequence
  self->write_index = 0;
  
  // Mark Flutter textures as invalid
  for (int i = 0; i < NUM_BUFFERS; i++) {
    self->flutter_textures_valid[i] = FALSE;
  }
  
  // Mark buffers as initialized and update dimensions
  self->buffers_initialized = TRUE;
  self->current_width = required_width;
  self->current_height = required_height;
  
  g_mutex_unlock(&self->resize_mutex);
  self->resizing.store(FALSE, std::memory_order_release);
}

// Helper function to select the best write buffer based on current state
// Returns the index of the buffer to write to, or -1 if no safe buffer available
static int select_write_buffer(TextureGL* self) {
  guint64 current_display_seq = self->display_seq.load(std::memory_order_acquire);
  int best_idx = -1;
  guint64 min_seq = UINT64_MAX;
  
  for (int i = 0; i < NUM_BUFFERS; i++) {
    guint64 buf_seq = self->buffers[i].seq.load(std::memory_order_acquire);
    // NEVER select the buffer currently being displayed by consumer
    if (buf_seq == current_display_seq && current_display_seq != 0) continue;
    if (buf_seq < min_seq) {
      min_seq = buf_seq;
      best_idx = i;
    }
  }
  
  // No fallback that ignores display_seq - we must protect the display buffer
  // If best_idx is still -1, it means all non-display buffers have the same seq
  // This can only happen with 3 buffers when display has one and other two are equal
  // In this case, pick any buffer that's not the display buffer
  if (best_idx == -1) {
    for (int i = 0; i < NUM_BUFFERS; i++) {
      guint64 buf_seq = self->buffers[i].seq.load(std::memory_order_acquire);
      if (buf_seq != current_display_seq || current_display_seq == 0) {
        best_idx = i;
        break;
      }
    }
  }
  
  return best_idx;
}

gboolean texture_gl_render(TextureGL* self) {
  VideoOutput* video_output = self->video_output;
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  EGLContext egl_context = video_output_get_egl_context(video_output);
  mpv_render_context* render_context = video_output_get_render_context(video_output);
  
  if (!render_context) {
    return FALSE;
  }
  
  // Select the best write buffer based on current display_seq
  // This ensures we always pick a buffer that's not being displayed
  int write_idx = select_write_buffer(self);
  
  // If no safe buffer available (shouldn't happen with 3 buffers), skip this frame
  if (write_idx < 0) {
    return FALSE;
  }
  
  self->write_index = write_idx;
  RenderBuffer* write_buf = &self->buffers[write_idx];
  
  if (write_buf->fbo == 0) {
    return FALSE;
  }
  
  // Destroy old render_sync if exists (from previous render cycle on this buffer)
  if (write_buf->render_sync != EGL_NO_SYNC_KHR) {
    _eglDestroySyncKHR(egl_display, write_buf->render_sync);
    write_buf->render_sync = EGL_NO_SYNC_KHR;
  }
  
  gint32 required_width = self->current_width;
  gint32 required_height = self->current_height;
  
  // Switch to mpv's isolated context for rendering
  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
  
  // Bind write buffer's FBO
  glBindFramebuffer(GL_FRAMEBUFFER, write_buf->fbo);
  
  // Render mpv frame to write buffer's texture
  mpv_opengl_fbo fbo{(gint32)write_buf->fbo, required_width, required_height, 0};
  int flip_y = 0;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };
  mpv_render_context_render(render_context, params);
  
  // Unbind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // Flush to ensure rendering commands are submitted to GPU
  glFlush();
  
  // Create sync fence to mark render completion
  // Flutter will check this sync before reading to ensure rendering is done
  write_buf->render_sync = _eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
  
  return TRUE;
}

void texture_gl_swap_buffers(TextureGL* self) {
  // This is called from the GL render thread after rendering is complete
  // Assign sequence number to the just-rendered buffer to mark it as complete
  // The next write buffer will be selected in texture_gl_render before the next frame
  
  int write_idx = self->write_index;
  
  // Assign sequence number to the just-rendered buffer and increment
  guint64 current_seq = self->producer_seq.fetch_add(1, std::memory_order_acq_rel);
  self->buffers[write_idx].seq.store(current_seq, std::memory_order_release);
  
  // Note: write_index will be updated in texture_gl_render() before the next frame
  // This ensures we always select based on the latest display_seq
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
  EGLDisplay egl_display = video_output_get_egl_display(video_output);
  
  // Asynchronously trigger initialization on first call
  if (!self->initialization_posted && !self->buffers_initialized) {
    gint64 required_width = video_output_get_width(video_output);
    gint64 required_height = video_output_get_height(video_output);
    
    if (required_width > 0 && required_height > 0 && gl_thread) {
      self->initialization_posted = TRUE;
      video_output_notify_render(video_output);
    }
  }
  
  // If resize is in progress, return dummy texture to avoid accessing invalid buffers
  if (self->resizing.load(std::memory_order_acquire)) {
    *target = GL_TEXTURE_2D;
    static guint32 dummy_texture = 0;
    if (dummy_texture == 0) {
      glGenTextures(1, &dummy_texture);
      glBindTexture(GL_TEXTURE_2D, dummy_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    *name = dummy_texture;
    *width = 1;
    *height = 1;
    return TRUE;
  }
  
  // Find the buffer with the highest seq that is greater than consumer_seq
  // This implements proper consumer sampling - only switch when there's a newer frame
  int best_idx = -1;
  guint64 best_seq = self->consumer_seq;
  
  for (int i = 0; i < NUM_BUFFERS; i++) {
    guint64 buf_seq = self->buffers[i].seq.load(std::memory_order_acquire);
    if (buf_seq > best_seq) {
      RenderBuffer* buf = &self->buffers[i];
      
      // Check if rendering is complete for this buffer (non-blocking)
      gboolean render_complete = TRUE;
      if (buf->render_sync != EGL_NO_SYNC_KHR) {
        EGLint result = _eglClientWaitSyncKHR(egl_display, buf->render_sync, 0, 0);
        if (result == EGL_TIMEOUT_EXPIRED_KHR) {
          // Render not complete yet - skip this buffer
          render_complete = FALSE;
        } else {
          // Render complete, destroy the sync
          _eglDestroySyncKHR(egl_display, buf->render_sync);
          buf->render_sync = EGL_NO_SYNC_KHR;
        }
      }
      
      if (render_complete) {
        best_seq = buf_seq;
        best_idx = i;
      }
    }
  }
  
  // If we found a newer frame, update consumer_seq
  if (best_idx >= 0) {
    self->consumer_seq = best_seq;
  }
  
  // Determine display index: use best_idx if found, otherwise find any valid buffer
  int display_idx = best_idx;
  if (display_idx < 0) {
    // No new frame, find the buffer with highest seq (the most recent frame we've seen)
    guint64 max_seq = 0;
    for (int i = 0; i < NUM_BUFFERS; i++) {
      guint64 buf_seq = self->buffers[i].seq.load(std::memory_order_acquire);
      if (buf_seq > max_seq) {
        max_seq = buf_seq;
        display_idx = i;
      }
    }
  }
  
  // If still no valid buffer (no frames rendered yet), use buffer 0
  if (display_idx < 0) {
    display_idx = 0;
  }
  
  // Update display_seq to protect this buffer from being overwritten by producer
  guint64 selected_seq = self->buffers[display_idx].seq.load(std::memory_order_acquire);
  if (selected_seq > 0) {
    self->display_seq.store(selected_seq, std::memory_order_release);
  }
  
  RenderBuffer* display_buf = &self->buffers[display_idx];
  
  // Check if we need to create/recreate Flutter texture for this buffer
  if (!self->flutter_textures_valid[display_idx] && display_buf->egl_image != EGL_NO_IMAGE_KHR) {
    // Delete old texture if exists
    if (self->flutter_textures[display_idx] != 0) {
      glDeleteTextures(1, &self->flutter_textures[display_idx]);
    }
    
    // Create Flutter's texture from this buffer's EGLImage
    glGenTextures(1, &self->flutter_textures[display_idx]);
    glBindTexture(GL_TEXTURE_2D, self->flutter_textures[display_idx]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, display_buf->egl_image);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    self->flutter_textures_valid[display_idx] = TRUE;
    
    // Notify Flutter about texture availability
    video_output_notify_texture_update(video_output);
  }
  
  *target = GL_TEXTURE_2D;
  *name = self->flutter_textures[display_idx];
  *width = self->current_width;
  *height = self->current_height;
  
  if (!self->flutter_textures_valid[display_idx] || self->flutter_textures[display_idx] == 0) {
    // First frame not yet available - create dummy texture
    static guint32 dummy_texture = 0;
    if (dummy_texture == 0) {
      glGenTextures(1, &dummy_texture);
      glBindTexture(GL_TEXTURE_2D, dummy_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    *name = dummy_texture;
    *width = 1;
    *height = 1;
  }
  
  return TRUE;
}
