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
 * zstdgpu_srt_tool.c
 *
 * Code generator for the declarative SRT system. Includes zstdgpu_srt_tool.h
 *
 * Build (MSVC):
 *      cl.exe /nologo /Zc:preprocessor /W4 /WX zstdgpu_srt_tool.c /Fe:zstdgpu_srt_tool.exe
 *
 * Run:
 *      zstdgpu_srt_tool.exe <outputDirectory>
 *
 * /Zc:preprocessor is required: the conformant preprocessor is needed for stringification of macro
 * parameters that share a name with a struct member.
 */

#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <sal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define MAX_ROOT_DWORDS 64
#define STAGE_COUNT     3

#define kGroupBindKindCount 2

enum
{
    /** corresponds to non-RW objects in HLSL and D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE in D3D12 */
    kAccessRO = 0,

    /** corresponds to non-RW objects in HLSL and D3D12_RESOURCE_STATE_UNORDERED_ACCESS in D3D12 */
    kAccessRW = 1,

    /**
     *  exists to establish a promise the shader binds the resource as RW resource but never writes
     *  so the barrier between two passes with RNW access can be avoided
     */
    kAccessRNW = 2,

    /** exists solely to track the fact that some buffers are required to be in indirect state (in D3D12)
     *  it doesn't generate any binding code for D3D12 */
    kAccessIndirect = 3,

    /** exists solely to merge kAccessIndirect and kAccessRO together in `accessMerge` */
    kAccessROIndirect = 4,

    /** exists to establish a promise in the pass declaration that the shader isn't going to use
     *  the resource so if resource transition is required, the barrier could be deferred or skipped */
    kAccessNone = 5
};

/** Indexed by kAccessRO..kAccessROIndirect, in enum order; kAccessNone never reaches emission. */
static const char *const kAccessTokenText[] =
{
    "kzstdgpu_Srt_Access_ShaderRead",           /* kAccessRO         */
    "kzstdgpu_Srt_Access_ShaderReadWrite",      /* kAccessRW         */
    "kzstdgpu_Srt_Access_ShaderReadNoWrite",    /* kAccessRNW        */
    "kzstdgpu_Srt_Access_IndirectRead",         /* kAccessIndirect   */
    "kzstdgpu_Srt_Access_ShaderIndirectRead"    /* kAccessROIndirect */
};

enum
{
    kKindStruct = 0,
    kKindTyped = 1,
    kKindByte = 2
};

enum
{
    Direct = 0,
    Indirect = 1
};

enum
{
    Stage0 = 0,
    Stage1 = 1,
    Stage2 = 2
};

enum
{
    /** Heap bind group: corresponds to a descriptor table in D3D12/HLSL */
    kGroupHeap          = 0,

    /** Root bind group: corresponds to a range of root descriptors in D3D12/HLSL */
    kGroupRoot          = 1,

    /** Indirect const group: a range of constants that correspond to RootConstants block in D3D12/HLSL that is set by indirect Dispatch */
    kGroupConstIndirect = 2,

    /** Const group: a range of constants that correspond to RootConstants block in D3D12/HLSL */
    kGroupConst         = 3,

    /** Inline const group: a range of constants that are set within shader */
    kGroupConstInline   = 4,

    /** A group of buffers driving indirect dispatch, corresponds to indirect arguments and counts buffers in D3D12 */
    kGroupIndirect      = 5,

    kGroupTypeCount     = 6
};

static const char *const kGroupTypeSufx[kGroupTypeCount] =
{
    "HeapGroup",
    "RootGroup",
    "ConstIndirectGroup",
    "ConstGroup",
    "ConstInlineGroup",
    "IndirectGroup"
};

static const char *const kGroupTypeDesc[kGroupTypeCount] =
{
    "heap bind group",
    "root bind group",
    "indirect constant group",
    "constant group",
    "inline constant group",
    "indirect buffer group"
};


static const uint16_t kBoundConstKinds[2] = { kGroupConstIndirect, kGroupConst };

static int groupTypeIsConst(uint16_t type)
{
    return kGroupConstIndirect == type || kGroupConst == type || kGroupConstInline == type;
}

static int  g_errorCount = 0;

static void fail(_Printf_format_string_ const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "[zstdgpu_srt_tool] [FAIL] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    g_errorCount += 1;
}

static void warn(_Printf_format_string_ const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "[zstdgpu_srt_tool] [WARN] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/** Append-only string-map index: 0 is error, 1 is empty, and 0xffff is never allocated. */
typedef uint16_t    NameId;

static const NameId kNameIdFull = 0xffff;
static const NameId kNameIdError = 0;
static const NameId kNameIdEmpty = 1;

typedef struct CStrToNameId
{
    char  *key; /**< owned by the map's string arena */
    size_t len;
} CStrToNameId;

static CStrToNameId *gCStrToNameId = NULL;

#ifdef _MSC_VER
__pragma(warning(push))
__pragma(warning(disable : 4090))
#endif

static NameId cstrIntern(const char *cstr)
{
    CStrToNameId cstr2nameS;
    ptrdiff_t    found;

    if (NULL == gCStrToNameId)
    {
        sh_new_arena(gCStrToNameId);

        cstr2nameS.key = "(error)";
        cstr2nameS.len = strlen(cstr2nameS.key);
        shputs(gCStrToNameId, cstr2nameS);

        cstr2nameS.key = "";
        cstr2nameS.len = strlen(cstr2nameS.key);
        shputs(gCStrToNameId, cstr2nameS);
    }

    found = shgeti(gCStrToNameId, cstr);
    if (-1 == found)
    {
        const size_t rawLen = shlenu(gCStrToNameId);
        if (rawLen >= kNameIdFull)
        {
            fail("`gCStrToNameId` is full. Increase `NameId` bit width");
            return kNameIdError;
        }
        cstr2nameS.key = cstr;
        cstr2nameS.len = strlen(cstr2nameS.key);
        shputs(gCStrToNameId, cstr2nameS);
        return (NameId)rawLen;
    }
    return (NameId)found;
}
#ifdef _MSC_VER
__pragma(warning(pop))
#endif

static const char *nameToCStr(NameId id)
{
    assert((size_t)id < shlenu(gCStrToNameId) && "NameId out of range");
    return gCStrToNameId[id].key;
}

static size_t nameToCStrLen(NameId id)
{
    assert((size_t)id < shlenu(gCStrToNameId) && "NameId out of range");
    return gCStrToNameId[id].len;
}

typedef struct Entry
{
    /** Default access specified by SRT: kAccess{RO,RW,RNW,Indirect} */
    uint16_t bindAccess : 3;

    /** The actual access a pass inherits from SRT or overrides: kAccess{RO,RW,RNW,Indirect} + optionally {None} */
    uint16_t realAccess : 3;

    uint16_t kind   : 2;
    uint16_t glc    : 1;
    uint16_t pad    : 7;

    NameId   hlslType;
    NameId   dataType;

    /** Shader-facing slot name, unchanged by resource rebinding within pass */
    NameId   bindName;

    /** CPU resource name, originally identical to `bindName`, though may differ from `bindName`
     *  if SRT slot is alias or there's a pass that rebinds the resource with separate resource name. */
    NameId   resName;
} Entry;

/** The three strings derived from an Entry's identity.
 *  They are deliberately *not* in Entry because it's used as a hash key */
typedef struct EntryText
{
    NameId macroText;
    NameId memberText;
    NameId globalText;
} EntryText;

typedef struct Group
{
    NameId   name;

    /** kNameIdError for a declared template; Pass::binderName for a pass-private clone. */
    NameId   pass;
} Group;

typedef struct GroupData
{
    /** if this bind group (B) was cloned from another bind group (A) because pass
     *  has overridden the binding (different resource name or access):
     *      - `base` holds `groupId` of group A;
     *  otherwise:
     *      - `base` holds `groupId` of itself
     */
    uint16_t base;

    /** assigned HLSL register space */
    uint16_t space;

    /** 1 - means this group is not explicitly named, so can't be re-used by other SRTs and doesn't emit a header, 0 - otherwise */
    uint16_t unnamed;

    /** Start in gConstIds for any constant kind, otherwise in gEntryIds. */
    uint16_t entryStart;
    uint16_t entryCount;

    /** Column width for EntryText::macroText or Const::type, padded before emission. */
    uint16_t maxMacroLen;

    /** Column width for EntryText::memberText or Const::name, padded before emission. */
    uint16_t maxMemberLen;

    /** Column width for EntryText::globalText or Const::name, padded before emission. */
    uint16_t maxGlobalLen;

    /** Bit N set when this descriptor table needs a copy in stage N.
     *  Zero means no pass binds this heap group.
     *  It's currently for bind group matching D3D12 descriptor table versioning:
     *      - a new stage re-creates D3D12 descriptors from D3D12 resources.
     *      - so whenever resource change (e.g. re-created or initialised for the first time) -- a new D3D12 descriptor table is created
     */
    uint32_t stageMask;

    NameId   cloneSuffix;

    uint16_t tableOwner;

    /** Emitted name for a bound heap owner, assigned once when its ownership is settled. */
    NameId   tableName;

    uint16_t type;
} GroupData;

typedef struct Const
{
    NameId      type;
    NameId      name;
} Const;

typedef struct Srt
{
    /** SRT name, used as key in gSrts lookup */
    NameId key;

    /** This SRT's range in gGroupIds, preserving declaration order across kinds. */
    uint16_t groupStart;
    uint16_t groupCount;

    /** This SRT's contiguous range in each gGroupIdsByType[type] array. */
    uint16_t perTypeGroupStart[kGroupTypeCount];
    uint16_t perTypeGroupCount[kGroupTypeCount];

    /** Derived from indirect inputs when the SRT declaration closes. */
    uint16_t indirect : 1;

    /** Stage index, not a mask; checked against the earliest pass stage. */
    uint16_t declStage : 2;

    /** At least one explicit pass was declared, suppressing the implicit default pass.
     *  Does not imply that any resource or access changed. */
    uint16_t passOverrides : 1;

    /** Set only if resName changes (not access) */
    uint16_t hasResourceRebindings : 1;

    uint16_t hasPass : 1;
    uint16_t earliestPassStage : 2;
    uint16_t hasIndirectPass : 1;
    uint16_t padding : 7;
} Srt;

/** Make sure `declStage` and `earliestPassStage` can hold stage count (<=4),
 *  otherwise bitfield width needs to be increased  */
typedef char zstdgpu_srt_assert_declStage_holds_STAGE_COUNT[(STAGE_COUNT <= 4) ? 1 : -1];

typedef struct Pass
{
    int      srtIdx;
    int      indirect;
    NameId   name; /**< always set; the pass addDefaultPasses() synthesises is named "Default" */

    /** Submission stage index; explicit passes declare it, the implicit pass inherits declStage. */
    int      stage;

    /** <Srt> for the implicit default pass, <Srt>_<Pass> otherwise. */
    NameId   binderName;

    /** Ranges in gGroupIdsByType[type], shared with the SRT until that kind is first modified.
     *  Substitution preserves group and entry positions. */
    uint16_t perTypeGroupStart[kGroupBindKindCount];
    uint16_t perTypeGroupCount[kGroupBindKindCount];
} Pass;

typedef struct EntryId { uint16_t id; } EntryId;
typedef struct ConstId { uint16_t id; } ConstId;
typedef struct GroupId { uint16_t id; } GroupId;

static const GroupId kInvalidGroup = { UINT16_MAX };

static int groupIdIsValid(GroupId id) { return id.id != kInvalidGroup.id; }

typedef struct EntryKey { Entry key; } EntryKey;
typedef struct ConstKey { Const key; } ConstKey;
typedef struct GroupKey { Group key; } GroupKey;

static EntryKey  *gEntries = NULL;
static EntryText *gEntryTexts = NULL; /* parallel to gEntries, indexed by EntryId */
static EntryId   *gEntryIds = NULL;

static ConstKey  *gConsts = NULL;
static ConstId   *gConstIds = NULL;

static GroupKey  *gGroups = NULL;
static GroupData *gGroupData = NULL;  /* parallel to gGroups, indexed by GroupId */
static GroupId   *gGroupIds = NULL;

/** Per-kind group-id ranges shared by SRTs and unmodified passes. */
static GroupId *gGroupIdsByType[kGroupTypeCount] = { NULL };

static int       gEntryDeclCount = 0;
static int       gConstDeclCount = 0;

static Srt    *gSrts = NULL;
static Pass   *gPasses = NULL;

static size_t srtCount(void)
{
    return hmlenu(gSrts);
}

static size_t passCount(void)
{
    return arrlenu(gPasses);
}

static GroupId g_currentGroup = { UINT16_MAX }; /* kInvalidGroup */

static int    g_currentSrt = -1;
static int    g_currentPass = -1;

/** stb_ds character array: arrlen includes the terminating NUL; NULL represents an empty string. */
typedef struct StrBuilder
{
    char *data;
} StrBuilder;

static size_t sb_Len(const StrBuilder *sb)
{
    const size_t used = arrlenu(sb->data);
    return (used > 0) ? used - 1 : 0;
}

static void sb_Reset(StrBuilder *sb)
{
    sb->data[0] = '\0';
    arrsetlen(sb->data, 1);
}

static StrBuilder *sb_AppendCStr(StrBuilder *sb, const char *text, size_t len)
{
    const size_t used = sb_Len(sb);
    arrsetlen(sb->data, used + len + 1);
    memcpy(&sb->data[used], text, len);
    sb->data[used + len] = '\0';
    return sb;
}

/** Caller must supply a string literal; sizeof(lit) does not validate that contract. */
#define sb_StrLit(sb, lit)    sb_AppendCStr(sb, lit, sizeof(lit) - 1)
#define sb_StrLitEoL(sb, lit) sb_AppendCStr(sb, lit "\n", sizeof(lit))
#define sb_ExtraLine(sb)      sb_AppendCStr(sb, "\n", sizeof("\n") - 1)

static StrBuilder *sb_Str(StrBuilder *sb, const char *text)
{
    return sb_AppendCStr(sb, text, strlen(text));
}

static StrBuilder *sb_Fmt(StrBuilder *sb, _Printf_format_string_ const char *fmt, ...)
{
    va_list args0, args1;
    int     len;

    va_start(args0, fmt);
    va_copy(args1, args0);
    len = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);

    if (len < 0)
    {
        assert(0 && "sb_Fmt: vsnprintf failed to measure a literal format");
    }
    else
    {
        size_t used = sb_Len(sb);
        size_t need = (size_t)len + 1;
        arrsetlen(sb->data, used + need);
        vsnprintf(&sb->data[used], need, fmt, args0);
    }
    va_end(args0);
    return sb;
}

static const char *sb_CStr(StrBuilder *sb)
{
    return (NULL != sb->data) ? sb->data : "";
}

static void sb_BeginFile(StrBuilder *sb, const char *guard)
{
    sb_StrLitEoL(sb,
        "/**\n"
        " * Copyright (c) Microsoft. All rights reserved.\n"
        " * This code is licensed under the MIT License (MIT).\n"
        " * THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF\n"
        " * ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY\n"
        " * IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR\n"
        " * PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.\n"
        " *\n"
        " * Advanced Technology Group (ATG)\n"
        " *\n"
        " * AUTO-GENERATED by zstdgpu_srt_tool.exe. DO NOT EDIT.\n"
        " * Source of truth: zstdgpu_srt_decl.h\n"
        " */\n");
    sb_Fmt(sb,
        "#ifndef %s\n"
        "#define %s\n"
        "\n",
        guard, guard);
}

static void sb_EndFile(StrBuilder *sb, const char *guard, const char *path)
{
    size_t len;
    FILE  *outputFile;

    sb_Fmt(sb, "#endif /* %s */\n", guard);

    len = sb_Len(sb);

    outputFile = fopen(path, "wb");
    if (NULL == outputFile)
    {
        fail("cannot open '%s' for writing", path);
        return;
    }
    if (len != fwrite(sb->data, 1, len, outputFile))
    {
        fail("failed to write '%s'", path);
    }
    fclose(outputFile);

    arrfree(sb->data);
    sb->data = NULL;
}

static StrBuilder gScratch = { NULL };

static NameId nameConcatWithUnderscore(NameId nameA, NameId nameB)
{
    sb_Str(&gScratch, nameToCStr(nameA));
    sb_StrLit(&gScratch, "_");
    sb_Str(&gScratch, nameToCStr(nameB));

    const NameId name = cstrIntern(gScratch.data);
    sb_Reset(&gScratch);
    return name;
}

static NameId nameConcat2(NameId nameA, NameId nameB)
{
    return (kNameIdEmpty == nameB) ? nameA : nameConcatWithUnderscore(nameA, nameB);
}

static NameId nameConcatIndex(NameId prefix, const char *suffix, int index)
{
    NameId name;
    sb_Str(&gScratch, nameToCStr(prefix));
    sb_Fmt(&gScratch, "%s%d", suffix, index);
    name = cstrIntern(gScratch.data);
    sb_Reset(&gScratch);
    return name;
}

typedef struct NameIdSet
{
    NameId key;
} NameIdSet;

static NameIdSet *gResourceNames = NULL;

static int resourceCount(void)
{
    return (int)hmlen(gResourceNames);
}

static const char *resourceIdToCStr(int id)
{
    assert((size_t)id < hmlenu(gResourceNames) && "Supplied resource `id` doesn't exist");
    return nameToCStr(gResourceNames[id].key);
}

static void registerResource(NameId name)
{
    NameIdSet entry = { name };
    hmputs(gResourceNames, entry);
}

typedef struct GroupRange
{
    const GroupId *ids;
    size_t         count;
} GroupRange;

static GroupRange srtGroupRangePerType(const Srt *srt, uint16_t type)
{
    GroupRange range;

    assert(type < kGroupTypeCount && "unknown group kind");
    range.count = srt->perTypeGroupCount[type];
    range.ids = (0 == range.count) ? NULL : gGroupIdsByType[type] + srt->perTypeGroupStart[type];
    return range;
}

static GroupRange passGroupRangePerType(const Pass *pass, uint16_t type)
{
    GroupRange range;

    assert(type < kGroupBindKindCount && "a pass keeps ranges only for the bind kinds");
    range.count = pass->perTypeGroupCount[type];
    range.ids = (0 == range.count) ? NULL : gGroupIdsByType[type] + pass->perTypeGroupStart[type];
    return range;
}

#define forEachGroup(gid, ...)                                                                     \
    do                                                                                             \
    {                                                                                              \
        const uint16_t allGroupCnt_ = (uint16_t)hmlenu(gGroups);                                   \
        for (uint16_t idx_ = 0; idx_ < allGroupCnt_; ++idx_)                                       \
        {                                                                                          \
            const GroupId  gid = { idx_ };                                                         \
            GroupData     *groupData = groupDataGet(gid);                                          \
            const uint16_t groupType = groupData->type;                                            \
            (void)groupData;                                                                       \
            (void)groupType;                                                                       \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    } while (0)

#define forEachStageTable(stage, gid, ...)                                                          \
    forEachGroup(gid,                                                                               \
    {                                                                                               \
        if (kGroupHeap == groupType && groupOwnsTable(gid) && 0 != (groupData->stageMask & (1u << (stage))))\
        {                                                                                           \
            __VA_ARGS__                                                                             \
        }                                                                                           \
    })

#define forEachSrtGroup(srt, gid, groupIdx, ...)                                                   \
    do                                                                                             \
    {                                                                                              \
        const Srt *srt_ = (srt);                                                                   \
        for (uint16_t groupIdx = 0; groupIdx < srt_->groupCount; ++groupIdx)                       \
        {                                                                                          \
            const GroupId  gid = gGroupIds[srt_->groupStart + groupIdx];                           \
            GroupData     *groupData = groupDataGet(gid);                                          \
            const uint16_t groupType = groupData->type;                                            \
            (void)groupData;                                                                       \
            (void)groupType;                                                                       \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    } while (0)

#define forEachGroupInRange(range, gid, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        const GroupRange range_ = (range);                                                         \
        for (size_t group_ = 0; group_ < range_.count; ++group_)                                   \
        {                                                                                          \
            const GroupId gid = range_.ids[group_];                                                \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    } while (0)

#define forEachEntryInGroup(gid, idx, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
        const GroupId    entryGroup_ = (gid);                                                      \
        const GroupData *entryGroupData_ = groupDataGet(entryGroup_);                              \
        assert(!groupTypeIsConst(entryGroupData_->type) && "not an entry group");                   \
        for (uint16_t idx = 0; idx < entryGroupData_->entryCount; ++idx)                           \
        {                                                                                          \
            const EntryId entryId_ = gEntryIds[(uint32_t)entryGroupData_->entryStart + idx];       \
            const Entry *entry = &gEntries[entryId_.id].key;                                       \
            const EntryText *entryText = &gEntryTexts[entryId_.id];                                \
            (void)entry; (void)entryText;                                                          \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    } while (0)

#define forEachGroupEntry(range, gid, idx, ...)                                                    \
    forEachGroupInRange(range, gid,                                                                \
    {                                                                                              \
        forEachEntryInGroup(gid, idx, __VA_ARGS__);                                                \
    })

#define forEachGroupConst(range, gid, idx, ...)                                                    \
    forEachGroupInRange(range, gid,                                                                \
    {                                                                                              \
        const GroupData *groupData_ = groupDataGet(gid);                                           \
        assert(groupTypeIsConst(groupData_->type) && "not a constant group");                       \
        for (uint16_t idx = 0; idx < groupData_->entryCount; ++idx)                                \
        {                                                                                          \
            const Const *cst = &gConsts[gConstIds[(uint32_t)groupData_->entryStart + idx].id].key; \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    })

#define forEachSrtBoundConst(srt, groupType, gid, idx, ...)                                        \
    do                                                                                             \
    {                                                                                              \
        for (int boundKind_ = 0; boundKind_ < 2; ++boundKind_)                                     \
        {                                                                                          \
            const uint16_t groupType = kBoundConstKinds[boundKind_];                               \
                                                                                                   \
            forEachGroupConst(srtGroupRangePerType(srt, groupType), gid, idx, __VA_ARGS__);        \
        }                                                                                          \
    } while (0)

static GroupId groupFindByName(NameId name)
{
    Group group = { name, kNameIdError };
    ptrdiff_t found = hmgeti(gGroups, group);
    if (found >= 0)
    {
        assert(found < (ptrdiff_t)kInvalidGroup.id && "incorrect group index returned from gGroups");
        GroupId groupId = { (uint16_t)found };
        return groupId;
    }
    return kInvalidGroup;
}

static GroupData *groupDataGet(GroupId gi)
{
    assert((size_t)gi.id < arrlenu(gGroupData) && "group index out of range");
    return &gGroupData[gi.id];
}

static const Group *group(GroupId gi)
{
    assert((size_t)gi.id < hmlenu(gGroups) && "group index out of range");
    return &gGroups[gi.id].key;
}

static int groupIsClone(GroupId gi)
{
    return groupDataGet(gi)->base != gi.id;
}

static GroupId groupBase(GroupId gi)
{
    const GroupId base = { groupDataGet(gi)->base };
    return base;
}

static GroupId groupTableOwner(GroupId gi)
{
    const GroupId owner = { groupDataGet(gi)->tableOwner };
    return owner;
}

static int groupOwnsTable(GroupId gi)
{
    return groupTableOwner(gi).id == gi.id && 0 != groupDataGet(gi)->stageMask;
}

static NameId groupTableName(GroupId gi)
{
    const NameId name = groupDataGet(groupTableOwner(gi))->tableName;

    assert(kNameIdError != name && "table names must be assigned before emission");
    return name;
}

static const Entry *groupEntry(GroupId gi, int groupEntryIndex)
{
    const GroupData *groupData = groupDataGet(gi);

    assert((uint32_t)groupEntryIndex < groupData->entryCount && "entry index out of range");
    return &gEntries[gEntryIds[(uint32_t)groupData->entryStart + (uint32_t)groupEntryIndex].id].key;
}

static const EntryText *groupEntryText(GroupId gi, int groupEntryIndex)
{
    const GroupData *groupData = groupDataGet(gi);

    assert((uint32_t)groupEntryIndex < groupData->entryCount && "entry index out of range");
    return &gEntryTexts[gEntryIds[(uint32_t)groupData->entryStart + (uint32_t)groupEntryIndex].id];
}

static uint16_t groupEntryReg(GroupId gi, int groupEntryIndex)
{
    const int ro = (kAccessRO == groupEntry(gi, groupEntryIndex)->bindAccess);
    uint16_t  shaderRegisterIndex = 0;
    int       j;

    for (j = 0; j < groupEntryIndex; ++j)
    {
        shaderRegisterIndex += (uint16_t)((kAccessRO == groupEntry(gi, j)->bindAccess) == ro);
    }
    return shaderRegisterIndex;
}

#define INTERN_BY_VALUE_AND_RETURN(map, keyType, keyValue, idType, onInsert, onFound)\
    do                                                                      \
    {                                                                       \
        idType    id_ = { 0 };                                              \
        ptrdiff_t found_ = hmgeti(map, keyValue);                           \
        if (found_ >= 0)                                                    \
        {                                                                   \
            id_.id = (uint16_t)found_;                                      \
            onFound;                                                        \
        }                                                                   \
        else                                                                \
        {                                                                   \
            const size_t count_ = hmlenu(map);                              \
            if (count_ >= UINT16_MAX)                                       \
            {                                                               \
                fail("more than %d distinct " #map " -- widen " #idType, (int)UINT16_MAX);\
            }                                                               \
            else                                                            \
            {                                                               \
                keyType slot_;                                              \
                slot_.key = (keyValue);                                     \
                hmputs(map, slot_);                                         \
                id_.id = (uint16_t)count_;                                  \
                assert(hmgeti(map, keyValue) == (ptrdiff_t)id_.id && "stb_ds appended somewhere else"); \
                onInsert;                                                   \
            }                                                               \
        }                                                                   \
        return id_;                                                         \
    } while (0)

static NameId emitSrtBindEntryType(const Entry *entryValue);
static NameId emitSrtBindEntryName(const Entry *entryValue, const char *roNamePrefix, const char *rwNamePrefix);

static void entryTextsAdd(const Entry *entryValue)
{
    EntryText text;

    text.macroText = emitSrtBindEntryType(entryValue);
    text.memberText = emitSrtBindEntryName(entryValue, "in", "inout");
    text.globalText = emitSrtBindEntryName(entryValue, "ZstdIn", "ZstdInOut");
    arrput(gEntryTexts, text);
    assert(arrlenu(gEntryTexts) == hmlenu(gEntries) && "gEntryTexts drifted from gEntries");
}

static EntryId entryInternValue(const Entry *entry)
{
    INTERN_BY_VALUE_AND_RETURN(gEntries, EntryKey, *entry, EntryId, entryTextsAdd(entry), (void)0);
}

static EntryId entryIntern(uint16_t access, uint16_t kind, uint16_t glc, const char *hlslType, const char *dataType, const char *name, const char *aliasPostfix)
{
    Entry entry;

    gEntryDeclCount += 1;

    /* Entry is a raw-byte hash key; zero its padding before interning. */
    memset(&entry, 0, sizeof(Entry));
    entry.bindAccess = access;
    entry.realAccess = access;
    entry.kind = kind;
    entry.glc = glc;
    entry.hlslType = cstrIntern(hlslType);
    entry.dataType = cstrIntern(dataType);
    entry.resName = cstrIntern(name);
    entry.bindName = nameConcat2(entry.resName, cstrIntern(aliasPostfix));
    return entryInternValue(&entry);
}

static void groupsAdd(uint16_t type, GroupId base)
{
    GroupData groupData;

    memset(&groupData, 0, sizeof(groupData));
    groupData.base = base.id;
    groupData.type = type;
    groupData.entryStart = (uint16_t)(groupTypeIsConst(type) ? arrlenu(gConstIds) : arrlenu(gEntryIds));
    arrput(gGroupData, groupData);
    assert(arrlenu(gGroupData) == hmlenu(gGroups) && "gGroupData drifted from gGroups");
}

static GroupId groupIntern(NameId name, uint16_t type)
{
    Group group;

    memset(&group, 0, sizeof(group));
    group.name = name;

    INTERN_BY_VALUE_AND_RETURN(gGroups, GroupKey, group, GroupId, groupsAdd(type, id_), fail("bind group '%s' is declared more than once", nameToCStr(name)));
}

static void groupUpdateMaxTextLen(GroupData *groupData, NameId macro, NameId member, NameId global)
{
    const uint16_t macroLen = (uint16_t)nameToCStrLen(macro);
    const uint16_t memberLen = (uint16_t)nameToCStrLen(member);
    const uint16_t globalLen = (uint16_t)nameToCStrLen(global);

    groupData->maxMacroLen = groupData->maxMacroLen > macroLen ? groupData->maxMacroLen : macroLen;
    groupData->maxMemberLen = groupData->maxMemberLen > memberLen ? groupData->maxMemberLen : memberLen;
    groupData->maxGlobalLen = groupData->maxGlobalLen > globalLen ? groupData->maxGlobalLen : globalLen;
}

static void groupAppendEntry(GroupId gi, EntryId id)
{
    GroupData *groupData = groupDataGet(gi);

    if ((size_t)groupData->entryStart + (size_t)groupData->entryCount != arrlenu(gEntryIds))
    {
        fail("group '%s': its entries are not one uninterrupted run -- another group was declared in the middle of it", nameToCStr(group(gi)->name));
    }
    else
    {
        const EntryText *entryText = &gEntryTexts[id.id];

        arrput(gEntryIds, id);
        groupData->entryCount += 1;
        groupUpdateMaxTextLen(groupData, entryText->macroText, entryText->memberText, entryText->globalText);
    }
}

static void groupEnd(void)
{
    g_currentGroup = kInvalidGroup;
}

static void srtAppendGroup(Srt *srt, GroupId id)
{
    const uint16_t type = groupDataGet(id)->type;
    GroupId      **kind = &gGroupIdsByType[type];
    uint16_t      *perTypeGroupStart = &srt->perTypeGroupStart[type];
    uint16_t      *perTypeGroupCount = &srt->perTypeGroupCount[type];

    assert(type < kGroupTypeCount && "unknown group kind");
    if (arrlenu(gGroupIds) >= UINT16_MAX || arrlenu(*kind) >= UINT16_MAX)
    {
        fail("more than %d group uses -- widen GroupId", (int)UINT16_MAX);
        return;
    }
    if (srt->groupCount > 0)
    {
        if ((size_t)srt->groupStart + (size_t)srt->groupCount != arrlenu(gGroupIds))
        {
            fail("SRT '%s': its groups are not one uninterrupted run", nameToCStr(srt->key));
            return;
        }
    }
    else
    {
        srt->groupStart = (uint16_t)arrlenu(gGroupIds);
    }
    if (*perTypeGroupCount > 0)
    {
        if ((size_t)*perTypeGroupStart + (size_t)*perTypeGroupCount != arrlenu(*kind))
        {
            fail("SRT '%s': its groups of one kind are not one uninterrupted run", nameToCStr(srt->key));
            return;
        }
    }
    else
    {
        *perTypeGroupStart = (uint16_t)arrlenu(*kind);
    }
    arrput(gGroupIds, id);
    srt->groupCount += 1;
    arrput(*kind, id);
    *perTypeGroupCount += 1;
}

enum
{
    kTextMacro = 0,
    kTextMember = 1,
    kTextGlobal = 2
};

static uint16_t groupMaxLen(const GroupData *groupData, int which)
{
    switch (which)
    {
    case kTextMacro:  return groupData->maxMacroLen;
    case kTextMember: return groupData->maxMemberLen;
    default:          return groupData->maxGlobalLen;
    }
}

static uint32_t srtKindMaxLen(const Srt *srt, uint16_t groupType, int which)
{
    uint32_t max = 0;

    forEachGroupInRange(srtGroupRangePerType(srt, groupType), gid,
    {
        const uint16_t len = groupMaxLen(groupDataGet(gid), which);

        max = max > len ? max : len;
    });
    return max;
}

static int srtGroupEntryCount(const Srt *srt, uint16_t type)
{
    int memberCount = 0;

    forEachGroupInRange(srtGroupRangePerType(srt, type), gid,
    {
        memberCount += groupDataGet(gid)->entryCount;
    });
    return memberCount;
}

static ConstId constIntern(const char *type, const char *name)
{
    Const constantValue;

    gConstDeclCount += 1;

    memset(&constantValue, 0, sizeof(constantValue));
    constantValue.type = cstrIntern(type);
    constantValue.name = cstrIntern(name);

    INTERN_BY_VALUE_AND_RETURN(gConsts, ConstKey, constantValue, ConstId, (void)0, (void)0);
}

static const Const *groupConst(GroupId gi, int constantIndex)
{
    const GroupData *groupData = groupDataGet(gi);

    assert(groupTypeIsConst(groupData->type) && "not a constant group");
    assert((uint32_t)constantIndex < groupData->entryCount && "constant index out of range");
    return &gConsts[gConstIds[(uint32_t)groupData->entryStart + (uint32_t)constantIndex].id].key;
}

static void groupAppendConst(GroupId gi, ConstId id)
{
    GroupData *groupData = groupDataGet(gi);

    if ((size_t)groupData->entryStart + (size_t)groupData->entryCount != arrlenu(gConstIds))
    {
        fail("group '%s': its constants are not one uninterrupted run -- another group was declared in the middle of it", nameToCStr(group(gi)->name));
    }
    else
    {
        const Const *constantValue = &gConsts[id.id].key;

        arrput(gConstIds, id);
        groupData->entryCount += 1;
        groupUpdateMaxTextLen(groupData, constantValue->type, constantValue->name, constantValue->name);
    }
}

static int groupHasBoundConst(GroupId gi)
{
    const uint16_t type = groupDataGet(gi)->type;

    return (kGroupConstIndirect == type || kGroupConst == type) && groupDataGet(gi)->entryCount > 0;
}

static int groupHasHeader(GroupId gi)
{
    if (groupDataGet(gi)->unnamed)
    {
        return 0;
    }
    return kGroupHeap == groupDataGet(gi)->type || groupHasBoundConst(gi);
}

static int groupIsInlined(GroupId gi)
{
    return !groupHasHeader(gi) && (kGroupHeap == groupDataGet(gi)->type || groupHasBoundConst(gi));
}

static int groupGetD3D12RootParamCount(GroupId id)
{
    switch (groupDataGet(id)->type)
    {
        case kGroupHeap:
            return 1;

        case kGroupRoot:
            return (int)groupDataGet(id)->entryCount;

        case kGroupConstIndirect:
        case kGroupConst:
            return groupDataGet(id)->entryCount > 0;

        case kGroupConstInline:
        case kGroupIndirect:
            return 0;

        default:
            assert(0 && "unknown group type");
            return 0;
    }
}

/** First root parameter of `id`'s template in `srt`; `kInvalidGroup` returns the SRT's total root parameter count. */
static int srtGroupRootParam(const Srt *srt, GroupId id)
{
    const GroupId base = groupIdIsValid(id) ? groupBase(id) : kInvalidGroup;
    int rootParameterIndex = 0;

    forEachSrtGroup(srt, gid, grpIdx,
    {
        if (gid.id == base.id)
        {
            return rootParameterIndex;
        }
        rootParameterIndex += groupGetD3D12RootParamCount(gid);
    });
    assert(!groupIdIsValid(id) && "group is not used by this SRT");
    return rootParameterIndex;
}

static uint16_t checkPassDispatch(const char *name, int dispatch)
{
    if (Direct != dispatch && Indirect != dispatch)
    {
        fail("pass '%s': dispatch must be Direct or Indirect", name);
        return Direct;
    }
    return (uint16_t)dispatch;
}

static void srtBegin(const char *name, int stage)
{
    const NameId nameId = cstrIntern(name);

    if (g_currentSrt >= 0)
    {
        fail("SRT '%s' begins before SRT '%s' ends", name, nameToCStr(gSrts[g_currentSrt].key));
        return;
    }
    if (stage < 0 || stage >= STAGE_COUNT)
    {
        fail("SRT '%s' names stage %d, which is outside 0..%d", name, stage, STAGE_COUNT - 1);
        return;
    }

    if (hmgeti(gSrts, nameId) >= 0)
    {
        fail("SRT '%s' is declared more than once", name);
    }
    else
    {
        Srt srt;
        memset(&srt, 0, sizeof(Srt));
        srt.key = nameId;
        srt.declStage = (uint16_t)stage;
        hmputs(gSrts, srt);
        g_currentSrt = (int)srtCount() - 1;
        assert(hmgeti(gSrts, nameId) == g_currentSrt && "stb_ds appended somewhere else");
    }
}

#define kSlotNotDeclared kGroupTypeCount

typedef struct SrtSlot
{
    NameId      name;
    uint16_t    srtId;
} SrtSlot;

#define kPassNone 0xffffu

typedef struct SrtSlotData
{
    SrtSlot  key;

    /** `srtIndexSlots` sets it to `kGroupRoot` or `kGroupHeap` */
    uint16_t groupType : 3;

    /** `srtIndexSlots` sets it to `Entry::bindAccess`*/
    uint16_t access : 3;

    uint16_t padding: 10;

    /** selects bind group (heap/root) index within SRT */
    uint16_t typedGroupIdx;

    /** selects entry index within specified (by `typedGroupIdx`) bind group (heap/root) */
    uint16_t groupEntryIdx;

    /** Pass index or kPassNone */
    uint16_t statedBy;
} SrtSlotData;

static SrtSlotData *gSrtSlots = NULL;

static void srtIndexSlots(uint16_t srtId)
{
    static const uint16_t kIndexOrder[] = { kGroupRoot, kGroupHeap };

    const Srt *srt = &gSrts[srtId];
    int        k;

    for (k = 0; k < (int)(sizeof(kIndexOrder) / sizeof(kIndexOrder[0])); ++k)
    {
        const uint16_t type = kIndexOrder[k];
        const GroupRange groups = srtGroupRangePerType(srt, type);

        for (uint16_t typedGroupIdx = 0; typedGroupIdx < groups.count; ++typedGroupIdx)
        {
            const GroupId  gid        = groups.ids[typedGroupIdx];

            forEachEntryInGroup(gid, groupEntryIdx,
            {
                const SrtSlot slot = { entry->bindName, srtId };

                if (hmgeti(gSrtSlots, slot) >= 0)
                {
                    warn("SRT '%s' declares %s bind group entry '%s' more than once.", nameToCStr(srt->key), (kGroupRoot == type) ? "root" : "heap", nameToCStr(entry->bindName));
                }
                else
                {
                    SrtSlotData slotEntry;

                    slotEntry.key           = slot;
                    slotEntry.groupType     = type;
                    slotEntry.access        = entry->bindAccess;
                    slotEntry.padding       = 0;
                    slotEntry.typedGroupIdx = typedGroupIdx;
                    slotEntry.groupEntryIdx = groupEntryIdx;
                    slotEntry.statedBy      = kPassNone;

                    hmputs(gSrtSlots, slotEntry);
                }
            });
        }
    }
}

static void srtEnd(void)
{
    if (g_currentSrt >= 0)
    {
        Srt *srt = &gSrts[g_currentSrt];

        srt->indirect = (srtGroupRangePerType(srt, kGroupIndirect).count > 0 ||
                         srtGroupRangePerType(srt, kGroupConstIndirect).count > 0) ? Indirect : Direct;
        srtIndexSlots((uint16_t)g_currentSrt);
    }
    g_currentSrt = -1;
    g_currentGroup = kInvalidGroup; /* an unterminated group block must not leak into the next SRT */
}

static void srtUseGroup(const char *groupName)
{
    if (g_currentSrt < 0)
    {
        fail("ZSTDGPU_SRT_USE_BIND_GROUP(%s) outside of an SRT", groupName);
    }
    else
    {
        const GroupId id = groupFindByName(cstrIntern(groupName));

        if (groupIdIsValid(id))
        {
            srtAppendGroup(&gSrts[g_currentSrt], id);

            return;
        }
        fail("SRT '%s' references undefined bind group '%s'", nameToCStr(gSrts[g_currentSrt].key), groupName);
    }
}

static const char *const kKindWord[] = { "BUFFER", "TYPED_BUFFER", "RAW_BUFFER" };

/**
 * Expands `Entry` into the following macro:
 *      ZSTDGPU_{RO|RW}_{|TYPED_|RAW_}BUFFER[_GLC](hlslType[, dataType])
 */
static NameId emitSrtBindEntryType(const Entry *entryValue)
{
    NameId name;
    sb_Fmt(&gScratch, "ZSTDGPU_%s_%s", (kAccessRO == entryValue->bindAccess) ? "RO" : "RW", kKindWord[entryValue->kind]);
    if (0 != entryValue->glc)
    {
        sb_StrLit(&gScratch, "_GLC");
    }

    sb_Fmt(&gScratch, "(%s", nameToCStr(entryValue->hlslType));
    if (kKindTyped == entryValue->kind)
    {
        sb_Fmt(&gScratch, ", %s", nameToCStr(entryValue->dataType));
    }
    sb_StrLit(&gScratch, ")");

    name = cstrIntern(gScratch.data);
    sb_Reset(&gScratch);
    return name;
}

static NameId emitSrtBindEntryName(const Entry *entryValue, const char *roNamePrefix, const char *rwNamePrefix)
{
    NameId name;
    sb_Str(&gScratch, (kAccessRO == entryValue->bindAccess) ? roNamePrefix : rwNamePrefix);
    sb_Str(&gScratch, nameToCStr(entryValue->bindName));
    name = cstrIntern(gScratch.data);
    sb_Reset(&gScratch);
    return name;
}

/** Named groups may be declared at file or SRT scope; an in-SRT declaration also adds a use there. */
static void groupBeginNamed(const char *name, uint16_t type)
{
    if (groupIdIsValid(g_currentGroup))
    {
        fail("%s '%s' declared inside '%s' -- groups do not nest", kGroupTypeDesc[type], name, nameToCStr(group(g_currentGroup)->name));
        return;
    }
    g_currentGroup = groupIntern(cstrIntern(name), type);
    if (g_currentSrt >= 0)
    {
        srtAppendGroup(&gSrts[g_currentSrt], g_currentGroup);
    }
}

/** How many unnamed groups of `type` the SRT already has */
static int srtUnnamedGroupCount(const Srt *srt, uint16_t type)
{
    int unnamedGroupCount = 0;

    forEachGroupInRange(srtGroupRangePerType(srt, type), gid,
    {
        unnamedGroupCount += (0 != groupDataGet(gid)->unnamed);
    });
    return unnamedGroupCount;
}

static GroupId srtNewUnnamedGroup(uint16_t type)
{
    Srt    *srt;
    GroupId id;

    if (g_currentSrt < 0)
    {
        fail("an unnamed %s must be declared inside an SRT", kGroupTypeDesc[type]);
        return kInvalidGroup;
    }
    srt = &gSrts[g_currentSrt];
    id = groupIntern(nameConcatIndex(srt->key, kGroupTypeSufx[type], srtUnnamedGroupCount(srt, type)), type);
    groupDataGet(id)->unnamed = 1;
    srtAppendGroup(srt, id);
    return id;
}

static GroupId srtImplicitGroup(uint16_t type)
{
    const Srt *srt;

    if (g_currentSrt < 0)
    {
        fail("an unnamed %s must be declared inside an SRT", kGroupTypeDesc[type]);
        return kInvalidGroup;
    }
    srt = &gSrts[g_currentSrt];
    if (srt->groupCount > 0)
    {
        const GroupId last = gGroupIds[srt->groupStart + srt->groupCount - 1];

        if (type == groupDataGet(last)->type && 0 != groupDataGet(last)->unnamed)
        {
            return last;
        }
    }
    return srtNewUnnamedGroup(type);
}

static void groupBeginUnnamed(uint16_t type)
{
    if (groupIdIsValid(g_currentGroup))
    {
        fail("an unnamed %s is declared inside '%s' -- groups do not nest", kGroupTypeDesc[type], nameToCStr(group(g_currentGroup)->name));
        return;
    }
    g_currentGroup = srtNewUnnamedGroup(type);
}

static const char *dxgiFormatForCpuType(const char *elementType)
{
    static const char *const kMap[][2] = {
        {  "uint8_t",   "DXGI_FORMAT_R8_UINT" },
        { "uint16_t",  "DXGI_FORMAT_R16_UINT" },
        { "uint32_t",  "DXGI_FORMAT_R32_UINT" },
        {  "int16_t",  "DXGI_FORMAT_R16_SINT" },
        {  "int32_t",  "DXGI_FORMAT_R32_SINT" },
        {    "float", "DXGI_FORMAT_R32_FLOAT" }
    };
    int i;

    for (i = 0; i < (int)(sizeof(kMap) / sizeof(kMap[0])); ++i)
    {
        if (0 == strcmp(elementType, kMap[i][0]))
        {
            return kMap[i][1];
        }
    }
    return NULL;
}

static void addBuf(uint16_t access, uint16_t kind, uint16_t glc, const char *hlslType, const char *dataType, const char *name, const char *aliasPostfix)
{
    GroupId target = g_currentGroup;

    if (!groupIdIsValid(target))
    {
        /* Do not leave an implicit root group open: following constants need their own group. */
        target = srtImplicitGroup(kGroupRoot);
        if (!groupIdIsValid(target))
        {
            return;
        }
    }

    if (groupTypeIsConst(groupDataGet(target)->type))
    {
        fail("constant group '%s': buffer entry '%s' cannot be declared in it", nameToCStr(group(target)->name), name);
        return;
    }
    if (kGroupIndirect == groupDataGet(target)->type)
    {
        fail("indirect buffer group '%s': buffer '%s' must use ZSTDGPU_SRT_BUF_INDIRECT", nameToCStr(group(target)->name), name);
        return;
    }
    if (kKindTyped == kind && kGroupHeap != groupDataGet(target)->type)
    {
        fail("'%s': typed buffer '%s' cannot be a root descriptor -- D3D12 allows only raw/byte buffers or structured buffers there, so put it in a bind group", nameToCStr(group(target)->name), name);
    }
    if (kKindTyped == kind && NULL == dxgiFormatForCpuType(dataType))
    {
        fail("typed buffer '%s': no DXGI format mapping for element type '%s' -- add it to dxgiFormatForCpuType", name, dataType);
    }

    groupAppendEntry(target, entryIntern(access, kind, glc, hlslType, dataType, name, aliasPostfix));
}

static void addConst(uint16_t groupType, const char *type, const char *name)
{
    if (groupIdIsValid(g_currentGroup))
    {
        if (groupType != groupDataGet(g_currentGroup)->type)
        {
            fail("%s '%s': constant '%s' cannot be declared in it -- it belongs in a %s", kGroupTypeDesc[groupDataGet(g_currentGroup)->type], nameToCStr(group(g_currentGroup)->name), name, kGroupTypeDesc[groupType]);
            return;
        }
        groupAppendConst(g_currentGroup, constIntern(type, name));
        return;
    }
    if (g_currentSrt < 0)
    {
        fail("constant '%s' declared outside of an SRT or a constant group", name);
        return;
    }
    groupAppendConst(srtImplicitGroup(groupType), constIntern(type, name));
}

static Pass *passNew(int srtIdx, NameId name, int stage, int indirect)
{
    Pass     pass;
    uint16_t type;

    if (passCount() >= kPassNone)
    {
        fail("more than %u passes -- widen SrtSlotData::statedBy so no pass index reaches kPassNone", (unsigned)kPassNone);
        return NULL;
    }

    memset(&pass, 0, sizeof(Pass));
    pass.srtIdx = srtIdx;
    pass.name = name;
    pass.indirect = indirect;
    pass.stage = stage;
    pass.binderName = gSrts[srtIdx].passOverrides ? nameConcatWithUnderscore(gSrts[srtIdx].key, name) : gSrts[srtIdx].key;

    for (type = 0; type < kGroupBindKindCount; ++type)
    {
        pass.perTypeGroupStart[type] = gSrts[srtIdx].perTypeGroupStart[type];
        pass.perTypeGroupCount[type] = gSrts[srtIdx].perTypeGroupCount[type];
    }
    arrput(gPasses, pass);
    Srt *srt = &gSrts[srtIdx];
    if (!srt->hasPass || stage < srt->earliestPassStage)
    {
        srt->earliestPassStage = (uint16_t)stage;
    }
    srt->hasPass = 1;
    srt->hasIndirectPass |= (Indirect == indirect);
    return &gPasses[passCount() - 1];
}

static void passBegin(const char *kernel, const char *name, int stage, int dispatch)
{
    const NameId    kernelId = cstrIntern(kernel);
    const ptrdiff_t srtIdx = hmgeti(gSrts, kernelId);

    if (g_currentSrt >= 0)
    {
        fail("pass '%s' of SRT '%s' must be declared after ZSTDGPU_SRT_END()", name, kernel);
        return;
    }
    if (srtIdx < 0)
    {
        fail("pass '%s' references undefined SRT '%s'", name, kernel);
        return;
    }
    if (stage < 0 || stage >= STAGE_COUNT)
    {
        fail("pass '%s' of SRT '%s' names stage %d, which is outside 0..%d", name, kernel, stage, STAGE_COUNT - 1);
        return;
    }
    dispatch = checkPassDispatch(name, dispatch);
    if (Indirect == dispatch && Direct == gSrts[srtIdx].indirect)
    {
        fail("pass '%s' of SRT '%s' dispatches Indirect but its SRT is Direct", name, kernel);
        return;
    }
    gSrts[srtIdx].passOverrides = 1;
    passNew((int)srtIdx, cstrIntern(name), stage, dispatch);
    g_currentPass = (int)passCount() - 1;
}

static void passEnd(void)
{
    g_currentPass = -1;
}

#define kAccessKeep 0xffffu

static void groupCloneAdd(GroupId id, GroupId base, NameId clonedPassName)
{
    uint16_t i;

    groupsAdd(groupDataGet(base)->type, base);
    groupDataGet(id)->unnamed     = groupDataGet(base)->unnamed;
    groupDataGet(id)->cloneSuffix = clonedPassName;
    for (i = 0; i < groupDataGet(base)->entryCount; ++i)
    {
        groupAppendEntry(id, gEntryIds[groupDataGet(base)->entryStart + i]);
    }
}

static GroupId passOwnGroup(Pass *pass, GroupId base)
{
    Group key;

    if (group(base)->pass == pass->binderName)
    {
        return base;
    }
    assert(!groupIsClone(base) && "a pass's list holds either its own clone or the template");

    memset(&key, 0, sizeof(key));
    key.name = group(base)->name;
    key.pass = pass->binderName;

    INTERN_BY_VALUE_AND_RETURN(gGroups, GroupKey, key, GroupId, groupCloneAdd(id_, base, pass->name), (void)0);
}

static GroupId passOwnGroupAt(Pass *pass, uint16_t type, uint16_t typedGroupIdx)
{
    const uint16_t count = pass->perTypeGroupCount[type];
    uint16_t       start = pass->perTypeGroupStart[type];

    assert(type < kGroupBindKindCount && "a pass keeps ranges only for the bind kinds");
    assert(typedGroupIdx < count && "position is not in this pass's range");

    if (start == gSrts[pass->srtIdx].perTypeGroupStart[type])
    {
        if (arrlenu(gGroupIdsByType[type]) + count >= UINT16_MAX)
        {
            fail("more than %d group uses -- widen Pass::perTypeGroupStart", (int)UINT16_MAX);
            return gGroupIdsByType[type][start + typedGroupIdx];
        }
        else
        {
            uint16_t srcStart = start;
            uint16_t dstStart = (uint16_t)arraddnindex(gGroupIdsByType[type], count);

            memcpy(gGroupIdsByType[type] + dstStart, gGroupIdsByType[type] + srcStart, (size_t)count * sizeof(GroupId));

            pass->perTypeGroupStart[type] = start = dstStart;
        }
    }
    {
        const GroupId owned = passOwnGroup(pass, gGroupIdsByType[type][start + typedGroupIdx]);

        gGroupIdsByType[type][start + typedGroupIdx] = owned;
        return owned;
    }
}

static GroupId passCloneOf(const Pass *pass, GroupId baseGid)
{
    forEachGroupInRange(passGroupRangePerType(pass, groupDataGet(baseGid)->type), gid,
    {
        if (groupBase(gid).id == baseGid.id)
        {
            return gid;
        }
    });
    return kInvalidGroup;
}

static GroupId srtSharedTable(int srtIdx, GroupId baseGid)
{
    GroupId found = kInvalidGroup;

    for (size_t i = 0; i < passCount(); ++i)
    {
        GroupId clone;

        if (gPasses[i].srtIdx != srtIdx)
        {
            continue;
        }
        clone = passCloneOf(&gPasses[i], baseGid);
        if (!groupIdIsValid(clone))
        {
            continue;
        }
        if (!groupIdIsValid(found))
        {
            found = groupTableOwner(clone);
        }
        else if (found.id != groupTableOwner(clone).id)
        {
            return kInvalidGroup;
        }
    }
    return found;
}

static void passBindSlot(const char *macro, uint16_t slotBindGroup, const char *slot, const char *resource, uint16_t slotAccess)
{
    Pass      *pass;
    NameId     slotId;
    NameId     resourceId;
    uint16_t   access;
    uint16_t   bindGroup;
    ptrdiff_t  found;

    if (g_currentPass < 0)
    {
        fail("%s(%s) outside of a pass", macro, slot);
        return;
    }
    pass = &gPasses[g_currentPass];
    slotId = cstrIntern(slot);
    resourceId = (NULL == resource) ? slotId : cstrIntern(resource);

    bindGroup = kSlotNotDeclared;
    access = kAccessNone;

    {
        const SrtSlot key = { slotId, (uint16_t)pass->srtIdx };

        found = hmgeti(gSrtSlots, key);
    }
    if (found >= 0)
    {
        access = gSrtSlots[found].access;
        bindGroup = gSrtSlots[found].groupType;
    }
    /* Recognise ExecuteIndirect buffers to diagnose attempts to override their mandatory access. */
    if (kSlotNotDeclared == bindGroup && Indirect == pass->indirect)
    {
        forEachGroupEntry(srtGroupRangePerType(&gSrts[pass->srtIdx], kGroupIndirect), indirectGid, j,
        {
            if (entry->bindName == slotId)
            {
                access = kAccessIndirect;
                bindGroup = kGroupIndirect;
                break;
            }
        });
    }

    if (bindGroup != slotBindGroup)
    {
        switch (bindGroup)
        {
        case kGroupRoot:
            fail("%s(%s): '%s' is a root descriptor of SRT '%s' -- use ZSTDGPU_SRT_ROOT_BIND%s", macro, slot, slot, nameToCStr(gSrts[pass->srtIdx].key), macro + sizeof("ZSTDGPU_SRT_HEAP_BIND") - 1);
            return;
        case kGroupHeap:
            fail("%s(%s): '%s' is a bind group entry, not a root descriptor -- use ZSTDGPU_SRT_HEAP_BIND%s", macro, slot, slot, macro + sizeof("ZSTDGPU_SRT_ROOT_BIND") - 1);
            return;
        case kGroupIndirect:
            fail("%s(%s): ExecuteIndirect reads '%s' whatever the shader does, so neither what it binds nor its access is the shader's to change", macro, slot, slot);
            return;
        default:
            fail("%s(%s): SRT '%s' does not declare '%s'", macro, slot, nameToCStr(gSrts[pass->srtIdx].key), slot);
            return;
        }
    }
    if (kAccessRNW == slotAccess && kAccessRW != access)
    {
        fail("%s(%s): SRT '%s' does not declare '%s' as RW, so there is no write to drop", macro, slot, nameToCStr(gSrts[pass->srtIdx].key), slot);
        return;
    }
    assert(found >= 0 && "kind matched wantKind, so the SRT index holds this slot");
    if ((uint16_t)g_currentPass == gSrtSlots[found].statedBy)
    {
        fail("%s(%s): pass '%s' already states what it does with '%s' -- one statement per slot, so say the resource and the access together with a _BY_NAME_NA or _BY_NAME_RNW form",
             macro, slot, nameToCStr(pass->name), slot);
        return;
    }
    gSrtSlots[found].statedBy = (uint16_t)g_currentPass;

    if (kAccessKeep != slotAccess || resourceId != slotId)
    {
        const uint16_t type          = bindGroup;
        const uint16_t typedGroupIdx = gSrtSlots[found].typedGroupIdx;
        const uint16_t groupEntryIdx = gSrtSlots[found].groupEntryIdx;
        const GroupId  gid           = passOwnGroupAt(pass, type, typedGroupIdx);
        Entry          entryValue;
        EntryId        entryId;

        entryValue = gEntries[gEntryIds[groupDataGet(gid)->entryStart + groupEntryIdx].id].key;
        assert(entryValue.bindName == slotId && "the pass's groups no longer sit where the SRT's do");

        if (resourceId != slotId && resourceId != entryValue.resName)
        {
            gSrts[pass->srtIdx].hasResourceRebindings = 1;
            entryValue.resName = resourceId;
        }
        if (kAccessKeep != slotAccess && !(kAccessRNW == slotAccess && kAccessRW != entryValue.realAccess))
        {
            entryValue.realAccess = slotAccess;
        }
        entryId = entryInternValue(&entryValue);

        gEntryIds[groupDataGet(gid)->entryStart + groupEntryIdx] = entryId;
    }
}

static void addIndirectBuf(const char *name)
{
    if (!groupIdIsValid(g_currentGroup) || kGroupIndirect != groupDataGet(g_currentGroup)->type)
    {
        fail("indirect buffer '%s' must be declared inside an indirect bind group", name);
        return;
    }
    groupAppendEntry(g_currentGroup, entryIntern(kAccessIndirect, kKindStruct, 0, "uint32_t", "uint32_t", name, ""));
}

#define ZSTDGPU_SRT_HEAP_BIND_GROUP_NAMED_BEGIN(name)         groupBeginNamed(#name, kGroupHeap);
#define ZSTDGPU_SRT_HEAP_BIND_GROUP_BEGIN()                   groupBeginUnnamed(kGroupHeap);
#define ZSTDGPU_SRT_HEAP_BIND_GROUP_END()                     groupEnd();

#define ZSTDGPU_SRT_ROOT_BIND_GROUP_NAMED_BEGIN(name)         groupBeginNamed(#name, kGroupRoot);
#define ZSTDGPU_SRT_ROOT_BIND_GROUP_BEGIN()                   groupBeginUnnamed(kGroupRoot);
#define ZSTDGPU_SRT_ROOT_BIND_GROUP_END()                     groupEnd();

#define ZSTDGPU_SRT_CONST_GROUP_NAMED_BEGIN(name)             groupBeginNamed(#name, kGroupConst);
#define ZSTDGPU_SRT_CONST_GROUP_BEGIN()                       groupBeginUnnamed(kGroupConst);
#define ZSTDGPU_SRT_CONST_GROUP_END()                         groupEnd();

#define ZSTDGPU_SRT_CONST_INDIRECT_GROUP_NAMED_BEGIN(name)    groupBeginNamed(#name, kGroupConstIndirect);
#define ZSTDGPU_SRT_CONST_INDIRECT_GROUP_BEGIN()              groupBeginUnnamed(kGroupConstIndirect);
#define ZSTDGPU_SRT_CONST_INDIRECT_GROUP_END()                groupEnd();

#define ZSTDGPU_SRT_BEGIN(name, stage)                        srtBegin(#name, (stage));
#define ZSTDGPU_SRT_END()                                     srtEnd();
#define ZSTDGPU_SRT_USE_BIND_GROUP(name)                      srtUseGroup(#name);

#define ZSTDGPU_SRT_PASS_BEGIN(srt, pass, stage, disp)        passBegin(#srt, #pass, (stage), (disp));
#define ZSTDGPU_SRT_PASS_END()                                passEnd();

#define ZSTDGPU_SRT_PASS(srt, pass, stage, disp)              passBegin(#srt, #pass, (stage), (disp)); passEnd();

#define ZSTDGPU_SRT_ROOT_BIND(slot)                           passBindSlot("ZSTDGPU_SRT_ROOT_BIND", kGroupRoot, #slot, NULL, kAccessKeep);
#define ZSTDGPU_SRT_ROOT_BIND_NA(slot)                        passBindSlot("ZSTDGPU_SRT_ROOT_BIND_NA", kGroupRoot, #slot, NULL, kAccessNone);
#define ZSTDGPU_SRT_ROOT_BIND_RNW(slot)                       passBindSlot("ZSTDGPU_SRT_ROOT_BIND_RNW", kGroupRoot, #slot, NULL, kAccessRNW);

#define ZSTDGPU_SRT_ROOT_BIND_BY_NAME(slot, name)             passBindSlot("ZSTDGPU_SRT_ROOT_BIND_BY_NAME", kGroupRoot, #slot, #name, kAccessKeep);
#define ZSTDGPU_SRT_ROOT_BIND_BY_NAME_NA(slot, name)          passBindSlot("ZSTDGPU_SRT_ROOT_BIND_BY_NAME_NA", kGroupRoot, #slot, #name, kAccessNone);
#define ZSTDGPU_SRT_ROOT_BIND_BY_NAME_RNW(slot, name)         passBindSlot("ZSTDGPU_SRT_ROOT_BIND_BY_NAME_RNW", kGroupRoot, #slot, #name, kAccessRNW);

#define ZSTDGPU_SRT_HEAP_BIND_NA(slot)                        passBindSlot("ZSTDGPU_SRT_HEAP_BIND_NA", kGroupHeap, #slot, NULL, kAccessNone);
#define ZSTDGPU_SRT_HEAP_BIND_RNW(slot)                       passBindSlot("ZSTDGPU_SRT_HEAP_BIND_RNW", kGroupHeap, #slot, NULL, kAccessRNW);

/** Rebind a heap slot in the pass's private group. */
#define ZSTDGPU_SRT_HEAP_BIND_BY_NAME(slot, name)             passBindSlot("ZSTDGPU_SRT_HEAP_BIND_BY_NAME", kGroupHeap, #slot, #name, kAccessKeep);
#define ZSTDGPU_SRT_HEAP_BIND_BY_NAME_NA(slot, name)          passBindSlot("ZSTDGPU_SRT_HEAP_BIND_BY_NAME_NA", kGroupHeap, #slot, #name, kAccessNone);
#define ZSTDGPU_SRT_HEAP_BIND_BY_NAME_RNW(slot, name)         passBindSlot("ZSTDGPU_SRT_HEAP_BIND_BY_NAME_RNW", kGroupHeap, #slot, #name, kAccessRNW);

#define ZSTDGPU_SRT_BUF_RO_STRUCT(type, name)                 addBuf(kAccessRO, kKindStruct, 0, #type, #type, #name, "");
#define ZSTDGPU_SRT_BUF_RNW_STRUCT(type, name)                addBuf(kAccessRNW, kKindStruct, 0, #type, #type, #name, "");
#define ZSTDGPU_SRT_BUF_RW_STRUCT(type, name)                 addBuf(kAccessRW, kKindStruct, 0, #type, #type, #name, "");
#define ZSTDGPU_SRT_BUF_RWGLC_STRUCT(type, name)              addBuf(kAccessRW, kKindStruct, 1, #type, #type, #name, "");

#define ZSTDGPU_SRT_BUF_RO_TYPED(hlslType, dataType, name)    addBuf(kAccessRO, kKindTyped, 0, #hlslType, #dataType, #name, "");
#define ZSTDGPU_SRT_BUF_RNW_TYPED(hlslType, dataType, name)   addBuf(kAccessRNW, kKindTyped, 0, #hlslType, #dataType, #name, "");
#define ZSTDGPU_SRT_BUF_RW_TYPED(hlslType, dataType, name)    addBuf(kAccessRW, kKindTyped, 0, #hlslType, #dataType, #name, "");
#define ZSTDGPU_SRT_BUF_RWGLC_TYPED(hlslType, dataType, name) addBuf(kAccessRW, kKindTyped, 1, #hlslType, #dataType, #name, "");

#define ZSTDGPU_SRT_BUF_RO_BYTE(name)                         addBuf(kAccessRO, kKindByte, 0, "uint32_t", "uint32_t", #name, "");
#define ZSTDGPU_SRT_BUF_RNW_BYTE(name)                        addBuf(kAccessRNW, kKindByte, 0, "uint32_t", "uint32_t", #name, "");
#define ZSTDGPU_SRT_BUF_RW_BYTE(name)                         addBuf(kAccessRW, kKindByte, 0, "uint32_t", "uint32_t", #name, "");
#define ZSTDGPU_SRT_BUF_RWGLC_BYTE(name)                      addBuf(kAccessRW, kKindByte, 1, "uint32_t", "uint32_t", #name, "");

#define ZSTDGPU_SRT_BUF_RO_STRUCT_ALIAS(type, name, postfix)  addBuf(kAccessRO, kKindStruct, 0, #type, #type, #name, #postfix);
#define ZSTDGPU_SRT_BUF_RW_STRUCT_ALIAS(type, name, postfix)  addBuf(kAccessRW, kKindStruct, 0, #type, #type, #name, #postfix);

#define ZSTDGPU_SRT_CONST(type, name)                         addConst(kGroupConst, #type, #name);
#define ZSTDGPU_SRT_CONST_INDIRECT(type, name)                addConst(kGroupConstIndirect, #type, #name);
#define ZSTDGPU_SRT_CONST_INLINE(type, name)                  addConst(kGroupConstInline, #type, #name);

#define ZSTDGPU_SRT_INDIRECT_BIND_GROUP_NAMED_BEGIN(name)     groupBeginNamed(#name, kGroupIndirect);
#define ZSTDGPU_SRT_INDIRECT_BIND_GROUP_BEGIN()               groupBeginUnnamed(kGroupIndirect);
#define ZSTDGPU_SRT_INDIRECT_BIND_GROUP_END()                 groupEnd();
#define ZSTDGPU_SRT_BUF_INDIRECT(name)                        addIndirectBuf(#name);

#ifndef ZSTDGPU_SRT_DECL_PATH
#define ZSTDGPU_SRT_DECL_PATH zstdgpu_srt_decl.h
#endif
#define ZSTDGPU_STRINGIFY_(tokens) #tokens
#define ZSTDGPU_STRINGIFY(tokens)  ZSTDGPU_STRINGIFY_(tokens)

static void collect(void)
{
#include ZSTDGPU_STRINGIFY(ZSTDGPU_SRT_DECL_PATH)
    if (g_currentSrt >= 0)
    {
        fail("SRT '%s' is missing ZSTDGPU_SRT_END()", nameToCStr(gSrts[g_currentSrt].key));
        srtEnd();
    }
}

/** After explicit pass declarations, add default ones one pass only where `passOverrides` is clear */
static void addDefaultPasses(void)
{
    for (size_t s = 0; s < srtCount(); ++s)
    {
        Srt *srt = &gSrts[s];

        if (0 == srt->passOverrides)
        {
            passNew((int)s, cstrIntern("Default"), srt->declStage, srt->indirect);
        }
    }
}

static void deriveGroupUsage(void)
{
    for (size_t p = 0; p < passCount(); ++p)
    {
        const Pass *pass = &gPasses[p];

        forEachGroupInRange(passGroupRangePerType(pass, kGroupHeap), gid,
        {
            groupDataGet(gid)->stageMask |= (uint32_t)(1u << pass->stage);
        });
    }
}

static void checkSrtStages(void)
{
    for (size_t s = 0; s < srtCount(); ++s)
    {
        const Srt *srt = &gSrts[s];
        if (srt->hasPass && srt->earliestPassStage != srt->declStage)
        {
            fail("SRT '%s' is declared in stage %d, but its earliest pass runs in stage %d", nameToCStr(srt->key), (int)srt->declStage, (int)srt->earliestPassStage);
        }
    }
}

static void registerBoundResources(void)
{
    for (size_t i = 0; i < passCount(); ++i)
    {
        uint16_t type;

        for (type = 0; type < kGroupBindKindCount; ++type)
        {
            forEachGroupEntry(passGroupRangePerType(&gPasses[i], type), gid, j,
            {
                registerResource(entry->resName);
            });
        }
    }
}

static int srtIndirectConstsRootSlot(const Srt *srt)
{
    const GroupRange groups = srtGroupRangePerType(srt, kGroupConstIndirect);
    return srtGroupRootParam(srt, groups.count > 0 ? groups.ids[0] : kInvalidGroup);
}

static void checkIndirectInputs(void)
{
    for (size_t s = 0; s < srtCount(); ++s)
    {
        const Srt *srt = &gSrts[s];
        const GroupRange buffers = srtGroupRangePerType(srt, kGroupIndirect);

        if (Direct == srt->indirect)
        {
            continue;
        }
        if (!srt->hasIndirectPass)
        {
            warn("SRT '%s' is Indirect but all its passes are Direct", nameToCStr(srt->key));
        }
        if (0 == srtGroupEntryCount(srt, kGroupConstIndirect))
        {
            fail("SRT '%s' dispatches Indirect but declares no ZSTDGPU_SRT_CONST_INDIRECT -- the command signature has nothing to inject", nameToCStr(srt->key));
        }
        else if (srtGroupRangePerType(srt, kGroupConstIndirect).count != 1)
        {
            fail("SRT '%s' dispatches Indirect but spreads its constants over %u ZSTDGPU_SRT_CONST_INDIRECT group(s) -- exactly one is required: the command signature injects one root constants block", nameToCStr(srt->key), (unsigned)srtGroupRangePerType(srt, kGroupConstIndirect).count);
        }
        if (buffers.count != 1)
        {
            fail("SRT '%s' dispatches Indirect but uses %u indirect buffer group(s) -- exactly one is required", nameToCStr(srt->key), (unsigned)buffers.count);
            continue;
        }
        {
            const GroupId gid = buffers.ids[0];

            if (2 != groupDataGet(gid)->entryCount)
            {
                fail("indirect buffer group '%s' holds %d buffer(s) -- declare exactly 2: argument buffer then count buffer", nameToCStr(group(gid)->name), (int)groupDataGet(gid)->entryCount);
                continue;
            }
            forEachEntryInGroup(gid, i,
            {
                registerResource(entry->resName);
            });
            if (groupEntry(gid, 0)->resName == groupEntry(gid, 1)->resName)
            {
                fail("indirect buffer group '%s' names '%s' as both the argument buffer and the count buffer -- declare two distinct resources", nameToCStr(group(gid)->name), nameToCStr(groupEntry(gid, 0)->resName));
            }
        }
    }
}

/** Reserve at least one separating space, then round each column width up to four columns. */
static void alignGroupTextLens(void)
{
    forEachGroup(gid,
    {
        groupData->maxMacroLen = (uint16_t)((groupData->maxMacroLen + 3 + 1) & ~3u);
        groupData->maxMemberLen = (uint16_t)((groupData->maxMemberLen + 3 + 1) & ~3u);
        groupData->maxGlobalLen = (uint16_t)((groupData->maxGlobalLen + 3 + 1) & ~3u);
    });
}

static void assignGroupSpaces(void)
{
    uint16_t next = 1;
    forEachGroup(gid,
    {
        /* Templates are interned first, so a clone inherits an already-assigned register space. */
        if (groupIsClone(gid))
        {
            groupData->space = groupDataGet(groupBase(gid))->space;
            continue;
        }
        if (kGroupIndirect != groupType)
        {
            groupData->space = next++;
        }
    });
}

static void checkRootBudget(void)
{
    for (size_t s = 0; s < srtCount(); ++s)
    {
        const Srt *srt = &gSrts[s];
        const int  rootDwords = (int)srtGroupRangePerType(srt, kGroupHeap).count * 1
                              + srtGroupEntryCount(srt, kGroupRoot) * 2
                              + srtGroupEntryCount(srt, kGroupConstIndirect)
                              + srtGroupEntryCount(srt, kGroupConst);

        if (rootDwords > MAX_ROOT_DWORDS)
        {
            fail("SRT '%s': root signature costs %d (limit %d) DWORDs", nameToCStr(srt->key), rootDwords, MAX_ROOT_DWORDS);
        }
    }
}

static void emitHLSLBindPointNamesWithRegisters(StrBuilder *builder, GroupId gi, int space)
{
    const GroupData *groupData = groupDataGet(gi);
    const uint16_t   entryCount = groupData->entryCount;
    uint32_t         macroTextLen = groupData->maxMacroLen;
    uint32_t         globalTextLen = groupData->maxGlobalLen;

    forEachEntryInGroup(gi, i,
    {
        sb_Fmt(builder, "%-*s%-*s: register(%s%u", macroTextLen, nameToCStr(entryText->macroText), globalTextLen, nameToCStr(entryText->globalText), (kAccessRO == entry->bindAccess) ? "t" : "u", groupEntryReg(gi, i));
        if (space > 0)
            sb_Fmt(builder, ", space%d", space);
        sb_StrLitEoL(builder, ");");
    });

    if (entryCount > 0)
        sb_ExtraLine(builder);
}

static void emitHLSLRootBindPointNamesWithRegisters(StrBuilder *builder, const Srt *srt)
{
    const uint32_t macroTextLen = srtKindMaxLen(srt, kGroupRoot, kTextMacro);
    const uint32_t globalTextLen = srtKindMaxLen(srt, kGroupRoot, kTextGlobal);

    forEachGroupEntry(srtGroupRangePerType(srt, kGroupRoot), gid, idx,
    {
        const int space = (int)groupDataGet(gid)->space;

        sb_Fmt(builder, "%-*s%-*s: register(%s%u", macroTextLen, nameToCStr(entryText->macroText), globalTextLen, nameToCStr(entryText->globalText), (kAccessRO == entry->bindAccess) ? "t" : "u", groupEntryReg(gid, idx));
        if (space > 0)
            sb_Fmt(builder, ", space%d", space);
        sb_StrLitEoL(builder, ");");
    });

    if (srtGroupEntryCount(srt, kGroupRoot) > 0)
        sb_ExtraLine(builder);
}

static void emitFillFromGlobals(StrBuilder *builder, GroupId gi)
{
    const uint32_t memberTextLen = groupDataGet(gi)->maxMemberLen;

    forEachEntryInGroup(gi, i,
    {
        sb_Fmt(builder, "    srt.%-*s= %s;\n", memberTextLen, nameToCStr(entryText->memberText), nameToCStr(entryText->globalText));
    });
}

static void emitFillFromResources(StrBuilder *builder, GroupId gi)
{
    const uint32_t memberTextLen = groupDataGet(gi)->maxMemberLen;

    forEachEntryInGroup(gi, i,
    {
        sb_Fmt(builder, "    srt.%-*s= cpuRes.%s;\n", memberTextLen, nameToCStr(entryText->memberText), nameToCStr(entry->resName));
    });
}

static void emitGroupRsFragment(StrBuilder *builder, GroupId gi, const char *lead)
{
    const GroupData *groupData = groupDataGet(gi);
    const uint16_t   type = groupData->type;
    const uint16_t   memberCount = groupData->entryCount;
    int              i;

    sb_StrLit(builder, "\"");
    sb_Str(builder, lead);

    if (kGroupHeap == type)
    {
        int reg[2] = { 0, 0 };
        int emitted = 0;

        sb_StrLit(builder, "DescriptorTable(");
        i = 0;
        while (i < memberCount)
        {
            const int ro = (kAccessRO == groupEntry(gi, i)->bindAccess);
            int       run = 1;

            while (i + run < memberCount && (kAccessRO == groupEntry(gi, i + run)->bindAccess) == ro)
            {
                ++run;
            }
            if (emitted++ > 0)
            {
                sb_StrLit(builder, ", ");
            }
            sb_Fmt(builder, "%s(%s%d, space=%d, numDescriptors=%d)", ro ? "SRV" : "UAV", ro ? "t" : "u", reg[ro], (int)groupData->space, run);
            reg[ro] += run;
            i += run;
        }
        sb_StrLit(builder, ")\"");
        return;
    }

    assert(groupHasBoundConst(gi) && "emitGroupRsFragment: expected a bound-constant group");

    sb_Fmt(builder, "RootConstants(b0, space=%d, num32BitConstants=%d)\"", (int)groupData->space, (int)memberCount);
}

static void emitConstGroupBlock(StrBuilder *builder, GroupId gi)
{
    const GroupData *groupData = groupDataGet(gi);
    const char      *gname = nameToCStr(group(gi)->name);
    const uint16_t   constantCount = groupData->entryCount;
    const uint32_t   maxTypeLen = groupData->maxMacroLen;
    int              i;

    sb_Fmt(builder, "typedef struct zstdgpu_%s_Consts\n{\n", gname);
    for (i = 0; i < constantCount; ++i)
        sb_Fmt(builder, "    %-*s%s;\n", maxTypeLen, nameToCStr(groupConst(gi, i)->type), nameToCStr(groupConst(gi, i)->name));

    sb_Fmt(builder, "} zstdgpu_%s_Consts;\n\n", gname);

    sb_Fmt(builder, "ConstantBuffer<zstdgpu_%s_Consts> ZstdConstants_%s : register(b0", gname, gname);
    if (groupData->space > 0)
        sb_Fmt(builder, ", space%d", (int)groupData->space);

    sb_StrLitEoL(builder, ");\n");
}

static void emitGroupHeader(const char *dir, GroupId gi)
{
    StrBuilder  sb = { NULL };
    StrBuilder  guard = { NULL };
    StrBuilder  path = { NULL };

    const GroupData *groupData = groupDataGet(gi);
    const char      *gname = nameToCStr(group(gi)->name);
    const int        isConst = groupTypeIsConst(groupData->type);

    sb_Fmt(&guard, "ZSTDGPU_SRT_GENERATED_RS_BIND_GROUP_%s_H", gname);
    sb_Fmt(&path, "%s/ZstdGpuSrt_BindGroup_%s.h", dir, gname);

    sb_BeginFile(&sb, sb_CStr(&guard));

    sb_StrLitEoL(&sb, "#ifdef __hlsl_dx_compiler");
    sb_ExtraLine(&sb);

    sb_Fmt(&sb, "#define ZSTDGPU_SRT_RS_BIND_GROUP_%s ", gname);
    emitGroupRsFragment(&sb, gi, "");
    sb_StrLitEoL(&sb, "\n");

    if (isConst)
    {
        const uint16_t constantCount = groupData->entryCount;
        const uint32_t maxNameLen = groupData->maxMemberLen;
        int            i;

        emitConstGroupBlock(&sb, gi);

        sb_Fmt(&sb, "template<typename T>\nstatic void zstdgpu_Srt_FillBindGroup_%s(ZSTDGPU_PARAM_INOUT(T) srt, zstdgpu_%s_Consts consts)\n{\n", gname, gname);
        for (i = 0; i < constantCount; ++i)
            sb_Fmt(&sb, "    srt.%-*s= consts.%s;\n", maxNameLen, nameToCStr(groupConst(gi, i)->name), nameToCStr(groupConst(gi, i)->name));

        sb_StrLitEoL(&sb, "}\n");
    }
    else
    {
        emitHLSLBindPointNamesWithRegisters(&sb, gi, (int)groupData->space);

        sb_Fmt(&sb, "template<typename T>\nstatic void zstdgpu_Srt_FillBindGroup_%s(ZSTDGPU_PARAM_INOUT(T) srt)\n{\n", gname);
        emitFillFromGlobals(&sb, gi);
        sb_StrLitEoL(&sb, "}\n");
        sb_StrLitEoL(&sb, "#else\n");

        {
            const int total = (int)hmlen(gGroups);
            int       c;

            assert(kGroupHeap == groupData->type && "expected a heap group");

            for (c = 0; c < total; ++c)
            {
                const GroupId clone = { (uint16_t)c };

                if (groupBase(clone).id == gi.id && groupOwnsTable(clone))
                {
                    sb_Fmt(&sb, "template<typename T>\nstatic void zstdgpu_Srt_FillBindGroup_%s(T &srt, const zstdgpu_ResourceDataCpu &cpuRes)\n{\n", nameToCStr(groupTableName(clone)));
                    emitFillFromResources(&sb, clone);
                    sb_StrLitEoL(&sb, "}\n");
                }
            }
        }
    }
    sb_StrLitEoL(&sb, "#endif /* #ifdef __hlsl_dx_compiler */");

    sb_EndFile(&sb, sb_CStr(&guard), sb_CStr(&path));

    arrfree(guard.data);
    arrfree(path.data);
}

static void emitSrtHeader(const char *dir, int srtIdx)
{
    const Srt   *srt = &gSrts[srtIdx];
    StrBuilder   sb = { NULL };
    StrBuilder   guard = { NULL };
    StrBuilder   path = { NULL };
    int          resourcesUsed;
    const int    multiPass = (0 != srt->hasResourceRebindings);
    uint32_t     maxTypeLen = 0, maxNameLen = 0;

    if (multiPass)
        maxTypeLen = srtKindMaxLen(srt, kGroupRoot, kTextMacro);
    maxNameLen = srtKindMaxLen(srt, kGroupRoot, kTextMember);

    for (int i = 0; i < (int)(sizeof(kBoundConstKinds) / sizeof(kBoundConstKinds[0])); ++i)
    {
        const uint32_t tlen = srtKindMaxLen(srt, kBoundConstKinds[i], kTextMacro);
        const uint32_t nlen = srtKindMaxLen(srt, kBoundConstKinds[i], kTextMember);

        maxTypeLen = maxTypeLen > tlen ? maxTypeLen : tlen;
        maxNameLen = maxNameLen > nlen ? maxNameLen : nlen;
    }

    sb_Fmt(&guard, "ZSTDGPU_SRT_GENERATED_%s_H", nameToCStr(srt->key));
    sb_Fmt(&path, "%s/ZstdGpuSrt_%s.h", dir, nameToCStr(srt->key));

    sb_BeginFile(&sb, sb_CStr(&guard));

    {
        int included = 0;

        forEachSrtGroup(srt, id, grpIdx,
        {
            if (groupHasHeader(id))
            {
                sb_Fmt(&sb, "#include \"ZstdGpuSrt_BindGroup_%s.h\"\n", nameToCStr(group(id)->name));
                included += 1;
            }
        });
        if (included > 0)
        {
            sb_ExtraLine(&sb);
        }
    }

    sb_StrLitEoL(&sb, "#ifdef __hlsl_dx_compiler\n");

    forEachGroupInRange(srtGroupRangePerType(srt, kGroupHeap), id,
    {
        if (groupIsInlined(id))
        {
            emitHLSLBindPointNamesWithRegisters(&sb, id, (int)groupDataGet(id)->space);
        }
    });

    emitHLSLRootBindPointNamesWithRegisters(&sb, srt);

    forEachSrtGroup(srt, id, grpIdx,
    {
        if (groupIsInlined(id) && kGroupHeap != groupType)
        {
            emitConstGroupBlock(&sb, id);
        }
    });

    sb_Fmt(&sb, "#define ZSTDGPU_SRT_RS_%s", nameToCStr(srt->key));
    {
        int emitted = 0;   /* root parameters written so far; drives the ", " separator */

        forEachSrtGroup(srt, id, grpIdx,
        {
            if (groupHasHeader(id) || groupIsInlined(id))
            {
                sb_StrLit(&sb, " ");
                if (groupHasHeader(id))
                {
                    if (emitted > 0)
                        sb_StrLit(&sb, "\", \" ");

                    sb_Str(&sb, "ZSTDGPU_SRT_RS_BIND_GROUP_");
                    sb_Str(&sb, nameToCStr(group(id)->name));
                }
                else
                {
                    emitGroupRsFragment(&sb, id, (emitted > 0) ? ", " : "");
                }
                emitted += 1;
            }
            else if (kGroupRoot == groupType)
            {
                forEachEntryInGroup(id, j,
                {
                    sb_StrLit(&sb, " \"");
                    if (emitted > 0)
                        sb_StrLit(&sb, ", ");

                    sb_Fmt(&sb, (kAccessRO == entry->bindAccess) ? "SRV(t%d, space=%d)\"" : "UAV(u%d, space=%d)\"", (int)groupEntryReg(id, j), (int)groupData->space);
                    emitted += 1;
                });
            }
        });
    }
    sb_StrLitEoL(&sb, "\n");

    sb_Fmt(&sb, "static void zstdgpu_Srt_Fill(ZSTDGPU_PARAM_INOUT(zstdgpu_%s_SRT) srt)\n{\n", nameToCStr(srt->key));
    {
        int filled = 0;

        forEachSrtGroup(srt, id, grpIdx,
        {
            const char *gname = nameToCStr(group(id)->name);

            if (groupHasHeader(id))
            {
                if (kGroupHeap == groupType)
                    sb_Fmt(&sb, "    zstdgpu_Srt_FillBindGroup_%s(srt);\n", gname);
                else
                    sb_Fmt(&sb, "    zstdgpu_Srt_FillBindGroup_%s(srt, ZstdConstants_%s);\n", gname, gname);

                filled += 1;
            }
            else if (groupIsInlined(id))
            {
                if (kGroupHeap == groupType)
                {
                    emitFillFromGlobals(&sb, id);
                }
                else
                {
                    for (uint16_t j = 0; j < groupData->entryCount; ++j)
                        sb_Fmt(&sb, "    srt.%-*s= ZstdConstants_%s.%s;\n", maxNameLen, nameToCStr(groupConst(id, j)->name), gname, nameToCStr(groupConst(id, j)->name));
                }
                filled += 1;
            }
        });
        if (filled > 0 && srtGroupEntryCount(srt, kGroupRoot) > 0)
        {
            sb_ExtraLine(&sb);
        }
    }

    forEachGroupEntry(srtGroupRangePerType(srt, kGroupRoot), gid, j,
    {
        sb_Fmt(&sb, "    srt.%-*s= %s;\n", maxNameLen, nameToCStr(entryText->memberText), nameToCStr(entryText->globalText));
    });

    sb_StrLitEoL(&sb, "}\n\n#else\n");

    resourcesUsed = (srtGroupRangePerType(srt, kGroupHeap).count > 0) || (0 == multiPass && srtGroupEntryCount(srt, kGroupRoot) > 0);

    static const char fillPrefix[] = "static void zstdgpu_Srt_Fill(";
    sb_Fmt(&sb, "%szstdgpu_%s_SRT &srt, const zstdgpu_ResourceDataCpu %s", fillPrefix, nameToCStr(srt->key), (0 != resourcesUsed) ? "&cpuRes" : "&");

    if (0 != multiPass)
    {
        forEachGroupEntry(srtGroupRangePerType(srt, kGroupRoot), gid, j,
        {
            sb_Fmt(&sb, ",\n%*s%-*s %s", (int)(sizeof(fillPrefix) - 1), "", maxTypeLen, nameToCStr(entryText->macroText), nameToCStr(entryText->memberText));
        });
    }

    forEachSrtBoundConst(srt, groupType, gid, idx,
    {
        sb_Fmt(&sb, ",\n%*s%-*s %s", (int)(sizeof(fillPrefix) - 1), "", maxTypeLen, nameToCStr(cst->type), nameToCStr(cst->name));
    });

    sb_StrLitEoL(&sb, ")\n{");

    forEachGroupInRange(srtGroupRangePerType(srt, kGroupHeap), id,
    {
        const GroupId shared = srtSharedTable(srtIdx, id);

        if (groupIdIsValid(shared))
        {
            if (groupHasHeader(id))
            {
                sb_Fmt(&sb, "    zstdgpu_Srt_FillBindGroup_%s(srt, cpuRes);\n", nameToCStr(groupTableName(shared)));
            }
            else
            {
                emitFillFromResources(&sb, shared);
            }
        }
    });

    if (srtGroupRangePerType(srt, kGroupHeap).count > 0 && srtGroupEntryCount(srt, kGroupRoot) > 0)
        sb_ExtraLine(&sb);

    forEachGroupEntry(srtGroupRangePerType(srt, kGroupRoot), gid, j,
    {
        sb_Fmt(&sb, "    srt.%-*s= ", maxNameLen, nameToCStr(entryText->memberText));
        if (0 != multiPass)
        {
            sb_Str(&sb, nameToCStr(entryText->memberText));
        }
        else
        {
            sb_Fmt(&sb, "cpuRes.%s", nameToCStr(entry->resName));
        }
        sb_StrLitEoL(&sb, ";");
    });
    forEachSrtBoundConst(srt, groupType, gid, idx,
    {
        sb_Fmt(&sb, "    srt.%-*s= %s;\n", maxNameLen, nameToCStr(cst->name), nameToCStr(cst->name));
    });

    sb_StrLitEoL(&sb, "}\n");

    if (0 != multiPass)
    {
        for (size_t passIdx = 0; passIdx < passCount(); ++passIdx)
        {
            const Pass *pass = &gPasses[passIdx];

            if (pass->srtIdx == srtIdx)
            {
                sb_Fmt(&sb, "static void zstdgpu_Srt_Fill_%s(zstdgpu_%s_SRT &srt, const zstdgpu_ResourceDataCpu &cpuRes", nameToCStr(pass->name), nameToCStr(srt->key));
                forEachSrtBoundConst(srt, groupType, gid, idx,
                {
                    sb_Fmt(&sb, ", %s %s", nameToCStr(cst->type), nameToCStr(cst->name));
                });

                sb_StrLitEoL(&sb, ")\n{");
                forEachGroupInRange(passGroupRangePerType(pass, kGroupHeap), gid,
                {
                    if (!groupIdIsValid(srtSharedTable(srtIdx, groupBase(gid))))
                    {
                        sb_Fmt(&sb, "    zstdgpu_Srt_FillBindGroup_%s(srt, cpuRes);\n", nameToCStr(groupTableName(gid)));
                    }
                });
                sb_StrLit(&sb, "    zstdgpu_Srt_Fill(srt, cpuRes");
                forEachGroupEntry(passGroupRangePerType(pass, kGroupRoot), rootGid, rootIdx,
                {
                    sb_Fmt(&sb, ", cpuRes.%s", nameToCStr(entry->resName));
                });
                forEachSrtBoundConst(srt, groupType, gid, idx,
                {
                    sb_Fmt(&sb, ", %s", nameToCStr(cst->name));
                });

                sb_StrLitEoL(&sb, ");\n}\n");
            }
        }
    }

    sb_StrLitEoL(&sb, "#endif\n");

    if (srtGroupEntryCount(srt, kGroupConstInline) > 0)
    {

        sb_Fmt(&sb, "static void zstdgpu_Srt_FillInline(ZSTDGPU_PARAM_INOUT(zstdgpu_%s_SRT) srt", nameToCStr(srt->key));
        forEachGroupConst(srtGroupRangePerType(srt, kGroupConstInline), gid, idx,
        {
            sb_Fmt(&sb, ", %s %s", nameToCStr(cst->type), nameToCStr(cst->name));
        });

        sb_StrLitEoL(&sb, ")\n{");
        forEachGroupConst(srtGroupRangePerType(srt, kGroupConstInline), gid, idx,
        {
            sb_Fmt(&sb, "    srt.%-*s= %s;\n", 48, nameToCStr(cst->name), nameToCStr(cst->name));
        });

        sb_StrLitEoL(&sb, "}\n");
    }

    sb_EndFile(&sb, sb_CStr(&guard), sb_CStr(&path));

    arrfree(guard.data);
    arrfree(path.data);
}


static void emitStructs(const char *dir)
{
    StrBuilder  sb = { NULL };
    StrBuilder  path = { NULL };
    const char *guard = "ZSTDGPU_SRT_GENERATED_STRUCTS_H";

    sb_Fmt(&path, "%s/zstdgpu_srt_structs.h", dir);

    sb_BeginFile(&sb, guard);

    for (size_t s = 0; s < srtCount(); ++s)
    {
        const Srt *srt = &gSrts[s];

        sb_Fmt(&sb, "typedef struct zstdgpu_%s_SRT\n{\n", nameToCStr(srt->key));

        forEachGroupEntry(srtGroupRangePerType(srt, kGroupHeap), gi, k,
        {
            sb_Fmt(&sb, "    %-*s%s;\n", 56, nameToCStr(entryText->macroText), nameToCStr(entryText->memberText));
        });
        forEachGroupEntry(srtGroupRangePerType(srt, kGroupRoot), gi, k,
        {
            sb_Fmt(&sb, "    %-*s%s;\n", 56, nameToCStr(entryText->macroText), nameToCStr(entryText->memberText));
        });
        forEachSrtBoundConst(srt, groupType, gid, idx,
        {
            sb_Fmt(&sb, "    %-*s%s;\n", 56, nameToCStr(cst->type), nameToCStr(cst->name));
        });
        forEachGroupConst(srtGroupRangePerType(srt, kGroupConstInline), gid, idx,
        {
            sb_Fmt(&sb, "    %-*s%s;\n", 56, nameToCStr(cst->type), nameToCStr(cst->name));
        });
        sb_Fmt(&sb, "} zstdgpu_%s_SRT;\n\n", nameToCStr(srt->key));
    }

    sb_EndFile(&sb, guard, sb_CStr(&path));

    arrfree(path.data);
}

static void emitConstOffsets(StrBuilder *builder)
{
    int emitted = 0;

    for (size_t i = 0; i < srtCount(); ++i)
    {
        const Srt  *srt = &gSrts[i];
        const int   indirectCnt = srtGroupEntryCount(srt, kGroupConstIndirect);
        const char *srtName = nameToCStr(srt->key);
        const int   gap = (25 > (int)strlen(srtName)) ? 25 - (int)strlen(srtName) : 1;

        if (0 == indirectCnt + srtGroupEntryCount(srt, kGroupConst))
            continue;

        emitted = 1;

        if (indirectCnt > 0)
        {
            sb_Fmt(builder, "static const uint32_t kzstdgpu_Srt_ConstIndirect_Cnt_%s%*s= %d;\n", srtName, gap, "", indirectCnt);
        }
        sb_Fmt(builder, "static const uint32_t kzstdgpu_Srt_ConstIndirect_Idx_%s%*s= %d;\n", srtName, gap, "", srtIndirectConstsRootSlot(srt));
    }

    if (0 != emitted)
    {
        sb_ExtraLine(builder);
        emitted = 0;
    }

    for (size_t i = 0; i < srtCount(); ++i)
    {
        const Srt *srt = &gSrts[i];

        forEachSrtBoundConst(srt, groupType, owner, inGroup,
        {
            const char *srtName = nameToCStr(srt->key);
            const char *cName = nameToCStr(cst->name);
            const int   len = (int)strlen(srtName) + 1 + (int)strlen(cName);
            const int   gap = (45 > len) ? 45 - len : 1;
            const int   idxGap = (44 > len) ? 44 - len : 1;

            emitted = 1;

            sb_Fmt(builder, "static const uint32_t kzstdgpu_Srt_Const_Idx_%s_%s%*s= %d;\n", srtName, cName, idxGap, "", srtGroupRootParam(srt, owner));
            sb_Fmt(builder, "static const uint32_t kzstdgpu_Srt_Const_Ofs_%s_%s%*s= %d;\n", srtName, cName, gap, "", (int)inGroup);
        });
    }

    if (0 != emitted)
    {
        sb_ExtraLine(builder);
    }
}

static void emitBindGroupEntryPush(StrBuilder *builder, const Entry *entryValue, NameId res)
{
    const char *name = nameToCStr(res);
    const char *view = (kAccessRO == entryValue->bindAccess) ? "Srv" : "Uav";

    if (kKindByte == entryValue->kind)
    {
        sb_Fmt(builder, "    zstdgpu_Srt_PushRawBuffer%s(cpuDest, descSize, device, b.%s, resInfo.%s_ByteSize);\n", view, name, name);
    }
    else if (kKindTyped == entryValue->kind)
    {
        const char *format = dxgiFormatForCpuType(nameToCStr(entryValue->dataType));

        assert(NULL != format && "typed buffer reached emission with no DXGI format");
        sb_Fmt(builder, "    zstdgpu_Srt_PushTypedBuffer%s(cpuDest, descSize, device, b.%s, resInfo.%s_ByteSize, %s, sizeof(%s));\n", view, name, name, format, nameToCStr(entryValue->dataType));
    }
    else
    {
        assert(kKindStruct == entryValue->kind && "Entry::kind holds a value no declaration macro can produce");
        sb_Fmt(builder, "    zstdgpu_Srt_PushStructBuffer%s(cpuDest, descSize, device, b.%s, resInfo.%s_ByteSize, sizeof(%s));\n", view, name, name, nameToCStr(entryValue->dataType));
    }
}

static void emitBindGroups(StrBuilder *builder)
{
    int stage;

    emitConstOffsets(builder);

    sb_StrLit(builder, "static const uint32_t kzstdgpu_Srt_HeapDescCounts[] = {");
    for (stage = 0; stage < STAGE_COUNT; ++stage)
    {
        int total = 0;

        forEachStageTable(stage, gid,
        {
            total += groupData->entryCount;
        });
        sb_Fmt(builder, "%s %d", (0 == stage) ? "" : ",", total);
    }
    sb_StrLitEoL(builder, " };\n");

    for (stage = 0; stage < STAGE_COUNT; ++stage)
    {
        sb_Fmt(builder, "/** GPU descriptor table handles of the bind groups that live in stage %d. */\n", stage);
        sb_Fmt(builder, "struct zstdgpu_Srt_BindGroups_Stage%d\n{\n", stage);
        forEachStageTable(stage, gid,
        {
            sb_StrLit(builder, "    D3D12_GPU_DESCRIPTOR_HANDLE ");
            sb_Str(builder, nameToCStr(groupTableName(gid)));
            sb_StrLitEoL(builder, ";");
        });
        sb_StrLitEoL(builder, "};\n");
    }

    sb_StrLitEoL(builder, "/**\n"
                    " * Everything a generated binder needs: the shader-visible descriptor heap, the compute\n"
                    " * PSO/root-signature pair of every SRT, and the per-stage bind group handles.\n"
                    " */\n"
                    "struct zstdgpu_Srts\n"
                    "{");
    sb_StrLitEoL(builder, "    ID3D12DescriptorHeap         *heap;");
    sb_StrLitEoL(builder, "    uint32_t                      heapOffset;\n");
    for (size_t i = 0; i < srtCount(); ++i)
    {
        sb_StrLit(builder, "    d3d12aid_ComputeRsPs          ");
        sb_Str(builder, nameToCStr(gSrts[i].key));
        sb_StrLitEoL(builder, ";");
    }
    sb_ExtraLine(builder);
    for (stage = 0; stage < STAGE_COUNT; ++stage)
    {
        sb_Fmt(builder, "    zstdgpu_Srt_BindGroups_Stage%d stage%d;\n", stage, stage);
    }
    sb_StrLitEoL(builder, "};\n");

    sb_StrLitEoL(builder,
        "static D3D12_SHADER_RESOURCE_VIEW_DESC *zstdgpu_SRV_InitAsInvalidBuffer(D3D12_SHADER_RESOURCE_VIEW_DESC *outDesc, uint64_t sizeInBytes, uint32_t strideSizeInBytes)\n"
        "{\n"
        "    ZSTDGPU_ASSERT(0 == sizeInBytes % strideSizeInBytes);\n"
        "    UINT elemCount = (UINT)(sizeInBytes / strideSizeInBytes);\n"
        "    outDesc->Format                     = DXGI_FORMAT_UNKNOWN;\n"
        "    outDesc->ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;\n"
        "    outDesc->Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;\n"
        "    outDesc->Buffer.FirstElement        = 0/*elemStart*/;\n"
        "    outDesc->Buffer.NumElements         = elemCount;\n"
        "    outDesc->Buffer.StructureByteStride = 0;\n"
        "    outDesc->Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_UNORDERED_ACCESS_VIEW_DESC *zstdgpu_UAV_InitAsInvalidBuffer(D3D12_UNORDERED_ACCESS_VIEW_DESC *outDesc, uint64_t sizeInBytes, uint32_t strideSizeInBytes)\n"
        "{\n"
        "    ZSTDGPU_ASSERT(0 == sizeInBytes % strideSizeInBytes);\n"
        "    UINT elemCount = (UINT)(sizeInBytes / strideSizeInBytes);\n"
        "    outDesc->Format                     = DXGI_FORMAT_UNKNOWN;\n"
        "    outDesc->ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;\n"
        "    outDesc->Buffer.FirstElement        = 0/*elemStart*/;\n"
        "    outDesc->Buffer.NumElements         = elemCount;\n"
        "    outDesc->Buffer.StructureByteStride = 0;\n"
        "    outDesc->Buffer.CounterOffsetInBytes= 0;\n"
        "    outDesc->Buffer.Flags               = D3D12_BUFFER_UAV_FLAG_NONE;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_SHADER_RESOURCE_VIEW_DESC *zstdgpu_SRV_InitAsRawBuffer(D3D12_SHADER_RESOURCE_VIEW_DESC *outDesc, uint64_t sizeInBytes)\n"
        "{\n"
        "    zstdgpu_SRV_InitAsInvalidBuffer(outDesc, sizeInBytes, sizeof(uint32_t));\n"
        "    outDesc->Format = DXGI_FORMAT_R32_TYPELESS;\n"
        "    outDesc->Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_UNORDERED_ACCESS_VIEW_DESC *zstdgpu_UAV_InitAsRawBuffer(D3D12_UNORDERED_ACCESS_VIEW_DESC *outDesc, uint64_t sizeInBytes)\n"
        "{\n"
        "    zstdgpu_UAV_InitAsInvalidBuffer(outDesc, sizeInBytes, sizeof(uint32_t));\n"
        "    outDesc->Format = DXGI_FORMAT_R32_TYPELESS;\n"
        "    outDesc->Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_SHADER_RESOURCE_VIEW_DESC *zstdgpu_SRV_InitAsStructBuffer(D3D12_SHADER_RESOURCE_VIEW_DESC *outDesc, uint64_t sizeInBytes, uint32_t structSizeInBytes)\n"
        "{\n"
        "    zstdgpu_SRV_InitAsInvalidBuffer(outDesc, sizeInBytes, structSizeInBytes);\n"
        "    outDesc->Buffer.StructureByteStride = structSizeInBytes;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_UNORDERED_ACCESS_VIEW_DESC *zstdgpu_UAV_InitAsStructBuffer(D3D12_UNORDERED_ACCESS_VIEW_DESC *outDesc, uint64_t sizeInBytes, uint32_t structSizeInBytes)\n"
        "{\n"
        "    zstdgpu_UAV_InitAsInvalidBuffer(outDesc, sizeInBytes, structSizeInBytes);\n"
        "    outDesc->Buffer.StructureByteStride = structSizeInBytes;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_SHADER_RESOURCE_VIEW_DESC *zstdgpu_SRV_InitAsTypedBuffer(D3D12_SHADER_RESOURCE_VIEW_DESC *outDesc, uint64_t sizeInBytes, DXGI_FORMAT format, uint32_t elemSizeInBytes)\n"
        "{\n"
        "    zstdgpu_SRV_InitAsInvalidBuffer(outDesc, sizeInBytes, elemSizeInBytes);\n"
        "    outDesc->Format = format;\n"
        "    return outDesc;\n"
        "}\n"
        "static D3D12_UNORDERED_ACCESS_VIEW_DESC *zstdgpu_UAV_InitAsTypedBuffer(D3D12_UNORDERED_ACCESS_VIEW_DESC *outDesc, uint64_t sizeInBytes, DXGI_FORMAT format, uint32_t elemSizeInBytes)\n"
        "{\n"
        "    zstdgpu_UAV_InitAsInvalidBuffer(outDesc, sizeInBytes, elemSizeInBytes);\n"
        "    outDesc->Format = format;\n"
        "    return outDesc;\n"
        "}\n"
        "/** Descriptor writers. Each advances `cpuDest` by exactly one descriptor. */\n"
        "static void zstdgpu_Srt_PushRawBufferSrv(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize)\n"
        "{\n"
        "    D3D12_SHADER_RESOURCE_VIEW_DESC SRV;\n"
        "    device->CreateShaderResourceView(resource, zstdgpu_SRV_InitAsRawBuffer(&SRV, byteSize), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n"
        "\n"
        "static void zstdgpu_Srt_PushRawBufferUav(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize)\n"
        "{\n"
        "    D3D12_UNORDERED_ACCESS_VIEW_DESC UAV;\n"
        "    device->CreateUnorderedAccessView(resource, NULL, zstdgpu_UAV_InitAsRawBuffer(&UAV, byteSize), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n"
        "\n"
        "static void zstdgpu_Srt_PushStructBufferSrv(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize, uint32_t stride)\n"
        "{\n"
        "    D3D12_SHADER_RESOURCE_VIEW_DESC SRV;\n"
        "    device->CreateShaderResourceView(resource, zstdgpu_SRV_InitAsStructBuffer(&SRV, byteSize, stride), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n"
        "\n"
        "static void zstdgpu_Srt_PushStructBufferUav(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize, uint32_t stride)\n"
        "{\n"
        "    D3D12_UNORDERED_ACCESS_VIEW_DESC UAV;\n"
        "    device->CreateUnorderedAccessView(resource, NULL, zstdgpu_UAV_InitAsStructBuffer(&UAV, byteSize, stride), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n"
        "\n"
        "static void zstdgpu_Srt_PushTypedBufferSrv(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize, DXGI_FORMAT format, uint32_t stride)\n"
        "{\n"
        "    D3D12_SHADER_RESOURCE_VIEW_DESC SRV;\n"
        "    device->CreateShaderResourceView(resource, zstdgpu_SRV_InitAsTypedBuffer(&SRV, byteSize, format, stride), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n"
        "\n"
        "static void zstdgpu_Srt_PushTypedBufferUav(D3D12_CPU_DESCRIPTOR_HANDLE &cpuDest, uint32_t descSize, ID3D12Device *device, ID3D12Resource *resource, uint32_t byteSize, DXGI_FORMAT format, uint32_t stride)\n"
        "{\n"
        "    D3D12_UNORDERED_ACCESS_VIEW_DESC UAV;\n"
        "    device->CreateUnorderedAccessView(resource, NULL, zstdgpu_UAV_InitAsTypedBuffer(&UAV, byteSize, format, stride), cpuDest);\n"
        "    cpuDest.ptr += descSize;\n"
        "}\n");

    {
        const int total = (int)hmlen(gGroups);
        int       c;

        for (c = 0; c < total; ++c)
        {
            const GroupId clone = { (uint16_t)c };

            if (kGroupHeap == groupDataGet(clone)->type && groupOwnsTable(clone))
            {
                const GroupData *tableOwnerData = groupDataGet(clone);

                sb_Fmt(builder, "static D3D12_GPU_DESCRIPTOR_HANDLE zstdgpu_Srt_InitBindGroup_%s(zstdgpu_Srts &srts, ID3D12Device *device, const zstdgpu_ResourceInfo &resInfo, const zstdgpu_GpuOnlyBuffers &b)\n{\n", nameToCStr(groupTableName(clone)));
                sb_StrLitEoL(builder, "    const uint32_t descSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);");
                sb_StrLitEoL(builder, "    const D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = d3d12aid_DescriptorHeap_GetCpuStart(srts.heap);");
                sb_StrLitEoL(builder, "    const D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = d3d12aid_DescriptorHeap_GetGpuStart(srts.heap);");
                sb_StrLitEoL(builder, "    const D3D12_GPU_DESCRIPTOR_HANDLE gpuDest  = { gpuStart.ptr + (UINT64)srts.heapOffset * descSize };");
                sb_StrLitEoL(builder, "    D3D12_CPU_DESCRIPTOR_HANDLE       cpuDest  = { cpuStart.ptr + (SIZE_T)srts.heapOffset * descSize };\n");
                forEachEntryInGroup(clone, j,
                {
                    emitBindGroupEntryPush(builder, entry, entry->resName);
                });
                sb_Fmt(builder, "\n    srts.heapOffset += %d;\n    return gpuDest;\n}\n\n", (int)tableOwnerData->entryCount);
            }
        }
    }

    for (stage = 0; stage < STAGE_COUNT; ++stage)
    {
        sb_Fmt(builder, "static void zstdgpu_Srt_InitBindGroups_Stage%d(zstdgpu_Srts &srts, ID3D12Device *device, const zstdgpu_ResourceInfo &resInfo, const zstdgpu_GpuOnlyBuffers &b)\n{\n", stage);
        int hasTables = 0;

        forEachStageTable(stage, gid,
        {
            const char *tableName = nameToCStr(groupTableName(gid));

            sb_Fmt(builder, "    srts.stage%d.%s = zstdgpu_Srt_InitBindGroup_%s(srts, device, resInfo, b);\n", stage, tableName, tableName);
            hasTables = 1;
        });
        if (0 == hasTables)
        {
            sb_StrLitEoL(builder, "    /* no descriptor tables in this stage */");
        }
        sb_StrLitEoL(builder, "}\n");
    }

    sb_StrLitEoL(builder, "static void zstdgpu_Srt_InitBindGroups_Stage(zstdgpu_Srts &srts, ID3D12Device *device, const zstdgpu_ResourceInfo &resInfo, const zstdgpu_GpuOnlyBuffers &b, uint32_t stageIndex)\n"
                    "{\n"
                    "    switch (stageIndex)\n"
                    "    {");
    for (stage = 0; stage < STAGE_COUNT; ++stage)
    {
        sb_Fmt(builder, "    case %d: zstdgpu_Srt_InitBindGroups_Stage%d(srts, device, resInfo, b); break;\n", stage, stage);
    }
    sb_StrLitEoL(builder, "    default: ZSTDGPU_ASSERT(0); break;\n"
                    "    }\n"
                    "}\n");
}

static void assignTableOwners(void)
{
    forEachGroup(gi,
    {
        if (kGroupHeap == groupType)
        {
            groupData->tableOwner = gi.id;
            if (groupIsClone(gi) && 0 != groupData->stageMask)
            {
                const GroupId base = groupBase(gi);

                for (uint16_t other = base.id; other < gi.id; ++other)
                {
                    const GroupId sib = { other };

                    if (groupBase(sib).id == base.id && groupOwnsTable(sib))
                    {
                        int same = 1;

                        forEachEntryInGroup(gi, j,
                        {
                            if (entry->resName != groupEntry(sib, j)->resName)
                            {
                                same = 0;
                                break;
                            }
                        });
                        if (same)
                        {
                            groupData->tableOwner = sib.id;
                            groupDataGet(sib)->stageMask |= groupData->stageMask;
                            break;
                        }
                    }
                }
            }
            if (groupOwnsTable(gi))
            {
                if (groupIsClone(gi))
                {
                    groupData->tableName = nameConcatWithUnderscore(group(gi)->name, groupData->cloneSuffix);
                }
                else
                {
                    groupData->tableName = group(gi)->name;
                }
            }
        }
    });
}

typedef struct ResAccess
{
    NameId   res;
    uint16_t access;
} ResAccess;

typedef struct ResAccessRange
{
    uint16_t first;
    uint8_t  count;
} ResAccessRange;

static ResAccess      *gResAccessAll = NULL;
static ResAccessRange *gPassResAccessRange = NULL;

static int accessIsRead(uint16_t access)
{
    return kAccessRO == access || kAccessIndirect == access || kAccessROIndirect == access;
}

static uint16_t accessMerge(uint16_t leftAccess, uint16_t rightAccess)
{
    if (leftAccess == rightAccess)
    {
        return leftAccess;
    }
    if ((kAccessRW == leftAccess || kAccessRNW == leftAccess) && (kAccessRW == rightAccess || kAccessRNW == rightAccess))
    {
        return kAccessRW;   /* one written view makes the whole resource a written UAV */
    }
    if (accessIsRead(leftAccess) && accessIsRead(rightAccess))
    {
        return kAccessROIndirect;
    }
    return kAccessNone;
}

static void resAccessAdd(const Pass *pass, const Srt *srt, int first, NameId res, uint16_t access)
{
    assert(access < (uint16_t)(sizeof(kAccessTokenText) / sizeof(kAccessTokenText[0])) && "resource access has no emitted token");
    for (ptrdiff_t i = first; i < arrlen(gResAccessAll); ++i)
    {
        if (gResAccessAll[i].res == res)
        {
            const uint16_t merged = accessMerge(gResAccessAll[i].access, access);

            if (kAccessNone == merged)
            {
                fail("SRT '%s' pass '%s' needs resource '%s' in incompatible states for one dispatch -- UNORDERED_ACCESS cannot combine with a read state", nameToCStr(srt->key), nameToCStr(pass->name), nameToCStr(res));
                return;
            }
            gResAccessAll[i].access = merged;
            return;
        }
    }

    ResAccess row;

    row.res = res;
    row.access = access;
    arrput(gResAccessAll, row);
}

static void buildPassResAccess(const Pass *pass, int first)
{
    const Srt *srt = &gSrts[pass->srtIdx];

    forEachGroupEntry(passGroupRangePerType(pass, kGroupHeap), gi, k,
    {
        if (kAccessNone != entry->realAccess)
        {
            resAccessAdd(pass, srt, first, entry->resName, entry->realAccess);
        }
    });
    forEachGroupEntry(passGroupRangePerType(pass, kGroupRoot), gid, k,
    {
        if (kAccessNone != entry->realAccess)
        {
            resAccessAdd(pass, srt, first, entry->resName, entry->realAccess);
        }
    });
    if (pass->indirect)
    {
        forEachGroupEntry(srtGroupRangePerType(srt, kGroupIndirect), gid, j,
        {
            resAccessAdd(pass, srt, first, entry->resName, kAccessIndirect);
        });
    }
}

static void buildResAccess(void)
{
    arrsetlen(gPassResAccessRange, 0);
    arrsetlen(gResAccessAll, 0);

    for (size_t i = 0; i < passCount(); ++i)
    {
        const Pass    *pass = &gPasses[i];
        const Srt     *srt = &gSrts[pass->srtIdx];
        const int      first = (int)arrlen(gResAccessAll);

        buildPassResAccess(pass, first);
        const int accessCount = (int)arrlen(gResAccessAll) - first;

        if (first > 0xffff)
        {
            fail("more than %d resource access row(s) in total", 0xffff);
            return;
        }
        if (accessCount > 0xff)
        {
            fail("SRT '%s' pass '%s' accesses %d resource(s), more than a uint8_t count can hold", nameToCStr(srt->key), nameToCStr(pass->name), accessCount);
            return;
        }
        ResAccessRange range;

        range.first = (uint16_t)first;
        range.count = (uint8_t)accessCount;
        arrput(gPassResAccessRange, range);
    }
}

static void emitBarrierTables(const char *dir)
{
    StrBuilder  sb = { NULL };
    StrBuilder  path = { NULL };
    const char *guard = "ZSTDGPU_SRT_GENERATED_BARRIER_TABLES_H";

    sb_Fmt(&path, "%s/zstdgpu_srt_barrier_tables.h", dir);

    sb_BeginFile(&sb, guard);

    sb_StrLitEoL(&sb, "#define ZSTDGPU_SRT_RES_LIST()                            \\");
    for (int i = 0; i < resourceCount(); ++i)
    {
        sb_Fmt(&sb, "    ZSTDGPU_SRT_RES(%-*s)%s\n", 36, resourceIdToCStr(i), (i + 1 < resourceCount()) ? " \\" : "");
    }
    sb_ExtraLine(&sb);

    sb_StrLitEoL(&sb, "typedef enum zstdgpu_Srt_ResId\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_RES(name) kzstdgpu_BarrierTracker_ResId_##name,\n"
                    "    ZSTDGPU_SRT_RES_LIST()\n"
                    "#undef ZSTDGPU_SRT_RES\n"
                    "    kzstdgpu_BarrierTracker_ResId_Count\n"
                    "} zstdgpu_Srt_ResId;\n");

    sb_StrLitEoL(&sb, "static const uint16_t kzstdgpu_BarrierTracker_ResOfs[kzstdgpu_BarrierTracker_ResId_Count] =\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_RES(name) (uint16_t)offsetof(zstdgpu_GpuOnlyBuffers, name),\n"
                    "    ZSTDGPU_SRT_RES_LIST()\n"
                    "#undef ZSTDGPU_SRT_RES\n"
                    "};\n");

    sb_StrLitEoL(&sb, "static const char *const kzstdgpu_BarrierTracker_ResName[kzstdgpu_BarrierTracker_ResId_Count] =\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_RES(name) #name,\n"
                    "    ZSTDGPU_SRT_RES_LIST()\n"
                    "#undef ZSTDGPU_SRT_RES\n"
                    "};\n");

    sb_StrLitEoL(&sb,
        "typedef enum zstdgpu_Srt_Access\n"
        "{\n"
        "    kzstdgpu_Srt_Access_ShaderRead = 0,         /**< bound RO                                     */\n"
        "    kzstdgpu_Srt_Access_ShaderReadWrite = 1,    /**< bound RW or RWGLC -- the only dirtying access */\n"
        "    kzstdgpu_Srt_Access_ShaderReadNoWrite = 2,  /**< bound RNW: a UAV the shader only reads        */\n"
        "    kzstdgpu_Srt_Access_IndirectRead = 3,       /**< not bound -- consumed by ExecuteIndirect      */\n"
        "    kzstdgpu_Srt_Access_ShaderIndirectRead = 4  /**< bound RO and consumed by ExecuteIndirect      */\n"
        "} zstdgpu_Srt_Access;\n"
        "\n"
        "typedef struct zstdgpu_Srt_ResAccess\n"
        "{\n"
        "    uint16_t res;\n"
        "    uint16_t access;\n"
        "} zstdgpu_Srt_ResAccess;\n"
        "\n"
        "/** define the range of resource access in kzstdgpu_Srt_ResAccessAll */\n"
        "typedef struct zstdgpu_Srt_ResAccessRange\n"
        "{\n"
        "    uint16_t first;\n"
        "    uint8_t  count;\n"
        "} zstdgpu_Srt_ResAccessRange;\n");

    sb_StrLitEoL(&sb, "/** Per pass lists of resource accesses */");
    sb_StrLitEoL(&sb, "#define ZSTDGPU_SRT_PASS_LIST()                                                \\");
    for (size_t i = 0; i < passCount(); ++i)
    {
        sb_Fmt(&sb, "    ZSTDGPU_SRT_PASS(%-*s, %4d, %3d)%s\n", 45, nameToCStr(gPasses[i].binderName),
               (int)gPassResAccessRange[i].first, (int)gPassResAccessRange[i].count, (i + 1 < passCount()) ? " \\" : "");
    }
    sb_ExtraLine(&sb);

    sb_StrLitEoL(&sb, "typedef enum zstdgpu_Srt_Pass\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_PASS(name, first, count) kzstdgpu_Srt_Pass_##name,\n"
                    "    ZSTDGPU_SRT_PASS_LIST()\n"
                    "#undef ZSTDGPU_SRT_PASS\n"
                    "    kzstdgpu_Srt_Pass_Count\n"
                    "} zstdgpu_Srt_Pass;\n");

    sb_StrLitEoL(&sb, "/** Report only. */\n"
                    "static const char *const kzstdgpu_Srt_Pass_Name[kzstdgpu_Srt_Pass_Count] =\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_PASS(name, first, count) #name,\n"
                    "    ZSTDGPU_SRT_PASS_LIST()\n"
                    "#undef ZSTDGPU_SRT_PASS\n"
                    "};\n");

    sb_Fmt(&sb, "static const zstdgpu_Srt_ResAccess kzstdgpu_Srt_ResAccessAll[%d] =\n{\n", (int)arrlen(gResAccessAll));
    for (size_t i = 0; i < passCount(); ++i)
    {
        const int firstRow = gPassResAccessRange[i].first;
        const int rowCount = gPassResAccessRange[i].count;

        sb_Fmt(&sb, "    /* [%3d] %s", firstRow, nameToCStr(gPasses[i].binderName));
        sb_Fmt(&sb, " -- %d row(s) */\n", rowCount);

        for (int j = 0; j < rowCount; ++j)
        {
            const ResAccess row = gResAccessAll[firstRow + j];
            const char     *res = nameToCStr(row.res);
            const int       pad = 52 - (int)(sizeof("kzstdgpu_BarrierTracker_ResId_,") - 1 + strlen(res));

            sb_Fmt(&sb, "    { kzstdgpu_BarrierTracker_ResId_%s,%*s %s },\n", res, (pad > 0) ? pad : 0, "", kAccessTokenText[row.access]);
        }
    }
    sb_StrLitEoL(&sb, "};\n");

    sb_StrLitEoL(&sb, "static const zstdgpu_Srt_ResAccessRange kzstdgpu_Srt_Pass_ResAccess[kzstdgpu_Srt_Pass_Count] =\n"
                    "{\n"
                    "#define ZSTDGPU_SRT_PASS(name, first, count) { first, count },\n"
                    "    ZSTDGPU_SRT_PASS_LIST()\n"
                    "#undef ZSTDGPU_SRT_PASS\n"
                    "};\n");

    sb_EndFile(&sb, guard, sb_CStr(&path));

    arrfree(path.data);
}

static void emitBind(const char *dir)
{
    StrBuilder  sb = { NULL };
    StrBuilder  path = { NULL };
    const char *guard = "ZSTDGPU_SRT_GENERATED_BIND_H";

    sb_Fmt(&path, "%s/zstdgpu_srt_bind.h", dir);

    sb_BeginFile(&sb, guard);

    emitBindGroups(&sb);

    for (size_t i = 0; i < passCount(); ++i)
    {
        const Pass *pass = &gPasses[i];
        const Srt  *srt = &gSrts[pass->srtIdx];
        const int   stage = pass->stage;

        sb_Fmt(&sb, "static void zstdgpu_Bind_%s", nameToCStr(pass->binderName));
        /* Barrier lookup needs b even when this binder has no root-resource setters. */
        sb_StrLit(&sb, "(ID3D12GraphicsCommandList *cmdList, zstdgpu_BarrierTracker *tracker, const zstdgpu_Srts &srts, const zstdgpu_GpuOnlyBuffers &b");

        /* Indirect dispatch supplies its indirect constants through the command signature.
         * Direct passes take them as explicit parameters. */
        forEachSrtBoundConst(srt, groupType, gid, idx,
        {
            if (pass->indirect == Direct || kGroupConstIndirect != groupType)
                sb_Fmt(&sb, ", %s %s", nameToCStr(cst->type), nameToCStr(cst->name));
        });
        sb_StrLitEoL(&sb, ")\n{");
        sb_Fmt(&sb, "    zstdgpu_BarrierTracker_Bind(cmdList, tracker, &b, kzstdgpu_Srt_Pass_%s);\n", nameToCStr(pass->binderName));
        sb_Fmt(&sb, "    d3d12aid_ComputeRsPs_Set(&srts.%s, cmdList);\n", nameToCStr(srt->key));

        if (srtGroupRangePerType(srt, kGroupHeap).count > 0)
        {
            sb_StrLitEoL(&sb, "    cmdList->SetDescriptorHeaps(1, &srts.heap);");
            forEachGroupInRange(passGroupRangePerType(pass, kGroupHeap), gid,
            {
                const char *gname = nameToCStr(group(gid)->name);
                const char *tableName = nameToCStr(groupTableName(gid));

                sb_Fmt(&sb, "    cmdList->SetComputeRootDescriptorTable(%d /* %s */, srts.stage%d.%s);\n", srtGroupRootParam(srt, gid), gname, stage, tableName);
            });
        }

        forEachGroupEntry(passGroupRangePerType(pass, kGroupRoot), gid, k,
        {
            const Entry *entryValue = entry;
            const NameId res = entry->resName;
            const int    rootParam = srtGroupRootParam(srt, gid) + (int)k;

            if (kAccessNone == entryValue->realAccess)
            {
                sb_Fmt(&sb, "    /* root param %d (%s) left unset -- this pass does not access %s */\n", rootParam, nameToCStr(entryValue->bindName), nameToCStr(res));
                continue;
            }
            sb_Fmt(&sb, "    cmdList->SetComputeRoot%sView(%d /* %s */,", (kAccessRO == entryValue->bindAccess) ? "ShaderResource" : "UnorderedAccess", rootParam, nameToCStr(entryValue->bindName));
            sb_Fmt(&sb, " b.%s->GetGPUVirtualAddress());\n", nameToCStr(res));
        });

        forEachSrtBoundConst(srt, groupType, owner, inGroup,
        {
            if (pass->indirect == Direct || kGroupConstIndirect != groupType)
            {
                sb_Fmt(&sb, "    cmdList->SetComputeRoot32BitConstant(%d /* %s */, %s, %d /* %s */);\n", srtGroupRootParam(srt, owner), nameToCStr(group(owner)->name), nameToCStr(cst->name), (int)inGroup, nameToCStr(cst->name));
            }
        });

        sb_StrLitEoL(&sb, "}\n");
    }

    sb_EndFile(&sb, guard, sb_CStr(&path));

    arrfree(path.data);
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".generated";

    collect();
    addDefaultPasses();
    deriveGroupUsage();
    registerBoundResources();
    checkSrtStages();
    alignGroupTextLens();
    checkIndirectInputs();
    assignGroupSpaces();
    checkRootBudget();
    assignTableOwners();
    buildResAccess();

    if (g_errorCount > 0)
    {
        fprintf(stderr, "[zstdgpu_srt_tool] [FAIL] %d error(s), no output written\n", g_errorCount);
        return 1;
    }

    forEachGroup(gid,
    {
        if (!groupIsClone(gid) && groupHasHeader(gid))
        {
            emitGroupHeader(dir, gid);
        }
    });
    for (size_t i = 0; i < srtCount(); ++i)
    {
        emitSrtHeader(dir, (int)i);
    }
    emitStructs(dir);
    emitBarrierTables(dir);
    emitBind(dir);

    if (g_errorCount > 0)
    {
        fprintf(stderr, "[zstdgpu_srt_tool] [FAIL] %d error(s) while writing output\n", g_errorCount);
        return 1;
    }

    {
        int heapGroups = 0;

        forEachGroup(gid,
        {
            heapGroups += (kGroupHeap == groupType && !groupIsClone(gid));
        });
        printf("[zstdgpu_srt_tool] [INFO] %d bind group(s), %zu SRT(s), %zu pass(es), %d resource(s) -> %s\n", heapGroups, srtCount(), passCount(), resourceCount(), dir);
        printf("[zstdgpu_srt_tool] [INFO] %d group(s) total, %d entry declaration(s) interned to %d distinct entry(s), %d constant declaration(s) interned to %d distinct constant(s)\n", (int)hmlen(gGroups), gEntryDeclCount, (int)hmlen(gEntries), gConstDeclCount, (int)hmlen(gConsts));
    }
    return 0;
}
