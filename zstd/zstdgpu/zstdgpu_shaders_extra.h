/**
 * Copyright (C) 2026, by Pavel Martishevsky
 * Report bugs and download new versions at https://github.com/pm4rtx/DirectStorage
 *
 * This file is distributed under the MIT License. See notice at the end of this file.
 */

#pragma once

#ifndef ZSTDGPU_SHADERS_EXTRA_H
#define ZSTDGPU_SHADERS_EXTRA_H

#include "zstdgpu_shaders.h"

// NOTE(pamartis): Make sure 4505 is not leaked into a file including this header. 4505 is fine because some non-inlined function may be unused
ZSTDGPU_WARN_PUSH_MSVC()
ZSTDGPU_WARN_STOP_MSVC(4505) /**< warning C4505: 'function name': unreferenced function with internal linkage has been removed */

static void zstdgpu_ShaderEntry_InitCopyLiteralGroups(ZSTDGPU_PARAM_INOUT(zstdgpu_InitCopyLiteralGroups_SRT) srt, uint32_t groupId)
{
    ZSTDGPU_ASSERT(groupId < srt.workItemCount);

    const uint32_t seqStreamId   = zstdgpu_BinarySearch(srt.inSeqStreamToLitCopyGroupPrefix, 0, srt.inCounters[0].Seq_Streams, groupId);
    const uint32_t blkGroupId = groupId - srt.inSeqStreamToLitCopyGroupPrefix[seqStreamId];
    const uint32_t grpByteOfs = blkGroupId * kzstdgpu_CopyLiteralBytes_BytesPerTGroup;

    const uint32_t litByteCnt = zstdgpu_DecodeLitSize(srt.inSeqStreamToRef2[seqStreamId].size);
    ZSTDGPU_ASSERT(grpByteOfs < litByteCnt);
    const uint32_t grpByteCnt = zstdgpu_MinU32(litByteCnt - grpByteOfs, kzstdgpu_CopyLiteralBytes_BytesPerTGroup);

    uint32_t globalSeqOfs = 0;
    uint32_t globalSeqEnd = 0;
    zstdgpu_GetSequenceBeginEnd(globalSeqOfs, globalSeqEnd, seqStreamId, srt.inPerSeqStreamSeqStart, srt.inCounters);
    uint32_t globalSeqCnt = globalSeqEnd - globalSeqOfs;

    const uint32_t streamSeqBase = zstdgpu_BinarySearch(srt.inDecompressedSequenceLLen, globalSeqOfs, globalSeqCnt, grpByteOfs) - globalSeqOfs;
    const uint32_t streamSeqLast = zstdgpu_BinarySearch(srt.inDecompressedSequenceLLen, globalSeqOfs, globalSeqCnt, grpByteOfs + grpByteCnt - 1u) - globalSeqOfs;
    ZSTDGPU_ASSERT(streamSeqBase <= streamSeqLast);
    const uint32_t streamSeqCnt = streamSeqLast - streamSeqBase + 1u;
    ZSTDGPU_ASSERT(streamSeqBase <= 0xffffu && streamSeqCnt <= 0xffffu);

    zstdgpu_CopyLiteralGroup group;
    group.seqStreamId  = seqStreamId;
    group.seqBaseCount = streamSeqBase | (streamSeqCnt << 16u);
    srt.inoutCopyLiteralGroups[groupId] = group;
}

static void zstdgpu_ShaderEntry_ExecuteMatches(ZSTDGPU_PARAM_INOUT(zstdgpu_ExecuteMatches_SRT) srt)
{
    const uint32_t frameCnt = srt.inoutCounters[0].Frames;

    uint32_t frameIdx = 0;
    if (WaveIsFirstLane())
    {
        InterlockedAdd(srt.inoutCounters[0].Frames_ExecuteSequences, 1, frameIdx);
    }
    frameIdx = WaveReadLaneFirst(frameIdx);

    if (frameIdx >= frameCnt)
    {
        return;
    }

    const uint32_t cmpBlockBeg = srt.inPerFrameBlockCountCMP[frameIdx];
    const uint32_t cmpBlockEnd = (frameIdx + 1u < frameCnt)
                               ? srt.inPerFrameBlockCountCMP[frameIdx + 1u]
                               : srt.inoutCounters[0].Blocks_CMP;

    for (uint32_t cmpBlockIdx = cmpBlockBeg; cmpBlockIdx < cmpBlockEnd; ++cmpBlockIdx)
    {
        const uint32_t blockIdx = srt.inGlobalBlockIndexPerCmpBlock[cmpBlockIdx];

        uint32_t blockOfs = 0;
        ZSTDGPU_BRANCH if (blockIdx > 0)
        {
            blockOfs = srt.inBlockSizePrefix[blockIdx - 1u];
        }

        const uint32_t blockSize = srt.inBlockSizePrefix[blockIdx] - blockOfs;
        const uint32_t blockByteBeg = srt.inBlockDestOffs[blockIdx];
        const uint32_t blockByteEnd = blockByteBeg + blockSize;
        const zstdgpu_CompressedBlockData block = srt.inCompressedBlocks[cmpBlockIdx];
        const uint32_t seqStreamIdx = block.seqStreamIndex;

        uint32_t seqOfs = 0;
        uint32_t seqEnd = 0;
        uint32_t seqLLenTotal = 0;
        // NOTE: BRANCH is used to make sure no stream metadata is (speculatively) loaded for a block without sequences
        ZSTDGPU_BRANCH if (seqStreamIdx != ~0u)
        {
            zstdgpu_GetSequenceBeginEnd(seqOfs, seqEnd, seqStreamIdx, srt.inPerSeqStreamSeqStart, srt.inoutCounters);
#if ZSTDGPU_USE_PREFIXED_LLEN_MLEN
            const uint32_t seqMLenTotal = blockSize - zstdgpu_DecodeLitSize(block.literal.size);
            const uint32_t seqByteTotal = srt.inDecompressedSequenceMLen[seqEnd - 1u];
            seqLLenTotal = seqByteTotal - seqMLenTotal;
#endif
        }

        uint32_t dstOfs = blockByteBeg;
        ZSTDGPU_LOOP for (uint32_t seqIdx = seqOfs; seqIdx < seqEnd; ++seqIdx)
        {
            const zstdgpu_Sequence seq = zstdgpu_LoadSequence(srt, seqIdx, seqOfs, seqEnd, seqLLenTotal);
            dstOfs += seq.llen;
            zstdgpu_MatchCopy(srt.inoutUnCompressedFramesData, dstOfs, seq, blockByteEnd);
        }
    }
}

ZSTDGPU_WARN_POP_MSVC()

#endif // ZSTDGPU_SHADERS_EXTRA_H

/**
 * Copyright (c) 2026 Pavel Martishevsky
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
