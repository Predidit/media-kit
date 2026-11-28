// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2026 Predidit.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef MAILBOX_SWAP_CHAIN_H_
#define MAILBOX_SWAP_CHAIN_H_

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

#include <atomic>
#include <cstdint>
#include <memory>

// Double-buffered D3D11 render targets backed by a single persistent DXGI
// shared presentation texture.
//
// Why a single persistent shared HANDLE is required for Flutter on Windows:
// Flutter's Windows ExternalTextureD3d caches the ANGLE EGLSurface by comparing
// the incoming descriptor handle against |last_surface_handle_|. If a plugin
// rotates multiple DXGI shared handles across frames, |handle !=
// last_surface_handle_| fires on every single video frame, forcing Flutter to
// tear down and recreate the EGLSurface (OpenSharedResource + eglBindTexImage)
// 24-60 times per second. On Intel Iris Xe drivers this causes severe 3D GPU
// overhead and runaway shared GPU memory growth that persists until process
// exit.
//
// Instead, libmpv renders into two private (non-shared) render targets in
// round-robin order, and ProducerCommit() issues an intra-device GPU-to-GPU
// CopyResource() into |present_texture_| followed by Flush(). ConsumerAcquire()
// always returns the same stable |present_shared_handle_| for a given texture
// size, so Flutter binds the shared surface once per Resize().
class MailboxSwapChain final {
 public:
  // device must outlive the returned swap chain.
  static HRESULT Create(ID3D11Device* device,
                        int32_t width,
                        int32_t height,
                        std::unique_ptr<MailboxSwapChain>* out);

  ~MailboxSwapChain();

  // Render target of the frame being produced, passed to libmpv as
  // mpv_d3d11_fbo::tex. Borrowed pointer, valid until the next
  // ProducerCommit() or Resize(). Producer thread only.
  ID3D11Texture2D* ProducerTexture() const {
    return render_slots_[write_slot_].Get();
  }

  // Called from the producer thread after mpv_render_context_render returns.
  // Copies the freshly rendered frame into the persistent shared presentation
  // texture, flushes the immediate context so the write is visible to Flutter's
  // D3D11 device, and advances write_slot_ for the next frame.
  void ProducerCommit();

  // Called from the consumer thread (Flutter GpuSurfaceTexture callback).
  // Returns the persistent DXGI shared HANDLE of |present_texture_| via a
  // single atomic acquire load.
  HANDLE ConsumerAcquire();

  // Recreates all textures at the new dimensions.
  // Must only be called from the producer thread with no active consumer.
  HRESULT Resize(int32_t width, int32_t height);

  // Returns the persistent shared HANDLE without mutating state.
  // Safe to call before the consumer thread starts.
  HANDLE ReadHandleSnapshot() const {
    return present_shared_handle_.load(std::memory_order_acquire);
  }

  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

 private:
  MailboxSwapChain() = default;

  MailboxSwapChain(const MailboxSwapChain&) = delete;
  MailboxSwapChain& operator=(const MailboxSwapChain&) = delete;

  HRESULT AllocateSlots();
  void ReleaseSlots();

  ID3D11Device* device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;

  int32_t width_ = 1;
  int32_t height_ = 1;

  // Private non-shared render targets alternated by libmpv so the next frame
  // can start rendering without waiting on the previous CopyResource().
  Microsoft::WRL::ComPtr<ID3D11Texture2D> render_slots_[2];

  // Single persistent DXGI shared texture exported to Flutter for the lifetime
  // of the current (width_, height_).
  Microsoft::WRL::ComPtr<ID3D11Texture2D> present_texture_;
  std::atomic<HANDLE> present_shared_handle_{nullptr};

  int write_slot_ = 0;  // producer-private
};

#endif  // MAILBOX_SWAP_CHAIN_H_
