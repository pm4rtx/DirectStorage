/**
 * ZstdGpuMemsetMemcpy.hlsl
 *
 * A compute shader populating contiguous region of memory from an uncompressed ZSTD block (RAW or RLE)
 * or from a compressed ZSTD block. If the source is compressed ZSTD block,
 * - if sequences are not present, the shader writes the entire decompressed literal (could
 *   be RAW/RLE literal stored in the source compressed region of memory or could be decompressed
 *   representation of Huffman compressed literal)
 * - if sequences are present, the shader writes only the tail of decompressed literal not covered
 *   by sequences.
 *
 * Each threadgroup populates a memory region of a fixed size except the last threadgroup in every ZSTD block
 * in case of the block is not exact multiple of the specified fixed size.
 *
 * Each threadgroup finds ZSTD block it needs to copy from via binary search using group id and the prefix
 * of the number of threadgroups per ZSTD block.
 *
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

#include "../.generated/ZstdGpuSrt_MemsetMemcpy.h"

[RootSignature(ZSTDGPU_SRT_RS_MemsetMemcpy)]
[numthreads(kzstdgpu_TgSizeX_MemsetMemcpy, 1, 1)]
void main(uint2 groupId2 : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_MemsetMemcpy_SRT srt;
    zstdgpu_Srt_Fill(srt);

    zstdgpu_ShaderEntry_MemsetMemcpy(srt, zstdgpu_ConvertTo32BitGroupId(groupId2, srt.tgOffset), i);
}
