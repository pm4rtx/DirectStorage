/**
 * ZstdGpuExecuteSequences.hlsl
 *
 * A compute shader that executes sequences.
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

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                  ZSTDGPU_RO_BUFFER(type)                    ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                  ZSTDGPU_RW_BUFFER(type)                    ZstdInOut##name : register(u##index);
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RO_TYPED_BUFFER(hlsl_type, type)   ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RW_TYPED_BUFFER(hlsl_type, type)   ZstdInOut##name : register(u##index);

ZSTDGPU_EXECUTE_SEQUENCES_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

#define MAX_COPY_SIZE 1024

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=13), UAV(u0, numDescriptors=1))")]
[numthreads(MAX_COPY_SIZE, 1, 1)]
void main(uint groupId : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_ExecuteSequences_SRT srt;

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                      srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;

    ZSTDGPU_EXECUTE_SEQUENCES_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

    zstdgpu_ShaderEntry_ExecuteSequences(srt, groupId, i, MAX_COPY_SIZE);
}
