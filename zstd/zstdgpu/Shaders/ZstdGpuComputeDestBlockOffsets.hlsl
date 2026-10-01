/**
 * ZstdGpuComputeDestBlockOffsets.hlsl
 *
 * Computes the absolute destination offset of every uncompressed block.
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


#include "../.generated/ZstdGpuSrt_ComputeDestBlockOffsets.h"

[RootSignature(ZSTDGPU_SRT_RS_ComputeDestBlockOffsets)]
[numthreads(kzstdgpu_TgSizeX_ComputeDestBlockOffset, 1, 1)]
void main(uint2 groupId : SV_GroupId, uint threadId : SV_GroupThreadId)
{
    zstdgpu_ComputeDestBlockOffsets_SRT srt;

    zstdgpu_Srt_Fill(srt);

    const uint32_t blockIdx = zstdgpu_ConvertTo32BitGroupId(groupId, srt.tgOffset) * kzstdgpu_TgSizeX_ComputeDestBlockOffset + threadId;
    if (blockIdx >= srt.workItemCount)
    {
        return;
    }

    zstdgpu_ShaderEntry_ComputeDestBlockOffsets(srt, blockIdx);
}
