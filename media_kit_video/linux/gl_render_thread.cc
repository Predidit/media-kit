// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2025 & onwards, Predidit.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "include/media_kit_video/gl_render_thread.h"
#include <pthread.h>
#include <sched.h>
 
GLRenderThread::GLRenderThread() : stop_(false), running_(false) {
  thread_ = std::thread([this]() { Run(); });
  pthread_t thread_handle = thread_.native_handle();
  struct sched_param params;
  params.sched_priority = sched_get_priority_max(SCHED_OTHER);
  pthread_setschedparam(thread_handle, SCHED_OTHER, &params);
}

GLRenderThread::~GLRenderThread() {
  RequestShutdown();
  if (thread_.joinable()) {
    thread_.join();
  }
}

bool GLRenderThread::Post(std::function<void()> task) {
  if (stop_.load(std::memory_order_acquire)) {
    return false;
  }
  
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load(std::memory_order_acquire)) {
      return false;
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
  return true;
}

bool GLRenderThread::PostAndWait(std::function<void()> task) {
  if (stop_.load(std::memory_order_acquire)) {
    return false;
  }

  // Avoid deadlock if a task synchronously posts to itself.
  if (IsCurrentThread()) {
    task();
    return true;
  }
  
  std::mutex wait_mutex;
  std::condition_variable wait_cv;
  bool done = false;

  bool posted = Post([&]() {
    task();
    {
      std::lock_guard<std::mutex> lock(wait_mutex);
      done = true;
    }
    wait_cv.notify_one();
  });

  if (!posted) {
    return false;
  }

  std::unique_lock<std::mutex> lock(wait_mutex);
  wait_cv.wait(lock, [&]() { return done; });
  return true;
}

bool GLRenderThread::IsCurrentThread() const {
  return std::this_thread::get_id() == thread_id_;
}

void GLRenderThread::RequestShutdown() {
  stop_.store(true, std::memory_order_release);
  cv_.notify_all();
}

bool GLRenderThread::IsRunning() const {
  return running_.load(std::memory_order_acquire);
}

void GLRenderThread::Run() {
  // Store thread ID and mark as started
  {
    std::lock_guard<std::mutex> lock(mutex_);
    thread_id_ = std::this_thread::get_id();
    running_.store(true, std::memory_order_release);
  }
  cv_.notify_all();
  
  // Main loop
  while (true) {
    std::function<void()> task;
    
    {
      std::unique_lock<std::mutex> lock(mutex_);
      
      cv_.wait(lock, [this]() { 
        return stop_.load(std::memory_order_acquire) || !tasks_.empty(); 
      });
      
      if (stop_.load(std::memory_order_acquire) && tasks_.empty()) {
        break;
      }
      
      if (!tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
      }
    }
    
    // Execute task outside of lock
    if (task) {
      task();
    }
  }

  running_.store(false, std::memory_order_release);
}
