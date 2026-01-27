/**
 * ZstdGpuUpdateDispatchArgs.hlsl
 *
 * A compute shader that updates arguments for indirect Dispatch calls from
 * corresponding counters and threadgroup dimensions.
 * The shader needs to be dispatched as single threadgroup.
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

RWStructuredBuffer<uint32_t> ZstdCounters : register(u0);

[RootSignature("UAV(u0)")]
[numthreads(1, 1, 1)]
void main()
{
    // NOTE(pamartis): This number of groups doing the decompression of Huffman weights depends on
    // the number FSE tables for Huffman weights because those numbers are the same.
    // This is because each FSE table decompresses its own Huffman weights' stream.
    ZstdCounters[kzstdgpu_CounterIndex_DecompressHuffmanWeightsGroups] = ZSTDGPU_TG_COUNT(ZstdCounters[kzstdgpu_CounterIndex_FseHufW], kzstdgpu_TgSizeX_DecompressHuffmanWeights);

    // NOTE(pamartis): We also do decoding of uncompressed Huffman Weights stored as two nibbles
    // per byte to make sure final representation (a byte per weight) becomes identical, so it's
    // easier to use during literal decoding
    ZstdCounters[kzstdgpu_CounterIndex_DecodeHuffmanWeightsGroups] = ZSTDGPU_TG_COUNT(ZstdCounters[kzstdgpu_CounterIndex_HUF_WgtStreams], kzstdgpu_TgSizeX_DecodeHuffmanWeights);

    ZstdCounters[kzstdgpu_CounterIndex_GroupCompressedLiteralsGroups] = ZSTDGPU_TG_COUNT(ZstdCounters[kzstdgpu_CounterIndex_HUF_Streams], 32);

    ZstdCounters[kzstdgpu_CounterIndex_DecompressSequencesGroups] = ZSTDGPU_TG_COUNT(ZstdCounters[kzstdgpu_CounterIndex_Seq_Streams], kzstdgpu_TgSizeX_DecompressSequences);
}
