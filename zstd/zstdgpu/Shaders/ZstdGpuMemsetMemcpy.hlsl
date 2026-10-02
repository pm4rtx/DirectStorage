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
void main(uint2 groupId2 : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_MemsetMemcpy_SRT srt;
    zstdgpu_Srt_Fill(srt);

    const uint32_t groupId = zstdgpu_ConvertTo32BitGroupId(groupId2, srt.tgOffset);
    const uint32_t blockCnt = srt.inCounters[0].Blocks_UNC + srt.inCounters[0].Blocks_CMP;

    const uint32_t blockId = zstdgpu_BinarySearch(srt.inBlockToCopyGroupPrefix, 0, blockCnt, groupId);

    const zstdgpu_OffsetAndSize blockRef = srt.inBlocksUncRefs[blockId];

    const uint32_t blockGroupId = groupId - srt.inBlockToCopyGroupPrefix[blockId];

    const uint32_t dstBlockOfs = srt.inBlockDestOffs[blockId];
    const uint32_t dstGroupOfs = dstBlockOfs + blockGroupId * kzstdgpu_CopyBlockBytes_BytesPerTGroup;

    const uint32_t blockByteCnt = zstdgpu_DecodeLitSize(blockRef.size);
    const uint32_t groupByteCnt = zstdgpu_MinU32(blockByteCnt - blockGroupId * kzstdgpu_CopyBlockBytes_BytesPerTGroup, kzstdgpu_CopyBlockBytes_BytesPerTGroup);

    const uint32_t blkType = zstdgpu_DecodeLitType(blockRef.size);

    ZSTDGPU_BRANCH if (zstdgpu_IsLitTypeRaw(blkType))
    {
        const uint32_t srcBlockOfs = blockRef.offs;
        const uint32_t srcGroupOfs = srcBlockOfs + blockGroupId * kzstdgpu_CopyBlockBytes_BytesPerTGroup;

        ZSTDGPU_FOR_WORK_ITEMS(byteIdx, groupByteCnt, i, kzstdgpu_TgSizeX_MemsetMemcpy)
        {
            const uint32_t byteOfs = srcGroupOfs + byteIdx;
            zstdgpu_TypedStoreU8(srt.inoutUnCompressedFramesData, dstGroupOfs + byteIdx, (srt.inCompressedData[byteOfs >> 2u] >> ((byteOfs & 3u) << 3u)) & 0xffu);
        }
    }
    else
    {
        ZSTDGPU_FOR_WORK_ITEMS(byteIdx, groupByteCnt, i, kzstdgpu_TgSizeX_MemsetMemcpy)
        {
            zstdgpu_TypedStoreU8(srt.inoutUnCompressedFramesData, dstGroupOfs + byteIdx, blockRef.offs);
        }
    }
}
