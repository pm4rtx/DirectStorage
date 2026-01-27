/**
 * ZstdGpuParseCompressedBlocks.hlsl
 *
 * A compute shader that parses compressed Zstd blocks, extracts locations of
 * other sub-blocks such as literals, Huffman Weights and FSE tables, and
 * executes FSE table index propagation via "Decoupled Lookback",
 * so each block refer its FSE tables directly.
 * The shader maps one compressed block to a single thread.
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
    uint32_t compressedBlockCount;
    uint32_t compressedBufferSizeInBytes;
    uint32_t frameCount;
};

ConstantBuffer<Consts> Constants : register(b0);

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                          ZSTDGPU_RO_BUFFER(type)                    ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                          ZSTDGPU_RW_BUFFER(type)                    ZstdInOut##name : register(u##index);
#define ZSTDGPU_RW_BUFFER_DECL_GLC(type, name, index)                      ZSTDGPU_RW_BUFFER_GLC(type)                ZstdInOut##name : register(u##index);
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)         ZSTDGPU_RW_TYPED_BUFFER(hlsl_type, type)   ZstdInOut##name : register(u##index);

ZSTDGPU_PARSE_COMPRESSED_BLOCKS_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL_GLC
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

#ifdef __XBOX_SCARLETT
#define __XBOX_ENABLE_WAVE32 1
#endif

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=4), UAV(u0, numDescriptors=16)), RootConstants(b0, num32BitConstants=3)")]
[numthreads(kzstdgpu_TgSizeX_ParseCompressedBlocks, 1, 1)]
void main(uint i : SV_DispatchThreadId)
{
    zstdgpu_ParseCompressedBlocks_SRT srt;

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                      srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RW_BUFFER_DECL_GLC(type, name, index)                  srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;
    ZSTDGPU_PARSE_COMPRESSED_BLOCKS_SRT()
#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL_GLC
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

    srt.compressedBlockCount        = Constants.compressedBlockCount;
    srt.compressedBufferSizeInBytes = Constants.compressedBufferSizeInBytes;
    srt.frameCount                  = Constants.frameCount;

    zstdgpu_ShaderEntry_ParseCompressedBlocks(srt, i);
}
