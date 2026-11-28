// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2026 Predidit.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "mailbox_swap_chain.h"

#include <iostream>
#include <new>

MailboxSwapChain::~MailboxSwapChain() {
  ReleaseSlots();
}

HRESULT MailboxSwapChain::Create(ID3D11Device* device,
                                  int32_t width,
                                  int32_t height,
                                  std::unique_ptr<MailboxSwapChain>* out) {
  if (!device || !out) return E_INVALIDARG;

  std::unique_ptr<MailboxSwapChain> p(new (std::nothrow) MailboxSwapChain());
  if (!p) return E_OUTOFMEMORY;

  p->device_ = device;
  p->width_ = (width > 0) ? width : 1;
  p->height_ = (height > 0) ? height : 1;

  device->GetImmediateContext(&p->context_);
  if (!p->context_) {
    std::cout << "media_kit: MailboxSwapChain: GetImmediateContext failed."
              << std::endl;
    return E_FAIL;
  }

  const HRESULT hr = p->AllocateSlots();
  if (FAILED(hr)) {
    return hr;
  }

  *out = std::move(p);
  return S_OK;
}

void MailboxSwapChain::ProducerCommit() {
  if (!context_ || !present_texture_ || !render_slots_[write_slot_]) {
    return;
  }

  // D3D11 immediate-context commands execute in FIFO order on the GPU, so
  // CopyResource() is guaranteed to observe the completed mpv render without
  // any CPU wait. Flush() publishes the write to the shared allocation so
  // Flutter's D3D11 device sees the updated frame before
  // MarkTextureFrameAvailable() wakes the raster thread.
  context_->CopyResource(present_texture_.Get(),
                         render_slots_[write_slot_].Get());
  context_->Flush();

  write_slot_ = 1 - write_slot_;
}

HANDLE MailboxSwapChain::ConsumerAcquire() {
  return present_shared_handle_.load(std::memory_order_acquire);
}

HRESULT MailboxSwapChain::Resize(int32_t width, int32_t height) {
  ReleaseSlots();
  width_ = (width > 0) ? width : 1;
  height_ = (height > 0) ? height : 1;
  write_slot_ = 0;
  return AllocateSlots();
}

HRESULT MailboxSwapChain::AllocateSlots() {
  D3D11_TEXTURE2D_DESC render_desc = {};
  render_desc.Width = static_cast<UINT>(width_);
  render_desc.Height = static_cast<UINT>(height_);
  render_desc.MipLevels = 1;
  render_desc.ArraySize = 1;
  render_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  render_desc.SampleDesc.Count = 1;
  render_desc.SampleDesc.Quality = 0;
  render_desc.Usage = D3D11_USAGE_DEFAULT;
  render_desc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  render_desc.CPUAccessFlags = 0;
  render_desc.MiscFlags = 0;

  for (int i = 0; i < 2; ++i) {
    const HRESULT hr =
        device_->CreateTexture2D(&render_desc, nullptr, &render_slots_[i]);
    if (FAILED(hr)) {
      std::cout << "media_kit: MailboxSwapChain: CreateTexture2D render slot "
                << i << " failed (hr=0x" << std::hex << hr << std::dec << ")"
                << std::endl;
      ReleaseSlots();
      return hr;
    }
  }

  D3D11_TEXTURE2D_DESC present_desc = render_desc;
  present_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  HRESULT hr =
      device_->CreateTexture2D(&present_desc, nullptr, &present_texture_);
  if (FAILED(hr)) {
    std::cout << "media_kit: MailboxSwapChain: CreateTexture2D present slot "
                 "failed (hr=0x"
              << std::hex << hr << std::dec << ")" << std::endl;
    ReleaseSlots();
    return hr;
  }

  Microsoft::WRL::ComPtr<IDXGIResource> resource;
  hr = present_texture_.As(&resource);
  if (FAILED(hr)) {
    std::cout << "media_kit: MailboxSwapChain: As<IDXGIResource> failed (hr=0x"
              << std::hex << hr << std::dec << ")" << std::endl;
    ReleaseSlots();
    return hr;
  }

  HANDLE shared_handle = nullptr;
  hr = resource->GetSharedHandle(&shared_handle);
  if (FAILED(hr)) {
    std::cout << "media_kit: MailboxSwapChain: GetSharedHandle failed (hr=0x"
              << std::hex << hr << std::dec << ")" << std::endl;
    ReleaseSlots();
    return hr;
  }

  present_shared_handle_.store(shared_handle, std::memory_order_release);
  return S_OK;
}

void MailboxSwapChain::ReleaseSlots() {
  present_shared_handle_.store(nullptr, std::memory_order_release);
  present_texture_.Reset();
  for (auto& slot : render_slots_) {
    slot.Reset();
  }
}
