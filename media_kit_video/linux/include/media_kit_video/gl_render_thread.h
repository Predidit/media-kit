// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2025 & onwards, Predidit.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef GL_RENDER_THREAD_H_
#define GL_RENDER_THREAD_H_

#include <glib.h>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>

class GLRenderThread {
 public:
  GLRenderThread();
  ~GLRenderThread();

  // Prevent copying and moving to avoid resource management issues
  GLRenderThread(const GLRenderThread&) = delete;
  GLRenderThread& operator=(const GLRenderThread&) = delete;
  GLRenderThread(GLRenderThread&&) = delete;
  GLRenderThread& operator=(GLRenderThread&&) = delete;

  // Post a task to the GL render thread (asynchronous)
  // Returns false if thread is shutting down.
  bool Post(std::function<void()> task);

  // Post a task and wait for completion (synchronous)
  // If called from the GL thread itself, runs inline to avoid deadlock.
  // Returns false if thread is shutting down.
  bool PostAndWait(std::function<void()> task);
  
  // Check if we're on the GL render thread
  bool IsCurrentThread() const;
  
  // Request graceful shutdown (does not wait).
  void RequestShutdown();

  // Best-effort running check (thread may be exiting).
  bool IsRunning() const;

 private:
  void Run();

  std::thread thread_;
  std::thread::id thread_id_;
  std::queue<std::function<void()>> tasks_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> stop_;
  std::atomic<bool> running_;
};

#endif  // GL_RENDER_THREAD_H_
