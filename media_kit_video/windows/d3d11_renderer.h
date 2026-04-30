// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © Predidit.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef D3D11_RENDERER_H_
#define D3D11_RENDERER_H_

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

#include <cstdint>
#include <iostream>

#include "mailbox_swap_chain.h"
#include "utils.h"

// D3D11Renderer creates a D3D11 device and owns a MailboxSwapChain that
// implements the lock-free triple-buffer mailbox between the libmpv rendering
// thread (producer) and Flutter's render thread (consumer).
//
// The MailboxSwapChain is passed directly to mpv as the IDXGISwapChain* in
// mpv_dxgi_init_params.  mpv calls GetBuffer(0, ...) to obtain a render
// target, renders into it, and flushes.  The plugin then calls
// ProducerCommit() to atomically publish the frame.  Flutter's
// GpuSurfaceTexture callback calls ConsumerAcquire() to receive the DXGI
// shared HANDLE of the newest complete frame — with no copy and no OS lock.
class D3D11Renderer {
 public:
  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

  // Raw device pointer used by VideoOutput to populate mpv_dxgi_init_params.
  ID3D11Device* device() const { return d3d_11_device_; }

  // IDXGISwapChain* facade backed by MailboxSwapChain.
  // Passed to mpv as mpv_dxgi_init_params::swapchain (void*).
  IDXGISwapChain* swap_chain() const { return mailbox_swap_chain_.Get(); }

  D3D11Renderer(int32_t width, int32_t height);
  ~D3D11Renderer();

  // Recreates the three mailbox slots at the new dimensions.
  // Must be called from the producer thread only.
  void SetSize(int32_t width, int32_t height);

  // Called from the producer thread (mpv thread pool) after
  // mpv_render_context_render returns.  Publishes the rendered frame.
  void ProducerCommit();

  // Called from the consumer thread (Flutter GpuSurfaceTexture callback).
  // Returns the DXGI shared HANDLE of the most recent complete frame.
  HANDLE ConsumerAcquire();

  // Returns the DXGI shared HANDLE for the current read slot without
  // advancing mailbox state.  Used once during texture registration before
  // the consumer thread starts.
  HANDLE ReadHandleSnapshot() const;

 private:
  bool CreateD3D11Device();
  bool CreateMailbox();

  int32_t width_ = 1;
  int32_t height_ = 1;

  ID3D11Device* d3d_11_device_ = nullptr;
  ID3D11DeviceContext* d3d_11_device_context_ = nullptr;

  Microsoft::WRL::ComPtr<MailboxSwapChain> mailbox_swap_chain_;

  static int instance_count_;
};

#endif  // D3D11_RENDERER_H_
