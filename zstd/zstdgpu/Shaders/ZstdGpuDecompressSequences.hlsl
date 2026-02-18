/**
 * ZstdGpuDecompressSequences.hlsl
 *
 * A compute shader that decompresses FSE-compressed Sequences.
 * The shader maps one stream of FSE-compressed sequences to a single thread.
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

#ifdef __XBOX_SCARLETT
#define __XBOX_ENABLE_WAVE32 1
#endif

//#define USE_LDS_OUT_CACHE 1

#ifdef USE_LDS_OUT_CACHE
#define SEQ_CACHE_LEN 128
#endif

#include "../zstdgpu_shaders.h"

struct Consts
{
    uint32_t tgOffset;
};

ConstantBuffer<Consts> Constants : register(b0);

#include "../zstdgpu_srt_decl_bind.h"
ZSTDGPU_DECOMPRESS_SEQUENCES_SRT()
#include "../zstdgpu_srt_decl_undef.h"

#if defined(USE_LDS_OUT_CACHE)
groupshared uint32_t Lds[kzstdgpu_DecompressSequences_LdsOutCache_LdsSize];
#define ZSTDGPU_LDS Lds
#include "../zstdgpu_lds_hlsl.h"
#endif

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=8), UAV(u0, numDescriptors=7)), RootConstants(b0, num32BitConstants=1)")]
#ifdef USE_LDS_OUT_CACHE
#define NUM_THREADS 32
[numthreads(NUM_THREADS, 1, 1)]
#else
[numthreads(kzstdgpu_TgSizeX_DecompressSequences, 1, 1)]
#endif
void main(uint32_t2 groupId2 : SV_GroupId, uint i : SV_GroupThreadId)
{
#if defined(__XBOX_SCARLETT) || defined(__XBOX_ONE)
    const uint32_t groupId = groupId2.x;
#else
    const uint32_t groupId = Constants.tgOffset + groupId2.y * 65535 + groupId2.x;
#endif

#ifndef USE_LDS_OUT_CACHE
    i += groupId * kzstdgpu_TgSizeX_DecompressSequences;
#endif
    zstdgpu_DecompressSequences_SRT srt;

    #include "../zstdgpu_srt_decl_copy.h"
    ZSTDGPU_DECOMPRESS_SEQUENCES_SRT()
    #include "../zstdgpu_srt_decl_undef.h"

#if defined(USE_LDS_OUT_CACHE)
    zstdgpu_ShaderEntry_DecompressSequences_LdsOutCache(srt, groupId, i);
#else
    zstdgpu_ShaderEntry_DecompressSequences(srt, i);
#endif
}
