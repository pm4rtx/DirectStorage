//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//

#pragma once

#include "gpuwork.h"

#include <d3d12.h>
#include <windows.h>
#include <winrt/base.h>

#include <optional>

using winrt::check_hresult;
using winrt::com_ptr;

#define DWORD_ALIGN(count) ((count + 3) & ~3)
#define ALIGN_TO_4K(size) (((size) + 4095) & ~static_cast<size_t>(4095))

class BufferValidator : public GpuWork
{
    com_ptr<ID3D12Resource> m_readbackBuffer;

public:
    BufferValidator(ID3D12Device* device);
    bool Validate(ID3D12Resource* buffer, const uint8_t* expected, size_t dataSize);

private:
    com_ptr<ID3D12Resource> CreateBuffer(
        ID3D12Device* device,
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initialResourceState,
        uint64_t size,
        const wchar_t* name);
};