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

// Minimal IDXGISwapChain facade backed by a lock-free triple-buffer mailbox.
//
// Three BGRA8 textures are kept, each with a DXGI shared HANDLE.
// mailbox_state_ is a single atomic<uint32_t>:
//   bits [1:0]  slot index in the mailbox (0-2)
//   bit  [2]    dirty flag: 1 = producer has committed a new frame
//
// {write_slot_, mailbox slot, read_slot_} is always a permutation of {0,1,2}.
class MailboxSwapChain final : public IDXGISwapChain {
 public:
  // Returns an AddRef'd pointer (ref count = 1). device must outlive this.
  static HRESULT Create(ID3D11Device* device,
                        int32_t width,
                        int32_t height,
                        MailboxSwapChain** out);

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppv) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT,
                                           const void*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID,
                                                    const IUnknown*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetParent(REFIID, void**) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void**) override {
    return E_NOTIMPL;
  }

  // Present is a no-op: mpv drives presentation via done_frame / Flush.
  HRESULT STDMETHODCALLTYPE Present(UINT, UINT) override { return S_OK; }

  // Called by the libmpv DXGI back-end each frame to obtain a render target.
  HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer,
                                      REFIID riid,
                                      void** ppSurface) override;

  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) override;

  HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL,
                                               IDXGIOutput*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL*,
                                               IDXGIOutput**) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT, UINT, UINT, DXGI_FORMAT,
                                          UINT) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput**) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetFrameStatistics(
      DXGI_FRAME_STATISTICS*) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT*) override {
    return E_NOTIMPL;
  }

  // Called from the producer thread after mpv_render_context_render returns.
  void ProducerCommit();

  // Called from the consumer thread (Flutter GpuSurfaceTexture callback).
  // Returns the DXGI shared HANDLE of the most recent complete frame.
  HANDLE ConsumerAcquire();

  // Recreates all three texture slots at the new dimensions.
  // Must only be called from the producer thread with no active consumer.
  HRESULT Resize(int32_t width, int32_t height);

  // Returns the current read-slot HANDLE without advancing mailbox state.
  HANDLE ReadHandleSnapshot() const {
    return slots_[read_slot_].shared_handle;
  }

  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

 private:
  MailboxSwapChain() = default;
  ~MailboxSwapChain();

  MailboxSwapChain(const MailboxSwapChain&) = delete;
  MailboxSwapChain& operator=(const MailboxSwapChain&) = delete;

  HRESULT AllocateSlots();
  void ReleaseSlots();

  struct TextureSlot {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HANDLE shared_handle = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Fence> fence;
    HANDLE fence_event = nullptr;
    uint64_t fence_value = 0;
  };

  ID3D11Device* device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4_;

  int32_t width_ = 1;
  int32_t height_ = 1;

  TextureSlot slots_[3];

  // Lock-free mailbox: bits [1:0] = slot index (0-2), bit [2] = dirty; init = 2u.
  std::atomic<uint32_t> mailbox_state_{2u};

  int write_slot_ = 0;  // producer-private
  int read_slot_ = 1;   // consumer-private

  std::atomic<ULONG> ref_count_{1u};
};

#endif  // MAILBOX_SWAP_CHAIN_H_