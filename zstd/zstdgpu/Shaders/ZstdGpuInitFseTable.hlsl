/**
 * ZstdGpuInitFseTable.hlsl
 *
 * A compute shader that initializes FSE table given FSE probability distribution.
 * The shader initializes single FSE table per threadgroup.
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

#include "../zstdgpu_structs.h"

#define ZSTD_BITCNT_NSTATE_METHOD_REFERENCE 0
#define ZSTD_BITCNT_NSTATE_METHOD_DEFAULT 1
#define ZSTD_BITCNT_NSTATE_METHOD_EXPERIMENTAL 2  // doesn't work on XBOX for now
#define ZSTD_BITCNT_NSTATE_METHOD ZSTD_BITCNT_NSTATE_METHOD_DEFAULT

#if defined(__XBOX_SCARLETT)
 //#   define __XBOX_ENABLE_WAVE32 1
#   if !defined(__XBOX_ENABLE_WAVE32) || (__XBOX_ENABLE_WAVE32 == 0)
#       undef kzstdgpu_WaveSize_Min
#       define kzstdgpu_WaveSize_Min 64
#   endif
#endif

#define kzstdgpu_WaveCountMax_InitFseTable (kzstdgpu_TgSizeX_InitFseTable / kzstdgpu_WaveSize_Min)

#define PREFER_LDS 0

#if kzstdgpu_WaveCountMax_InitFseTable > 1 || PREFER_LDS == 1
#   define IS_MULTI_WAVE 1
#else
#   define IS_MULTI_WAVE 0
#endif

#include "../zstdgpu_shaders.h"

struct Consts
{
    uint32_t tableStartIndex;
};

ConstantBuffer<Consts> Constants : register(b0);

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                  ZSTDGPU_RO_BUFFER(type)                    ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                  ZSTDGPU_RW_BUFFER(type)                    ZstdInOut##name : register(u##index);
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RO_TYPED_BUFFER(hlsl_type, type)   ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RW_TYPED_BUFFER(hlsl_type, type)   ZstdInOut##name : register(u##index);

ZSTDGPU_INIT_FSE_TABLE_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

groupshared uint32_t Lds[
    0 +
    #if (ZSTD_BITCNT_NSTATE_METHOD == ZSTD_BITCNT_NSTATE_METHOD_DEFAULT) || (ZSTD_BITCNT_NSTATE_METHOD == ZSTD_BITCNT_NSTATE_METHOD_REFERENCE)
        + kzstdgpu_MaxCount_FseProbs
        + kzstdgpu_MaxCount_FseElemsAllDigitBits
    #elif ZSTD_BITCNT_NSTATE_METHOD == ZSTD_BITCNT_NSTATE_METHOD_EXPERIMENTAL
        + kzstdgpu_MaxCount_FseElems * 2
        + kzstdgpu_MaxCount_FseElemsOneDigitBits * 2 // kzstdgpu_MaxCount_FseElemsOneDigitBits - masks, kzstdgpu_MaxCount_FseElemsOneDigitBits - ones prefix
    #endif

    #if IS_MULTI_WAVE
        + kzstdgpu_WaveCountMax_InitFseTable * 3 + 3
    #endif
];

#define ZSTDGPU_LDS Lds
#include "../zstdgpu_lds_hlsl.h"

[RootSignature("DescriptorTable(SRV(t0, numDescriptors = 2), UAV(u0, numDescriptors=3)), RootConstants(b0, num32BitConstants=1)")]
[numthreads(kzstdgpu_TgSizeX_InitFseTable, 1, 1)]
void main(uint32_t groupId : SV_GroupId, uint32_t i : SV_GroupThreadId)
{
    zstdgpu_InitFseTable_SRT srt;

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                      srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;
    ZSTDGPU_INIT_FSE_TABLE_SRT()
#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL
    srt.tableStartIndex = Constants.tableStartIndex;
    zstdgpu_ShaderEntry_InitFseTable(srt, groupId, i);
}
