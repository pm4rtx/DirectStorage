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
 * This header contains declaration of functions accessing into Local Data Share (LDS), such as
 * and load/store and atomics function, and macro to declare LDS space in a shader C++/HLSL function.
 *
 * This file aims to aid portability between C++/HLSL
 */

#pragma once

#ifndef ZSTDGPU_LDS_H
#define ZSTDGPU_LDS_H

#ifdef __hlsl_dx_compiler

#ifndef ZSTDGPU_START_GROUPSHARED
#define ZSTDGPU_START_GROUPSHARED() \
    uint32_t GS_Region = 0;
#endif

#ifndef ZSTDGPU_DECLARE_GROUPSHARED
#define ZSTDGPU_DECLARE_GROUPSHARED(name, size)     \
    static const uint32_t GS_##name##_Size = size;  \
    const uint32_t GS_##name = GS_Region;           \
    GS_Region += GS_##name##_Size;
#endif

// For HLSL this file contains only definitions that don't contain references to the LDS because
// HLSL doesn't support passing `groupshared` variables into functions

#pragma dxc push
#pragma dxc diagnostic ignored "-Wundefined-internal"
static uint32_t zstdgpu_LdsLoadU32(uint32_t offsetInUInt32);
static void zstdgpu_LdsStoreU32(uint32_t offsetInUInt32, uint32_t x);
static void zstdgpu_LdsAtomicAddU32(uint32_t offsetInUInt32, uint32_t x);
static void zstdgpu_LdsAtomicMaxU32(uint32_t offsetInUInt32, uint32_t x);
static void zstdgpu_LdsAtomicMinU32(uint32_t offsetInUInt32, uint32_t x);
static void zstdgpu_LdsAtomicAndU32(uint32_t offsetInUInt32, uint32_t x);
static void zstdgpu_LdsAtomicOrU32(uint32_t offsetInUInt32, uint32_t x);
#pragma dxc pop

#else

#ifndef ZSTDGPU_START_GROUPSHARED
#define ZSTDGPU_START_GROUPSHARED() \
    uint32_t GS_Region = 0;

#endif

#ifndef ZSTDGPU_DECLARE_GROUPSHARED
#define ZSTDGPU_DECLARE_GROUPSHARED(name, size)     \
    static const uint32_t GS_##name##_Size = size;  \
    GS_Region += size;                              \
    uint32_t GS_##name[size];                       \
    /*ASSERT(GS_Remain < &GS_Region[GS_Region_Size]);*/
#endif

static inline uint32_t zstdgpu_LdsLoadU32(const uint32_t *address)
{
    return *address;
}

static inline void zstdgpu_LdsStoreU32(uint32_t *address, uint32_t x)
{
    *address = x;
}

static inline void zstdgpu_LdsAtomicAddU32(uint32_t *address, uint32_t x)
{
    *address += x;
}

static inline void zstdgpu_LdsAtomicMaxU32(uint32_t *address, uint32_t x)
{
    *address = zstdgpu_MaxU32(*address, x);
}

static inline void zstdgpu_LdsAtomicMinU32(uint32_t *address, uint32_t x)
{
    *address = zstdgpu_MinU32(*address, x);
}

static inline void zstdgpu_LdsAtomicAndU32(uint32_t *address, uint32_t x)
{
    *address &= x;
}

static inline void zstdgpu_LdsAtomicOrU32(uint32_t *address, uint32_t x)
{
    *address |= x;
}

#endif /** #ifdef __hlsl_dx_compiler */

#endif /** ZSTDGPU_LDS_H */
