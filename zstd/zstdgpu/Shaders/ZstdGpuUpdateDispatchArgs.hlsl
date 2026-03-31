/**
 * ZstdGpuUpdateDispatchArgs.hlsl
 *
 * A compute shader that reads source counters from the Counters and writes dispatch arguments
 * into the DispatchArgs buffer. The shader also updates derived counter fields in the Counters buffer.
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

#include "../zstdgpu_shaders.h"

RWStructuredBuffer<zstdgpu_Counters>  ZstdCounters     : register(u0);
RWStructuredBuffer<uint32_t>          ZstdDispatchArgs : register(u1);
RWStructuredBuffer<uint32_t>          ZstdDispatchCnts : register(u2);

struct UpdateDispatchArgsConsts
{
    uint32_t decompressSequences_StreamsPerTG;
};

ConstantBuffer<UpdateDispatchArgsConsts> Consts : register(b0);

[RootSignature("UAV(u0), UAV(u1), UAV(u2), RootConstants(b0, num32BitConstants=1)")]
[numthreads(1, 1, 1)]
void main()
{
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_FseHufW,         ZstdCounters[0].FseHufW,        1);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_FseLLen,         ZstdCounters[0].FseLLen,        1);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_FseOffs,         ZstdCounters[0].FseOffs,        1);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_FseMLen,         ZstdCounters[0].FseMLen,        1);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_HUF_WgtStreams,  ZstdCounters[0].HUF_WgtStreams, 1);

    // NOTE(pamartis): The number of groups running the decompression of Huffman weights depends on the number FSE tables
    // for Huffman weights because those numbers are the same because each FSE table decompresses its own Huffman weights' stream.
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_DecompressHuffmanWeights, ZstdCounters[0].FseHufW,        kzstdgpu_TgSizeX_DecompressHuffmanWeights);

    // NOTE(pamartis): We also do decoding of uncompressed Huffman Weights stored as two nibbles per byte to make sure final representation
    // (a byte per weight) becomes identical, so identical representation simplfies initialisation of Huffman tables to use during literal decoding
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_DecodeHuffmanWeights,     ZstdCounters[0].HUF_WgtStreams, kzstdgpu_TgSizeX_DecodeHuffmanWeights);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_GroupCompressedLiterals,  ZstdCounters[0].HUF_Streams,    32);
    zstdgpu_EmitDispatch(ZstdDispatchArgs, ZstdDispatchCnts, kzstdgpu_DispatchSlot_DecompressSequences,      ZstdCounters[0].Seq_Streams,    Consts.decompressSequences_StreamsPerTG);

    // NOTE: DecompressLiterals slot is written by ComputePrefixSum (runs after this shader)

    // Update derived counter field in Counters (kept for shader bounds checks)
    ZstdCounters[0].DecompressSequencesGroups = ZSTDGPU_TG_COUNT(ZstdCounters[0].Seq_Streams, Consts.decompressSequences_StreamsPerTG);
}
