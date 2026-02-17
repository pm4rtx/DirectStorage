/**
 * ZstdGpuFinaliseSequenceOffsets.hlsl
 *
 * A compute shader that converts sequence offsets from encoded representation
 * which includes "repeat" offsets into absolute offsets using information from
 * previous blocks such as final "repeat" offset and the prefix of block sizes
 *
 * The shader maps each sequence offset to a thread.
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
};

ConstantBuffer<Consts> Constants : register(b0);

#include "../zstdgpu_srt_decl_bind.h"
ZSTDGPU_FINALISE_SEQUENCE_OFFSETS_SRT()
#include "../zstdgpu_srt_decl_undef.h"

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=8), UAV(u0, numDescriptors=1)), RootConstants(b0, num32BitConstants=1)")]
[numthreads(kzstdgpu_TgSizeX_FinaliseSequenceOffsets, 1, 1)]
void main(uint2 groupId : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_FinaliseSequenceOffsets_SRT srt;

    #include "../zstdgpu_srt_decl_copy.h"
    ZSTDGPU_FINALISE_SEQUENCE_OFFSETS_SRT()
    #include "../zstdgpu_srt_decl_undef.h"

#if defined(__XBOX_SCARLETT) || defined(__XBOX_ONE)
    i += groupId.x * kzstdgpu_TgSizeX_FinaliseSequenceOffsets;
#else
    i += (Constants.tgOffset + groupId.y * 65535 + groupId.x) * kzstdgpu_TgSizeX_FinaliseSequenceOffsets;
#endif

    zstdgpu_ShaderEntry_FinaliseSequenceOffsets(srt, i);
}
