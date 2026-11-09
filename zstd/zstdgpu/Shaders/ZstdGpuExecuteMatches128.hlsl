/**
 * Copyright (C) 2026, by Pavel Martishevsky
 * Report bugs and download new versions at https://github.com/pm4rtx/DirectStorage
 *
 * This file is distributed under the MIT License. See notice at the end of `zstdgpu_shaders_extra.h` file.
 */

/**
 * A specialisation for the compute shader executing match copies.
 */

#define MAX_COPY_SIZE 128
#include "ZstdGpuExecuteMatches.hlsli"
