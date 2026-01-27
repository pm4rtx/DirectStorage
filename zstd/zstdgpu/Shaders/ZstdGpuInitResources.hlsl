/**
 * ZstdGpuInitResources.hlsl
 *
 * A compute shader that initializes various resources to their defauls.
 *      - Counters for various entities
 *      - Arguments for indirect Dispatch calls
 *      - Default FSE tables
 *      - Lookback resources
 *
 * The caller needs to call Dispatch(zstdgpu_InitResources_GetDispatchSizeX, 1, 1)
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
    uint32_t allBlockCount;
    uint32_t cmpBlockCount;
    uint32_t frameCount;
    uint32_t initResourcesStage;
};

ConstantBuffer<Consts> Constants : register(b0);

#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RO_TYPED_BUFFER(hlsl_type, type)   ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RW_TYPED_BUFFER(hlsl_type, type)   ZstdInOut##name : register(u##index);
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                  ZSTDGPU_RW_BUFFER(type)                    ZstdInOut##name : register(u##index);

ZSTDGPU_INIT_RESOURCES_SRT()

#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=1), UAV(u0, numDescriptors=19)), RootConstants(b0, num32BitConstants=4)")]
[numthreads(kzstdgpu_TgSizeX_InitCounters, 1, 1)]
void main(uint i : SV_DispatchThreadId)
{
    zstdgpu_InitResources_SRT srt;

#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
    ZSTDGPU_INIT_RESOURCES_SRT()
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL

    srt.allBlockCount           = Constants.allBlockCount;
    srt.cmpBlockCount           = Constants.cmpBlockCount;
    srt.frameCount              = Constants.frameCount;
    srt.initResourcesStage      = Constants.initResourcesStage;
    zstdgpu_ShaderEntry_InitResources(srt, i);
}
