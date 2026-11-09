/**
 * Copyright (C) 2026, by Pavel Martishevsky
 * Report bugs and download new versions at https://github.com/pm4rtx/DirectStorage
 *
 * This file is distributed under the MIT License. See notice at the end of `zstdgpu_shaders_extra.h` file.
 */

/**
 * A compute shader that executes only the match copies of the sequences,
 * after the sequence literals were scattered in a separate pass and
 * trailing literals  / uncompressed blocks were copied by [Copy Block Bytes].
 */

#include "../zstdgpu_shaders_extra.h"

#include "../.generated/ZstdGpuSrt_ExecuteMatches.h"

[RootSignature(ZSTDGPU_SRT_RS_ExecuteMatches)]
[numthreads(MAX_COPY_SIZE, 1, 1)]
void main(uint groupId : SV_GroupId, uint i : SV_GroupThreadId)
{
    zstdgpu_ExecuteMatches_SRT srt;
    zstdgpu_Srt_Fill(srt);

    zstdgpu_ShaderEntry_ExecuteMatches(srt);
}
