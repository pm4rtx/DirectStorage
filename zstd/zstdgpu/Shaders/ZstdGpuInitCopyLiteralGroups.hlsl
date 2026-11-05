/**
 * Copyright (C) 2026, by Pavel Martishevsky
 * Report bugs and download new versions at https://github.com/pm4rtx/DirectStorage
 *
 * This file is distributed under the MIT License. See notice at the end of `zstdgpu_shaders_extra.h` file.
 */

#include "../zstdgpu_shaders_extra.h"

#include "../.generated/ZstdGpuSrt_InitCopyLiteralGroups.h"

[RootSignature(ZSTDGPU_SRT_RS_InitCopyLiteralGroups)]
[numthreads(kzstdgpu_TgSizeX_InitCopyLiteralGroups, 1, 1)]
void main(uint2 groupId : SV_GroupId, uint32_t threadId : SV_GroupThreadId)
{
    zstdgpu_InitCopyLiteralGroups_SRT srt;
    zstdgpu_Srt_Fill(srt);

    const uint32_t groupIdx = zstdgpu_ConvertTo32BitGroupId(groupId, srt.tgOffset) * kzstdgpu_TgSizeX_InitCopyLiteralGroups + threadId;
    if (groupIdx >= srt.workItemCount)
    {
        return;
    }

    zstdgpu_ShaderEntry_InitCopyLiteralGroups(srt, groupIdx);
}
