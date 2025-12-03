// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef TEXTURE_GL_H_
#define TEXTURE_GL_H_

#include <flutter_linux/flutter_linux.h>

#include "video_output.h"

#define TEXTURE_GL_TYPE (texture_gl_get_type())

G_DECLARE_FINAL_TYPE(TextureGL, texture_gl, TEXTURE_GL, TEXTURE_GL, FlTextureGL)

#define TEXTURE_GL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), texture_gl_get_type(), TextureGL))

TextureGL* texture_gl_new(VideoOutput* video_output);

/**
 * @brief Checks if texture needs resize and performs it if necessary.
 * This manages double buffering - creates/resizes both front and back buffers.
 */
void texture_gl_check_and_resize(TextureGL* self, gint64 required_width, gint64 required_height);

/**
 * @brief Renders mpv frame to the back buffer (called from dedicated thread).
 * Uses EGLSync for synchronization instead of glFinish.
 */
void texture_gl_render(TextureGL* self);

/**
 * @brief Swaps front and back buffers after rendering is complete.
 * Called from dedicated GL thread after render finishes.
 */
void texture_gl_swap_buffers(TextureGL* self);

/**
 * @brief Populates texture with video frame from the current front buffer.
 */
gboolean texture_gl_populate_texture(FlTextureGL* texture,
                                     guint32* target,
                                     guint32* name,
                                     guint32* width,
                                     guint32* height,
                                     GError** error);

#endif  // TEXTURE_GL_H_
