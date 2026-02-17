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
 * zstdgpu_srt_decl_copy.h
 *
 * Defines ZSTDGPU_*_DECL macros that copy from named HLSL global resources
 * (ZstdIn##name / ZstdInOut##name) into SRT struct members (srt.in##name / srt.inout##name).
 * Include this file before invoking a ZSTDGPU_*_SRT() macro to populate the SRT struct,
 * then include zstdgpu_srt_decl_undef.h afterwards to clean up.
 */

#define ZSTDGPU_RO_RAW_BUFFER_DECL(type, name, index)                  srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RO_BUFFER_DECL(type, name, index)                      srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_BUFFER_DECL(type, name, index)                      srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RW_BUFFER_DECL_GLC(type, name, index)                  srt.inout##name = ZstdInOut##name;
#define ZSTDGPU_RO_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.in##name    = ZstdIn##name;
#define ZSTDGPU_RW_TYPED_BUFFER_DECL(hlsl_type, type, name, index)     srt.inout##name = ZstdInOut##name;
