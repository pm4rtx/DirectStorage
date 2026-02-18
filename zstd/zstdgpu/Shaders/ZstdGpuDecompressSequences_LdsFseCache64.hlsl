/**
 * ZstdGpuDecompressSequences_LdsFseCache64.hlsl
 *
 * A variant of the compute shader decompressessing FSE-compressed sequences with
 * FSE tables being pre-cached into LDS that targets hardware with maximal supported
 * wave size of 64 lanes.
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

#define kzstdgpu_TgSizeX_DecompressSequences_LdsFseCache 64
#include "ZstdGpuDecompressSequences_LdsFseCache.hlsli"
