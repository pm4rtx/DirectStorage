/**
 * ZstdGpuMemsetMemcpy.hlsl
 *
 * A compute shader that does either a memset operation on the buffer or a memcpy.
 *
 * Copyright (c) Microsoft. All rights reserved.
 * This code is licensed under the MIT License (MIT).
 * THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 * ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 * IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 * PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
 *
 * Advanced Technology Group (ATG)
 * Author(s):   Pavel Martishevsky (pamartis@microsoft.com)
 */

#include "../zstdgpu_shaders.h"

struct Consts
{
    uint32_t tgOffset;
    uint32_t workItemCount;
    uint32_t flags;
};

ConstantBuffer<Consts>                  Constants                           : register(b0);

#include "../zstdgpu_srt_decl_bind.h"
ZSTDGPU_MEMSET_MEMCPY_SRT()
#include "../zstdgpu_srt_decl_undef.h"

RWStructuredBuffer<uint32_t>              ZstdInBlockSizePrefixTyped        : register(u4);

RWStructuredBuffer<zstdgpu_OffsetAndSize> ZstdInBlocksRefsTyped             : register(u5);

RWStructuredBuffer<uint32_t>              ZstdInGlobalBlockIndexTyped       : register(u6);

[RootSignature("DescriptorTable(UAV(u0, numDescriptors=4)), UAV(u4), UAV(u5), UAV(u6), RootConstants(b0, num32BitConstants=3)")]
[numthreads(kzstdgpu_TgSizeX_MemsetMemcpy, 1, 1)]
void main(uint2 groupId : SV_GroupId, uint i : SV_GroupThreadId)
{
    i += zstdgpu_ConvertTo32BitGroupId(groupId, Constants.tgOffset) * kzstdgpu_TgSizeX_MemsetMemcpy;

    if (i >= Constants.workItemCount)
    {
        return;
    }

    const uint32_t blockCnt = (Constants.flags & 0x1u) ? ZstdInCounters[0].Blocks_RAW : ZstdInCounters[0].Blocks_RLE;
    const uint32_t blockIdx = zstdgpu_BinarySearch(ZstdInBlockSizePrefixTyped, 0, blockCnt, i);

    const zstdgpu_OffsetAndSize blockRef = ZstdInBlocksRefsTyped[blockIdx];

    const uint32_t byteIdx = i - ZstdInBlockSizePrefixTyped[blockIdx];

    const uint32_t globalBlockIdx = ZstdInGlobalBlockIndexTyped[blockIdx];

    const uint32_t dstBlockOffset = ZstdInBlockDestOffs[globalBlockIdx];

    if (byteIdx >= blockRef.size)
    {
        return;
    }

    [branch] if (Constants.flags & 0x1u)
    {
        const uint32_t byteOfs = blockRef.offs + byteIdx;

        ZstdInOutUnCompressedFramesData[dstBlockOffset + byteIdx] = (ZstdInCompressedData[byteOfs >> 2u] >> ((byteOfs & 3u) << 3u)) & 0xffu;
    }
    else
    {
        ZstdInOutUnCompressedFramesData[dstBlockOffset + byteIdx] = blockRef.offs;
    }
}