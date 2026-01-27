/**
 * ZstdGpuGroupCompressedLiterals.hlsl
 *
 * A compute shader that groups compressed literals by identical Huffman table index
 * by using the input prefix sum of the number of literals per every Huffman table index
 * and the relative index of the literal in the group of literals with the same Huffman table index
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

StructuredBuffer<uint32_t>                              ZstdLitStreamCountPrefix    : register(t0);
StructuredBuffer<zstdgpu_CompressedLiteralHuffmanBucket> ZstdLitStreamHuffmanBuckets : register(t1);
StructuredBuffer<uint32_t>                              ZstdCounters                : register(t2);
RWStructuredBuffer<uint32_t>                            ZstdLitStreamMap            : register(u0);

[RootSignature("SRV(t0), SRV(t1), SRV(t2), UAV(u0)")]
[numthreads(32, 1, 1)]
void main(uint litStreamId : SV_DispatchThreadId)
{
    if (litStreamId >= ZstdCounters[kzstdgpu_CounterIndex_HUF_Streams])
        return;

    const zstdgpu_CompressedLiteralHuffmanBucket bucket = ZstdLitStreamHuffmanBuckets[litStreamId];

    uint32_t groupStart = 0;
    if (bucket.huffmanBucketIndex != 0)
    {
        groupStart = ZstdLitStreamCountPrefix[bucket.huffmanBucketIndex - 1];
    }
    const uint32_t groupOffset = bucket.huffmanBucketOffset;

    ZstdLitStreamMap[groupStart + groupOffset] = litStreamId;
}
