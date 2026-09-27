/**
 * Copyright (c) Microsoft. All rights reserved.
 * This code is licensed under the MIT License (MIT).
 * THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 * ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 * IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 * PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
 *
 * Advanced Technology Group (ATG)
 * Author(s):   Pavel Martishevsky (pamartis@microsoft.com)
 *
 * zstdgpu_barrier_tracker.h
 *
 */

#ifndef ZSTDGPU_BARRIER_TRACKER_H
#define ZSTDGPU_BARRIER_TRACKER_H

#include <stdio.h>
#include ".generated/zstdgpu_srt_barrier_tables.h"

static const zstdgpu_Srt_Pass kzstdgpu_BarrierTracker_NoPass = kzstdgpu_Srt_Pass_Count;

static const zstdgpu_Srt_Pass kzstdgpu_BarrierTracker_Clean = (zstdgpu_Srt_Pass)0xFFFFu;

static_assert(kzstdgpu_BarrierTracker_NoPass < kzstdgpu_BarrierTracker_Clean, "kzstdgpu_BarrierTracker_Clean must not collide with any writer identity");

#define ZSTDGPU_BARRIER_STATE_LIST()                                            \
    X(Common                 , D3D12_RESOURCE_STATE_COMMON)                     \
    X(IndirectRead           , D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)          \
    X(ShaderCopyRead         , D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_SOURCE) \
    X(ShaderCopyIndirectRead , D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) \
    X(CopyWrite              , D3D12_RESOURCE_STATE_COPY_DEST)                  \
    X(ShaderReadWrite        , D3D12_RESOURCE_STATE_UNORDERED_ACCESS)           \

typedef enum zstdgpu_BarrierTracker_State
{
#define X(name, bits)   kzstdgpu_BarrierTracker_State_##name,
    ZSTDGPU_BARRIER_STATE_LIST()
#undef X
    kzstdgpu_BarrierTracker_State_Count,
    kzstdgpu_BarrierTracker_State_Predication = kzstdgpu_BarrierTracker_State_IndirectRead
} zstdgpu_BarrierTracker_State;

static const D3D12_RESOURCE_STATES kzstdgpu_BarrierTracker_StateBits[kzstdgpu_BarrierTracker_State_Count] =
{
#define X(name, bits)   bits,
    ZSTDGPU_BARRIER_STATE_LIST()
#undef X
};


static const char *const kzstdgpu_BarrierTracker_StateName[kzstdgpu_BarrierTracker_State_Count] =
{
#define X(name, bits)   #name,
    ZSTDGPU_BARRIER_STATE_LIST()
#undef X
};

static const zstdgpu_BarrierTracker_State kzstdgpu_Srt_Access_To_BarrierTracker_State[5] =
{
    kzstdgpu_BarrierTracker_State_ShaderCopyRead,           /**< kzstdgpu_Srt_Access_ShaderRead         */
    kzstdgpu_BarrierTracker_State_ShaderReadWrite,          /**< kzstdgpu_Srt_Access_ShaderReadWrite    */
    kzstdgpu_BarrierTracker_State_ShaderReadWrite,          /**< kzstdgpu_Srt_Access_ShaderReadNoWrite  */
    kzstdgpu_BarrierTracker_State_IndirectRead,             /**< kzstdgpu_Srt_Access_IndirectRead       */
    kzstdgpu_BarrierTracker_State_ShaderCopyIndirectRead    /**< kzstdgpu_Srt_Access_ShaderIndirectRead */
};

static const uint32_t kzstdgpu_Srt_Access_To_BarrierTracker_State_Count = _countof(kzstdgpu_Srt_Access_To_BarrierTracker_State);

#define ZSTDGPU_BARRIER_TRACKER_PENDING_MAX     kzstdgpu_BarrierTracker_ResId_Count

#ifndef ZSTDGPU_BARRIER_REPORT
#   define ZSTDGPU_BARRIER_REPORT 0
#endif

#ifndef ZSTDGPU_BARRIER_REPORT_PATH
#   define ZSTDGPU_BARRIER_REPORT_PATH "zstdgpu_barriers.csv"
#endif

#if ZSTDGPU_BARRIER_REPORT
#   define ZSTDGPU_BARRIER_TRACKER_REPORT_MAX      4096
#else
#   define ZSTDGPU_BARRIER_TRACKER_REPORT_MAX      1
#endif

typedef enum zstdgpu_BarrierTracker_AccessSource
{
    kzstdgpu_BarrierTracker_AccessSource_Bind   = 0,
    kzstdgpu_BarrierTracker_AccessSource_Future = 1
} zstdgpu_BarrierTracker_AccessSource;

typedef struct zstdgpu_BarrierTrackerReportRow
{
    zstdgpu_Srt_Pass consumerPass;
    zstdgpu_Srt_Pass producerPass;

    uint16_t consumerPassId;
    uint16_t producerPassId;

    zstdgpu_Srt_ResId resId;

    zstdgpu_BarrierTracker_State prev;
    zstdgpu_BarrierTracker_State next;

    uint16_t fused  : 1;
    uint16_t isUav  : 1;
    uint16_t line;
} zstdgpu_BarrierTracker_ReportRow;


typedef struct zstdgpu_BarrierTrackerResState
{
    zstdgpu_BarrierTracker_State    curr;
    zstdgpu_BarrierTracker_State    prev;
    zstdgpu_Srt_Pass                writePass;
    uint16_t                        writePassId;
} zstdgpu_BarrierTrackerResState;

typedef struct zstdgpu_BarrierTracker
{
    zstdgpu_BarrierTrackerResState      state[kzstdgpu_BarrierTracker_ResId_Count];
    uint16_t                            bindCount[kzstdgpu_Srt_Pass_Count + 1];

    zstdgpu_Srt_ResId                   pending[ZSTDGPU_BARRIER_TRACKER_PENDING_MAX];
    uint32_t                            pendingCount;

    zstdgpu_BarrierTracker_ReportRow    reportRows[ZSTDGPU_BARRIER_TRACKER_REPORT_MAX];
    uint32_t                            reportRowCount;
} zstdgpu_BarrierTracker;

static ID3D12Resource *zstdgpu_BarrierTracker_Resource(const zstdgpu_GpuOnlyBuffers *b, uint32_t resId)
{
    return *(ID3D12Resource *const *)((const char *)b + kzstdgpu_BarrierTracker_ResOfs[resId]);
}

static void zstdgpu_BarrierTracker_Reset(zstdgpu_BarrierTracker *tracker)
{
    for (uint32_t i = 0; i < (uint32_t)kzstdgpu_BarrierTracker_ResId_Count; ++i)
    {
        tracker->state[i].curr = kzstdgpu_BarrierTracker_State_Common;
        tracker->state[i].writePass = kzstdgpu_BarrierTracker_Clean;
        tracker->state[i].writePassId = 0;
        tracker->state[i].prev = kzstdgpu_BarrierTracker_State_Count;
    }
    for (uint32_t i = 0; i < _countof(tracker->bindCount); ++i)
    {
        tracker->bindCount[i] = 0;
    }
    tracker->pendingCount = 0;
}

static void zstdgpu_BarrierTracker_BeginRequest(zstdgpu_BarrierTracker *tracker)
{
    tracker->reportRowCount = 0;
    zstdgpu_BarrierTracker_Reset(tracker);
}

static void zstdgpu_BarrierTracker_EndCmdList(zstdgpu_BarrierTracker *tracker)
{
    zstdgpu_BarrierTracker_Reset(tracker);
}

static zstdgpu_BarrierTracker_ReportRow *zstdgpu_BarrierTracker_Row(zstdgpu_BarrierTracker *tracker, zstdgpu_Srt_Pass consumerPass, uint16_t consumerPassId, zstdgpu_Srt_ResId resId, const zstdgpu_BarrierTrackerResState *state, zstdgpu_BarrierTracker_State next, uint32_t line)
{
    zstdgpu_BarrierTracker_ReportRow *row = &tracker->reportRows[tracker->reportRowCount];

    if (tracker->reportRowCount + 1 < ZSTDGPU_BARRIER_TRACKER_REPORT_MAX)
    {
        tracker->reportRowCount ++;
    }

    row->consumerPass   = consumerPass;
    row->producerPass   = state->writePass;
    row->consumerPassId = consumerPassId;
    row->producerPassId = (kzstdgpu_BarrierTracker_Clean == state->writePass) ? 0xFFFFu : state->writePassId;
    row->resId          = resId;
    row->prev           = state->curr;
    row->next           = next;
    row->fused          = 0;
    row->isUav          = 0;
    row->line           = (uint16_t)line;
    return row;
}

static void zstdgpu_BarrierTracker_PushPrevState(zstdgpu_BarrierTracker *tracker, zstdgpu_Srt_ResId resId)
{
    zstdgpu_BarrierTrackerResState *state = &tracker->state[resId];
    if (kzstdgpu_BarrierTracker_State_Count != state->prev)
    {
        return;
    }

    ZSTDGPU_ASSERT_RET(tracker->pendingCount < (uint32_t)ZSTDGPU_BARRIER_TRACKER_PENDING_MAX);
    state->prev = state->curr;
    tracker->pending[tracker->pendingCount++] = resId;
}

static void zstdgpu_BarrierTracker_Flush(ID3D12GraphicsCommandList *cmdList, zstdgpu_BarrierTracker *tracker, const zstdgpu_GpuOnlyBuffers *buffers)
{
    D3D12_RESOURCE_BARRIER  barriers[ZSTDGPU_BARRIER_TRACKER_PENDING_MAX];
    uint32_t count = 0;
    uint32_t needsUav = 0;

    for (uint32_t i = 0; i < tracker->pendingCount; ++i)
    {
        const zstdgpu_Srt_ResId resId = tracker->pending[i];
        zstdgpu_BarrierTrackerResState *state = &tracker->state[resId];
        const zstdgpu_BarrierTracker_State prev = state->prev;
        const zstdgpu_BarrierTracker_State next = state->curr;

        state->prev = kzstdgpu_BarrierTracker_State_Count;

        if (prev != next)
        {
            barriers[count].Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[count].Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[count].Transition.pResource    = zstdgpu_BarrierTracker_Resource(buffers, resId);
            barriers[count].Transition.Subresource  = 0;
            barriers[count].Transition.StateBefore  = kzstdgpu_BarrierTracker_StateBits[prev];
            barriers[count].Transition.StateAfter   = kzstdgpu_BarrierTracker_StateBits[next];
            ++count;
        }
        else if (kzstdgpu_BarrierTracker_State_ShaderReadWrite == prev)
        {
            needsUav = 1;
        }
    }
    tracker->pendingCount = 0;

    if (needsUav)
    {
        barriers[count].Type            = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[count].Flags           = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[count].UAV.pResource   = NULL;
        ++count;
    }
    if (count > 0)
    {
        cmdList->ResourceBarrier(count, barriers);
    }
}

typedef struct zstdgpu_BarrierTrackerFusion
{
    zstdgpu_Srt_Pass    producer;
    uint16_t            producerOccurrence;
    zstdgpu_Srt_Pass    consumer;
    uint16_t            consumerOccurrence;
    zstdgpu_Srt_ResId   resId;
} zstdgpu_BarrierTrackerFusion;

static const zstdgpu_BarrierTrackerFusion kzstdgpu_BarrierTracker_Fusion[] =
{
    { kzstdgpu_Srt_Pass_DecompressHuffmanWeights, 0, kzstdgpu_Srt_Pass_DecodeHuffmanWeights,  0, kzstdgpu_BarrierTracker_ResId_DecompressedHuffmanWeights     },
    { kzstdgpu_Srt_Pass_DecompressHuffmanWeights, 0, kzstdgpu_Srt_Pass_DecodeHuffmanWeights,  0, kzstdgpu_BarrierTracker_ResId_DecompressedHuffmanWeightCount },
    { kzstdgpu_Srt_Pass_InitHuffmanTable,         0, kzstdgpu_Srt_Pass_InitHuffmanTable,      1, kzstdgpu_BarrierTracker_ResId_HuffmanTableInfo               },
    { kzstdgpu_Srt_Pass_InitHuffmanTable,         0, kzstdgpu_Srt_Pass_InitHuffmanTable,      1, kzstdgpu_BarrierTracker_ResId_HuffmanTableCodeAndSymbol      },
    { kzstdgpu_Srt_Pass_InitHuffmanTable,         0, kzstdgpu_Srt_Pass_InitHuffmanTable,      1, kzstdgpu_BarrierTracker_ResId_HuffmanTableRankIndex          },
    { kzstdgpu_Srt_Pass_MemsetMemcpy_MemcpyRAW,   0, kzstdgpu_Srt_Pass_MemsetMemcpy_MemsetRLE, 0, kzstdgpu_BarrierTracker_ResId_UnCompressedFramesData        }
};


static uint32_t zstdgpu_BarrierTracker_IsDeclaredFusion(zstdgpu_Srt_Pass producer, uint32_t producerOccurrence, zstdgpu_Srt_Pass consumer, uint32_t consumerOccurrence, zstdgpu_Srt_ResId resId)
{
    uint32_t i;

    for (i = 0; i < sizeof(kzstdgpu_BarrierTracker_Fusion) / sizeof(kzstdgpu_BarrierTracker_Fusion[0]); ++i)
    {
        const zstdgpu_BarrierTrackerFusion *f = &kzstdgpu_BarrierTracker_Fusion[i];

        if (f->producer == producer && f->producerOccurrence == producerOccurrence &&
            f->consumer == consumer && f->consumerOccurrence == consumerOccurrence &&
            f->resId == resId)
        {
            return 1;
        }
    }
    return 0;
}

static void zstdgpu_BarrierTracker_TrackAccess(zstdgpu_BarrierTracker *tracker,
                                              const zstdgpu_GpuOnlyBuffers *buffers,
                                              zstdgpu_Srt_ResId resId,
                                              zstdgpu_BarrierTracker_State passState,
                                              uint32_t writes,
                                              zstdgpu_Srt_Pass pass,
                                              zstdgpu_BarrierTracker_AccessSource source,
                                              uint32_t line)
{
    zstdgpu_BarrierTrackerResState *state = &tracker->state[resId];
    const D3D12_RESOURCE_STATES wanted = kzstdgpu_BarrierTracker_StateBits[passState];
    const uint16_t passId = tracker->bindCount[pass];

    ZSTDGPU_ASSERT(resId < kzstdgpu_BarrierTracker_ResId_Count);
    ZSTDGPU_ASSERT(passState < kzstdgpu_BarrierTracker_State_Count);

    if (NULL == zstdgpu_BarrierTracker_Resource(buffers, resId))
    {
        /** NOTE(pamartis): we early out if ResId wasn't created yet. This is possible when TrackAccess is called in multi-stage submission mode */
        return;
    }

    if (kzstdgpu_BarrierTracker_State_Common == state->curr || wanted != (kzstdgpu_BarrierTracker_StateBits[state->curr] & wanted))
    {
        if (kzstdgpu_BarrierTracker_State_Common != state->curr)
        {
            zstdgpu_BarrierTracker_Row(tracker, pass, passId, resId, state, passState, line);
            zstdgpu_BarrierTracker_PushPrevState(tracker, resId);
        }
        /** TODO(pamartis): may need to set `curr` to `ShaderCopyIndirectRead` or `ShaderCopyRead` to
         *  1. prevent read to read transitions
         *  2. promote `Common` to the widest Read state to accomodate as many readers as possible */
        state->curr = passState;
        state->writePass = kzstdgpu_BarrierTracker_Clean;
    }
    else if (kzstdgpu_BarrierTracker_State_ShaderReadWrite == passState && kzstdgpu_BarrierTracker_Clean != state->writePass)
    {
        zstdgpu_BarrierTracker_ReportRow *row = zstdgpu_BarrierTracker_Row(tracker, pass, passId, resId, state, passState, line);
        const uint32_t fused = kzstdgpu_BarrierTracker_AccessSource_Bind == source && zstdgpu_BarrierTracker_IsDeclaredFusion(state->writePass, state->writePassId, pass, passId, resId);

        row->isUav  = 1;

        if (fused)
        {
            row->fused = 1;
        }
        else
        {
            zstdgpu_BarrierTracker_PushPrevState(tracker, resId);
            state->writePass = kzstdgpu_BarrierTracker_Clean;
        }
    }

    if (kzstdgpu_BarrierTracker_AccessSource_Bind == source && writes)
    {
        state->writePass   = pass;
        state->writePassId = passId;
    }
}

static void zstdgpu_BarrierTracker_RunPass(zstdgpu_BarrierTracker *tracker, const zstdgpu_GpuOnlyBuffers *buffers, zstdgpu_Srt_Pass pass, zstdgpu_BarrierTracker_AccessSource source, uint32_t line, uint64_t *seen)
{
    const zstdgpu_Srt_ResAccessRange range = kzstdgpu_Srt_Pass_ResAccess[pass];

    for (uint32_t i = 0, count = (uint32_t)range.count; i < count; ++i)
    {
        const zstdgpu_Srt_ResAccess *access = &kzstdgpu_Srt_ResAccessAll[range.first + i];

        ZSTDGPU_ASSERT(access->access < kzstdgpu_Srt_Access_To_BarrierTracker_State_Count);
        ZSTDGPU_ASSERT(access->res < kzstdgpu_BarrierTracker_ResId_Count);

        const uint32_t seenIdx = access->res / 64;
        const uint64_t seenBit = 1ull << (access->res % 64);

        if (NULL == seen || 0 == (seen[seenIdx] & seenBit))
        {
            zstdgpu_BarrierTracker_TrackAccess(tracker, buffers, (zstdgpu_Srt_ResId)access->res, kzstdgpu_Srt_Access_To_BarrierTracker_State[access->access], access->access == kzstdgpu_Srt_Access_ShaderReadWrite, pass, source, line);
            if (NULL != seen)
            {
                seen[seenIdx] |= seenBit;
            }
        }
    }
}

static void zstdgpu_BarrierTracker_Bind(ID3D12GraphicsCommandList *cmdList, zstdgpu_BarrierTracker *tracker, const zstdgpu_GpuOnlyBuffers *buffers, zstdgpu_Srt_Pass pass)
{
    ZSTDGPU_ASSERT(pass < kzstdgpu_Srt_Pass_Count);

    zstdgpu_BarrierTracker_RunPass(tracker, buffers, pass, kzstdgpu_BarrierTracker_AccessSource_Bind, 0xffffu /* no line */, NULL);
    tracker->bindCount[pass] ++;

    zstdgpu_BarrierTracker_Flush(cmdList, tracker, buffers);
}

static void zstdgpu_BarrierTracker_FutureAccessList(zstdgpu_BarrierTracker *tracker, const zstdgpu_GpuOnlyBuffers *buffers, const zstdgpu_Srt_Pass *passes, uint32_t passCount, uint32_t line)
{
    uint64_t seen[(kzstdgpu_BarrierTracker_ResId_Count + 63) / 64] = {};

    for (uint32_t i = 0; i < passCount; ++i)
    {
        ZSTDGPU_ASSERT(passes[i] < kzstdgpu_Srt_Pass_Count);
        zstdgpu_BarrierTracker_RunPass(tracker, buffers, passes[i], kzstdgpu_BarrierTracker_AccessSource_Future, line, seen);
    }
}

/** Helper macro to create an array of passes in the upcoming order to track
 *  future resource access and potentially resolve barriers earlier */
#define zstdgpu_BarrierTracker_FutureAccess(tracker, buffers, ...)          \
    do                                                                      \
    {                                                                       \
        static const zstdgpu_Srt_Pass passes_[] = { __VA_ARGS__ };          \
        zstdgpu_BarrierTracker_FutureAccessList(tracker, buffers, passes_, (sizeof(passes_) / sizeof(passes_[0])), (uint32_t)__LINE__);\
    } while (0)

#define zstdgpu_BarrierTracker_ExternAccess(tracker, buffers, resid, state) \
    zstdgpu_BarrierTracker_ExternAccessImpl(tracker, buffers, kzstdgpu_BarrierTracker_ResId_##resid, kzstdgpu_BarrierTracker_State_##state, (uint32_t)(__LINE__))

static void zstdgpu_BarrierTracker_ExternAccessImpl(zstdgpu_BarrierTracker *tracker, const zstdgpu_GpuOnlyBuffers *buffers, zstdgpu_Srt_ResId resId, zstdgpu_BarrierTracker_State state, uint32_t line)
{
    const uint32_t writes = state == kzstdgpu_BarrierTracker_State_CopyWrite || state == kzstdgpu_BarrierTracker_State_ShaderReadWrite;
    zstdgpu_BarrierTracker_TrackAccess(tracker, buffers, resId, state, writes, kzstdgpu_BarrierTracker_NoPass, kzstdgpu_BarrierTracker_AccessSource_Bind, line);
    tracker->bindCount[kzstdgpu_BarrierTracker_NoPass] ++;
}

static void zstdgpu_BarrierTracker_WriteReport(const zstdgpu_BarrierTracker *tracker)
{
#if ZSTDGPU_BARRIER_REPORT
    FILE *file = NULL;

    if (0 != fopen_s(&file, ZSTDGPU_BARRIER_REPORT_PATH, "wb"))
    {
        file = NULL;
    }
    ZSTDGPU_ASSERT_RET(NULL != file);

    fprintf(file, "\"verdict\",\"consumer\",\"occurrence\",\"resource\",\"kind\",\"before\",\"after\",\"producer\",\"producer_occurrence\",\"line\"\n");
    for (uint32_t i = 0; i < tracker->reportRowCount; ++i)
    {
        const zstdgpu_BarrierTracker_ReportRow *row = &tracker->reportRows[i];

        const char *consumer = (row->consumerPass < kzstdgpu_Srt_Pass_Count)
                             ? kzstdgpu_Srt_Pass_Name[row->consumerPass] : "<non-SRT>";

        const char *producer = (row->producerPass < kzstdgpu_Srt_Pass_Count)
                             ? kzstdgpu_Srt_Pass_Name[row->producerPass] : "<none>";

        fprintf(file, "\"%s\",\"%s\",%u,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%u,%u\n",
                row->fused ? "FUSED" : "EMITTED",
                consumer,
                (uint32_t)row->consumerPassId,
                kzstdgpu_BarrierTracker_ResName[row->resId],
                row->isUav ? "uav" : "transition",
                kzstdgpu_BarrierTracker_StateName[row->prev],
                kzstdgpu_BarrierTracker_StateName[row->next],
                producer,
                (uint32_t)row->producerPassId,
                (uint32_t)row->line);
    }
    fclose(file);
#else
    (void)tracker;
#endif
}

#endif /* ZSTDGPU_BARRIER_TRACKER_H */
