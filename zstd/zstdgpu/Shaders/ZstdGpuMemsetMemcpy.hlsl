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

#include "../.generated/ZstdGpuSrt_MemsetMemcpy.h"

[RootSignature(ZSTDGPU_SRT_RS_MemsetMemcpy)]
[numthreads(kzstdgpu_TgSizeX_MemsetMemcpy, 1, 1)]
void main(uint2 groupId : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_MemsetMemcpy_SRT srt;

    zstdgpu_Srt_Fill(srt);

    i += zstdgpu_ConvertTo32BitGroupId(groupId, srt.tgOffset) * kzstdgpu_TgSizeX_MemsetMemcpy;

    if (i >= srt.workItemCount)
    {
        return;
    }

    const uint32_t blockIdx = zstdgpu_BinarySearch(srt.inUncBlockSizePrefix, 0, srt.inCounters[0].Blocks_UNC, i);

    const zstdgpu_OffsetAndSize blockRef = srt.inBlocksUncRefs[blockIdx];

    const uint32_t byteIdx = i - srt.inUncBlockSizePrefix[blockIdx];

    const uint32_t globalBlockIdx = srt.inGlobalBlockIndexPerUncBlock[blockIdx];

    const uint32_t dstBlockOffset = srt.inBlockDestOffs[globalBlockIdx];

    if (byteIdx >= zstdgpu_DecodeLitSize(blockRef.size))
    {
        return;
    }

    [branch] if (zstdgpu_IsLitTypeRaw(zstdgpu_DecodeLitType(blockRef.size)))
    {
        const uint32_t byteOfs = blockRef.offs + byteIdx;

        srt.inoutUnCompressedFramesData[dstBlockOffset + byteIdx] = (srt.inCompressedData[byteOfs >> 2u] >> ((byteOfs & 3u) << 3u)) & 0xffu;
    }
    else
    {
        srt.inoutUnCompressedFramesData[dstBlockOffset + byteIdx] = blockRef.offs;
    }
}
