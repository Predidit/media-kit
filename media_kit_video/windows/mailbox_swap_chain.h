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
#include <d3d11_4.h>
#include <dxgi.h>
#include <wrl.h>

#include <atomic>
#include <cstdint>
#include <memory>

// Lock-free 4-slot mailbox of render targets with a last-completed-frame
// cache.  The producer hands ProducerTexture() to libmpv as the D3D11 render
// target of each frame; the consumer reads the newest GPU-complete slot.
//
// Four BGRA8 textures are kept, each with a DXGI shared HANDLE.
// mailbox_state_ is a single atomic<uint32_t>:
//   bits [1:0] = free_slot         (0-3): producer takes this for the next frame
//   bits [3:2] = completed_slot    (0-3): most recent fence-confirmed frame;
//                                          safe Consumer fallback at any time
//   bits [5:4] = pending_or_extra  (0-3): has_pending=1 → latest submitted frame
//                                          (fence may still be in-flight);
//                                          has_pending=0 → second free slot
//   bit  [6]   = has_pending            : 1 = a new frame is waiting to be consumed
//
// 4-slot invariant (all roles are always distinct):
//   has_pending=1: write_slot_(private) | free | pending | completed  = 4 slots
//   has_pending=0: write_slot_(private) | free | extra_free | completed = 4 slots
//
// Initial value 57u = 0b0_11_10_01:
//   has_pending=0, extra_free=3, completed=2, free=1, write_slot_=0 (private)
class MailboxSwapChain final {
 public:
  // device must outlive the returned mailbox.
  static HRESULT Create(ID3D11Device* device,
                        int32_t width,
                        int32_t height,
                        std::unique_ptr<MailboxSwapChain>* out);

  ~MailboxSwapChain();

  // Render target of the frame being produced, passed to libmpv as
  // mpv_d3d11_fbo::tex.  Borrowed pointer, valid until the next
  // ProducerCommit() or Resize().  Producer thread only.
  ID3D11Texture2D* ProducerTexture() const {
    return slots_[write_slot_].texture.Get();
  }

  // Called from the producer thread after mpv_render_context_render returns.
  // In addition to publishing write_slot_ as the new pending frame, it
  // non-blockingly polls the *previous* pending frame's fence and, if the GPU
  // has already completed it, promotes it to completed and updates
  // latest_completed_slot_ (release store).  This is the sole site that
  // advances latest_completed_slot_; ConsumerAcquire never touches the fence.
  void ProducerCommit();

  // Called from the consumer thread (Flutter GpuSurfaceTexture callback).
  // Returns the DXGI shared HANDLE of the most recent fence-confirmed frame.
  // Implementation is a single acquire load of latest_completed_slot_ —
  // no CAS, no fence poll, no flush, no stall, no KeyedMutex.
  HANDLE ConsumerAcquire();

  // Recreates all texture slots at the new dimensions.
  // Must only be called from the producer thread with no active consumer.
  HRESULT Resize(int32_t width, int32_t height);

  // Returns the latest GPU-confirmed HANDLE without advancing mailbox state.
  // Safe to call before the consumer thread starts.
  HANDLE ReadHandleSnapshot() const {
    return slots_[latest_completed_slot_.load(std::memory_order_acquire)]
        .shared_handle;
  }

  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

 private:
  MailboxSwapChain() = default;

  MailboxSwapChain(const MailboxSwapChain&) = delete;
  MailboxSwapChain& operator=(const MailboxSwapChain&) = delete;

  HRESULT AllocateSlots();
  void ReleaseSlots();

  struct TextureSlot {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HANDLE shared_handle = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Fence> fence;
    uint64_t fence_value = 0;
  };

  ID3D11Device* device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4_;

  int32_t width_ = 1;
  int32_t height_ = 1;

  TextureSlot slots_[4];

  // Lock-free mailbox state — see bit-field comment at top of class.
  // Initial value 57u = 0b0_11_10_01.
  std::atomic<uint32_t> mailbox_state_{57u};

  // Cache of the most recently fence-confirmed completed slot.
  // ConsumerAcquire reads this directly (one atomic load, no CAS, no fence
  // poll).  Updated by ProducerCommit after a successful non-blocking
  // pending→completed promotion.  Initialised to 2, which matches the
  // 'completed' field in mailbox_state_'s initial value 57u.
  std::atomic<int> latest_completed_slot_{2};

  int write_slot_ = 0;  // producer-private
};

#endif  // MAILBOX_SWAP_CHAIN_H_