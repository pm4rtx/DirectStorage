/**
 * ZstdGpuDecodeHuffmanWeights.hlsl
 *
 * A compute shader that decode Huffman Weights from the original compressed block.
 * The shader maps one stream of Huffman Weights to a single thread.
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
    uint32_t ZstdCompressedBlockCount;
    uint32_t ZstdCompressedBufferSizeInBytes;
};

ConstantBuffer<Consts> Constants : register(b0);

#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                  ZSTDGPU_RO_BUFFER(type)                    ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                  ZSTDGPU_RW_BUFFER(type)                    ZstdInOut##name : register(u##index);
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RO_TYPED_BUFFER(hlsl_type, type)   ZstdIn##name    : register(t##index);
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index) ZSTDGPU_RW_TYPED_BUFFER(hlsl_type, type)   ZstdInOut##name : register(u##index);

    ZSTDGPU_DECODE_HUFFMAN_WEIGHTS_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

#ifdef __XBOX_SCARLETT
#define __XBOX_ENABLE_WAVE32 1
#endif

[RootSignature("DescriptorTable(SRV(t0, numDescriptors=3), UAV(u0, numDescriptors=2)), RootConstants(b0, num32BitConstants=2)")]
[numthreads(kzstdgpu_TgSizeX_DecodeHuffmanWeights, 1, 1)]
void main(uint i : SV_DispatchThreadId)
{
    zstdgpu_DecodeHuffmanWeights_SRT srt;
#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                      srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;

    ZSTDGPU_DECODE_HUFFMAN_WEIGHTS_SRT()

#undef ZSTDGPU_RW_TYPED_BUFFER_DECL
#undef ZSTDGPU_RO_TYPED_BUFFER_DECL
#undef ZSTDGPU_RW_BUFFER_DECL
#undef ZSTDGPU_RO_BUFFER_DECL

    srt.compressedBlockCount        = Constants.ZstdCompressedBlockCount;
    srt.compressedBufferSizeInBytes = Constants.ZstdCompressedBufferSizeInBytes;
    zstdgpu_ShaderEntry_DecodeHuffmanWeights(srt, i);
}
