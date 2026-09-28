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
 * Contains zstdgpu-specific assert macro definition
 */

#pragma once

#ifndef ZSTDGPU_ASSERT_H
#define ZSTDGPU_ASSERT_H

#ifndef ZSTDGPU_ASSERT
#   ifdef __hlsl_dx_compiler
#       define ZSTDGPU_ASSERT(cond)
#       define ZSTDGPU_ASSERT_MSG(cond, msg, ...)
#   else
#       ifdef NDEBUG
            /**
             *  NOTE(pamartis): it's not a bug, we keep asserts in instrumented mode
             *  while the development is ongoing. Without debugger, they are configured
             *  to not break and produce useful output to the log if something breaks
             */
#           define TTA_ASSERT_MODE TTA_ASSERT_MODE_INSTRUMENTED
#       else
#           define TTA_ASSERT_MODE TTA_ASSERT_MODE_INSTRUMENTED
#       endif
#       include <tta_assert.h>
#       define ZSTDGPU_ASSERT(cond) TTA_ASSERT(cond)
#       define ZSTDGPU_ASSERT_MSG(cond, msg, ...) TTA_ASSERT_MSG(cond, msg, __VA_ARGS__)

#       define ZSTDGPU_ASSERT_IF(cond) TTA_ASSERT_IF(cond)
#       define ZSTDGPU_ASSERT_IF_MSG(cond, msg, ...) TTA_ASSERT_IF_MSG(cond, msg, __VA_ARGS__)

#       define ZSTDGPU_ASSERT_RETV(cond, retv) TTA_ASSERT_RET(cond, retv)
#       define ZSTDGPU_ASSERT_RETV_MSG(cond, retv, msg, ...) TTA_ASSERT_RET_MSG(cond, retv, msg, __VA_ARGS__)

#       define ZSTDGPU_ASSERT_RET(cond) do { ZSTDGPU_ASSERT_IF(cond) { } else return; } while (0)
#       define ZSTDGPU_ASSERT_RET_MSG(cond, msg, ...) do { ZSTDGPU_ASSERT_IF_MSG(cond, msg, __VA_ARGS__) { } else return; } while (0)
#   endif
#endif

#ifndef ZSTDGPU_BREAK
#   ifdef __hlsl_dx_compiler
#       define ZSTDGPU_BREAK()
#   else
#       define ZSTDGPU_BREAK() ZSTDGPU_ASSERT(0)
#   endif
#endif

#endif /* #define ZSTDGPU_ASSERT_H */
