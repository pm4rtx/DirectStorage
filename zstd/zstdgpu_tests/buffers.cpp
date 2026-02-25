//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//

#include "buffers.h"

#include <d3dx12.h>

BufferValidator::BufferValidator(ID3D12Device* device)
    : GpuWork(device, L"Buffer Validator")
{
}

com_ptr<ID3D12Resource> BufferValidator::CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialResourceState,
    uint64_t size,
    const wchar_t* name)
{
    assert(name);

    com_ptr<ID3D12Resource> resource;
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
    CD3DX12_HEAP_PROPERTIES heapProperties(heapType);

    check_hresult(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialResourceState,
        nullptr,
        IID_PPV_ARGS(resource.put())));

    resource->SetName(name);
    return resource;
}

bool BufferValidator::Validate(ID3D12Resource* buffer, const uint8_t* expected, size_t dataSize)
{
    if (!m_readbackBuffer || m_readbackBuffer->GetDesc().Width < dataSize)
    {
        m_readbackBuffer = CreateBuffer(
            m_device.get(),
            D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST,
            dataSize,
            L"Validation Readback Buffer");
    }

    // Copy the buffer to a readback buffer
    uint64_t bufferSize = buffer->GetDesc().Width;
    m_commandList->Reset(m_commandAllocator.get(), nullptr);
    m_commandList->CopyBufferRegion(m_readbackBuffer.get(), 0, buffer, 0, bufferSize);
    m_commandList->Close();
    ExecuteCommandListsSynchronously(m_commandList.get());

    // Map the readback buffer and compare the data
    std::vector<uint8_t> actualData(static_cast<size_t>(bufferSize));
    uint8_t* readbackPtr = nullptr;
    check_hresult(m_readbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&readbackPtr)));
    memcpy(reinterpret_cast<char*>(actualData.data()), readbackPtr, static_cast<size_t>(bufferSize));
    m_readbackBuffer->Unmap(0, nullptr);

    return memcmp(actualData.data(), expected, dataSize) == 0;
}
