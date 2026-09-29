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
 * zstdgpu_srt_decl.h
 *
 * Declarative definition of every kernel's Shader Resource Table (SRT) - a mechanism to describe
 * resource bindings from different perspectives:
 *      - how resources are used on GPU (in a shader)
 *      - how resources are used on CPU (in a C++ code)
 *
 * This mechanism also aims to avoid limitation of reflection API not carrying certain information
 * about resource usage on GPU/in a shader.
 *
 * This file is the single source of truth for kernel resource bindings.
 * Its macro lists are expanded by zstdgpu_srt_tool.c which emits:
 *
 *  1. ZstdGpuSrt_BindGroup_<GroupName>.h that contains:
 *      - a fragment of HLSL root signature for a list of resources
 *      - a declaration of HLSL resources with explicit register/space assignment
 *      - a helper function to assign HLSL resources to SRT-based structure (a way to access resources in a unified way in C++ / HLSL)
 *      - a helper function to assign C++ resources to SRT-based structures
 *
 *  2. ZstdGpuSrt_<SrtName>.h that contains
 *      - a list of ZstdGpuSrt_BindGroup_<GroupName>.h it relies on
 *      - the final HLSL root signature
 *      - declaration of HLSL resources which aren't part of any bind group
 *      - declaration of HLSL constant buffer / constant buffer structure containing all the constants
 *      - helper functions to fully initialise SRT-based structure from HLSL side (from resource bindings) and C++ side (a structure holding all resources)
 *
 *  3. zstdgpu_srt_structs.h that contains SRT-based structs shared betweeb C++ / HLSL
 *
 *  4. zstdgpu_srt_bind.h that creation of D3D12 descriptor tables and snippets to fully bind SRT-declared resources to a D3D12 command list
 */

#ifndef ZSTDGPU_SRT_DECL_H
#define ZSTDGPU_SRT_DECL_H

ZSTDGPU_SRT_INDIRECT_BIND_GROUP_NAMED_BEGIN(DispatchIndirect)
    ZSTDGPU_SRT_BUF_INDIRECT(DispatchArgs)
    ZSTDGPU_SRT_BUF_INDIRECT(DispatchCnts)
ZSTDGPU_SRT_INDIRECT_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(ParseFrames)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , CompressedData                )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , FramesRefs                    )

    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerFrameBlockCountUnc         )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerFrameBlockCountCMP         )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerFrameBlockCountAll         )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , UncBlockToCopyGroupPrfx       )

    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_OffsetAndSize         , BlocksUncRefs                 )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_OffsetAndSize         , BlocksCMPRefs                 )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , BlockSizePrefix               )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , GlobalBlockIndexPerUncBlock   )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , GlobalBlockIndexPerCmpBlock   )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(LiteralStreams)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , LitGroupEndPerHuffmanTable    )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_LitStreamInfo         , LitRefs                       )
    ZSTDGPU_SRT_BUF_RO_BYTE(CompressedData)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HufWIdToHufLitId              )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HufLitIdToLitStreamId         )
    ZSTDGPU_SRT_BUF_RW_TYPED(uint32_t, uint8_t              , DecompressedLiterals          )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(HuffmanTable)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HuffmanTableInfo              )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HuffmanTableCodeAndSymbol     )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HuffmanTableRankIndex         )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(LiteralDwords)
    ZSTDGPU_SRT_BUF_RW_STRUCT_ALIAS(uint32_t                , DecompressedLiterals, Dwords  )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(HuffmanWeights)
    ZSTDGPU_SRT_BUF_RO_TYPED(uint32_t, uint8_t              , DecompressedHuffmanWeights    )
    ZSTDGPU_SRT_BUF_RO_TYPED(uint32_t, uint8_t              , DecompressedHuffmanWeightCount)
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(HuffmanWeightsWrite)
    ZSTDGPU_SRT_BUF_RW_TYPED(uint32_t, uint8_t              , DecompressedHuffmanWeights    )
    ZSTDGPU_SRT_BUF_RW_TYPED(uint32_t, uint8_t              , DecompressedHuffmanWeightCount)
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(HuffmanTableWrite)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HuffmanTableInfo              )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HuffmanTableCodeAndSymbol     )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HuffmanTableRankIndex         )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(SequenceOutputs)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DecompressedSequenceLLen      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DecompressedSequenceMLen      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DecompressedSequenceOffs      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , BlockSizePrefix               )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset1      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset2      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset3      )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(LiteralBytes)
    ZSTDGPU_SRT_BUF_RO_TYPED(uint32_t, uint8_t              , CompressedData                )
    ZSTDGPU_SRT_BUF_RO_TYPED(uint32_t, uint8_t              , DecompressedLiterals          )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(FseProbsRead)
    ZSTDGPU_SRT_BUF_RO_TYPED(int32_t, int16_t               , FseProbs                      )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(FrameOutput)
    ZSTDGPU_SRT_BUF_RW_TYPED(uint32_t, uint8_t             , UnCompressedFramesData         )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(FseInit)
    ZSTDGPU_SRT_BUF_RO_TYPED(int32_t, int16_t              , FseProbsDefault                )
    ZSTDGPU_SRT_BUF_RW_TYPED(int32_t, int16_t              , FseProbs                       )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_FseInfo              , FseInfos                       )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                     , FseElems                       )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(ParseBlocksWrite)
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_FseInfo               , FseInfos                      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_CompressedBlockData   , CompressedBlocks              )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_OffsetAndSize         , HufRefs                       )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_LitStreamInfo         , LitRefs                       )
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_OffsetAndSize         , SeqStreamToRef                )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , SeqStreamToLLenFseId          )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , SeqStreamToOffsFseId          )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , SeqStreamToMLenFseId          )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , SeqStreamToBlockId            )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , BlockSizePrefix               )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerFrameSeqStreamMinIdx       )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamSeqStart          )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , SeqCountPrefixLookback        )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , BlockSeqCountPrefixLookback   )
    ZSTDGPU_SRT_BUF_RW_TYPED(int32_t, int16_t               , FseProbs                      )
    ZSTDGPU_SRT_BUF_RW_TYPED(uint32_t, uint8_t              , DecompressedHuffmanWeightCount)
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , LitStreamCountPrefixLookback  )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HufWIdToHufLitId              )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HufLitIdToLitStreamId         )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , HufLitIdToHufWId_DBG          )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , HufLitCompactionLookback      )
ZSTDGPU_SRT_HEAP_BIND_GROUP_END()

ZSTDGPU_SRT_BEGIN(ParseFrames, Stage0)
    ZSTDGPU_SRT_USE_BIND_GROUP(ParseFrames)

    ZSTDGPU_SRT_CONST(uint32_t                              , frameCount                    )
    ZSTDGPU_SRT_CONST(uint32_t                              , compressedBufferSizeInBytes   )
    ZSTDGPU_SRT_CONST(uint32_t                              , countBlocksOnly               )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_PASS_BEGIN(ParseFrames, CountBlocks, Stage0, Direct)
    ZSTDGPU_SRT_HEAP_BIND_RNW(BlocksUncRefs                 )
    ZSTDGPU_SRT_HEAP_BIND_RNW(BlocksCMPRefs                 )
    ZSTDGPU_SRT_HEAP_BIND_RNW(BlockSizePrefix               )
    ZSTDGPU_SRT_HEAP_BIND_RNW(UncBlockToCopyGroupPrfx       )
    ZSTDGPU_SRT_HEAP_BIND_RNW(GlobalBlockIndexPerUncBlock   )
    ZSTDGPU_SRT_HEAP_BIND_RNW(GlobalBlockIndexPerCmpBlock   )
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS(ParseFrames, WriteBlocks, Stage1, Direct)

ZSTDGPU_SRT_BEGIN(Memset, Stage0)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , Dest                          )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , value                         )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(DecompressLiterals, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(LiteralStreams)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanTable)
    ZSTDGPU_SRT_USE_BIND_GROUP(LiteralDwords)

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(InitHuffmanTableAndDecompressLiterals, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(LiteralStreams)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanWeights)

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(PrefixSum, Stage0)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , InCountsOutPrefix             )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , InCountsOutPrefixLookback     )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , outputInclusive               )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(PropagateFseIndex, Stage1)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , FseIds                        )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , FseIndexLookback              )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(ComputePrefixSum, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HufWIdToHufLitId              )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , HufLitIdToLitStreamId         )

    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , LitGroupEndPerHuffmanTable    )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , LitGroupEndPerHuffmanTableLookback)
    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , literalsPerGroup              )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(PrefixSequenceOffsets, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset1      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset2      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , PerSeqStreamFinalOffset3      )
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , PerSeqStreamFinalOffset1Lookback)
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , PerSeqStreamFinalOffset2Lookback)
    ZSTDGPU_SRT_BUF_RWGLC_STRUCT(uint32_t                   , PerSeqStreamFinalOffset3Lookback)

    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameSeqStreamMinIdx       )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameBlockCountAll         )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToBlockId            )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , frameCount                    )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(UpdateDispatchArgs, Stage0)
    /** RNW: every `inoutCounters` reference in ZstdGpuUpdateDispatchArgs.hlsl is a read. */
    ZSTDGPU_SRT_BUF_RNW_STRUCT(zstdgpu_Counters             , Counters                      )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DispatchArgs                  )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DispatchCnts                  )
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , Predicate                     )

    ZSTDGPU_SRT_CONST(uint32_t                              , decompressSequences_StreamsPerTG)
    ZSTDGPU_SRT_CONST(uint32_t                              , stage                         )
    ZSTDGPU_SRT_CONST(uint32_t                              , cmpBlockCountMax              )
    ZSTDGPU_SRT_CONST(uint32_t                              , uncBlockCountMax              )
    ZSTDGPU_SRT_CONST(uint32_t                              , litByteCountMax               )
    ZSTDGPU_SRT_CONST(uint32_t                              , seqElemCountMax               )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_PASS(UpdateDispatchArgs, AfterParseFrames, Stage0, Direct)

ZSTDGPU_SRT_PASS(UpdateDispatchArgs, AfterParseBlocks, Stage1, Direct)

ZSTDGPU_SRT_PASS_BEGIN(UpdateDispatchArgs, AfterPrefixLitGroups, Stage2, Direct)
    ZSTDGPU_SRT_ROOT_BIND_NA(Predicate)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_BEGIN(DecompressHuffmanWeights, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanWeightsWrite)

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_BYTE(CompressedData)
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , HufRefs                       )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_FseInfo               , FseInfos                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , FseElems                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(DecodeHuffmanWeights, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanWeightsWrite)

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , CompressedData                )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , HufRefs                       )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , compressedBufferSizeInBytes   )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(InitHuffmanTable, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanWeights)
    ZSTDGPU_SRT_USE_BIND_GROUP(HuffmanTableWrite)

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , fseCompressed                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(DecompressSequences, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(SequenceOutputs)

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_BYTE(CompressedData)
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , SeqStreamToRef                )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToLLenFseId          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToOffsFseId          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToMLenFseId          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToBlockId            )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_FseInfo               , FseInfos                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamSeqStart          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , FseElems                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(FinaliseSequenceOffsets, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DecompressedSequenceOffs      )

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamFinalOffset1      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamFinalOffset2      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamFinalOffset3      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamSeqStart          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameBlockCountAll         )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameSeqStreamMinIdx       )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToBlockId            )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(InitFseTable, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(FseProbsRead)

    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , FseElems                      )

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_FseInfo               , FseInfos                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , tableType                     )

    ZSTDGPU_SRT_CONST_INLINE(uint32_t                       , tableStartIndex               )
    ZSTDGPU_SRT_CONST_INLINE(uint32_t                       , tableDataStart                )
    ZSTDGPU_SRT_CONST_INLINE(uint32_t                       , tableDataCount                )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(ComputeDestBlockOffsets, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , BlockDestOffs                 )

    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , BlockSizePrefix               )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameBlockCountAll         )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , UnCompressedFramesRefs        )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , frameCount                    )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(ExecuteSequences, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(LiteralBytes)
    ZSTDGPU_SRT_USE_BIND_GROUP(FrameOutput)

    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameBlockCountCMP         )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , BlockSizePrefix               )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , BlockDestOffs                 )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , DecompressedSequenceLLen      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , DecompressedSequenceMLen      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , DecompressedSequenceOffs      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , GlobalBlockIndexPerCmpBlock   )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamSeqStart          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_CompressedBlockData   , CompressedBlocks              )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(ComputeDestSequenceOffsets, Stage2)
    ZSTDGPU_SRT_BUF_RW_STRUCT(uint32_t                      , DestSequenceOffsets           )

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , BlockDestOffs                 )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , DecompressedSequenceMLen      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerSeqStreamSeqStart          )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , SeqStreamToBlockId            )

    ZSTDGPU_SRT_CONST(uint32_t                              , tgOffset                      )
    ZSTDGPU_SRT_CONST(uint32_t                              , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(MemsetMemcpy, Stage2)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(FrameOutput)
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , UncBlockToCopyGroupPrfx       )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , BlocksUncRefs                 )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , GlobalBlockIndexPerUncBlock   )

    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_Counters              , Counters                      )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , CompressedData                )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , BlockDestOffs                 )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_BEGIN(InitResources, Stage0)
    ZSTDGPU_SRT_USE_BIND_GROUP(FseInit)

    ZSTDGPU_SRT_BUF_RW_STRUCT(zstdgpu_Counters              , Counters                      )

    ZSTDGPU_SRT_CONST(uint32_t                              , initResourcesStage            )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_PASS_BEGIN(InitResources, Counters, Stage0, Direct)

    ZSTDGPU_SRT_HEAP_BIND_RNW(FseProbs                      )
    ZSTDGPU_SRT_HEAP_BIND_RNW(FseInfos                      )
    ZSTDGPU_SRT_HEAP_BIND_RNW(FseElems                      )
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS(InitResources, FseElems, Stage1, Direct)

ZSTDGPU_SRT_BEGIN(ParseCompressedBlocks, Stage1)
    ZSTDGPU_SRT_USE_BIND_GROUP(DispatchIndirect)
    ZSTDGPU_SRT_USE_BIND_GROUP(ParseBlocksWrite)

    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , CompressedData                )
    ZSTDGPU_SRT_BUF_RO_STRUCT(zstdgpu_OffsetAndSize         , BlocksCMPRefs                 )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , PerFrameBlockCountCMP         )
    ZSTDGPU_SRT_BUF_RO_STRUCT(uint32_t                      , GlobalBlockIndexPerCmpBlock   )

    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , tgOffset                      )
    ZSTDGPU_SRT_CONST_INDIRECT(uint32_t                     , workItemCount                 )
    ZSTDGPU_SRT_CONST(uint32_t                              , compressedBufferSizeInBytes   )
    ZSTDGPU_SRT_CONST(uint32_t                              , frameCount                    )

    ZSTDGPU_SRT_CONST_INLINE(uint32_t                       , compressedBlockCount          )
ZSTDGPU_SRT_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, SeqStreamMinIdx, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerFrameSeqStreamMinIdx)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, BlockCountUncLookback, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerFrameBlockCountUncLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, BlockCountCmpLookback, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerFrameBlockCountCMPLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, BlockCountAllLookback, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerFrameBlockCountAllLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, UncBlockToCopyGroupPrfxLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, UncBlockToCopyGroupPrfxLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, LitGroupEndPerHuffmanTableLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, LitGroupEndPerHuffmanTableLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, PerSeqStreamFinalOffset1Lookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerSeqStreamFinalOffset1Lookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, PerSeqStreamFinalOffset2Lookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerSeqStreamFinalOffset2Lookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, PerSeqStreamFinalOffset3Lookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, PerSeqStreamFinalOffset3Lookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, SeqCountPrefixLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, SeqCountPrefixLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, BlockSeqCountPrefixLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, BlockSeqCountPrefixLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, LitStreamCountPrefixLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, LitStreamCountPrefixLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, HufLitCompactionLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, HufLitCompactionLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, FseIndexLookbackLLen, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, FseIndexLookbackLLen)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, FseIndexLookbackOffs, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, FseIndexLookbackOffs)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, FseIndexLookbackMLen, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, FseIndexLookbackMLen)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, HufWIdToHufLitId, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, HufWIdToHufLitId)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(Memset, BlockSizePrefixLookback, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(Dest, BlockSizePrefixLookback)
ZSTDGPU_SRT_PASS_END()


ZSTDGPU_SRT_PASS_BEGIN(PrefixSum, BlockCountUnc, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefix, PerFrameBlockCountUnc)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefixLookback, PerFrameBlockCountUncLookback)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PrefixSum, BlockCountCmp, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefix, PerFrameBlockCountCMP)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefixLookback, PerFrameBlockCountCMPLookback)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PrefixSum, BlockCountAll, Stage0, Direct)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefix, PerFrameBlockCountAll)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefixLookback, PerFrameBlockCountAllLookback)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PrefixSum, UncBlockCopyGroups, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefix, UncBlockToCopyGroupPrfx)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefixLookback, UncBlockToCopyGroupPrfxLookback)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PrefixSum, BlockSizesAll, Stage2, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefix, BlockSizePrefix)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(InCountsOutPrefixLookback, BlockSizePrefixLookback)
ZSTDGPU_SRT_PASS_END()

ZSTDGPU_SRT_PASS_BEGIN(PropagateFseIndex, LLen, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIds, SeqStreamToLLenFseId)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIndexLookback, FseIndexLookbackLLen)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PropagateFseIndex, Offs, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIds, SeqStreamToOffsFseId)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIndexLookback, FseIndexLookbackOffs)
ZSTDGPU_SRT_PASS_END()
ZSTDGPU_SRT_PASS_BEGIN(PropagateFseIndex, MLen, Stage1, Indirect)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIds, SeqStreamToMLenFseId)
    ZSTDGPU_SRT_ROOT_BIND_BY_NAME(FseIndexLookback, FseIndexLookbackMLen)
ZSTDGPU_SRT_PASS_END()

#endif /* ZSTDGPU_SRT_DECL_H */
