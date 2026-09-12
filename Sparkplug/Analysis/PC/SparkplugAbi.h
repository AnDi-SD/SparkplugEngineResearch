#pragma once

// Byte-exact PC evidence for the Code/Sparkplug layer.  Portable host classes
// deliberately do not claim this 32-bit MSVC layout.

#include "SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    // Common seven-word header consumed directly by spSerializerManager.
    // The following ObjectCount word is read by the unnamed FAT helper.
    struct spSerializerFileHeaderLayout final
    {
        std::uint32_t signature;       // 0x00: "FFPS" / 0x53504646
        std::uint32_t version;         // 0x04: 0x26
        std::uint32_t exportTag;       // 0x08: producer remains unresolved
        std::uint32_t declaredFileSize;// 0x0c
        std::uint32_t platformMask;    // 0x10
        std::uint32_t dataOffset;      // 0x14
        std::uint32_t dataSize;        // 0x18
    };

    struct spSerializerObjectHeaderLayout final
    {
        std::uint32_t classID;         // 0x00
        std::uint32_t marker;          // 0x04: bytes "SBOO", not checked here
    };

    struct spDataBlockHeaderLayout final
    {
        std::uint32_t fieldID;         // 0x00: 0xFFFFFFFF for terminator
        std::uint32_t payloadSize;     // 0x04
        std::uint32_t headerStreamPosition; // 0x08
        std::uint32_t dataStreamPosition;   // 0x0c
    };

    struct spDataBlockSerializerLayout final
    {
        std::uint8_t headerStack[0x0C];// 0x00: old-MSVC list state
        spDataBlockHeaderLayout currentHeader; // 0x0c
        std::uint32_t writerState1C;   // 0x1c: exact role/name unresolved
        Address32 writerStream;        // 0x20
        std::uint32_t reservedSizeCode;// 0x24
    };

    struct spSerializerRegistrationNodeLayout final
    {
        Address32 next;                // 0x00
        Address32 previous;            // 0x04
        std::uint32_t targetClassID;   // 0x08
        std::uint32_t platformMask;    // 0x0c
        std::uint32_t operationMask;   // 0x10: load=1, save=2
        Address32 serializer;          // 0x14: owned spSerializer*
    };

    // Old MSVC std::list representation observed in the PC destructor and
    // registration append path. It differs from the PS2 inline sentinel.
    struct spSerializerRegistrationListLayout final
    {
        std::uint32_t allocatorState;  // 0x00: exact role is ABI-specific
        Address32 sentinel;            // 0x04: allocated list sentinel
        std::uint32_t size;            // 0x08
    };

    struct spSerializerManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t platformMask;    // 0x10: current FFPS platform mask
        std::uint32_t operationMask;   // 0x14: load=1, save=2
        std::uint32_t serializationPolicy; // 0x18: 0/2 enable optional/default fields
        spSerializerRegistrationListLayout registrations; // 0x1c
        Address32 fat;                 // 0x28: owned FAT helper, exact type unresolved
    };

    // Analytical labels, not recovered C++ type/member names. PC allocation58
    // comes from the manager constructor; container offsets are independently
    // observed in native load/index/clear/cursor consumers, not a full ctor run.
    struct spResourceFATHelperObservedLayout final
    {
        spBaseObjectLayout base;
        std::uint32_t nextResourceID;  // 0x10
        std::uint8_t filesByID[0x0C];  // 0x14: MSVC allocator/head/count
        std::uint8_t resourcesByID[0x0C]; // 0x20
        std::uint8_t resourcesByObject[0x0C]; // 0x2c
        std::uint8_t orderedFiles[0x0C]; // 0x38
        std::uint32_t unknown44;       // 0x44: do not infer file cursor by symmetry
        std::uint8_t orderedResources[0x0C]; // 0x48
        Address32 resourceCursor;      // 0x54: clear may leave stale node pointer
    };

    struct spResourceFATEntryObservedLayout final
    {
        Address32 vtableAddress;
        std::uint32_t id;
        std::uint32_t fileID;
        Address32 ownedName;
        std::uint32_t classID;
        std::uint32_t offset;
        std::uint32_t size;
        std::uint8_t payloadWritten;   // 0x1c: NOT initialized by PC LoadIndex
        std::uint8_t padding1D[3];
        Address32 object;              // 0x20: borrowed; entry dtor does not delete
    };

    struct spResourceFATFileEntryObservedLayout final
    {
        Address32 vtableAddress;
        std::uint32_t fileID;
        Address32 ownedFilename;
    };

    struct spSerializerHookObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00: no proven base-local storage
    };

    // The factory allocation is protected. The destructor nevertheless proves
    // a complete old-MSVC list state at +0x10..+0x1B, hence this 0x1C prefix.
    struct spDXSerializerHookObservedPrefixLayout final
    {
        spSerializerHookObservedLayout base; // 0x00
        std::uint32_t listAllocatorState; // 0x10
        Address32 listSentinel;        // 0x14
        std::uint32_t listSize;        // 0x18
    };

    struct spResourceLayout final
    {
        spNamedObjectLayout base;      // 0x00: no resource-local storage
    };

    struct spResourceCacheEntryLayout final
    {
        std::uint32_t kind;            // 0x00: texture=1, mesh=2
        Address32 resource;            // 0x04: non-owning spResource*
    };

    // The protected PC factory hides a direct allocation instruction, but the
    // destructor accounts for all vector words through +0x2f. This is the
    // complete observed MSVC extent, not a claim about original member names.
    struct spResourceManagerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        std::uint8_t unknown14;        // 0x14
        std::uint8_t reserveEnabled;   // 0x15
        std::uint8_t padding16[2];     // 0x16
        std::uint32_t reserveCount;    // 0x18
        std::int32_t field1C;          // 0x1c: constructor default -1
        Address32 vectorAllocatorState;// 0x20: old MSVC vector state
        Address32 entriesBegin;        // 0x24
        Address32 entriesEnd;          // 0x28
        Address32 entriesCapacityEnd; // 0x2c
    };

    struct spTextureBufferLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t width;           // 0x10
        std::uint32_t height;          // 0x14
        std::uint32_t depth;           // 0x18: source name unresolved
        Address32 buffer;              // 0x1c: owned pixel storage
        std::uint32_t pixelFormat;     // 0x20
        Address32 auxiliaryObject;     // 0x24: owned type unresolved
        std::uint32_t pixelSize;       // 0x28: bytes per pixel
        std::uint8_t initialized;      // 0x2c
        std::uint8_t padding2D[3];     // 0x2d
    };

    // Complete observed extent. The PC constructor body is partly protected,
    // while all accesses through +0x34 agree with the exact PS2 0x38 layout.
    struct spTextureObservedLayout final
    {
        spResourceLayout base;         // 0x00: C++ lifetime base
        Address32 iTextureVTable;      // 0x14: spITexture subobject
        std::uint32_t field18;         // 0x18: overload-specific source state
        std::uint8_t field1C;          // 0x1c: one Init argument
        std::uint8_t padding1D[3];     // 0x1d
        std::uint32_t textureFlags;    // 0x20: spITexture::eTextureFlags
        std::uint8_t initialized;      // 0x24
        std::uint8_t padding25[3];     // 0x25
        std::uint32_t width;           // 0x28
        std::uint32_t height;          // 0x2c
        std::uint8_t dimensionsUnchanged; // 0x30
        std::uint8_t field31;          // 0x31
        std::uint8_t padding32[2];     // 0x32
        std::uint32_t field34;         // 0x34
    };

    struct spDXTextureObservedLayout final
    {
        spTextureObservedLayout base; //00..37
        Address32 device;             //38: ctor acquires renderer.C9E8
        Address32 texture;            //3C: retained COM texture
        Address32 palette;            //40: spPalette; setter deletes old, DX dtor leaves alive
        std::uint32_t runtimeFormat;  //44: ctor/nativeData4ABBA0 leave unset
        std::uint32_t byteCount;      //48: ctor0; runtime attach/size query set
    };
    static_assert(sizeof(spDXTextureObservedLayout)==0x4C);
    static_assert(offsetof(spDXTextureObservedLayout,runtimeFormat)==0x44);
    inline constexpr std::uint32_t spDXTextureClassID=0x3F3651B6;
    inline constexpr Address32 spDXTextureRegistration=0x00763210;
    inline constexpr Address32 spDXTextureFactory=0x004AB520;
    inline constexpr Address32 spDXTexturePrimaryVTable=0x006EF6E8;
    inline constexpr Address32 spDXTextureInterfaceVTable=0x006EF6D4;

    struct spPaletteObservedLayout final
    {
        std::uint8_t base[0x10];
        std::uint32_t index;          //10: ctorFFFFFFFF; renderer assigns
        std::uint8_t entries[1024];   //14: ctor-uninitialized; copy ctor copies, virtual clone does not
    };
    static_assert(sizeof(spPaletteObservedLayout)==0x414);
    static_assert(offsetof(spPaletteObservedLayout,entries)==0x14);
    inline constexpr std::uint32_t spPaletteClassID=0x591C0B9F;
    inline constexpr Address32 spPaletteRegistration=0x00763DE0;
    inline constexpr Address32 spPaletteFactory=0x004B2C80;
    inline constexpr Address32 spPaletteConstructor=0x004B2C10;
    inline constexpr Address32 spPaletteCopyConstructor=0x004B2C50;
    inline constexpr Address32 spPaletteClone=0x004B2CF0;
    inline constexpr Address32 spPalettePrimaryVTable=0x006F0A6C;
    inline constexpr Address32 spDXRendererRegisterPalette=0x004BB7D0;
    inline constexpr Address32 spDXRendererUnregisterPalette=0x004BB840;
    inline constexpr Address32 spDXTextureSetPalette=0x004B93C0;

    struct spTextureDataVectorLayout final
    {
        Address32 allocatorState;      // +0x00: MSVC allocator/container word
        Address32 begin;               // +0x04
        Address32 end;                 // +0x08
        Address32 capacityEnd;         // +0x0c
    };

    struct spTextureDataLayout final
    {
        spTextureObservedLayout base;  // 0x00
        spTextureBufferLayout textureBuffer; // 0x38
        std::uint8_t field68;          // 0x68: selects payload cleanup path
        std::uint8_t padding69[3];     // 0x69
        spTextureDataVectorLayout records6C; // 0x6c: 0x10-byte records
        std::uint8_t field7C;          // 0x7c
        std::uint8_t padding7D[3];     // 0x7d
        std::uint8_t platformState80[0x410]; // 0x80: roles unresolved
        spTextureDataVectorLayout records490; // 0x490: 0x14-byte records
    };

    struct spPlatformSpecificMeshDataLayout final
    {
        spNamedObjectLayout base;      // 0x00: no class-local storage
    };

    // The protected PC factory hides the final allocation size. Destructor
    // and serializer paths independently prove this complete prefix.
    struct spDXMeshDataObservedPrefixLayout final
    {
        spPlatformSpecificMeshDataLayout base; // 0x00
        std::uint8_t field14[4];       // 0x14: role unresolved
        Address32 indexBuffer;         // 0x18: owning spIndexBuffer*
        Address32 vertexBuffer;        // 0x1c: owning spVertexBuffer*
    };

    // Both PC virtual methods and the PC serializer access every field through
    // +0xFC. The protected factory still prevents a direct final-size proof,
    // so 0x100 remains a complete observed extent rather than an exact sizeof.
    struct spPS2MeshDataObservedLayout final
    {
        spPlatformSpecificMeshDataLayout base; // 0x00
        std::uint32_t field14;          // 0x14: default 4 on PS2
        std::uint32_t field18;          // 0x18: component-format selector
        std::uint32_t componentFlags;   // 0x1c: effective vertex component mask
        std::uint32_t field20;          // 0x20: packed stream count
        Address32 vertexBuffer;         // 0x24: transient/non-owning input
        Address32 indexBuffer;          // 0x28: transient/non-owning input
        std::uint32_t field2C;          // 0x2c
        std::uint32_t field30;          // 0x30: emitted vertex count
        std::uint32_t field34;          // 0x34: primitive count
        std::uint32_t field38;          // 0x38
        std::uint32_t field3C;          // 0x3c
        Address32 packet;               // 0x40: owned unless fieldFC != 0
        std::uint32_t packetQwordCount; // 0x44
        std::uint32_t field48[22];      // 0x48: per-component descriptor table
        std::int32_t fieldA0[22];       // 0xa0: per-component order table
        std::uint32_t fieldF8;          // 0xf8: constructor default 1
        std::uint8_t fieldFC;           // 0xfc: external/non-owning packet flag
        std::uint8_t paddingFD[3];      // 0xfd
    };

    struct spMeshLayout final
    {
        spResourceLayout base;         // 0x00
        Address32 secondaryVTable;     // 0x14: support/interface subobject
        std::uint8_t field18[0x10];    // 0x18: reset/rebuilt by init
        std::uint8_t boundsValid;      // 0x28
        std::uint8_t padding29[3];     // 0x29
        float boundsMinimum[3];        // 0x2c
        float boundsMaximum[3];        // 0x38
        std::uint32_t vertexComponentFlags; // 0x44
        std::uint32_t primitiveCount;  // 0x48
        std::uint32_t vertexCount;     // 0x4c
    };

    // PC spDXMesh begins its first derived field at +0x50, while the common
    // PS2 constructor independently shows that spRenderMesh only replaces the
    // two spMesh vtables. The identity layer is therefore exactly storage-free.
    struct spRenderMeshLayout final
    {
        spMeshLayout base;             // 0x00
    };

    struct spDXVertexBufferLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 direct3DVertexBuffer;// 0x10: IDirect3DVertexBuffer9*
        std::uint32_t fvfCode;         // 0x14: stored by Initialize
        std::uint32_t field18;         // 0x18: exact role unresolved
        std::uint32_t byteSize;        // 0x1c: stored by Initialize
    };

    struct spDXIndexBufferLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 direct3DIndexBuffer; // 0x10: IDirect3DIndexBuffer9*
        std::uint32_t field14;         // 0x14: exact role unresolved
        std::uint32_t byteSize;        // 0x18: stored by Initialize
    };

    struct spDXMeshCombinerLayout final
    {
        Address32 vTable;              // 0x00: one deleting-destructor entry
        std::uint32_t targetVertexCount; // 0x04
        std::uint32_t writtenVertexCount;// 0x08
        std::uint32_t fvfCode;         // 0x0c
        std::uint32_t vertexByteSize;  // 0x10
        std::uint32_t indexByteSize;   // 0x14
        Address32 vertexBuffer;        // 0x18: intrusive spDXVertexBuffer*
        Address32 indexBuffer;         // 0x1c: intrusive spDXIndexBuffer*
        Address32 vertexWriteCursor;   // 0x20: current locked destination
        Address32 indexWriteCursor;    // 0x24: current locked destination
        std::uint32_t writtenIndexCount;// 0x28
    };

    // The protected PC factory hides the allocation itself, so 0x1c remains
    // a complete observed extent rather than a claimed exact sizeof.
    struct spDXSharedMeshDataObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t field10;         // 0x10: exact role unresolved
        Address32 indexBuffer;         // 0x14: intrusive spDXIndexBuffer*
        Address32 vertexBuffer;        // 0x18: intrusive spDXVertexBuffer*
    };

    // Destructor and shared-data serializer prove every word through +0x38.
    // The factory/constructor are protected, so 0x3c is an observed extent.
    struct spDXCombinedVBObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t listAllocatorState; // 0x10: old-MSVC list state
        Address32 listSentinel;        // 0x14: nodes own payload at node +0x08
        std::uint32_t listSize;        // 0x18
        std::uint32_t mapAllocatorState; // 0x1c: old-MSVC tree/map state
        Address32 mapSentinel;         // 0x20
        std::uint32_t mapSize;         // 0x24
        std::uint32_t field28;         // 0x28: exact role unresolved
        Address32 vertexData;          // 0x2c: owned raw bytes
        Address32 indexData;           // 0x30: owned raw bytes
        std::uint32_t vertexByteSize;  // 0x34
        std::uint32_t indexByteSize;   // 0x38
    };

    struct spDXMeshObservedLayout final
    {
        spRenderMeshLayout base;       // 0x00
        std::uint32_t indexType;       // 0x50: spIndexBuffer::eIndexBufferType
        Address32 indexBuffer;         // 0x54: intrusive spDXIndexBuffer*
        Address32 vertexBuffer;        // 0x58: intrusive spDXVertexBuffer*
        Address32 sharedMeshData;      // 0x5c: intrusive spDXSharedMeshData*
        std::uint32_t indexByteSize;   // 0x60
        std::uint32_t vertexByteSize;  // 0x64
        Address32 cpuIndexData;        // 0x68: optional owned staging bytes
        Address32 cpuVertexData;       // 0x6c: optional owned staging bytes
        std::uint32_t fvfCode;         // 0x70
        std::uint32_t vertexStride;    // 0x74
        std::uint32_t indexBegin;      // 0x78
        std::uint32_t vertexBegin;     // 0x7c
        std::uint32_t componentWeightCount; // 0x80: bits02/04/08/10, NOT UV count; analytical name
        Address32 vertexDeclaration; // 0x84: borrowed spPCVertexDeclaration*, NOT numeric FVF
    };

    // The secondary serialization interface begins at +0x10. The protected
    // factory hides the allocation instruction, so this remains a complete
    // observed prefix rather than a direct sizeof claim.
    struct spDXMeshSerializerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00: spSerializer is storage-free
        Address32 serializerInterfaceVTable; // 0x10
    };

    struct spMeshDataLayout final
    {
        spMeshLayout base;             // 0x00
        Address32 indexBuffer;         // 0x50: owning spIndexBuffer*
        Address32 vertexBuffer;        // 0x54: owning spVertexBuffer*
    };

    struct spVertexBufferLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint16_t vertexStride;    // 0x10: byte size of one vertex
        std::uint8_t padding12[2];     // 0x12
        std::uint32_t componentFlags;  // 0x14: exact m_uComponentFlags name
        std::uint16_t componentCount;  // 0x18: count of 32-bit components
        std::uint8_t padding1A[2];     // 0x1a
        std::uint32_t vertexCount;     // 0x1c: exact m_uVertexCount name
        std::uint32_t vertexSize;      // 0x20: full data byte count
        std::uint16_t componentOffsets[22]; // 0x24: offsets in 32-bit units
        std::uint32_t flags;           // 0x50: exact m_uFlags name
        Address32 vertexData;          // 0x54: owned data
        std::uint8_t initialized;      // 0x58
        std::uint8_t padding59[3];     // 0x59
    };

    struct spRenderableCallbackRecordLayout final
    {
        Address32 callback; // cdecl(model,camera,support,ordinal,user), returns signed32
        Address32 user;
    };

    struct spRenderableCallbackVectorLayout final
    {
        Address32 allocatorState;      // +0x00: compiler-specific vector head
        Address32 begin;               // +0x04
        Address32 end;                 // +0x08
        Address32 capacityEnd;         // +0x0c
    };

    struct spRenderableLayout final
    {
        spNamedObjectLayout base;      // 0x00
        std::uint32_t runtimeMode;      // 0x14
        std::uint8_t alphaSortEnabled;  // 0x18
        std::uint8_t padding19[3];      // 0x19
        std::uint32_t priority;         // 0x1c
        Address32 material;             // 0x20
        Address32 fog;                  // 0x24
        std::uint32_t field28;           // 0x28: raw value from DebugManager41D4E0, not proven float
        Address32 callback2C;           // 0x2c
        Address32 callback30;           // 0x30
        spRenderableCallbackVectorLayout callbacks34; // 0x34
        spRenderableCallbackVectorLayout callbacks44; // 0x44
        std::uint8_t usePreCallbacks44; // 0x54:423E30 traverses vector44
        std::uint8_t usePostCallbacks34;// 0x55:423EA0 traverses vector34
        std::uint8_t padding56[2];      // 0x56
    };

    struct spModelLayout final
    {
        spRenderableLayout base;        // 0x00
        Address32 baseMeshData;         // 0x58
        std::uint32_t projectionGroup;  // 0x5c
    };

    // The PC factory is protected, so 0x70 is an observed complete field
    // extent rather than a direct sizeof claim. Copy, render, serializer and
    // destructor bodies account for all four members after spModel.
    struct spSkinObservedLayout final
    {
        spModelLayout base;             // 0x00
        std::uint32_t weightCount;      // 0x60: GetWeightCount diagnostic
        std::uint32_t boneCount;        // 0x64
        Address32 bones;                // 0x68: spNode*[boneCount]
        Address32 inverseBindMatrices;  // 0x6c: float[boneCount][16]
    };

    // The protected factory prevents an exact sizeof claim. These fields are
    // the continuous prefix directly touched by the PC reader, destructor,
    // Factory/constructor/track lifetimes additionally verified in bounded
    // original guest instructions. Engine RTTI is separate from C++ ancestry.
    struct AnimationDescriptorPoolLayout final
    {
        Address32 vtable;               // +00: one destructor slot
        Address32 activeBlock;          // +04: circular block list
        std::uint32_t entriesPerBlock;  // +08: 64
        std::uint32_t field0C;          // +0c: zero on construction
        std::uint32_t initialBlocks;    // +10: zero
        std::uint32_t blockLimit;       // +14: 0xffffffff
        std::uint32_t entrySize;        // +18: 16
        std::uint32_t blockCount;       // +1c
        std::uint32_t freeEntryCount;   // +20: active blocks only
        Address32 spareBlock;           // +24: retired last-free block
        std::uint32_t field28;          // +28: trunc(64*0.333...) = 21
    };
    struct spTrackLayout final
    {
        spNamedObjectLayout base;       // exact 0x14; no extra fields
    };
    struct spAnimTrackLayout final
    {
        spTrackLayout base;
        std::int32_t bindingSlot;       // +14: -1
        Address32 descriptors[3][3];    // +18: PRS, independent scalar axes
        std::uint8_t ownsKeyBuffers;    // +3c: one flag for the entire track
        std::uint8_t padding3D[3];
        Address32 animationOwner;       // +40: append initializes; factory does not
    };
    struct AnimationTagObservedLayout final
    {
        spBaseObjectLayout base;        // distinct vtable, inherited root RTTI
        Address32 ownedName;            // +10: ordinary char buffer, not shared entry
        float time;                    // +14
        std::uint32_t wireOrdinal;      // +18: reader order, retained after time sort
    };
    struct spAnimationLayout final
    {
        spNamedObjectLayout base;       // physical PC base; engine RTTI says controller
        float totalTime;                // 0x14: serializer field 0
        std::uint32_t priorityGroup;    // 0x18: actor priority high byte; analytical name
        Address32 tracks;               // 0x1c: 0x44-byte entries
        std::uint32_t trackCount;       // 0x20
        std::uint32_t trackCapacity;    // 0x24
        std::uint32_t tagVectorState;   // 0x28: compiler/container state
        Address32 tagsBegin;            // 0x2c: owned tag pointers
        Address32 tagsEnd;              // 0x30
        Address32 tagsCapacityEnd;      // 0x34
        Address32 auxiliaryBuffers[6];  // 0x38..0x4c: scalar/vector/quaternion linear/cubic
        Address32 times;                // 0x50: shared float array
        std::uint32_t debugCycleValue;  // 0x54: spDebugManager table, not a fixed default
        AnimationDescriptorPoolLayout descriptorPool; // 0x58..0x83
    };

    // Analytical field names, pinned by 0x005FE9C0/0x005FEBB0 and spActor
    // 0x005A1D26. Key-cache arrays are passed by address to 0x00479290.
    struct spTransformTrackEvalInputLayout final
    {
        Address32 playbackState;           // +0x00: actor 0x60-byte entry
        Address32 animationTrack;          // +0x04: animation 0x44-byte entry
        std::uint32_t priority;            // +0x08: unsigned ordering, default -1
        std::int32_t positionKeyIndices[3];// +0x0c
        std::int32_t rotationKeyIndices[3];// +0x18
        std::int32_t scaleKeyIndices[3];   // +0x24
    };

    // Exact concrete extent 0x78; caller capacity invariant remains open.
    struct spTransformTrackEvalObservedLayout final
    {
        spBaseObjectLayout base;            // 0x00: observed physical prefix
        std::uint32_t boundTransformSlot;   // 0x10: -1 before name binding
        std::uint32_t blendInputCount;      // 0x14: evaluator loop bound
        spTransformTrackEvalInputLayout blendInputs[2]; // 0x18
    };

    struct spNodeControllerLayout final
    {
        spBaseObjectLayout base; // physical prefix; registered spSubController
        Address32 node;          // +0x10: intrusive owned spNode
        Address32 evaluator;     // +0x14: directly deleted spTransformEval
    };

    struct spAnimationManagerLayout final
    {
        spBaseObjectLayout base;
        std::uint32_t frame;           // +10: constructor starts at one
        std::uint32_t nextNameSlot;    // +14: starts at one, never reuses erased IDs
        std::uint32_t nameMapState;    // +18: MSVC allocator state, unwritten
        Address32 nameMapSentinel;     // +1c: 0x34-byte red-black tree nodes
        std::uint32_t nameCount;       // +20
        Address32 controllerHead;      // +24: borrowed intrusive list
        Address32 controllerTail;      // +28
    };
    struct AnimationNameMapNodeLayout final
    {
        Address32 left, parent, right; // +00..08
        std::uint32_t stringState;     // +0c: unwritten allocator byte/padding
        std::uint8_t stringStorage[16];// +10: inline chars or heap pointer
        std::uint32_t stringSize;      // +20
        std::uint32_t stringCapacity;  // +24: inline capacity 15
        std::uint32_t slot;            // +28
        std::uint32_t references;      // +2c
        std::uint8_t color, isSentinel;// +30, +31
        std::uint8_t padding32[2];
    };
    struct spControllerLayout final
    {
        spBaseObjectLayout base;
        std::uint8_t enabled;          // +10: defaults true
        std::uint8_t padding11[3];
        Address32 next;                // +14
        Address32 previous;            // +18
    };

    // Full allocation size is pinned, but several field roles are still opaque.
    struct spActorObservedLayout final
    {
        spBaseObjectLayout base;
        std::uint8_t controllerFields[0x0c]; // +0x10
        std::uint8_t appliesNodeTransforms; // +0x1c
        std::uint8_t padding1D[3];
        float timeMultiplier;              // +0x20
        std::uint8_t advancesWhileDisabled; // +0x24: exact original name unknown
        std::uint8_t padding25[3];
        Address32 playbackStates;          // +0x28: 0x60 stride
        std::uint32_t playbackCapacity;    // +0x2c
        std::uint8_t slotMap[0x0c];         // +0x30
        std::uint32_t vectorState;         // +0x3c
        Address32 controllersBegin;        // +0x40
        Address32 controllersEnd;          // +0x44
        Address32 controllersCapacity;     // +0x48
        std::uint8_t unknown4C[8];         // +0x4c, allocation ends at 0x54
    };

    // Start helper 005A1E30 mutates request.weight; request != playback state.
    // Original API/field names unknown. Exact observed extent 0x38.
    struct spActorStartRequestObservedLayout final
    {
        Address32 animation;           // +00
        std::uint32_t playbackMode;    // +04
        std::uint8_t reverse;          // +08
        std::uint8_t padding09[3];
        float weight;                 // +0c: overwritten according to fadeMode
        std::uint32_t fadeMode;        // +10
        float fadeInDuration;          // +14: positive => inverse, else fallback
        float fallbackFadeInRate;      // +18
        float fadeOutDuration;         // +1c
        float fallbackFadeOutRate;     // +20
        float transitionDuration;     // +24
        Address32 loopCallback;        // +28
        Address32 callbackCookie;      // +2c
        float timeMultiplier;          // +30
        float initialTime;             // +34: divided by animation total time
    };

    struct spActorPlaybackObservedLayout final
    {
        Address32 animation;               // +0x00
        std::uint32_t playbackMode;         // +0x04: 0..3, original enum unknown
        std::uint8_t reverse;               // +0x08
        std::uint8_t padding09[3];
        float weight;                      // +0x0c
        std::uint32_t fadeMode;             // +0x10
        std::uint32_t unknown14;            // +0x14: Start does NOT copy request duration
        float fadeInRate;                   // +0x18
        std::uint32_t unknown1C;            // +0x1c: original role still open
        float fadeOutRate;                  // +0x20
        float transitionDuration;          // +0x24
        Address32 loopCallback;             // +0x28
        std::uint32_t callbackCookie;       // +0x2c: opaque callback argument
        float timeMultiplier;              // +0x30
        float sampleTime;                  // +0x34
        std::uint32_t unknown38;            // +0x38
        std::uint8_t stopAfterFade;         // +0x3c
        std::uint8_t padding3D[3];
        std::uint32_t playbackStatus;       // +0x40
        std::uint32_t slotIndex;            // +0x44
        std::uint32_t bindingUseCount;      // +0x48: evaluator insert/remove consumer
        std::uint8_t running;               // +0x4c
        std::uint8_t padding4D[3];
        std::uint32_t priority;             // +0x50
        float normalizedProgress;          // +0x54
        float fadeThreshold;               // +0x58
        float elapsedTime;                 // +0x5c
    };

    struct spIndexBufferLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint8_t initialized;      // 0x10
        std::uint8_t padding11[3];     // 0x11
        std::uint32_t type;            // 0x14: eIndexBufferType, default 2
        std::uint32_t primitiveCount;  // 0x18
        std::uint32_t indexCount;      // 0x1c
        std::uint32_t formatFlags;     // 0x20: bit 0 selects 32-bit indices
        Address32 indexData;           // 0x24: owned 16/32-bit array
    };

    struct spGameLevelSerializerLayout final
    {
        spBaseObjectLayout base;       // 0x000
        Address32 targetLevel;         // 0x010: spGameLevel*, non-owning
        Address32 inputStream;         // 0x014: spStream*, non-owning
        Address32 parsedDocument;      // 0x018: PC cleanup owns parser tree
        char instanceName[0x40];       // 0x01c
        char assetPath[0x100];         // 0x05c
        float position[3];             // 0x15c
        float rotation[4];             // 0x168: quaternion components
        float scale[3];                // 0x178
    };

    struct spGameLevelLayout final
    {
        spNamedObjectLayout base;      // 0x00
        Address32 listAllocatorState;  // 0x14
        Address32 listHead;            // 0x18: owned instance list sentinel
        std::uint32_t instanceCount;   // 0x1c
        Address32 field20;             // 0x20: initialized zero
        Address32 field24;             // 0x24: initialized zero
        Address32 field28;             // 0x28: initialized zero
    };

    struct spTemplateSerializerLayout final
    {
        spBaseObjectLayout base;       // 0x000
        Address32 targetObject;        // 0x010: spTemplateObject*, non-owning
        Address32 inputStream;         // 0x014: spStream*, non-owning
        Address32 parsedDocument;      // 0x018: parser-owned structure
        std::uint32_t objectType;      // 0x01c: XML attribute "Type"
        char assetPath[0x100];         // 0x020: XML attribute "AssetPath"
        std::uint32_t objectID;        // 0x120: XML attribute "ID"
        std::int32_t parentID;         // 0x124: "ParentID", default -1
        char parentName[0x40];         // 0x128: XML attribute "ParentName"
        char objectName[0x40];         // 0x168: XML attribute "Name"
        float position[3];             // 0x1a8: XML element "Position"
        float rotation[9];             // 0x1b4: matrix from "Rotation"
        float scale[3];                // 0x1d8: XML element "Scale"
        Address32 outputObject;        // 0x1e4: owned polymorphic object
        std::uint8_t outputReady;      // 0x1e8
        std::uint8_t padding1E9[3];    // 0x1e9
        std::uint32_t field1EC;        // 0x1ec
        std::uint8_t failed;           // 0x1f0
        std::uint8_t padding1F1[3];    // 0x1f1
    };

    struct spTemplateObjectLayout final
    {
        spNamedObjectLayout base;      // 0x000
        Address32 field14;             // 0x014: embedded pair, initialized zero
        Address32 field18;             // 0x018
        Address32 loadedObject;        // 0x01c: cleanup depends on state
        std::uint32_t state;           // 0x020: values 0..3
        Address32 streamOrResource;    // 0x024: released before state dispatch
        Address32 field28;             // 0x028: initialized zero
        char resourcePath[0x100];      // 0x02c; serializer-populated
        std::uint32_t field12C;        // 0x12c: initialized zero
        std::int32_t field130;         // 0x130: initialized to -1
        std::uint8_t field134[0x40];   // 0x134: initialized zero
        float constructedDefaults[15];// 0x174: constructor copies platform globals
    };

    struct spTemplateInstanceLayout final
    {
        spNamedObjectLayout base;      // 0x00
        Address32 field14;             // 0x14: initialized to zero
        Address32 listAllocatorState;  // 0x18
        Address32 listHead;            // 0x1c
        std::uint32_t attachedCount;   // 0x20
        Address32 instanceRoot;        // 0x24: ref-counted spNode
    };

    // PC factory is .rld-protected; all observed accesses cover this prefix.
    struct spTemplateManagerObservedPrefixLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        Address32 listAllocatorState;  // 0x14
        Address32 listHead;            // 0x18
        std::uint32_t templateCount;   // 0x1c
        Address32 field20;             // 0x20: role unresolved
    };

    // The protected factory is behind .rld. Methods account for every byte
    // through +0x1f, proving this prefix but not the final allocation size.
    struct spEntityManagerObservedPrefixLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        Address32 listAllocatorState;  // 0x14: empty-base/STL state
        Address32 listHead;            // 0x18: heap sentinel node
        std::uint32_t entityCount;     // 0x1c
    };

    struct spDebugManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10: singleton-support subobject
        std::uint8_t platformState14[0x24]; // 0x14: exact extent, roles unresolved
    };

    struct spFontManagerLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 supportVTable;       // 0x14: singleton-support subobject
        std::uint8_t managerState18[0x1C]; // 0x18: container/backend state
        Address32 primaryMaterial;         // 0x34: intrusive reference
        Address32 fallbackMaterial;        // 0x38: intrusive reference
    };

    struct spPCFontManagerLayout final
    {
        spFontManagerLayout base;      // 0x00; exact 0x3c allocation
    };

    struct spInputManagerObservedPrefixLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 supportVTable;       // 0x14: singleton-support subobject
        Address32 inputInterfaceVTable;// 0x18: platform device interface
        std::uint8_t field1C[4];       // 0x1c: role not closed on PC
    };

    // The protected factory is behind an .rld trampoline.  Destructor and
    // initialization access every byte through +0x4f, making 0x50 an exact
    // observed extent but not yet a direct sizeof claim.
    struct spDXInputManagerObservedLayout final
    {
        spInputManagerObservedPrefixLayout base; // 0x00
        Address32 windowHandle;         // 0x20: passed to DirectInput devices
        std::uint8_t initialized;       // 0x24: set by device-interface init
        std::uint8_t padding25[3];      // 0x25
        Address32 directInput;          // 0x28: released COM interface
        Address32 keyboardDevice;       // 0x2c
        Address32 mouseDevice;          // 0x30
        std::uint8_t deviceOptions34[0x08]; // 0x34
        std::uint8_t controllerCount;   // 0x3c
        std::uint8_t padding3D[3];      // 0x3d
        Address32 controllers[4];       // 0x40
    };

    // Exact base extent inferred from adjacent lifetime boundaries: the
    // destructor tears down the final 0x40-byte member at +0xc9c8, while the
    // derived spDXRenderer lifetime starts after +0xca08. Semantic names stop
    // where executable consumers do not yet distinguish the large state
    // blocks. The renderer-interface vptr at +0x18 has exactly 29 slots.
    struct spRendererAlphaRecordLayout final
    {
        Address32 camera;
        Address32 support;
        Address32 renderable;
        float distanceSquared;
        std::uint32_t priority; // renderer48 + renderable1c, wrapping unsigned
        std::uint8_t exactParticleSystem;
        std::uint8_t padding15[3];
    };

    struct spRendererGeneralRecordLayout final
    {
        Address32 renderable;
        Address32 support;
        Address32 camera;
        std::uint32_t materialKey; // material4c->18->10 virtual1c; original type unknown
        Address32 mesh; // Model or derived, else null
    };

    struct spRendererBucketRecordLayout final
    {
        Address32 renderable;
        Address32 support;
    };

    struct spRendererQueueVectorLayout final
    {
        std::uint32_t allocatorWord;
        Address32 begin;
        Address32 end;
        Address32 capacityEnd;
    };

    struct spRendererLayout final
    {
        spCrossPlatformLayout base;     // 0x0000
        Address32 supportVTable;        // 0x0014: one-slot adjustor subobject
        Address32 rendererInterfaceVTable; // 0x0018: 29 platform operations
        std::uint8_t state1C[0x28];    // 0x001c
        std::uint8_t alphaFlushActive; // 0x0044, prevents re-enqueue
        std::uint8_t alphaQueueEnabled;// 0x0045
        std::uint8_t padding46[2];
        std::uint32_t alphaPriority;   // 0x0048
        std::uint32_t alphaCount;      // 0x004c, capacity2048
        spRendererAlphaRecordLayout alphaRecords[2048]; // 0x0050..c04f
        std::uint8_t renderQueueEnabled;// 0xc050: gates submission
        std::uint8_t paddingC051[3];
        spRendererQueueVectorLayout generalQueue; // 0xc054, record20
        spRendererQueueVectorLayout modeQueues[9];// 0xc064, record8
        std::uint8_t stateC0F4[0x94];   // 0xc0f4
        std::uint8_t materialOverride; // 0xc188
        std::uint8_t paddingC189[3];
        Address32 currentMaterial;     // 0xc18c
        Address32 currentLightCache;   // 0xc190
        std::uint32_t currentField28;  // 0xc194, raw Renderable28
        std::uint8_t stateC198[0x2c];
        std::uint8_t materialStateC1C4;// saved in global7400fc, original name unknown
        std::uint8_t stateC1C5[0x6a3];
        std::uint32_t renderStateCache[12]; // 0xc868: invalidated to ~0u
        std::uint32_t textureStateCache[72];// 0xc898: invalidated to ~0u
        std::uint8_t lifetimeTailC9B8[0x50]; // 0xc9b8
    };

    struct spDXRendererLayout final
    {
        spRendererLayout base;          // 0x0000
        std::uint8_t dxStateCA08[0x2960]; // 0xca08
    };

    // spPCRenderer only changes vtables and factory/clone identity. Its
    // 0x004c5ab0 factory allocates the same 0xf368 bytes as the DX base.
    struct spPCRendererLayout final
    {
        spDXRendererLayout base;
    };

    // Read-only overlays for the already proven CP33/43/50/64 draw boundary.
    // These name consumed fields without changing the complete renderer layout
    // above or claiming that padding is reconstructed renderer behavior.
    inline constexpr Address32 spRendererSingletonAddress = 0x0075DB68;
    inline constexpr std::uint32_t spRendererDrawContextOffset = 0xC178;
    struct spRendererDrawContextObservedLayout final
    {
        float ambientRGBA[4];                 // C178: SubmitLights output
        std::uint8_t materialOverride;        // C188
        std::uint8_t paddingC189[3];
        Address32 selectedMaterial;           // C18C: borrowed
        Address32 selectedLightCache;         // C190: borrowed, not owner identity
        std::uint32_t renderableColorARGB;    // C194
        std::uint8_t opaqueC198[0x2C];
        std::uint8_t materialState;            // C1C4: override-table gate
        std::uint8_t opaqueC1C5[0x6A3];
        std::uint32_t renderStates[12];        // C868: effective raw engine states
        std::uint32_t textureStates[72];       // C898: eight groups of nine
        Address32 blendPalette;               // C9B8: borrowed matrix storage
        std::uint32_t activeBoneCount;         // C9BC: scoped by Skin::Render
        Address32 defaultMaterial;            // C9C0
        std::uint8_t opaqueC9C4[0x24];
        Address32 device;                     // C9E8: IDirect3DDevice9*
        std::uint8_t opaqueC9EC[0x10];
        Address32 vertexDeclaration;          // C9FC: native declaration object
        std::uint32_t opaqueCA00;
        Address32 indexBuffer;                // CA04: native DX buffer object
        Address32 vertexBuffer;               // CA08: native DX buffer object
    };
    static_assert(sizeof(spRendererDrawContextObservedLayout) == 0x894);
    static_assert(offsetof(spRendererDrawContextObservedLayout, selectedMaterial) +
        spRendererDrawContextOffset == offsetof(spRendererLayout, currentMaterial));
    static_assert(offsetof(spRendererDrawContextObservedLayout, renderStates) +
        spRendererDrawContextOffset == offsetof(spRendererLayout, renderStateCache));
    static_assert(offsetof(spRendererDrawContextObservedLayout, blendPalette) +
        spRendererDrawContextOffset == 0xC9B8);
    static_assert(offsetof(spRendererDrawContextObservedLayout, device) +
        spRendererDrawContextOffset == 0xC9E8);
    static_assert(offsetof(spRendererDrawContextObservedLayout, vertexBuffer) +
        spRendererDrawContextOffset == 0xCA08);

    inline constexpr std::uint32_t spDXRendererMaterialCacheOffset = 0xE47C;
    struct spDXRendererMaterialCacheObservedLayout final
    {
        Address32 installedMaterial;          // E47C, distinct from selected C18C
        std::uint8_t opaqueE480[0x24];
        // 4BE180 copies pre-controller material+78; 4BDB10 then transforms
        // this copy for the effective mode. It is NOT current source colors.
        float diffuseRGBA[4];                 // E4A4
        float ambientRGBA[4];                 // E4B4
        float specularRGBA[4];                // E4C4
        float emissiveRGBA[4];                // E4D4
        std::uint32_t specularPowerBits;       // E4E4
        std::uint32_t diffuseSource;           // E4E8: raw engine 10/11/12
        std::uint32_t ambientSource;           // E4EC
    };
    static_assert(sizeof(spDXRendererMaterialCacheObservedLayout) == 0x74);
    static_assert(offsetof(spDXRendererMaterialCacheObservedLayout, diffuseRGBA) +
        spDXRendererMaterialCacheOffset == 0xE4A4);
    static_assert(offsetof(spDXRendererMaterialCacheObservedLayout, ambientSource) +
        spDXRendererMaterialCacheOffset == 0xE4EC);

    // Every field in these target prefixes is referenced by native methods.
    // Protected PC factories prevent a direct sizeof claim for the DX common
    // leaves; names therefore retain the ObservedPrefix qualification.
    struct spRenderTargetObservedPrefixLayout final
    {
        spResourceLayout base;          // 0x00
        Address32 targetInterfaceVTable;// 0x14
        Address32 backingTexture;       // 0x18: intrusive relationship
        std::uint32_t width;            // 0x1c
        std::uint32_t height;           // 0x20
        std::uint32_t pixelFormat;      // 0x24: eTBPixelFormat
    };

    struct spCubeRenderTargetObservedPrefixLayout final
    {
        spRenderTargetObservedPrefixLayout base; // 0x00
        Address32 cubeTexture;          // 0x28: intrusive relationship
    };

    struct spDXRenderTargetObservedPrefixLayout final
    {
        spRenderTargetObservedPrefixLayout base; // 0x00
        Address32 renderSurface;        // 0x28: IDirect3DSurface8*
    };

    struct spPCRenderTargetObservedPrefixLayout final
    {
        spDXRenderTargetObservedPrefixLayout base;
    };

    struct spDXCubeRenderTargetObservedPrefixLayout final
    {
        spCubeRenderTargetObservedPrefixLayout base; // 0x00
        Address32 faceSurfaces[6];       // 0x2c: D3D cube faces
    };

    struct spRenderTargetListStateLayout final
    {
        Address32 allocatorState;
        Address32 head;
        std::uint32_t count;
    };

    struct spRenderTargetManagerObservedLayout final
    {
        spCrossPlatformLayout base;      // 0x00
        Address32 singletonSupportVTable;// 0x14
        spRenderTargetListStateLayout ordinaryTargets; // 0x18
        spRenderTargetListStateLayout cubeTargets;     // 0x24
        spRenderTargetListStateLayout layerTargets;    // 0x30
        std::uint32_t field3C;           // 0x3c: initialized zero
        std::uint32_t field40;           // 0x40: initialized one
    };

    struct spPCRenderTargetManagerObservedLayout final
    {
        spRenderTargetManagerObservedLayout base;
    };

    struct spMaterialTextureObservedLayout final
    {
        spBaseObjectLayout base;        // 0x00: direct registered/C++ base
        std::uint32_t textureStates[9]; // 0x10: PC etsMaxTextureStates
        Address32 fallbackTexture;      // 0x34: intrusive spTexture*
        Address32 uvController;        // 0x38: field12/1C0053D6 -> 467D90
        float uvTransform[9];           // 0x3c: 3x3 static transform
        std::uint8_t hasStaticUV;       // 0x60
        std::uint8_t padding61[3];      // 0x61
        Address32 animationController; // 0x64: field11/16FB0E47 -> 476680
    };

    struct spMaterialRenderTargetTextureObservedLayout final
    {
        spMaterialTextureObservedLayout base; // 0x00
        std::uint32_t opaque68;        // derived-only; actual common factory ends68
        std::uint32_t maxRecursionLevel;// 0x6c: default 1
        std::uint32_t currentRecursionLevel; // 0x70: default 0
        std::uint32_t width;            // 0x74: default 256
        std::uint32_t height;           // 0x78: default 256
        std::uint32_t pixelFormat;      // 0x7c: eTBPixelFormat, default 0
        Address32 targetsBegin;         // 0x80: old-MSVC vector state
        Address32 targetsEnd;           // 0x84
        Address32 targetsCapacityEnd;   // 0x88
    };

    struct spMaterialCameraViewTextureObservedLayout final
    {
        spMaterialRenderTargetTextureObservedLayout base; // 0x00
        Address32 cameraName;           // 0x8c: owned/shared string entry
        Address32 resolvedCamera;       // 0x90: runtime camera
    };

    struct spMaterialCubeMapTextureLayout final
    {
        spMaterialRenderTargetTextureObservedLayout base; // 0x00
        Address32 sourceRenderNode;     // 0x8c
        Address32 ownedCamera;          // 0x90
        std::uint32_t facesPerTick;     // 0x94: default 6, clamped 1..6
        std::uint32_t currentFace;      // 0x98: default 0
    };

    struct spTaskTimerLayout final
    {
        spBaseObjectLayout base;          // 0x00
        Address32 previousSibling;        // 0x10: core41C300 append writes old tail
        Address32 nextSibling;            // 0x14: read after virtual child update
        std::uint8_t active;              // 0x18
        std::uint8_t relative;            // 0x19: default1
        std::uint8_t padding1A[2];        // 0x1a: not initialized
        std::uint32_t currentMilliseconds;// 0x1c
        std::uint32_t startMilliseconds;  // 0x20
        std::uint32_t pausedMilliseconds; // 0x24
        float deltaSeconds;              // 0x28
        Address32 sourceClock;           // 0x2c: borrowed; optional ctor argument
        Address32 firstChild;            // 0x30: borrowed child list
        Address32 lastChild;             // 0x34: core41C300 append updates tail
        std::uint32_t childCount;        // 0x38: incremented on append
    };

    struct spEngineCoreLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        std::uint8_t initialized;      // 0x14
        std::uint8_t padding15[3];     // 0x15
        Address32 defaultScene;        // 0x18: owned polymorphic object
        Address32 defaultCamera;       // 0x1c: intrusive reference
        std::uint8_t container20[0x10];// 0x20: compiler-specific container
        Address32 firstFrameCallback;  // 0x30
        Address32 secondFrameCallback; // 0x34
        Address32 field38;             // 0x38: passed by value to renderer slot5
        Address32 field3C;             // 0x3c: spAnimationManager* (41B3B0/41CD50)
        Address32 field40;             // 0x40: spDXAudioManager* (registered factory4C4200)
        Address32 field44;             // 0x44: spGUIManager* (registered factory452380)
        Address32 field48;             // 0x48: spCinematicManager* (registered factory4533A0)
        Address32 field4C;             // 0x4c: spNetworkManager* (registered factory4512D0)
        Address32 field50;             // 0x50: manager pointer
        spTaskTimerLayout taskTimers54[2]; // 0x54,0x90: exact destructor/stride/type
        std::uint8_t eventQueuesCC[0x88]; // 0xcc,0x110: two44-byte queue helpers, original type name open
        Address32 field154;            // 0x154: released during shutdown
    };

    // Exact native allocation. The semantic transform/relationship fields are
    // named where runtime consumers and serializers agree. Cached world PRS
    // roles/update are proved by 0x420660/0x420710/0x421420 and spSkin.
    struct spNodeLayout final
    {
        spNamedObjectLayout base;      // 0x00
        Address32 childAllocatorState; // 0x14: compiler-specific list state
        Address32 childListHead;       // 0x18: owned sentinel
        std::uint32_t childCount;      // 0x1c
        float position[3];             // 0x20: local position
        Address32 parent;              // 0x2c: non-owning spNode*
        float scale[3];                // 0x30: local scale
        Address32 sceneLink;           // 0x3c: registered runtime object
        float orientation[9];          // 0x40: local 3x3 orientation
        Address32 collisionAllocatorState; // 0x64
        Address32 collisionBegin;      // 0x68: intrusive spCollisionInfo**
        Address32 collisionEnd;        // 0x6c
        Address32 collisionCapacityEnd;// 0x70
        float cachedWorldPosition[3];  // 0x74: consumed by point transform/skin
        float cachedWorldScale[3];     // 0x80
        float cachedWorldOrientation[9]; // 0x8c
        std::uint32_t flags;           // 0xb0
    };

    struct spSceneManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupport;    // 0x10: vtable6E7150
        Address32 listAllocatorState;  // 0x14: untouched by ctor
        Address32 sceneListHead;       // 0x18: owned12-byte sentinel
        std::uint32_t sceneCount;      // 0x1c: borrowed scene pointers
        Address32 currentScene;        // 0x20: set around root virtual30(0)
    };

    struct spSceneLayout final
    {
        spNamedObjectLayout base;      // 0x00
        Address32 systemRoot;          // 0x14: intrusive spNode, "System Root"
        Address32 firstRenderNode;     // 0x18: borrowed intrusive list, node128/12c
        Address32 lastRenderNode;      // 0x1c
        std::uint32_t renderNodeCount; // 0x20
        std::uint8_t viewportDepthPass;// 0x24: default0; camera chooses depth range
        std::uint8_t beforeManagerPass;// 0x25: default1; core graphics boundary
        std::uint8_t padding26[2];     // 0x26: untouched
        Address32 lensFlareManager;    // 0x28: owned spPCLensFlareManager
        Address32 projectionManager;   // 0x2c: owned spPCProjectionManager
        Address32 skyBoxManager;       // 0x30: owned spSkyBoxManager
        Address32 lightManager;        // 0x34: owned spLightManager; owner20=this
        Address32 partitionSystem;     // 0x38: intrusive reference, getter45D930
        Address32 fallbackPartition;   // 0x3c: intrusive reference, init45D850
        std::uint32_t field40;         // 0x40: default0; role not closed
        Address32 sortedAllocator;     // 0x44: untouched
        Address32 sortedBegin;         // 0x48: pairs<object,float>, sorting45EC70
        Address32 sortedEnd;           // 0x4c
        Address32 sortedCapacity;      // 0x50
    };

    // Exact238 allocation now executed for both PC concrete factories.
    // Retain the old type spelling to avoid unrelated consumer churn.
    struct spCameraObservedLayout final
    {
        spNodeLayout base;             // 0x000
        float verticalHalfExtent;      // 0x0b4: near*tan(viewAngle/2)
        float verticalHalfExtentCopy;  // 0x0b8
        float nearClipPlane;           // 0x0bc
        float farClipPlane;            // 0x0c0
        float aspectHalfExtent;        // 0x0c4
        std::uint8_t twoDimensional;   // 0x0c8: serialized Is2DMode
        std::uint8_t paddingC9[3];     // 0x0c9
        float viewMatrix[16];          // 0x0cc: corrected by ctor/renderer arg
        float projectionMatrix[16];    // 0x10c: corrected by ctor/renderer arg
        float viewBasis[9];            // 0x14c: forward/up/right
        std::uint8_t viewport170[0x18];// 0x170: untouched until configure/render
        float viewAngle;               // 0x188
        float scaledViewAngle;         // 0x18c: viewAngle*viewportRatio
        float pixelAspectRatio;        // 0x190
        float derivedBasis[12];        // 0x194
        float frustumPlanes[24];       // 0x1c4: six four-float planes
        std::uint32_t dirtyFlags;      // 0x224: view=1, projection=2
        std::uint32_t field228;        // 0x228: role unresolved
        std::uint32_t backendMode;     // 0x22c: final configure argument
        std::uint8_t viewportActive;   // 0x230
        std::uint8_t projectionBranch;// 0x231: not serialized Is2DMode
        std::uint8_t padding232[2];    // 0x232
        float viewportRatio;           // 0x234: height/width
    };

    // Both PC vtables inherit all common camera operations unchanged; only
    // deleting destructor, clone factory and RTTI identity differ.
    struct spCameraDataObservedLayout final
    {
        spCameraObservedLayout base;
    };

    struct spDXCameraObservedLayout final
    {
        spCameraObservedLayout base;
    };

    // Kept for existing prefix consumers; the complete1D4 layout follows.
    struct spRenderNodeObservedPrefixLayout final
    {
        spNodeLayout base;             // 0x00
        Address32 supportVTable;       // 0xb4: secondary6DCADC, six slots
        Address32 renderableAllocatorState; // 0xb8
        Address32 renderableBegin;     // 0xbc: intrusive spRenderable**
        Address32 renderableEnd;       // 0xc0
        Address32 renderableCapacityEnd; // 0xc4
    };

    // Analytical layout name: original support/helper class name is unknown.
    // PC490B20/490B50/490BA0, also embedded in render and partition payloads.
    struct spLightCacheObservedLayout final
    {
        Address32 ordinaryLights[8];  // 0x00: borrowed, stale unused slots possible
        Address32 ambientLight;       // 0x20: first ambient wins
        std::uint32_t lightCount;     // 0x24: ordinary only, max8
    };

    // Original protected factory425520 resolves to13C5390 and allocates1D4.
    // The embedded render support has no recovered original class name.
    struct spRenderNodeLayout final
    {
        spRenderNodeObservedPrefixLayout prefix; // 0x000
        float localSphere[4];          // 0x0c8: center3,radius
        float worldSphere[4];          // 0x0d8
        Address32 worldMatrixPointer;  // 0x0e8: points to this138
        Address32 inverseMatrixPointer;// 0x0ec: points to this178
        spLightCacheObservedLayout lightCache; // 0x0f0
        std::uint32_t field118;        // 0x118: compared to scene40 by support slot4
        std::uint32_t field11C;        // 0x11c: ctor0, exact role open
        std::uint8_t field120;         // 0x120: ctor0
        std::uint8_t excludeShadowVolumeLights; // 0x121: ctor1;46A850
        std::uint8_t field122;         // 0x122: ctor1
        std::uint8_t updateLights;     // 0x123: ctor1; gates46AC40
        Address32 self;                // 0x124: complete-object pointer
        Address32 previousInScene;     // 0x128: borrowed intrusive link
        Address32 nextInScene;         // 0x12c
        std::uint8_t bypassFrustumCull;// 0x130: ctor0
        std::uint8_t padding131[3];    // 0x131: untouched
        std::uint32_t matrixDirty;     // 0x134: bit1 lazy world/inverse cache
        float worldMatrix[16];         // 0x138: ctor identity
        float inverseWorldMatrix[16];  // 0x178: ctor identity
        float inverseWorldScale[3];    // 0x1b8: ctor1,1,1
        Address32 callbackAllocator;   // 0x1c4: untouched
        Address32 callbackBegin;       // 0x1c8: borrowed callback pointers
        Address32 callbackEnd;         // 0x1cc
        Address32 callbackCapacity;    // 0x1d0
    };

    // Analytical layout name: original C++ name of this embedded support is
    // still unknown. Same constructor469E00/storage74 at RenderNode+B4,
    // StaticRenderObject+14 and PartitionRenderable+10; NOT their RTTI base.
    struct spRenderSupportObservedLayout final
    {
        Address32 vtable;              // 0x00: six interface slots
        Address32 vectorAllocator;     // 0x04: untouched
        Address32 renderableBegin;     // 0x08: intrusive object references
        Address32 renderableEnd;       // 0x0c
        Address32 renderableCapacity;  // 0x10
        float localSphere[4];          // 0x14
        float worldSphere[4];          // 0x24
        Address32 worldMatrixPointer;  // 0x34
        Address32 inverseMatrixPointer;// 0x38
        spLightCacheObservedLayout lightCache; // 0x3c
        std::uint32_t visibilityMark;  // 0x64: compared with Scene40
        std::uint32_t field68;         // 0x68: ctor0, meaning open
        std::uint8_t controls[4];      // 0x6c: first differs by concrete owner
        Address32 completeObject;      // 0x70
    };

    struct spStaticRenderObjectLayout final
    {
        spNamedObjectLayout base;      // 0x00
        spRenderSupportObservedLayout support; // 0x14
        Address32 scene;               // 0x88: borrowed, NOT part of support
        float worldMatrix[16];         // 0x8c: constructor copies shared760058
        float inverseWorldMatrix[16];  // 0xcc: independently submitted as stored
    };

    struct spPartitionRenderableLayout final
    {
        spBaseObjectLayout base;       // 0x00
        spRenderSupportObservedLayout support; // 0x10
        std::uint32_t debugColor;      // 0x84: FF000000
        Address32 scene;               // 0x88: borrowed
    };

    struct spPCPartitionRenderableLayout final
    {
        spPartitionRenderableLayout base; // concrete factory4CD950 exact8c
    };

    struct spPartitionPointerVectorLayout final
    {
        Address32 allocator;           // untouched compiler word
        Address32 begin;
        Address32 end;
        Address32 capacity;
    };

    struct spPartitionNodeLayout final
    {
        spBaseObjectLayout base;       // 0x00: NOT scene spNode
        spPartitionPointerVectorLayout collisions; // 0x10: borrowed reciprocal
        spPartitionPointerVectorLayout renderNodes;// 0x20: borrowed reciprocal
        spPartitionPointerVectorLayout staticObjects; // 0x30: intrusive owned
        spPartitionPointerVectorLayout occlusionVolumes;// 0x40: borrowed reciprocal
        std::uint32_t debugColor;      // 0x50: FFFFFFFF
        Address32 parent;              // 0x54: borrowed PartitionNode
        Address32 children;            // 0x58: owned array; children direct deleted
        std::uint32_t childCount;      // 0x5c
        Address32 zone;                // 0x60: intrusive owned
        spPartitionPointerVectorLayout portals; // 0x64: intrusive owned
        Address32 partitionSystem;     // 0x74: borrowed
        Address32 partitionRenderable; // 0x78: direct owned, NOT intrusive
        std::uint32_t visibilityMark;  // 0x7c: visited stamp from Visibility14
        Address32 scene;               // 0x80: borrowed
    };

    struct spZoneLayout final
    {
        spNodeLayout base;             // 0x00
        spPartitionPointerVectorLayout localRoots; // 0xb4: BORROWED, duplicates
        std::uint32_t fieldC4;         // 0xc4: untouched, not claimed null
    };
    struct spZonePortalLayout final
    {
        spNamedObjectLayout base;     // 0x00
        Address32 destinationZone;    // 0x14: borrowed
        std::uint32_t vertexCount;    // 0x18
        Address32 vertices;           // 0x1c: directly owned XYZ array
        std::uint8_t open;            // 0x20: default1
        std::uint8_t padding21[3];
        float plane[4];               // 0x24: untouched until Init481130
        std::uint32_t visibilityMark; // 0x34: default0
    };
    struct spZonePortalNodeLayout final
    {
        spNodeLayout base;
        spPartitionPointerVectorLayout portals; // 0xb4: borrowed, duplicate-preserving
    };
    struct spOctreeRayCandidateObservedLayout final
    {
        std::uint32_t childIndex;
        float parameter;
    };
    struct spOctreeNodeLayout final
    {
        spPartitionNodeLayout base;    // 0x00: eight directly owned nullable slots
        float pivot[3];               // 0x84: ctor does NOT initialize
        spOctreeRayCandidateObservedLayout rayCandidates[4]; // 0x90: scratch, not serialized
        float mins[3];                // 0xb0: ctor zeros
        float maxs[3];                // 0xbc: ctor zeros
    };

    struct spBSPRayCandidateObservedLayout final
    {
        std::uint32_t childIndex;
        float parameter;
    };
    struct spBSPNodeLayout final
    {
        spPartitionNodeLayout base;
        float plane[4];               //0x84: original m_vNormal/m_fConstant, ctor untouched
        spBSPRayCandidateObservedLayout rayCandidates[2]; //0x94: uninitialized scratch
        Address32 polygon;            //0xa4: directly owned, ctor0
        std::uint32_t polygonCount;   //0xa8: ctor0
    };
    struct spPartitionSystemLayout final
    {
        // Actual physical/render implementation prefix. Native RTTI record
        // deliberately names spNode as direct base and skips spRenderNode.
        spRenderNodeLayout base;       // 0x000
        Address32 root;                // 0x1d4: directly owned PartitionNode
    };

    // Original helper class names are unknown. Explicit observed layouts,
    // not fabricated named engine classes or native C++ host containers.
    struct spVisibilityPlaneObservedLayout final
    {
        float equation[4];            // dot(n,p)-d
        std::uint8_t enabled;
        std::uint8_t padding11[3];
    };
    struct spVisibilityPlaneSetObservedLayout final
    {
        spPartitionPointerVectorLayout planes; // elements20, not pointers
        std::uint32_t activeCount;     //4902D0/4903E0 gate;490500 ignores
    };
    struct spOcclusionFaceObservedLayout final
    {
        float plane[4];
        Address32 vertices[3];
        std::uint32_t cameraSide;      // 0 front,1 back,2 epsilon band
    };
    struct spOcclusionEdgeObservedLayout final
    {
        Address32 firstVertex;
        Address32 secondVertex;
        Address32 oppositeFace;
        Address32 face;
        spPartitionPointerVectorLayout outgoingEdges;
        std::uint32_t walkStamp;
        std::uint8_t border;
        std::uint8_t padding25[3];
    };
    struct spOcclusionVolumeLayout final
    {
        spNodeLayout base;             // 0x000: original direct Node
        spPartitionPointerVectorLayout partitionNodes; // 0x0b4: borrowed reciprocal
        std::uint32_t visibilityMark;  // 0x0c4
        Address32 localVertexBuffer;  // 0x0c8: directly owned, position-only
        Address32 worldVertexBuffer;  // 0x0cc: directly owned transformed copy
        Address32 indexBuffer;        // 0x0d0: directly owned UInt16 copy
        std::uint32_t borderEdgeCount; // 0x0d4
        spPartitionPointerVectorLayout edges; // 0x0d8: owned edge28 pointers
        spPartitionPointerVectorLayout borderPositions; // 0x0e8: Vector3 values
        spPartitionPointerVectorLayout faces; // 0x0f8: face20 values
        spPartitionPointerVectorLayout cameraFacePlanes; // 0x108: plane10 values
        float lastCameraPosition[3];  // 0x118: world position, cache key
        std::byte field124[0x3C];      // geometric auxiliary state not fully named
        std::uint8_t planar;           // 0x160: nonzero border, coplanar faces
        std::uint8_t padding161[3];
        std::uint32_t borderWalkStamp; // 0x164
        spVisibilityPlaneSetObservedLayout occlusionPlanes; // 0x168
        float localMins[3];            // 0x17c: Init scan starts at zero
        float localMaxs[3];            // 0x188
        float localSphere[4];          // 0x194
        float worldSphere[4];          // 0x1a4
        std::uint8_t initialized;      // 0x1b4
        std::uint8_t cameraGeometryDirty; // 0x1b5
        std::uint8_t padding1B6[2];
    };
    struct spPolygonVertexObservedLayout final
    {
        Address32 freeNext;           //0x00: used only while returned to pool
        std::uint32_t identity;       //0x04: new serial even on reuse
        float position[3];           //0x08
        Address32 previous;           //0x14
        Address32 next;               //0x18
    };
    struct spVisibilityScratchObservedLayout final
    {
        std::uint32_t field00;         // untouched
        std::uint32_t identity;        // global740384 increment
        std::uint32_t nodeCount;       // logical vertex count, NOT capacity
        Address32 circularHead;        // shared740388 pool; may be stale when count0
        float plane10[4];
        std::uint32_t field20;         // untouched
        std::uint32_t field24;         // ctor0
        std::uint32_t field28;         // ctor0
    };
    struct spVisibilityManagerLayout final
    {
        spBaseObjectLayout base;
        Address32 supportVTable;       // 0x10:6E8CD8
        std::uint32_t frameStamp;      // 0x14: wraps including zero
        spPartitionPointerVectorLayout occluders; // 0x18: borrowed complete objects
        spPartitionPointerVectorLayout visibleSupports; // 0x28: borrowed adjusted supports
        Address32 overrideCamera;      // 0x38: borrowed
        std::uint8_t sphereCulling;    // 0x3c: ctor1
        std::uint8_t padding3D[3];
        spPartitionPointerVectorLayout planeStack; // 0x40: elements20
        std::uint32_t planeStackIndex; // 0x50
        spVisibilityScratchObservedLayout scratch[2]; // 0x54,0x80
    };

    struct spShadowVolumeManagerObservedPrefixLayout final
    {
        spCrossPlatformLayout base;    // 0x00: Named14 prefix
        Address32 supportVTable;       // 0x14
        std::uint8_t enabled;          // 0x18: ctor1, Scene render gate
        std::uint8_t option19;         // 0x19: ctor0, second light volume draw branch
        std::uint8_t padding1A[2];
    };
    struct spDXShadowVolumeManagerLayout final
    {
        spShadowVolumeManagerObservedPrefixLayout base;
        std::uint32_t field1C;         // ctor0
        std::uint32_t field20;         // ctor0
        std::uint32_t field24;         // ctor0
        std::uint32_t shader28;        // ctor0; Init ShadowVolumePoint lookup
        std::uint32_t shaderParameter2C; // ctor0; view_proj_matrix lookup
        std::uint32_t shaderParameter30; // untouched; Init LightPos lookup
        std::uint32_t shaderParameter34; // untouched; Init Range lookup
        std::uint32_t field38;         // ctor1
    };

    // Original49E4C0 allocates the same1D4 as its actual C++/RTTI RenderNode
    // base. Wire section order is not the native inheritance graph.
    struct spSkyBoxLayout final
    {
        spRenderNodeLayout base;
    };

    struct spSkyBoxManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint8_t enabled;          // 0x10: ctor0; SceneInit/game set1
        std::uint8_t padding11[3];     // 0x11: untouched
        Address32 attachmentRoot;      // 0x14: borrowed; game chooses DefaultCamera
        Address32 listAllocator;       // 0x18: untouched
        Address32 listSentinel;        // 0x1c: owned12-byte list nodes, borrowed sky pointers
        std::uint32_t skyBoxCount;     // 0x20: duplicate append allowed
    };

    struct spPCProjectionManagerLayout final
    {
        spCrossPlatformLayout base;    // 0x00: registered ProjectionManager base adds fields14+
        std::uint8_t enabled;          // 0x14: ctor0, Init45A290 sets1
        std::uint8_t padding15[3];     // 0x15: untouched
        Address32 listAllocator;       // 0x18: untouched
        Address32 listSentinel;        // 0x1c: borrowed pointers, duplicate scan before append
        std::uint32_t projectionCount;// 0x20
    };

    // Base prefix only: original LensFlareManager has no RTTI factory.
    struct spLensFlareManagerObservedLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        std::uint8_t enabled;          // 0x14
        std::uint8_t padding15[3];     // 0x15: untouched
        Address32 firstFlare;          // 0x18: borrowed intrusive; flare previous58/next5c
        Address32 lastFlare;           // 0x1c
        std::uint32_t flareCount;      // 0x20
    };

    struct spPCLensFlareManagerLayout final
    {
        spLensFlareManagerObservedLayout base; // 0x00
        std::uint8_t queryCapability;  // 0x24: result !=8876086A, not general HRESULT success
        std::uint8_t padding25[3];     // 0x25: untouched
        Address32 queryMapAllocator;   // 0x28: untouched
        Address32 queryMapSentinel;    // 0x2c: owned tree, payload query-group pointers
        std::uint32_t queryMapCount;   // 0x30
        std::uint32_t queryCount;      // 0x34: four query interfaces per entry
    };

    // No RTTI factory exposes a complete sizeof. The common destructor and
    // Optimize body nevertheless account for this entire 0x38 prefix.
    struct spSceneGraphOptimizerObservedPrefixLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 singletonVTable;     // 0x14: embedded singleton support
        Address32 callbackVTable;      // 0x18: OnStart/End/Node/Model subobject
        Address32 supportVTable;       // 0x1c: exact role unresolved
        Address32 temporaryAllocatorState; // 0x20
        Address32 temporaryBegin;      // 0x24
        Address32 temporaryEnd;        // 0x28
        Address32 temporaryCapacityEnd;// 0x2c
        Address32 detachListAllocatorState; // 0x30
        Address32 detachListHead;      // 0x34: owned sentinel
    };

    // DX callback code addresses these two old-MSVC containers relative to
    // its embedded callback subobject at complete-object +0x18. The protected
    // constructor/factory still prevent promoting this prefix to exact sizeof.
    struct spDXSceneGraphOptimizerObservedPrefixLayout final
    {
        spSceneGraphOptimizerObservedPrefixLayout base; // 0x00
        std::uint32_t field38;         // 0x38: exact role unresolved
        std::uint32_t batchListAllocatorState; // 0x3c
        Address32 batchListSentinel;   // 0x40
        std::uint32_t batchListSize;   // 0x44
        std::uint32_t groupMapAllocatorState; // 0x48
        Address32 groupMapSentinel;    // 0x4c
        std::uint32_t groupMapSize;    // 0x50
    };

    // Original concrete41A330 now proves exactF0. Keep former type spelling
    // for compatibility; allocator/padding/opaque bytes remain distinct.
    struct spLightObservedLayout final
    {
        spNodeLayout base;             // 0x00
        Address32 sceneLightVTable;    // 0xb4: embedded support subobject
        Address32 previousInScene;     // 0xb8: borrowed intrusive manager link
        Address32 nextInScene;         // 0xbc
        std::uint32_t type;            // 0xc0: 0..3 light type
        float colorRGBA[4];            // 0xc4: normalized R,G,B,A
        std::uint8_t attenuation;      // 0xd4: serializer field 3
        std::uint8_t paddingD5[3];     // 0xd5
        float intensity;               // 0xd8: serializer field 4
        std::uint32_t opaqueRuntimeDC; // 0xdc: copied, not initialized/serialized
        float range;                   // 0xe0: serializer field 5
        float hotspotAngle;            // 0xe4: serializer field 6
        float falloffAngle;            // 0xe8: serializer field 7
        std::uint8_t projectShadow;    // 0xec: serializer field 1
        std::uint8_t enabled;          // 0xed: serializer field 8
        std::uint8_t paddingEE[2];     // 0xee
    };

    // spLightData changes RTTI/factory/clone but adds no observed storage.
    struct spLightDataObservedLayout final
    {
        spLightObservedLayout base;    // 0x00
    };

    struct spLightManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 firstLight;          // 0x10: borrowed intrusive list
        Address32 lastLight;           // 0x14
        std::uint32_t lightCount;      // 0x18
        Address32 renderNodeList;      // 0x1c: ctor0;45D850 points to scene18
        Address32 ownerScene;          // 0x20: ctor UNTOUCHED; scene ctor writes this
    };

    struct spMaterialObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 name;                // 0x10: physical NamedObject, RTTI Base
        Address32 materialVTable;      // 0x14: secondary material interface
        std::uint32_t renderStates[11];// 0x18: not PS2-aligned 0x20
        std::uint32_t opaque44;        // 0x44: ctor untouched, base copy includes
        std::uint32_t passCount;       // 0x48
        Address32 passes[8];           // 0x4c
        std::uint8_t renderOverride;   // 0x6c
        std::uint8_t useVertexAlpha;   // 0x6d
        std::uint8_t padding6E[2];
        std::uint32_t opaqueRuntime70;
        Address32 materialColorController; // 0x74
    };

    struct spMaterialDataObservedLayout final
    {
        spMaterialObservedLayout base; // 0x00
        float diffuseRGBA[4];          // 0x78 (secondary this+0x64)
        float ambientRGBA[4];          // 0x88
        float specularRGBA[4];         // 0x98
        float emissiveRGBA[4];         // 0xa8
        float specularPower;           // 0xb8
    };

    struct spDXMaterialObservedLayout final
    {
        spMaterialObservedLayout base;
        float diffuseRGBA[4];
        float ambientRGBA[4];
        float specularRGBA[4];
        float emissiveRGBA[4];
        std::uint32_t specularPowerBits; // +B8 untouched by actual4A9460 factory
    };
    static_assert(sizeof(spDXMaterialObservedLayout)==0xBC);
    static_assert(offsetof(spDXMaterialObservedLayout,specularPowerBits)==0xB8);
    inline constexpr std::uint32_t spDXMaterialClassID=0x797B39EC;
    inline constexpr Address32 spDXMaterialRegistration=0x007630E8;
    inline constexpr Address32 spDXMaterialFactory=0x004A9460;
    inline constexpr Address32 spDXMaterialPrimaryVTable=0x006EF264;
    inline constexpr Address32 spDXMaterialInterfaceVTable=0x006EF238;
    inline constexpr std::uint32_t spDXMaterialSerializerClassID=0x177E2F26;
    inline constexpr Address32 spDXMaterialSerializerRegistration=0x00763AE0;
    inline constexpr Address32 spDXMaterialSerializerFactory=0x004B0DD0;
    inline constexpr Address32 spDXMaterialSerializerPrimaryVTable=0x006EFD74;
    inline constexpr Address32 spDXMaterialSerializerInterfaceVTable=0x006EFD68;

    struct spMaterialPassLayerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00: direct registered base
        std::uint32_t finalBlendOperation; // 0x10: serializer pass payload
        std::uint32_t layerCount;      // 0x14
        Address32 layers[8];           // 0x18: fixed direct-delete owners, refcount0
    };

    struct spMaterialTextureLayerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00: direct registered base
        Address32 materialTexture;     // 0x10: owned spMaterialTexture family
    };

    struct spStdLayerObservedLayout final
    {
        spMaterialTextureLayerObservedLayout base; // no derived storage
    };

    struct spFogObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 name;                // 0x10: physical NamedObject; ctor NULL
        std::uint32_t type;            // 0x14
        std::uint32_t colorARGB;       // 0x18
        float start;                   // 0x1c
        float end;                     // 0x20
        float density;                 // 0x24
    };

    // Complete observed extent. Every concrete PC serializer constructor
    // writes the secondary serialization-interface vptr at +0x10; protected
    // factories prevent promoting the aligned 0x14 extent to exact sizeof.
    struct spSerializerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 serializerVTable;    // 0x10: secondary interface
    };

    // Exact 0x4C allocation from the original PC animation serializer factory.
    // Reader 0x0043ECC0 receives this + 0x10 and resets these scratch counters.
    struct spAnimationSerializerLayout final
    {
        spSerializerObservedLayout base;
        std::uint32_t valuePoolCounts[6];   // 0x14..0x28
        std::uint32_t valuePoolOffsets[6];  // 0x2C..0x40: used entries, not pointers
        std::uint32_t timePoolOffset;      // 0x44: used float entries
        std::uint32_t timePoolCount;       // 0x48
    };

    // spNodeSerializer adds behavior and vtables but no storage. Original
    // PC4638F0 factory now confirms exact14 (probe_pc_node_serializer).
    // Keep the historical ObservedLayout spelling for source compatibility.
    struct spNodeSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spRenderNodeSerializerObservedLayout final
    {
        spNodeSerializerObservedLayout base; // no observed derived storage
    };

    struct spLightDataSerializerObservedLayout final
    {
        spNodeSerializerObservedLayout base;
    };

    struct spLightSerializerObservedLayout final
    {
        spNodeSerializerObservedLayout base;
    };

    struct spCameraSerializerObservedLayout final
    {
        spNodeSerializerObservedLayout base;
    };

    struct spCameraDataSerializerObservedLayout final
    {
        spCameraSerializerObservedLayout base;
    };

    struct spFogSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spMatColorControllerSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spLightControllerSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spAnimTexControllerSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spUVControllerSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spTransFunctionEvalSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spFunctionEvalSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spColorFuncEvalSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spSphereBVSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spBoxBVSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spOBBBVSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spRenderableSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spMaterialSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
        std::uint8_t dataBlockSerializerState[0x28];
    };

    struct spMaterialDataSerializerObservedLayout final
    {
        spMaterialSerializerObservedLayout base;
    };

    struct spDXMaterialDataSerializerObservedLayout final
    {
        spMaterialSerializerObservedLayout base;
    };

    struct spPS2MaterialDataSerializerObservedLayout final
    {
        spMaterialSerializerObservedLayout base;
    };

    struct spModelSerializerObservedLayout final
    {
        spRenderableSerializerObservedLayout base;
    };

    struct spSkinSerializerObservedLayout final
    {
        spModelSerializerObservedLayout base;
    };

    struct spMeshDataSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spPS2MeshDataSerializerObservedLayout final
    {
        spMeshDataSerializerObservedLayout base;
    };

    struct spDXMeshDataSerializerObservedLayout final
    {
        spMeshDataSerializerObservedLayout base;
    };

    struct spTextureDataSerializerObservedLayout final
    {
        spSerializerObservedLayout base;
    };

    struct spPS2TextureDataSerializerObservedLayout final
    {
        spTextureDataSerializerObservedLayout base;
    };

    struct spDXTextureDataSerializerObservedLayout final
    {
        spTextureDataSerializerObservedLayout base;
    };

    static_assert(sizeof(spMeshLayout) == 0x50);
    static_assert(offsetof(spMeshLayout, boundsValid) == 0x28);
    static_assert(offsetof(spMeshLayout, boundsMinimum) == 0x2C);
    static_assert(offsetof(spMeshLayout, vertexComponentFlags) == 0x44);
    static_assert(offsetof(spMeshLayout, primitiveCount) == 0x48);
    static_assert(offsetof(spMeshLayout, vertexCount) == 0x4C);
    static_assert(sizeof(spRenderMeshLayout) == 0x50);
    static_assert(sizeof(spDXSceneGraphOptimizerObservedPrefixLayout) == 0x54);
    static_assert(offsetof(spDXSceneGraphOptimizerObservedPrefixLayout,
        batchListSentinel) == 0x40);
    static_assert(offsetof(spDXSceneGraphOptimizerObservedPrefixLayout,
        groupMapSentinel) == 0x4C);
    static_assert(sizeof(spDXVertexBufferLayout) == 0x20);
    static_assert(offsetof(spDXVertexBufferLayout, direct3DVertexBuffer) == 0x10);
    static_assert(offsetof(spDXVertexBufferLayout, fvfCode) == 0x14);
    static_assert(offsetof(spDXVertexBufferLayout, byteSize) == 0x1C);
    static_assert(sizeof(spDXIndexBufferLayout) == 0x1C);
    static_assert(offsetof(spDXIndexBufferLayout, direct3DIndexBuffer) == 0x10);
    static_assert(offsetof(spDXIndexBufferLayout, byteSize) == 0x18);
    static_assert(sizeof(spDXMeshCombinerLayout) == 0x2C);
    static_assert(offsetof(spDXMeshCombinerLayout, targetVertexCount) == 0x04);
    static_assert(offsetof(spDXMeshCombinerLayout, vertexBuffer) == 0x18);
    static_assert(offsetof(spDXMeshCombinerLayout, writtenIndexCount) == 0x28);
    static_assert(sizeof(spDXSharedMeshDataObservedLayout) == 0x1C);
    static_assert(offsetof(spDXSharedMeshDataObservedLayout, indexBuffer) == 0x14);
    static_assert(offsetof(spDXSharedMeshDataObservedLayout, vertexBuffer) == 0x18);
    static_assert(sizeof(spDXCombinedVBObservedLayout) == 0x3C);
    static_assert(offsetof(spDXCombinedVBObservedLayout, mapSentinel) == 0x20);
    static_assert(offsetof(spDXCombinedVBObservedLayout, vertexData) == 0x2C);
    static_assert(offsetof(spDXCombinedVBObservedLayout, indexByteSize) == 0x38);
    static_assert(sizeof(spDXMeshObservedLayout) == 0x88);
    static_assert(offsetof(spDXMeshObservedLayout, indexType) == 0x50);
    static_assert(offsetof(spDXMeshObservedLayout, sharedMeshData) == 0x5C);
    static_assert(offsetof(spDXMeshObservedLayout, fvfCode) == 0x70);
    static_assert(offsetof(spDXMeshObservedLayout, vertexDeclaration) == 0x84);
    static_assert(sizeof(spDXMeshSerializerObservedLayout) == 0x14);
    static_assert(offsetof(spDXMeshSerializerObservedLayout,
        serializerInterfaceVTable) == 0x10);
    static_assert(sizeof(spMeshDataLayout) == 0x58);
    static_assert(offsetof(spMeshDataLayout, indexBuffer) == 0x50);
    static_assert(offsetof(spMeshDataLayout, vertexBuffer) == 0x54);
    static_assert(sizeof(spVertexBufferLayout) == 0x5C);
    static_assert(offsetof(spVertexBufferLayout, componentFlags) == 0x14);
    static_assert(offsetof(spVertexBufferLayout, componentOffsets) == 0x24);
    static_assert(offsetof(spVertexBufferLayout, flags) == 0x50);
    static_assert(offsetof(spVertexBufferLayout, initialized) == 0x58);
    static_assert(sizeof(spResourceLayout) == 0x14);
    static_assert(sizeof(spResourceCacheEntryLayout) == 0x08);
    static_assert(sizeof(spResourceManagerObservedLayout) == 0x30);
    static_assert(offsetof(spResourceManagerObservedLayout,
        reserveEnabled) == 0x15);
    static_assert(offsetof(spResourceManagerObservedLayout,
        reserveCount) == 0x18);
    static_assert(offsetof(spResourceManagerObservedLayout,
        entriesBegin) == 0x24);
    static_assert(sizeof(spTextureBufferLayout) == 0x30);
    static_assert(offsetof(spTextureBufferLayout, buffer) == 0x1C);
    static_assert(offsetof(spTextureBufferLayout, auxiliaryObject) == 0x24);
    static_assert(offsetof(spTextureBufferLayout, initialized) == 0x2C);
    static_assert(sizeof(spTextureDataVectorLayout) == 0x10);
    static_assert(sizeof(spTextureDataLayout) == 0x4A0);
    static_assert(offsetof(spTextureDataLayout, textureBuffer) == 0x38);
    static_assert(offsetof(spTextureDataLayout, field68) == 0x68);
    static_assert(offsetof(spTextureDataLayout, field7C) == 0x7C);
    static_assert(offsetof(spTextureDataLayout, records490) == 0x490);
    static_assert(sizeof(spTextureObservedLayout) == 0x38);
    static_assert(offsetof(spTextureObservedLayout, iTextureVTable) == 0x14);
    static_assert(offsetof(spTextureObservedLayout, width) == 0x28);
    static_assert(offsetof(spTextureObservedLayout, field34) == 0x34);
    static_assert(sizeof(spPlatformSpecificMeshDataLayout) == 0x14);
    static_assert(sizeof(spDXMeshDataObservedPrefixLayout) == 0x20);
    static_assert(offsetof(spDXMeshDataObservedPrefixLayout, indexBuffer) == 0x18);
    static_assert(offsetof(spDXMeshDataObservedPrefixLayout, vertexBuffer) == 0x1C);
    static_assert(sizeof(spPS2MeshDataObservedLayout) == 0x100);
    static_assert(offsetof(spPS2MeshDataObservedLayout, packet) == 0x40);
    static_assert(offsetof(spPS2MeshDataObservedLayout, field48) == 0x48);
    static_assert(offsetof(spPS2MeshDataObservedLayout, fieldA0) == 0xA0);
    static_assert(offsetof(spPS2MeshDataObservedLayout, fieldFC) == 0xFC);
    static_assert(sizeof(spRenderableCallbackRecordLayout) == 8);
    static_assert(sizeof(spRenderableCallbackVectorLayout) == 0x10);
    static_assert(sizeof(spRenderableLayout) == 0x58);
    static_assert(offsetof(spRenderableLayout, material) == 0x20);
    static_assert(offsetof(spRenderableLayout, callbacks44) == 0x44);
    static_assert(sizeof(spModelLayout) == 0x60);
    static_assert(offsetof(spModelLayout, baseMeshData) == 0x58);
    static_assert(offsetof(spModelLayout, projectionGroup) == 0x5C);
    static_assert(sizeof(spSkinObservedLayout) == 0x70);
    static_assert(offsetof(spSkinObservedLayout, weightCount) == 0x60);
    static_assert(offsetof(spSkinObservedLayout, boneCount) == 0x64);
    static_assert(offsetof(spSkinObservedLayout, bones) == 0x68);
    static_assert(offsetof(spSkinObservedLayout, inverseBindMatrices) == 0x6C);
    static_assert(sizeof(AnimationDescriptorPoolLayout) == 0x2C);
    static_assert(sizeof(spTrackLayout) == 0x14);
    static_assert(sizeof(spAnimTrackLayout) == 0x44);
    static_assert(offsetof(spAnimTrackLayout, descriptors) == 0x18);
    static_assert(offsetof(spAnimationLayout, priorityGroup) == 0x18);
    static_assert(sizeof(spActorStartRequestObservedLayout) == 0x38);
    static_assert(offsetof(spActorStartRequestObservedLayout, initialTime) == 0x34);
    static_assert(offsetof(spAnimTrackLayout, animationOwner) == 0x40);
    static_assert(sizeof(AnimationTagObservedLayout) == 0x1C);
    static_assert(sizeof(spAnimationLayout) == 0x84);
    static_assert(sizeof(spAnimationManagerLayout) == 0x2C);
    static_assert(offsetof(spAnimationManagerLayout, frame) == 0x10);
    static_assert(offsetof(spAnimationManagerLayout, nameMapSentinel) == 0x1C);
    static_assert(offsetof(spAnimationManagerLayout, controllerHead) == 0x24);
    static_assert(sizeof(AnimationNameMapNodeLayout) == 0x34);
    static_assert(offsetof(AnimationNameMapNodeLayout, slot) == 0x28);
    static_assert(offsetof(AnimationNameMapNodeLayout, references) == 0x2C);
    static_assert(offsetof(AnimationNameMapNodeLayout, isSentinel) == 0x31);
    static_assert(sizeof(spControllerLayout) == 0x1C);
    static_assert(offsetof(spControllerLayout, next) == 0x14);
    static_assert(offsetof(spAnimationLayout, totalTime) == 0x14);
    static_assert(offsetof(spAnimationLayout, tracks) == 0x1C);
    static_assert(offsetof(spAnimationLayout, tagsBegin) == 0x2C);
    static_assert(offsetof(spAnimationLayout, auxiliaryBuffers) == 0x38);
    static_assert(offsetof(spAnimationLayout, descriptorPool) == 0x58);
    static_assert(sizeof(spTransformTrackEvalObservedLayout) == 0x78);
    static_assert(sizeof(spTransformTrackEvalInputLayout) == 0x30);
    static_assert(offsetof(spTransformTrackEvalInputLayout, positionKeyIndices) == 0x0C);
    static_assert(offsetof(spTransformTrackEvalInputLayout, rotationKeyIndices) == 0x18);
    static_assert(offsetof(spTransformTrackEvalInputLayout, scaleKeyIndices) == 0x24);
    static_assert(sizeof(spNodeControllerLayout) == 0x18);
    static_assert(offsetof(spNodeControllerLayout, node) == 0x10);
    static_assert(offsetof(spNodeControllerLayout, evaluator) == 0x14);
    static_assert(sizeof(spActorObservedLayout) == 0x54);
    static_assert(offsetof(spActorObservedLayout, playbackStates) == 0x28);
    static_assert(offsetof(spActorObservedLayout, controllersBegin) == 0x40);
    static_assert(sizeof(spActorPlaybackObservedLayout) == 0x60);
    static_assert(offsetof(spActorPlaybackObservedLayout, sampleTime) == 0x34);
    static_assert(offsetof(spActorPlaybackObservedLayout, bindingUseCount) == 0x48);
    static_assert(offsetof(spTransformTrackEvalObservedLayout,
        boundTransformSlot) == 0x10);
    static_assert(offsetof(spTransformTrackEvalObservedLayout,
        blendInputCount) == 0x14);
    static_assert(offsetof(spTransformTrackEvalObservedLayout,
        blendInputs) == 0x18);
    static_assert(sizeof(spIndexBufferLayout) == 0x28);
    static_assert(offsetof(spIndexBufferLayout, primitiveCount) == 0x18);
    static_assert(offsetof(spIndexBufferLayout, indexData) == 0x24);
    static_assert(sizeof(spEngineCoreLayout) == 0x158);
    static_assert(sizeof(spGameLevelSerializerLayout) == 0x184);
    static_assert(offsetof(spGameLevelSerializerLayout, instanceName) == 0x1C);
    static_assert(offsetof(spGameLevelSerializerLayout, assetPath) == 0x5C);
    static_assert(offsetof(spGameLevelSerializerLayout, position) == 0x15C);
    static_assert(sizeof(spGameLevelLayout) == 0x2C);
    static_assert(offsetof(spGameLevelLayout, listHead) == 0x18);
    static_assert(offsetof(spGameLevelLayout, instanceCount) == 0x1C);
    static_assert(sizeof(spTemplateSerializerLayout) == 0x1F4);
    static_assert(offsetof(spTemplateSerializerLayout, targetObject) == 0x10);
    static_assert(offsetof(spTemplateSerializerLayout, assetPath) == 0x20);
    static_assert(offsetof(spTemplateSerializerLayout, parentID) == 0x124);
    static_assert(offsetof(spTemplateSerializerLayout, outputObject) == 0x1E4);
    static_assert(sizeof(spTemplateObjectLayout) == 0x1B0);
    static_assert(offsetof(spTemplateObjectLayout, loadedObject) == 0x1C);
    static_assert(offsetof(spTemplateObjectLayout, resourcePath) == 0x2C);
    static_assert(offsetof(spTemplateObjectLayout, field12C) == 0x12C);
    static_assert(offsetof(spTemplateObjectLayout, constructedDefaults) == 0x174);
    static_assert(sizeof(spTemplateInstanceLayout) == 0x28);
    static_assert(offsetof(spTemplateInstanceLayout, listHead) == 0x1C);
    static_assert(offsetof(spTemplateInstanceLayout, instanceRoot) == 0x24);
    static_assert(sizeof(spTemplateManagerObservedPrefixLayout) == 0x24);
    static_assert(offsetof(spTemplateManagerObservedPrefixLayout,
        templateCount) == 0x1C);
    static_assert(sizeof(spEntityManagerObservedPrefixLayout) == 0x20);
    static_assert(offsetof(spEntityManagerObservedPrefixLayout, listHead) == 0x18);
    static_assert(offsetof(spEntityManagerObservedPrefixLayout, entityCount) == 0x1C);
    static_assert(sizeof(spDebugManagerLayout) == 0x38);
    static_assert(offsetof(spDebugManagerLayout, supportVTable) == 0x10);
    static_assert(sizeof(spFontManagerLayout) == 0x3C);
    static_assert(offsetof(spFontManagerLayout, supportVTable) == 0x14);
    static_assert(offsetof(spFontManagerLayout, primaryMaterial) == 0x34);
    static_assert(offsetof(spFontManagerLayout, fallbackMaterial) == 0x38);
    static_assert(sizeof(spPCFontManagerLayout) == 0x3C);
    static_assert(sizeof(spInputManagerObservedPrefixLayout) == 0x20);
    static_assert(offsetof(spInputManagerObservedPrefixLayout,
        inputInterfaceVTable) == 0x18);
    static_assert(offsetof(spInputManagerObservedPrefixLayout, field1C) == 0x1C);
    static_assert(sizeof(spDXInputManagerObservedLayout) == 0x50);
    static_assert(offsetof(spDXInputManagerObservedLayout, initialized) == 0x24);
    static_assert(offsetof(spDXInputManagerObservedLayout, controllerCount) == 0x3C);
    static_assert(offsetof(spDXInputManagerObservedLayout, keyboardDevice) == 0x2C);
    static_assert(offsetof(spDXInputManagerObservedLayout, controllers) == 0x40);
    static_assert(sizeof(spRendererAlphaRecordLayout) == 0x18);
    static_assert(sizeof(spRendererGeneralRecordLayout) == 0x14);
    static_assert(sizeof(spRendererBucketRecordLayout) == 8);
    static_assert(sizeof(spRendererQueueVectorLayout) == 0x10);
    static_assert(sizeof(spRendererLayout) == 0xCA08);
    static_assert(offsetof(spRendererLayout, rendererInterfaceVTable) == 0x18);
    static_assert(offsetof(spRendererLayout, renderQueueEnabled) == 0xC050);
    static_assert(offsetof(spRendererLayout, alphaRecords) == 0x50);
    static_assert(offsetof(spRendererLayout, generalQueue) == 0xC054);
    static_assert(offsetof(spRendererLayout, modeQueues) == 0xC064);
    static_assert(offsetof(spRendererLayout, currentMaterial) == 0xC18C);
    static_assert(offsetof(spRendererLayout, currentLightCache) == 0xC190);
    static_assert(offsetof(spRendererLayout, currentField28) == 0xC194);
    static_assert(offsetof(spRendererLayout, materialStateC1C4) == 0xC1C4);
    static_assert(offsetof(spRendererLayout, renderStateCache) == 0xC868);
    static_assert(offsetof(spRendererLayout, textureStateCache) == 0xC898);
    static_assert(offsetof(spRendererLayout, lifetimeTailC9B8) == 0xC9B8);
    static_assert(sizeof(spDXRendererLayout) == 0xF368);
    static_assert(sizeof(spPCRendererLayout) == 0xF368);
    static_assert(sizeof(spRenderTargetObservedPrefixLayout) == 0x28);
    static_assert(offsetof(spRenderTargetObservedPrefixLayout,
        targetInterfaceVTable) == 0x14);
    static_assert(offsetof(spRenderTargetObservedPrefixLayout, width) == 0x1C);
    static_assert(offsetof(spRenderTargetObservedPrefixLayout, pixelFormat) == 0x24);
    static_assert(sizeof(spCubeRenderTargetObservedPrefixLayout) == 0x2C);
    static_assert(sizeof(spDXRenderTargetObservedPrefixLayout) == 0x2C);
    static_assert(sizeof(spPCRenderTargetObservedPrefixLayout) == 0x2C);
    static_assert(sizeof(spDXCubeRenderTargetObservedPrefixLayout) == 0x44);
    static_assert(offsetof(spDXCubeRenderTargetObservedPrefixLayout,
        faceSurfaces) == 0x2C);
    static_assert(sizeof(spRenderTargetListStateLayout) == 0x0C);
    static_assert(sizeof(spRenderTargetManagerObservedLayout) == 0x44);
    static_assert(offsetof(spRenderTargetManagerObservedLayout,
        ordinaryTargets) == 0x18);
    static_assert(offsetof(spRenderTargetManagerObservedLayout,
        cubeTargets) == 0x24);
    static_assert(offsetof(spRenderTargetManagerObservedLayout,
        layerTargets) == 0x30);
    static_assert(sizeof(spMaterialTextureObservedLayout) == 0x68);
    static_assert(offsetof(spMaterialTextureObservedLayout,
        fallbackTexture) == 0x34);
    static_assert(offsetof(spMaterialTextureObservedLayout,
        uvController) == 0x38);
    static_assert(offsetof(spMaterialTextureObservedLayout,
        animationController) == 0x64);
    static_assert(sizeof(spMaterialRenderTargetTextureObservedLayout) == 0x8C);
    static_assert(offsetof(spMaterialRenderTargetTextureObservedLayout,
        maxRecursionLevel) == 0x6C);
    static_assert(offsetof(spMaterialRenderTargetTextureObservedLayout,
        targetsBegin) == 0x80);
    static_assert(sizeof(spMaterialCameraViewTextureObservedLayout) == 0x94);
    static_assert(sizeof(spMaterialCubeMapTextureLayout) == 0x9C);
    static_assert(offsetof(spMaterialCubeMapTextureLayout,
        facesPerTick) == 0x94);
    static_assert(offsetof(spEngineCoreLayout, initialized) == 0x14);
    static_assert(offsetof(spEngineCoreLayout, defaultScene) == 0x18);
    static_assert(offsetof(spEngineCoreLayout, defaultCamera) == 0x1C);
    static_assert(offsetof(spEngineCoreLayout, firstFrameCallback) == 0x30);
    static_assert(offsetof(spEngineCoreLayout, field3C) == 0x3C);
    static_assert(sizeof(spTaskTimerLayout) == 0x3C);
    static_assert(offsetof(spTaskTimerLayout, active) == 0x18);
    static_assert(offsetof(spTaskTimerLayout, deltaSeconds) == 0x28);
    static_assert(offsetof(spTaskTimerLayout, sourceClock) == 0x2C);
    static_assert(offsetof(spTaskTimerLayout, previousSibling) == 0x10);
    static_assert(offsetof(spTaskTimerLayout, lastChild) == 0x34);
    static_assert(offsetof(spTaskTimerLayout, childCount) == 0x38);
    static_assert(offsetof(spEngineCoreLayout, taskTimers54) == 0x54);
    static_assert(offsetof(spEngineCoreLayout, taskTimers54) + sizeof(spTaskTimerLayout)
        + offsetof(spTaskTimerLayout, deltaSeconds) == 0xB8);
    static_assert(offsetof(spEngineCoreLayout, eventQueuesCC) == 0xCC);
    static_assert(offsetof(spEngineCoreLayout, field154) == 0x154);
    static_assert(sizeof(spNodeLayout) == 0xB4);
    static_assert(offsetof(spNodeLayout, childListHead) == 0x18);
    static_assert(offsetof(spNodeLayout, position) == 0x20);
    static_assert(offsetof(spNodeLayout, parent) == 0x2C);
    static_assert(offsetof(spNodeLayout, scale) == 0x30);
    static_assert(offsetof(spNodeLayout, sceneLink) == 0x3C);
    static_assert(offsetof(spNodeLayout, orientation) == 0x40);
    static_assert(offsetof(spNodeLayout, collisionBegin) == 0x68);
    static_assert(offsetof(spNodeLayout, cachedWorldPosition) == 0x74);
    static_assert(offsetof(spNodeLayout, cachedWorldScale) == 0x80);
    static_assert(offsetof(spNodeLayout, cachedWorldOrientation) == 0x8C);
    static_assert(offsetof(spNodeLayout, flags) == 0xB0);
    static_assert(sizeof(spSceneManagerLayout) == 0x24);
    static_assert(offsetof(spSceneManagerLayout, currentScene) == 0x20);
    static_assert(sizeof(spSceneLayout) == 0x54);
    static_assert(offsetof(spSceneLayout, systemRoot) == 0x14);
    static_assert(offsetof(spSceneLayout, firstRenderNode) == 0x18);
    static_assert(offsetof(spSceneLayout, renderNodeCount) == 0x20);
    static_assert(offsetof(spSceneLayout, lightManager) == 0x34);
    static_assert(offsetof(spSceneLayout, sortedBegin) == 0x48);
    static_assert(sizeof(spCameraObservedLayout) == 0x238);
    static_assert(offsetof(spCameraObservedLayout, nearClipPlane) == 0xBC);
    static_assert(offsetof(spCameraObservedLayout, twoDimensional) == 0xC8);
    static_assert(offsetof(spCameraObservedLayout, viewMatrix) == 0xCC);
    static_assert(offsetof(spCameraObservedLayout, projectionMatrix) == 0x10C);
    static_assert(offsetof(spCameraObservedLayout, viewBasis) == 0x14C);
    static_assert(offsetof(spCameraObservedLayout, viewport170) == 0x170);
    static_assert(offsetof(spCameraObservedLayout, viewAngle) == 0x188);
    static_assert(offsetof(spCameraObservedLayout, pixelAspectRatio) == 0x190);
    static_assert(offsetof(spCameraObservedLayout, frustumPlanes) == 0x1C4);
    static_assert(offsetof(spCameraObservedLayout, dirtyFlags) == 0x224);
    static_assert(offsetof(spCameraObservedLayout, viewportActive) == 0x230);
    static_assert(offsetof(spCameraObservedLayout, viewportRatio) == 0x234);
    static_assert(sizeof(spCameraDataObservedLayout) == 0x238);
    static_assert(sizeof(spDXCameraObservedLayout) == 0x238);
    static_assert(sizeof(spRenderNodeObservedPrefixLayout) == 0xC8);
    static_assert(offsetof(spRenderNodeObservedPrefixLayout,
        renderableBegin) == 0xBC);
    static_assert(offsetof(spRenderNodeObservedPrefixLayout,
        renderableEnd) == 0xC0);
    static_assert(sizeof(spRenderNodeLayout) == 0x1D4);
    static_assert(sizeof(spSkyBoxLayout) == 0x1D4);
    static_assert(sizeof(spSkyBoxManagerLayout) == 0x24);
    static_assert(offsetof(spSkyBoxManagerLayout, attachmentRoot) == 0x14);
    static_assert(sizeof(spPCProjectionManagerLayout) == 0x24);
    static_assert(offsetof(spPCProjectionManagerLayout, listSentinel) == 0x1C);
    static_assert(sizeof(spLensFlareManagerObservedLayout) == 0x24);
    static_assert(sizeof(spPCLensFlareManagerLayout) == 0x38);
    static_assert(offsetof(spPCLensFlareManagerLayout, queryCapability) == 0x24);
    static_assert(sizeof(spLightCacheObservedLayout) == 0x28);
    static_assert(offsetof(spLightCacheObservedLayout, ambientLight) == 0x20);
    static_assert(offsetof(spRenderNodeLayout, localSphere) == 0xC8);
    static_assert(offsetof(spRenderNodeLayout, worldSphere) == 0xD8);
    static_assert(offsetof(spRenderNodeLayout, lightCache) == 0xF0);
    static_assert(offsetof(spRenderNodeLayout, self) == 0x124);
    static_assert(offsetof(spRenderNodeLayout, previousInScene) == 0x128);
    static_assert(offsetof(spRenderNodeLayout, bypassFrustumCull) == 0x130);
    static_assert(offsetof(spRenderNodeLayout, worldMatrix) == 0x138);
    static_assert(offsetof(spRenderNodeLayout, inverseWorldMatrix) == 0x178);
    static_assert(offsetof(spRenderNodeLayout, inverseWorldScale) == 0x1B8);
    static_assert(offsetof(spRenderNodeLayout, callbackBegin) == 0x1C8);
    static_assert(sizeof(spRenderSupportObservedLayout) == 0x74);
    static_assert(offsetof(spRenderSupportObservedLayout, lightCache) == 0x3C);
    static_assert(offsetof(spRenderSupportObservedLayout, completeObject) == 0x70);
    static_assert(sizeof(spStaticRenderObjectLayout) == 0x10C);
    static_assert(offsetof(spStaticRenderObjectLayout, scene) == 0x88);
    static_assert(offsetof(spStaticRenderObjectLayout, worldMatrix) == 0x8C);
    static_assert(offsetof(spStaticRenderObjectLayout, inverseWorldMatrix) == 0xCC);
    static_assert(sizeof(spPartitionRenderableLayout) == 0x8C);
    static_assert(sizeof(spPCPartitionRenderableLayout) == 0x8C);
    static_assert(offsetof(spPartitionRenderableLayout, debugColor) == 0x84);
    static_assert(offsetof(spPartitionRenderableLayout, scene) == 0x88);
    static_assert(sizeof(spPartitionNodeLayout) == 0x84);
    static_assert(offsetof(spPartitionNodeLayout, debugColor) == 0x50);
    static_assert(offsetof(spPartitionNodeLayout, zone) == 0x60);
    static_assert(offsetof(spPartitionNodeLayout, portals) == 0x64);
    static_assert(offsetof(spPartitionNodeLayout, partitionRenderable) == 0x78);
    static_assert(offsetof(spPartitionNodeLayout, scene) == 0x80);
    static_assert(sizeof(spZoneLayout) == 0xC8);
    static_assert(sizeof(spZonePortalLayout) == 0x38);
    static_assert(offsetof(spZonePortalLayout, destinationZone) == 0x14);
    static_assert(offsetof(spZonePortalLayout, vertices) == 0x1C);
    static_assert(offsetof(spZonePortalLayout, plane) == 0x24);
    static_assert(offsetof(spZonePortalLayout, visibilityMark) == 0x34);
    static_assert(sizeof(spZonePortalNodeLayout) == 0xC4);
    static_assert(offsetof(spZonePortalNodeLayout, portals) == 0xB4);
    static_assert(sizeof(spOctreeRayCandidateObservedLayout) == 8);
    static_assert(sizeof(spOctreeNodeLayout) == 0xC8);
    static_assert(offsetof(spOctreeNodeLayout, pivot) == 0x84);
    static_assert(offsetof(spOctreeNodeLayout, rayCandidates) == 0x90);
    static_assert(offsetof(spOctreeNodeLayout, mins) == 0xB0);
    static_assert(offsetof(spOctreeNodeLayout, maxs) == 0xBC);
    static_assert(sizeof(spBSPRayCandidateObservedLayout) == 8);
    static_assert(sizeof(spBSPNodeLayout) == 0xAC);
    static_assert(offsetof(spBSPNodeLayout, plane) == 0x84);
    static_assert(offsetof(spBSPNodeLayout, rayCandidates) == 0x94);
    static_assert(offsetof(spBSPNodeLayout, polygon) == 0xA4);
    static_assert(offsetof(spBSPNodeLayout, polygonCount) == 0xA8);
    static_assert(offsetof(spZoneLayout, localRoots) == 0xB4);
    static_assert(sizeof(spPartitionSystemLayout) == 0x1D8);
    static_assert(offsetof(spPartitionSystemLayout, root) == 0x1D4);
    static_assert(offsetof(spPartitionNodeLayout, visibilityMark) == 0x7C);
    static_assert(sizeof(spVisibilityPlaneObservedLayout) == 20);
    static_assert(sizeof(spVisibilityPlaneSetObservedLayout) == 20);
    static_assert(sizeof(spOcclusionFaceObservedLayout) == 0x20);
    static_assert(sizeof(spOcclusionEdgeObservedLayout) == 0x28);
    static_assert(sizeof(spOcclusionVolumeLayout) == 0x1B8);
    static_assert(offsetof(spOcclusionVolumeLayout, partitionNodes) == 0xB4);
    static_assert(offsetof(spOcclusionVolumeLayout, localVertexBuffer) == 0xC8);
    static_assert(offsetof(spOcclusionVolumeLayout, borderPositions) == 0xE8);
    static_assert(offsetof(spOcclusionVolumeLayout, cameraFacePlanes) == 0x108);
    static_assert(offsetof(spOcclusionVolumeLayout, planar) == 0x160);
    static_assert(offsetof(spOcclusionVolumeLayout, occlusionPlanes) == 0x168);
    static_assert(offsetof(spOcclusionVolumeLayout, localSphere) == 0x194);
    static_assert(offsetof(spOcclusionVolumeLayout, initialized) == 0x1B4);
    static_assert(sizeof(spVisibilityScratchObservedLayout) == 0x2C);
    static_assert(sizeof(spPolygonVertexObservedLayout) == 0x1C);
    static_assert(offsetof(spPolygonVertexObservedLayout, position) == 0x08);
    static_assert(offsetof(spPolygonVertexObservedLayout, previous) == 0x14);
    static_assert(offsetof(spPolygonVertexObservedLayout, next) == 0x18);
    static_assert(sizeof(spVisibilityManagerLayout) == 0xAC);
    static_assert(offsetof(spVisibilityManagerLayout, visibleSupports) == 0x28);
    static_assert(offsetof(spVisibilityManagerLayout, planeStack) == 0x40);
    static_assert(offsetof(spVisibilityManagerLayout, scratch) == 0x54);
    static_assert(sizeof(spShadowVolumeManagerObservedPrefixLayout) == 0x1C);
    static_assert(sizeof(spDXShadowVolumeManagerLayout) == 0x3C);
    static_assert(offsetof(spDXShadowVolumeManagerLayout, shader28) == 0x28);
    static_assert(offsetof(spDXShadowVolumeManagerLayout, field38) == 0x38);
    static_assert(sizeof(spSceneGraphOptimizerObservedPrefixLayout) == 0x38);
    static_assert(offsetof(spSceneGraphOptimizerObservedPrefixLayout,
        callbackVTable) == 0x18);
    static_assert(offsetof(spSceneGraphOptimizerObservedPrefixLayout,
        temporaryBegin) == 0x24);
    static_assert(offsetof(spSceneGraphOptimizerObservedPrefixLayout,
        detachListHead) == 0x34);
    static_assert(sizeof(spLightObservedLayout) == 0xF0);
    static_assert(sizeof(spLightManagerLayout) == 0x24);
    static_assert(offsetof(spLightManagerLayout, ownerScene) == 0x20);
    static_assert(offsetof(spLightObservedLayout, previousInScene) == 0xB8);
    static_assert(offsetof(spLightObservedLayout, sceneLightVTable) == 0xB4);
    static_assert(offsetof(spLightObservedLayout, type) == 0xC0);
    static_assert(offsetof(spLightObservedLayout, colorRGBA) == 0xC4);
    static_assert(offsetof(spLightObservedLayout, attenuation) == 0xD4);
    static_assert(offsetof(spLightObservedLayout, intensity) == 0xD8);
    static_assert(offsetof(spLightObservedLayout, opaqueRuntimeDC) == 0xDC);
    static_assert(offsetof(spLightObservedLayout, range) == 0xE0);
    static_assert(offsetof(spLightObservedLayout, projectShadow) == 0xEC);
    static_assert(offsetof(spLightObservedLayout, enabled) == 0xED);
    static_assert(sizeof(spLightDataObservedLayout) == 0xF0);
    static_assert(sizeof(spMaterialObservedLayout) == 0x78);
    static_assert(offsetof(spMaterialObservedLayout, materialVTable) == 0x14);
    static_assert(offsetof(spMaterialObservedLayout, renderStates) == 0x18);
    static_assert(offsetof(spMaterialObservedLayout, passCount) == 0x48);
    static_assert(offsetof(spMaterialObservedLayout, passes) == 0x4C);
    static_assert(offsetof(spMaterialObservedLayout, renderOverride) == 0x6C);
    static_assert(offsetof(spMaterialObservedLayout, useVertexAlpha) == 0x6D);
    static_assert(offsetof(spMaterialObservedLayout,
        materialColorController) == 0x74);
    static_assert(sizeof(spMaterialDataObservedLayout) == 0xBC);
    static_assert(offsetof(spMaterialDataObservedLayout, diffuseRGBA) == 0x78);
    static_assert(offsetof(spMaterialDataObservedLayout, ambientRGBA) == 0x88);
    static_assert(offsetof(spMaterialDataObservedLayout, specularRGBA) == 0x98);
    static_assert(offsetof(spMaterialDataObservedLayout, emissiveRGBA) == 0xA8);
    static_assert(offsetof(spMaterialDataObservedLayout, specularPower) == 0xB8);
    static_assert(sizeof(spMaterialPassLayerObservedLayout) == 0x38);
    static_assert(offsetof(spMaterialPassLayerObservedLayout,
        finalBlendOperation) == 0x10);
    static_assert(offsetof(spMaterialPassLayerObservedLayout, layerCount) == 0x14);
    static_assert(offsetof(spMaterialPassLayerObservedLayout, layers) == 0x18);
    static_assert(sizeof(spMaterialTextureLayerObservedLayout) == 0x14);
    static_assert(offsetof(spMaterialTextureLayerObservedLayout,
        materialTexture) == 0x10);
    static_assert(sizeof(spStdLayerObservedLayout) == 0x14);
    static_assert(sizeof(spFogObservedLayout) == 0x28);
    static_assert(offsetof(spFogObservedLayout, type) == 0x14);
    static_assert(offsetof(spFogObservedLayout, density) == 0x24);
    static_assert(sizeof(spSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spAnimationSerializerLayout) == 0x4C);
    static_assert(offsetof(spAnimationSerializerLayout, valuePoolCounts) == 0x14);
    static_assert(offsetof(spAnimationSerializerLayout, valuePoolOffsets) == 0x2C);
    static_assert(offsetof(spAnimationSerializerLayout, timePoolOffset) == 0x44);
    static_assert(offsetof(spAnimationSerializerLayout, timePoolCount) == 0x48);
    static_assert(offsetof(spSerializerObservedLayout,
        serializerVTable) == 0x10);
    static_assert(sizeof(spNodeSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spRenderNodeSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spLightDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spLightSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spCameraSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spCameraDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spFogSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spMatColorControllerSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spLightControllerSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spAnimTexControllerSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spUVControllerSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spTransFunctionEvalSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spFunctionEvalSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spColorFuncEvalSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spSphereBVSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spBoxBVSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spOBBBVSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spRenderableSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spMaterialSerializerObservedLayout) == 0x3C);
    static_assert(offsetof(spMaterialSerializerObservedLayout,
        dataBlockSerializerState) == 0x14);
    static_assert(sizeof(spMaterialDataSerializerObservedLayout) == 0x3C);
    static_assert(sizeof(spDXMaterialDataSerializerObservedLayout) == 0x3C);
    static_assert(sizeof(spPS2MaterialDataSerializerObservedLayout) == 0x3C);
    static_assert(sizeof(spModelSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spSkinSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spMeshDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spPS2MeshDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spDXMeshDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spTextureDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spPS2TextureDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spDXTextureDataSerializerObservedLayout) == 0x14);
    static_assert(sizeof(spSerializerFileHeaderLayout) == 0x1C);
    static_assert(sizeof(spSerializerObjectHeaderLayout) == 0x08);
    static_assert(sizeof(spDataBlockHeaderLayout) == 0x10);
    static_assert(sizeof(spDataBlockSerializerLayout) == 0x28);
    static_assert(offsetof(spDataBlockSerializerLayout, currentHeader) == 0x0C);
    static_assert(offsetof(spDataBlockSerializerLayout, writerStream) == 0x20);
    static_assert(sizeof(spSerializerRegistrationNodeLayout) == 0x18);
    static_assert(sizeof(spSerializerRegistrationListLayout) == 0x0C);
    static_assert(sizeof(spSerializerManagerLayout) == 0x2C);
    static_assert(offsetof(spSerializerManagerLayout, platformMask) == 0x10);
    static_assert(offsetof(spSerializerManagerLayout, serializationPolicy) == 0x18);
    static_assert(offsetof(spSerializerManagerLayout, registrations) == 0x1C);
    static_assert(offsetof(spSerializerManagerLayout, fat) == 0x28);
    static_assert(sizeof(spResourceFATHelperObservedLayout) == 0x58);
    static_assert(offsetof(spResourceFATHelperObservedLayout, resourcesByID) == 0x20);
    static_assert(offsetof(spResourceFATHelperObservedLayout, resourcesByObject) == 0x2C);
    static_assert(offsetof(spResourceFATHelperObservedLayout, orderedFiles) == 0x38);
    static_assert(offsetof(spResourceFATHelperObservedLayout, orderedResources) == 0x48);
    static_assert(offsetof(spResourceFATHelperObservedLayout, resourceCursor) == 0x54);
    static_assert(sizeof(spResourceFATEntryObservedLayout) == 0x24);
    static_assert(offsetof(spResourceFATEntryObservedLayout, payloadWritten) == 0x1C);
    static_assert(offsetof(spResourceFATEntryObservedLayout, object) == 0x20);
    static_assert(sizeof(spResourceFATFileEntryObservedLayout) == 0x0C);
    static_assert(sizeof(spSerializerHookObservedLayout) == 0x10);
    static_assert(sizeof(spDXSerializerHookObservedPrefixLayout) == 0x1C);
    static_assert(offsetof(spDXSerializerHookObservedPrefixLayout,
        listSentinel) == 0x14);

    inline constexpr std::uint32_t spMeshClassID = 0x3F077B6C;
    inline constexpr std::uint32_t spNodeClassID = 0x695C0F65;
    inline constexpr std::uint32_t spRenderNodeClassID = 0x603625D0;
    inline constexpr Address32 spRenderNodeRegistration = 0x0075E150;
    inline constexpr Address32 spRenderNodeRegistrationInitializer = 0x006D2A20;
    inline constexpr Address32 spRenderNodeFactoryProtectedEntry = 0x00425520;
    inline constexpr Address32 spRenderNodeDeletingDestructor = 0x004255D0;
    inline constexpr Address32 spRenderNodeClone = 0x00425580;
    inline constexpr Address32 spRenderNodeCopy = 0x00424980;
    inline constexpr Address32 spRenderNodeRegistrationGetter = 0x00425030;
    inline constexpr Address32 spRenderNodeVTable = 0x006DCAA4;
    inline constexpr Address32 spRenderNodeSupportVTable = 0x006DCADC;
    inline constexpr Address32 spRenderNodeWorldUpdate = 0x004250F0;
    inline constexpr Address32 spRenderNodeCull = 0x00424840;
    inline constexpr Address32 spRenderNodeApplyMatrices = 0x004248D0;
    inline constexpr Address32 spRenderNodeRender = 0x00424B60;
    inline constexpr std::uint32_t spRenderNodeSize = 0x1D4;
    inline constexpr Address32 spRenderNodeClassName = 0x006DCAF4;
    inline constexpr std::uint32_t spRenderNodeRenderableBeginOffset = 0xBC;
    inline constexpr std::uint32_t spRenderNodeRenderableEndOffset = 0xC0;
    inline constexpr std::uint32_t spSceneGraphOptimizerClassID = 0x4FE639C2;
    inline constexpr Address32 spSceneGraphOptimizerRegistration = 0x00764588;
    inline constexpr Address32 spSceneGraphOptimizerRegistrationInitializer =
        0x006D5640;
    inline constexpr Address32 spSceneGraphOptimizerDeletingDestructor =
        0x004C1D80;
    inline constexpr Address32 spSceneGraphOptimizerRegistrationGetter =
        0x004C1CC0;
    inline constexpr Address32 spSceneGraphOptimizerOptimize = 0x004C1900;
    inline constexpr Address32 spSceneGraphOptimizerOptimizeNode = 0x004C19D0;
    inline constexpr Address32 spSceneGraphOptimizerVTable = 0x006F20BC;
    inline constexpr Address32 spSceneGraphOptimizerSingletonVTable = 0x006F20B8;
    inline constexpr std::uint32_t spDXSceneGraphOptimizerClassID = 0x7E120EC3;
    inline constexpr Address32 spDXSceneGraphOptimizerRegistration = 0x007643A8;
    inline constexpr Address32 spDXSceneGraphOptimizerRegistrationInitializer =
        0x006D5520;
    inline constexpr Address32 spDXSceneGraphOptimizerFactoryProtectedEntry =
        0x004C0050;
    inline constexpr Address32 spDXSceneGraphOptimizerDeletingDestructor =
        0x004BFF50;
    inline constexpr Address32 spDXSceneGraphOptimizerRegistrationGetter =
        0x004BFE70;
    inline constexpr Address32 spDXSceneGraphOptimizerVTable = 0x006F1E2C;
    inline constexpr Address32 spDXSceneGraphOptimizerCallbackVTable = 0x006F1E14;
    inline constexpr Address32 spDXSceneGraphOptimizerSingletonVTable = 0x006F1E28;
    inline constexpr Address32 spDXSceneGraphOptimizerGetCombinedVBProtectedEntry =
        0x004BEDF0;
    inline constexpr Address32 spDXSceneGraphOptimizerOnNode = 0x004BEEE0;
    inline constexpr Address32 spDXSceneGraphOptimizerOnEndProtectedEntry =
        0x004BF9A0;
    inline constexpr Address32 spDXSceneGraphOptimizerOnEndAdjustor = 0x004BFA10;
    inline constexpr Address32 spDXSceneGraphOptimizerDestructorProtectedEntry =
        0x004BFD90;
    inline constexpr Address32 spDXSceneGraphOptimizerForwardModelMesh = 0x004C0100;
    inline constexpr Address32 spDXSceneGraphOptimizerAddMeshProtectedEntry =
        0x004BFF70;
    inline constexpr Address32 spDXSceneGraphOptimizerCombineDataProtectedEntry =
        0x004C0540;
    inline constexpr std::uint32_t spDXSceneGraphOptimizerObservedPrefixSize = 0x54;
    inline constexpr std::uint32_t spRendererClassID = 0x2D9C0296;
    inline constexpr Address32 spRendererRegistration = 0x0075F8F0;
    inline constexpr Address32 spRendererRegistrationInitializer = 0x006D35E0;
    inline constexpr Address32 spRendererRegistrationGetter = 0x004562F0;
    inline constexpr Address32 spRendererDestructor = 0x004561E0;
    inline constexpr Address32 spRendererDeletingDestructor = 0x004568F0;
    inline constexpr Address32 spRendererPrimaryVTable = 0x006E6F78;
    inline constexpr Address32 spRendererSupportVTable = 0x006E6F74;
    inline constexpr Address32 spRendererInterfaceVTable = 0x006E6F00;
    inline constexpr Address32 spRendererInvalidateStateCaches = 0x00454940;
    inline constexpr Address32 spRendererScratchVertexBuffer = 0x004548C0;
    inline constexpr std::uint32_t spRendererAllocationSize = 0xCA08;
    inline constexpr std::uint32_t spRendererInterfaceSlotCount = 29;
    inline constexpr std::uint32_t spRendererRenderStateCount = 12;
    inline constexpr std::uint32_t spRendererTextureStateCount = 72;
    inline constexpr std::uint32_t spDXRendererClassID = 0x46004EE1;
    inline constexpr Address32 spDXRendererRegistration = 0x007639C0;
    inline constexpr Address32 spDXRendererRegistrationInitializer = 0x006D50D0;
    inline constexpr Address32 spDXRendererRegistrationGetter = 0x004AE350;
    inline constexpr Address32 spDXRendererConstructorProtectedEntry = 0x004AE370;
    inline constexpr Address32 spDXRendererDestructor = 0x004AE170;
    inline constexpr Address32 spDXRendererDeletingDestructor = 0x004AE610;
    inline constexpr Address32 spDXRendererPrimaryVTable = 0x006EFA40;
    inline constexpr Address32 spDXRendererSupportVTable = 0x006EFA3C;
    inline constexpr Address32 spDXRendererInterfaceVTable = 0x006EF9C8;
    inline constexpr std::uint32_t spDXRendererAllocationSize = 0xF368;
    inline constexpr std::uint32_t spPCRendererClassID = 0x26267C84;
    inline constexpr Address32 spPCRendererRegistration = 0x00764A10;
    inline constexpr Address32 spPCRendererRegistrationInitializer = 0x006D58B0;
    inline constexpr Address32 spPCRendererRegistrationGetter = 0x004C5A70;
    inline constexpr Address32 spPCRendererConstructorProtectedEntry = 0x004C5A10;
    inline constexpr Address32 spPCRendererFactory = 0x004C5AB0;
    inline constexpr Address32 spPCRendererDestructor = 0x004C5A80;
    inline constexpr Address32 spPCRendererDeletingDestructor = 0x004C5B60;
    inline constexpr Address32 spPCRendererClone = 0x004C5B10;
    inline constexpr Address32 spPCRendererPrimaryVTable = 0x006F2918;
    inline constexpr Address32 spPCRendererSupportVTable = 0x006F2914;
    inline constexpr Address32 spPCRendererInterfaceVTable = 0x006F28A0;
    inline constexpr std::uint32_t spPCRendererAllocationSize = 0xF368;
    inline constexpr Address32
        spPCRendererInterfaceTargets[spRendererInterfaceSlotCount]{
            0x004BC160, 0x004BC0D0, 0x004A1BF0, 0x004BB950,
            0x004BB9D0, 0x004BB980, 0x004BB9F0, 0x004BD380,
            0x004BD810, 0x004BC670, 0x004AD7A0, 0x004BC9A0,
            0x004BBAE0, 0x004BBB20, 0x004BBB60, 0x004AD330,
            0x004AD350, 0x004AD360, 0x004AD370, 0x004AD640,
            0x004AD660, 0x004AD680, 0x004BBA80, 0x004BB590,
            0x004BB4B0, 0x004AD380, 0x004AD390, 0x004AD4D0,
            0x004BD700,
        };
    // Analytical operation names, confirmed by camera/material callers and
    // the IDirect3DDevice9 endpoints reached by the concrete PC bodies.
    inline constexpr std::uint32_t spPCRendererBindCubeRenderTargetSlot = 0;
    inline constexpr std::uint32_t spPCRendererBindRenderTargetSlot = 1;
    inline constexpr std::uint32_t spPCRendererBeginSceneSlot = 3;
    inline constexpr std::uint32_t spPCRendererEndSceneSlot = 4;
    inline constexpr std::uint32_t spPCRendererClearSlot = 5;
    inline constexpr std::uint32_t spPCRendererSubmitMeshSlot = 9;
    inline constexpr Address32 spPCRendererSubmitMesh = 0x004BC670;
    inline constexpr Address32 spPCRendererSubmitMeshProtectedBridge =
        0x004BC4A0;
    inline constexpr Address32 spPCRendererDrawWrapper = 0x004BC290;
    inline constexpr std::uint32_t spPCRendererConfigure2DSlot = 10;
    inline constexpr std::uint32_t spPCRendererSetProjectionMatrixSlot = 12;
    inline constexpr std::uint32_t spPCRendererSetViewMatrixSlot = 13;
    inline constexpr std::uint32_t spPCRendererSetWorldMatrixSlot = 14;
    inline constexpr std::uint32_t spPCRendererSetViewportSlot = 22;
    inline constexpr std::uint32_t spPCRendererSetTextureTransformSlot = 23;
    inline constexpr Address32 spPCRendererSetTextureTransform = 0x004BB590;
    inline constexpr std::uint32_t spPCRendererSetFogSlot = 26;
    inline constexpr Address32 spPCRendererSetFog = 0x004AD390;
    inline constexpr std::uint32_t spRenderTargetClassID = 0x00D1229C;
    inline constexpr Address32 spRenderTargetRegistration = 0x007640E0;
    inline constexpr Address32 spRenderTargetRegistrationInitializer = 0x006D547D;
    inline constexpr std::uint32_t spCubeRenderTargetClassID = 0x0F8B095F;
    inline constexpr Address32 spCubeRenderTargetRegistration = 0x00764080;
    inline constexpr Address32 spCubeRenderTargetRegistrationInitializer = 0x006D544D;
    inline constexpr std::uint32_t spDXRenderTargetClassID = 0x189A4642;
    inline constexpr Address32 spDXRenderTargetRegistration = 0x007652B8;
    inline constexpr Address32 spDXRenderTargetRegistrationInitializer = 0x006D5D7D;
    inline constexpr Address32 spDXRenderTargetInit = 0x004CDC70;
    inline constexpr Address32 spDXRenderTargetReinitTargetsForDeviceReset = 0x004CDBE0;
    inline constexpr Address32 spDXRenderTargetReleaseTargetsForDeviceReset = 0x004CDD60;
    inline constexpr Address32 spDXRenderTargetInterfaceVTable = 0x006F33F8;
    inline constexpr std::uint32_t spDXCubeRenderTargetClassID = 0x5249684C;
    inline constexpr Address32 spDXCubeRenderTargetRegistration = 0x00763528;
    inline constexpr Address32 spDXCubeRenderTargetRegistrationInitializer = 0x006D5030;
    inline constexpr Address32 spDXCubeRenderTargetFactoryProtectedEntry = 0x004AC3B0;
    inline constexpr Address32 spDXCubeRenderTargetInit = 0x004AC580;
    inline constexpr Address32 spDXCubeRenderTargetReinitTargetsForDeviceReset = 0x004AC4F0;
    inline constexpr Address32 spDXCubeRenderTargetReleaseTargetsForDeviceReset = 0x004AC2A0;
    inline constexpr Address32 spDXCubeRenderTargetInterfaceVTable = 0x006EF80C;
    inline constexpr std::uint32_t spPCRenderTargetClassID = 0x0C681FC8;
    inline constexpr Address32 spPCRenderTargetRegistration = 0x00764A70;
    inline constexpr Address32 spPCRenderTargetRegistrationInitializer = 0x006D5900;
    inline constexpr Address32 spPCRenderTargetFactoryProtectedEntry = 0x004C5BB0;
    inline constexpr std::uint32_t spRenderTargetManagerClassID = 0x546C50F2;
    inline constexpr Address32 spRenderTargetManagerRegistration = 0x0075FDC0;
    inline constexpr Address32 spRenderTargetManagerRegistrationInitializer = 0x006D377D;
    inline constexpr Address32 spRenderTargetManagerReleaseTargetsForDeviceReset = 0x0045CD10;
    inline constexpr Address32 spRenderTargetManagerReinitTargetsForDeviceReset = 0x0045CE70;
    inline constexpr Address32 spRenderTargetManagerDeactivateOrdinaryTarget = 0x0045CBC0;
    inline constexpr Address32 spRenderTargetManagerDeactivateCubeTarget = 0x0045CBF0;
    inline constexpr std::uint32_t spPCRenderTargetManagerClassID = 0x165C006F;
    inline constexpr Address32 spPCRenderTargetManagerRegistration = 0x007648F0;
    inline constexpr Address32 spPCRenderTargetManagerRegistrationInitializer = 0x006D5840;
    inline constexpr Address32 spPCRenderTargetManagerFactoryProtectedEntry = 0x004C4970;
    inline constexpr std::uint32_t spMaterialTextureClassID = 0x694E6975;
    inline constexpr Address32 spMaterialTextureRegistration = 0x00760400;
    inline constexpr Address32 spMaterialTextureRegistrationInitializer = 0x006D3A90;
    inline constexpr Address32 spMaterialTextureFactoryProtectedEntry = 0x00467F30;
    // Exact ordinary family: destructor 467CF0 writes this vtable; slot7
    // getter467BE0 returns +34. Static identity requalified for the Remix source.
    inline constexpr Address32 spMaterialTextureVTable = 0x006E8440;
    inline constexpr Address32 spMaterialTextureGetTexture = 0x00467BE0;
    inline constexpr std::uint32_t spMaterialTextureStateCount = 9;
    inline constexpr std::uint32_t spMaterialRenderTargetTextureClassID = 0x535D1473;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistration = 0x007626D0;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistrationInitializer = 0x006D47C0;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistrationGetter = 0x0048E490;
    inline constexpr Address32 spMaterialRenderTargetTextureConstructorProtectedEntry = 0x0048E4A0;
    inline constexpr Address32 spMaterialRenderTargetTextureDeletingDestructor = 0x0048E610;
    inline constexpr Address32 spMaterialRenderTargetTextureGetTexture = 0x0048DE90;
    inline constexpr Address32 spMaterialRenderTargetTextureReleaseTargets = 0x0048DED0;
    inline constexpr Address32 spMaterialRenderTargetTextureReinitTargets = 0x0048DF10;
    inline constexpr Address32 spMaterialRenderTargetTextureVTable = 0x006EC4BC;
    inline constexpr std::uint32_t spMaterialCameraViewTextureClassID = 0x34EF51B9;
    inline constexpr Address32 spMaterialCameraViewTextureRegistration = 0x007609F8;
    inline constexpr Address32 spMaterialCameraViewTextureRegistrationInitializer = 0x006D3D30;
    inline constexpr Address32 spMaterialCameraViewTextureFactoryProtectedEntry = 0x00478090;
    inline constexpr std::uint32_t spMaterialCubeMapTextureClassID = 0x1C3B499A;
    inline constexpr Address32 spMaterialCubeMapTextureRegistration = 0x00764648;
    inline constexpr Address32 spMaterialCubeMapTextureRegistrationInitializer = 0x006D56A0;
    inline constexpr Address32 spMaterialCubeMapTextureFactory = 0x004C2430;
    inline constexpr std::uint32_t spMaterialCubeMapTextureAllocationSize = 0x9C;
    inline constexpr std::uint32_t spMaterialLayerRenderTargetFieldID = 13;
    inline constexpr std::uint32_t spMaterialLayerCameraFieldID = 14;
    inline constexpr std::uint32_t spMaterialLayerCubeMapFieldID = 15;
    inline constexpr std::uint32_t spMaterialLayerMovieFieldID = 16;
    inline constexpr std::uint32_t spSerializerClassID = 0x42429877;
    inline constexpr Address32 spSerializerRegistration = 0x007602E0;
    inline constexpr Address32 spSerializerRegistrationInitializer = 0x006D3A00;
    inline constexpr Address32 spSerializerConstructor = 0x004063F0;
    inline constexpr Address32 spSerializerDestructor = 0x004671B0;
    inline constexpr Address32 spSerializerDeletingDestructor = 0x00467240;
    inline constexpr Address32 spSerializerRegistrationGetter = 0x004671D0;
    inline constexpr Address32 spSerializerResolveClassID = 0x004671E0;
    inline constexpr Address32 spSerializerReadObjectHeader = 0x00467550;
    inline constexpr Address32 spSerializerIndexObject = 0x004672C0;
    inline constexpr Address32 spSerializerVTable = 0x006E81C0;
    inline constexpr Address32 spSerializerInterfaceVTable = 0x006E81B4;
    inline constexpr Address32 spDataBlockSerializerSelectSizeCode = 0x00472730;
    inline constexpr Address32 spDataBlockSerializerReadHeader = 0x004728F0;
    inline constexpr Address32 spDataBlockSerializerSkipData = 0x00472AC0;
    inline constexpr Address32 spDataBlockSerializerWriteTerminator = 0x00472B00;
    inline constexpr Address32 spDataBlockSerializerWriteField = 0x00472B40;
    inline constexpr Address32 spDataBlockSerializerConstructor = 0x00473000;
    inline constexpr Address32 spDataBlockSerializerBeginObject = 0x00472710;
    inline constexpr Address32 spDataBlockSerializerWriteBegin = 0x00472D30;
    inline constexpr Address32 spDataBlockSerializerWriteBeginResolvedTail = 0x0044EB66;
    inline constexpr Address32 spDataBlockSerializerWriteEnd = 0x00472E20;
    inline constexpr Address32 spDataBlockSerializerWriteHeaderResolved = 0x004F5AD0;
    // Save-reference protocol and unnamed FAT helper, independently executed
    // on PC. These addresses do not imply a complete FFPS file-save entry.
    inline constexpr Address32 spSerializerWriteObjectHeader = 0x00467260;
    inline constexpr Address32 spSerializerIndexResource = 0x004672C0;
    inline constexpr Address32 spSerializerIndexReference = 0x00467300;
    inline constexpr Address32 spSerializerWriteReference = 0x00467350;
    inline constexpr Address32 spSerializerReadReference = 0x004678B0;
    inline constexpr Address32 spSerializerResolveReadReference = 0x00467670;
    inline constexpr Address32 spResourceFATFindByIDPC = 0x004664C0;
    inline constexpr Address32 spResourceFATIndexObjectPC = 0x00466FA0;
    inline constexpr Address32 spResourceFATSaveEntryConstructorPC = 0x00465BF0;
    inline constexpr Address32 spResourceFATSaveEntryConstructorResolvedPC = 0x0047DD80;
    inline constexpr Address32 spDataBlockSerializerSourcePath = 0x006E93D8;
    inline constexpr std::uint32_t spDataBlockSerializerObservedSize = 0x28;
    inline constexpr std::uint32_t spSerializerHookClassID = 0x18092F8D;
    inline constexpr Address32 spSerializerHookRegistration = 0x00763D20;
    inline constexpr Address32 spSerializerHookRegistrationInitializer =
        0x006D5280;
    inline constexpr std::uint32_t spDXSerializerHookClassID = 0x0D832A30;
    inline constexpr Address32 spDXSerializerHookRegistration = 0x007631B0;
    inline constexpr Address32 spDXSerializerHookRegistrationInitializer =
        0x006D4D10;
    inline constexpr Address32 spDXSerializerHookFactoryProtectedEntry =
        0x004AA430;
    inline constexpr Address32 spDXSerializerHookFactoryThunkSlot = 0x013B2D98;
    inline constexpr Address32 spDXSerializerHookRegistrationGetter = 0x004AA3A0;
    inline constexpr Address32 spDXSerializerHookDestructor = 0x004AA370;
    inline constexpr Address32 spDXSerializerHookDeletingDestructor = 0x004AA410;
    inline constexpr Address32 spDXSerializerHookClone = 0x004AA490;
    inline constexpr Address32 spDXSerializerHookReadMeshInfo = 0x004AA4E0;
    inline constexpr Address32 spDXSerializerHookBatchAndLoad = 0x004AA870;
    inline constexpr Address32 spDXSerializerHookPlatformSlot = 0x004AAB80;
    inline constexpr Address32 spDXSerializerHookVTable = 0x006EF3C8;
    inline constexpr Address32 spDXSerializerHookClassName = 0x006EF6BC;
    inline constexpr Address32 spDXSerializerHookSourcePath = 0x006EF300;
    inline constexpr Address32 spDXSerializerHookActiveSharedMesh = 0x00763148;
    inline constexpr std::uint32_t spDXSerializerHookObservedPrefixSize = 0x1C;
    inline constexpr std::uint32_t spDXSerializerHookMaximumVertexCount = 0x4E20;
    inline constexpr std::uint32_t spSerializerManagerClassID = 0xE422E9EB;
    inline constexpr std::uint32_t spSerializerManagerRegisteredBaseClassID =
        0x415352A1;
    inline constexpr Address32 spSerializerManagerRegistration = 0x0075DDF0;
    inline constexpr Address32 spSerializerManagerRegistrationInitializer =
        0x006D2840;
    inline constexpr Address32 spSerializerManagerSingleton = 0x0075DDE8;
    inline constexpr Address32 spSerializerManagerValidateFileHeader = 0x00422260;
    inline constexpr Address32 spSerializerManagerLoadSceneGraph = 0x00422550;
    inline constexpr Address32 spSerializerManagerLoadGenericResource = 0x00422B50;
    inline constexpr Address32 spSerializerManagerMaterializeResources = 0x00422940;
    inline constexpr Address32 spSerializerManagerLookupClassID = 0x004224F0;
    inline constexpr Address32 spSerializerManagerLookupResolvedBody = 0x0042C9F0;
    inline constexpr Address32 spSerializerManagerClearRegistrations = 0x004228A0;
    inline constexpr Address32 spSerializerManagerRegisterSerializer = 0x00422D90;
    inline constexpr Address32 spSerializerManagerProtectedConstructorEntry =
        0x00422E00;
    inline constexpr Address32 spSerializerManagerRegistrationGetter = 0x00422EA0;
    inline constexpr Address32 spSerializerManagerDestructor = 0x00422EB0;
    inline constexpr Address32 spSerializerManagerFactoryProtectedEntry =
        0x00422F40;
    inline constexpr Address32 spSerializerManagerClone = 0x00422FA0;
    inline constexpr Address32 spSerializerManagerDeletingDestructor = 0x00422FF0;
    inline constexpr Address32 spSerializerManagerVTable = 0x006DC838;
    inline constexpr std::uint32_t spSerializerManagerAllocationSize = 0x2C;
    inline constexpr Address32 spResourceFATHelperLoadFileIndex = 0x00465CD0;
    inline constexpr Address32 spResourceFATHelperFirstEntry = 0x00465F00;
    inline constexpr Address32 spResourceFATHelperNextEntry = 0x00465F20;
    inline constexpr Address32 spResourceFATHelperClearResources = 0x00466760;
    inline constexpr Address32 spResourceFATHelperClearFiles = 0x00466870;
    inline constexpr Address32 spResourceFATHelperLoadFileIndexResolvedBody = 0x013BCA90;
    inline constexpr Address32 spResourceFATEntryDeletingDestructor = 0x00465CA0;
    inline constexpr Address32 spResourceFATFileEntryDeletingDestructor = 0x00465C70;
    inline constexpr Address32 spResourceFATHelperLoadIndex = 0x00466B90;
    inline constexpr Address32 spResourceFATApplyName = 0x004671F0;
    inline constexpr std::uint32_t spNodeSerializerClassID = 0x4545848A;
    inline constexpr Address32 spNodeSerializerRegistration = 0x007601C0;
    inline constexpr Address32 spNodeSerializerRegistrationInitializer = 0x006D3970;
    inline constexpr Address32 spNodeSerializerFactoryProtectedEntry = 0x004638F0;
    inline constexpr Address32 spNodeSerializerDestructor = 0x004638C0;
    inline constexpr Address32 spNodeSerializerDeletingDestructor = 0x004639B0;
    inline constexpr Address32 spNodeSerializerClone = 0x00463960;
    inline constexpr Address32 spNodeSerializerRegistrationGetter = 0x004638B0;
    inline constexpr Address32 spNodeSerializerTargetClassID = 0x004638E0;
    inline constexpr Address32 spNodeSerializerFinalizeRelationships = 0x004639D0;
    inline constexpr Address32 spNodeSerializerRead = 0x00463A70;
    inline constexpr Address32 spNodeSerializerWrite = 0x00463F10;
    inline constexpr Address32 spNodeSerializerVTable = 0x006E7850;
    inline constexpr Address32 spNodeSerializerInterfaceVTable = 0x006E7844;
    inline constexpr std::uint32_t spRenderNodeSerializerClassID = 0x66EF6060;
    inline constexpr std::uint32_t spRenderNodeSerializerTargetClassIDValue =
        0x603625D0;
    inline constexpr Address32 spRenderNodeSerializerRegistration = 0x007604C0;
    inline constexpr Address32 spRenderNodeSerializerRegistrationInitializer =
        0x006D3AF0;
    inline constexpr Address32 spRenderNodeSerializerFactoryProtectedEntry =
        0x00469040;
    inline constexpr Address32 spRenderNodeSerializerDestructor = 0x00469010;
    inline constexpr Address32 spRenderNodeSerializerDeletingDestructor =
        0x00469100;
    inline constexpr Address32 spRenderNodeSerializerClone = 0x004690B0;
    inline constexpr Address32 spRenderNodeSerializerRegistrationGetter =
        0x00469000;
    inline constexpr Address32 spRenderNodeSerializerTargetClassID = 0x00469030;
    inline constexpr Address32 spRenderNodeSerializerIndexRelationships =
        0x00469120;
    inline constexpr Address32 spRenderNodeSerializerRead = 0x00469190;
    inline constexpr Address32 spRenderNodeSerializerWrite = 0x00469340;
    inline constexpr Address32 spRenderNodeSerializerVTable = 0x006E8A44;
    inline constexpr Address32 spRenderNodeSerializerInterfaceVTable = 0x006E8A38;
    inline constexpr std::uint32_t spRenderNodeSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spCameraSerializerClassID = 0x440E53FB;
    inline constexpr std::uint32_t spCameraSerializerTargetClassIDValue = 0x18DF3845;
    inline constexpr Address32 spCameraSerializerRegistration = 0x0075EBE8;
    inline constexpr Address32 spCameraSerializerRegistrationInitializer = 0x006D2F70;
    inline constexpr Address32 spCameraSerializerFactoryProtectedEntry = 0x0043D230;
    inline constexpr Address32 spCameraSerializerDestructor = 0x0043D200;
    inline constexpr Address32 spCameraSerializerDeletingDestructor = 0x0043D2F0;
    inline constexpr Address32 spCameraSerializerClone = 0x0043D2A0;
    inline constexpr Address32 spCameraSerializerRegistrationGetter = 0x0043D1F0;
    inline constexpr Address32 spCameraSerializerTargetClassID = 0x0043D220;
    inline constexpr Address32 spCameraSerializerFinalizeRelationships = 0x004639D0;
    inline constexpr Address32 spCameraSerializerRead = 0x0043D310;
    inline constexpr Address32 spCameraSerializerWriteProtectedEntry = 0x0043D610;
    inline constexpr Address32 spCameraSerializerVTable = 0x006E0780;
    inline constexpr Address32 spCameraSerializerInterfaceVTable = 0x006E0774;
    inline constexpr std::uint32_t spCameraSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spCameraDataSerializerClassID = 0x759F1687;
    inline constexpr std::uint32_t spCameraDataSerializerTargetClassIDValue = 0x18DF3845;
    inline constexpr Address32 spCameraDataSerializerRegistration = 0x0075EDD0;
    inline constexpr Address32 spCameraDataSerializerRegistrationInitializer = 0x006D3060;
    inline constexpr Address32 spCameraDataSerializerFactoryProtectedEntry = 0x00441040;
    inline constexpr Address32 spCameraDataSerializerDestructor = 0x00440FF0;
    inline constexpr Address32 spCameraDataSerializerDeletingDestructor = 0x00441100;
    inline constexpr Address32 spCameraDataSerializerClone = 0x004410B0;
    inline constexpr Address32 spCameraDataSerializerRegistrationGetter = 0x00440FE0;
    inline constexpr Address32 spCameraDataSerializerTargetClassID = 0x00441010;
    inline constexpr Address32 spCameraDataSerializerLoadSignature = 0x00441120;
    inline constexpr Address32 spCameraDataSerializerReadWrapper = 0x00441020;
    inline constexpr Address32 spCameraDataSerializerWriteWrapper = 0x00441180;
    inline constexpr Address32 spCameraDataSerializerVTable = 0x006E1FC8;
    inline constexpr Address32 spCameraDataSerializerInterfaceVTable = 0x006E1FBC;
    inline constexpr std::uint32_t spCameraDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spFogSerializerClassID = 0x576A70CA;
    inline constexpr std::uint32_t spFogSerializerTargetClassIDValue = 0x7AC95AEC;
    inline constexpr Address32 spFogSerializerRegistration = 0x0075EAC8;
    inline constexpr Address32 spFogSerializerRegistrationInitializer = 0x006D2F00;
    inline constexpr Address32 spFogSerializerFactoryProtectedEntry = 0x0043B830;
    inline constexpr Address32 spFogSerializerDestructor = 0x0043B800;
    inline constexpr Address32 spFogSerializerDeletingDestructor = 0x0043B8F0;
    inline constexpr Address32 spFogSerializerClone = 0x0043B8A0;
    inline constexpr Address32 spFogSerializerRegistrationGetter = 0x0043B7F0;
    inline constexpr Address32 spFogSerializerTargetClassID = 0x0043B820;
    inline constexpr Address32 spFogSerializerRead = 0x0043B910;
    inline constexpr Address32 spFogSerializerWrite = 0x0043BCF0;
    inline constexpr Address32 spFogSerializerVTable = 0x006DFB5C;
    inline constexpr Address32 spFogSerializerInterfaceVTable = 0x006DFB50;
    inline constexpr std::uint32_t spFogSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spMatColorControllerSerializerClassID = 0x0F881A36;
    inline constexpr std::uint32_t spMatColorControllerSerializerTargetClassIDValue = 0x4C633E85;
    inline constexpr Address32 spMatColorControllerSerializerRegistration = 0x0075EE30;
    inline constexpr Address32 spMatColorControllerSerializerRegistrationInitializer = 0x006D30B0;
    inline constexpr Address32 spMatColorControllerSerializerFactoryProtectedEntry = 0x00441200;
    inline constexpr Address32 spMatColorControllerSerializerDestructor = 0x004411D0;
    inline constexpr Address32 spMatColorControllerSerializerDeletingDestructor = 0x004412C0;
    inline constexpr Address32 spMatColorControllerSerializerClone = 0x00441270;
    inline constexpr Address32 spMatColorControllerSerializerRegistrationGetter = 0x004411C0;
    inline constexpr Address32 spMatColorControllerSerializerTargetClassID = 0x004411F0;
    inline constexpr Address32 spMatColorControllerSerializerRead = 0x004412E0;
    inline constexpr Address32 spMatColorControllerSerializerWrite = 0x00441740;
    inline constexpr Address32 spMatColorControllerSerializerVTable = 0x006E2064;
    inline constexpr Address32 spMatColorControllerSerializerInterfaceVTable = 0x006E2058;
    inline constexpr std::uint32_t spMatColorControllerSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spLightControllerSerializerClassID = 0x70573E5E;
    inline constexpr std::uint32_t spLightControllerSerializerTargetClassIDValue = 0x10262533;
    inline constexpr std::uint32_t spLightControllerSerializerLightClassID = 0x72444900;
    inline constexpr Address32 spLightControllerSerializerRegistration = 0x0075EB88;
    inline constexpr Address32 spLightControllerSerializerRegistrationInitializer = 0x006D2F60;
    inline constexpr Address32 spLightControllerSerializerFactoryProtectedEntry = 0x0043C890;
    inline constexpr Address32 spLightControllerSerializerDestructor = 0x0043C860;
    inline constexpr Address32 spLightControllerSerializerDeletingDestructor = 0x0043C950;
    inline constexpr Address32 spLightControllerSerializerClone = 0x0043C900;
    inline constexpr Address32 spLightControllerSerializerRegistrationGetter = 0x0043C850;
    inline constexpr Address32 spLightControllerSerializerTargetClassID = 0x0043C880;
    inline constexpr Address32 spLightControllerSerializerRead = 0x0043C9B0;
    inline constexpr Address32 spLightControllerSerializerFinalizeRelationships = 0x0043C970;
    inline constexpr Address32 spLightControllerSerializerWrite = 0x0043CC90;
    inline constexpr Address32 spLightControllerSerializerVTable = 0x006E0198;
    inline constexpr Address32 spLightControllerSerializerInterfaceVTable = 0x006E018C;
    inline constexpr std::uint32_t spLightControllerSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spLightControllerSerializerDefaultColorARGB = 0xFF000000;
    inline constexpr std::uint32_t spAnimTexControllerSerializerClassID = 0x77793754;
    inline constexpr std::uint32_t spAnimTexControllerSerializerTargetClassIDValue = 0x16FB0E47;
    inline constexpr std::uint32_t spAnimTexControllerSerializerTextureClassID = 0x2F281E13;
    inline constexpr Address32 spAnimTexControllerSerializerRegistration = 0x0075EB28;
    inline constexpr Address32 spAnimTexControllerSerializerRegistrationInitializer = 0x006D2F30;
    inline constexpr Address32 spAnimTexControllerSerializerFactoryProtectedEntry = 0x0043C0C0;
    inline constexpr Address32 spAnimTexControllerSerializerDestructor = 0x0043C090;
    inline constexpr Address32 spAnimTexControllerSerializerDeletingDestructor = 0x0043C180;
    inline constexpr Address32 spAnimTexControllerSerializerClone = 0x0043C130;
    inline constexpr Address32 spAnimTexControllerSerializerRegistrationGetter = 0x0043C080;
    inline constexpr Address32 spAnimTexControllerSerializerTargetClassID = 0x0043C0B0;
    inline constexpr Address32 spAnimTexControllerSerializerRead = 0x0043C470;
    inline constexpr Address32 spAnimTexControllerSerializerReadTrack = 0x0043C2C0;
    inline constexpr Address32 spAnimTexControllerSerializerFinalizeRelationships = 0x0043C1A0;
    inline constexpr Address32 spAnimTexControllerSerializerWrite = 0x0043C5D0;
    inline constexpr Address32 spAnimTexControllerSerializerWriteTrack = 0x0043C200;
    inline constexpr Address32 spAnimTexControllerSerializerVTable = 0x006DFE20;
    inline constexpr Address32 spAnimTexControllerSerializerInterfaceVTable = 0x006DFE14;
    inline constexpr std::uint32_t spAnimTexControllerSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spUVControllerSerializerClassID = 0x591224D0;
    inline constexpr std::uint32_t spUVControllerSerializerTargetClassIDValue = 0x1C0053D6;
    inline constexpr Address32 spUVControllerSerializerRegistration = 0x0075ED70;
    inline constexpr Address32 spUVControllerSerializerRegistrationInitializer = 0x006D3050;
    inline constexpr Address32 spUVControllerSerializerFactoryProtectedEntry = 0x00440B00;
    inline constexpr Address32 spUVControllerSerializerDestructor = 0x00440AD0;
    inline constexpr Address32 spUVControllerSerializerDeletingDestructor = 0x00440BC0;
    inline constexpr Address32 spUVControllerSerializerClone = 0x00440B70;
    inline constexpr Address32 spUVControllerSerializerRegistrationGetter = 0x00440AC0;
    inline constexpr Address32 spUVControllerSerializerTargetClassID = 0x00440AF0;
    inline constexpr Address32 spUVControllerSerializerRead = 0x00440BE0;
    inline constexpr Address32 spUVControllerSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spUVControllerSerializerWrite = 0x00440D70;
    inline constexpr Address32 spUVControllerSerializerVTable = 0x006E1E6C;
    inline constexpr Address32 spUVControllerSerializerInterfaceVTable = 0x006E1E60;
    inline constexpr std::uint32_t spUVControllerSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerClassID = 0x2AE96657;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerTargetClassIDValue = 0x491432F0;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistration = 0x00761278;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistrationInitializer = 0x006D4170;
    inline constexpr Address32 spTransFunctionEvalSerializerFactoryProtectedEntry = 0x0047DAE0;
    inline constexpr Address32 spTransFunctionEvalSerializerDestructor = 0x0047DAB0;
    inline constexpr Address32 spTransFunctionEvalSerializerDeletingDestructor = 0x0047DBA0;
    inline constexpr Address32 spTransFunctionEvalSerializerClone = 0x0047DB50;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistrationGetter = 0x0047DAA0;
    inline constexpr Address32 spTransFunctionEvalSerializerTargetClassID = 0x0047DAD0;
    inline constexpr Address32 spTransFunctionEvalSerializerRead = 0x0047DBC0;
    inline constexpr Address32 spTransFunctionEvalSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spTransFunctionEvalSerializerWrite = 0x0047DE80;
    inline constexpr Address32 spTransFunctionEvalSerializerVTable = 0x006EACC0;
    inline constexpr Address32 spTransFunctionEvalSerializerInterfaceVTable = 0x006EACB4;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spFunctionEvalSerializerClassID = 0x1D2A151D;
    inline constexpr std::uint32_t spFunctionEvalSerializerTargetClassIDValue = 0x9450E590;
    inline constexpr Address32 spFunctionEvalSerializerRegistration = 0x00761338;
    inline constexpr Address32 spFunctionEvalSerializerRegistrationInitializer = 0x006D41B0;
    inline constexpr Address32 spFunctionEvalSerializerFactoryProtectedEntry = 0x0047ED20;
    inline constexpr Address32 spFunctionEvalSerializerDestructor = 0x0047ECF0;
    inline constexpr Address32 spFunctionEvalSerializerDeletingDestructor = 0x0047EDE0;
    inline constexpr Address32 spFunctionEvalSerializerClone = 0x0047ED90;
    inline constexpr Address32 spFunctionEvalSerializerRegistrationGetter = 0x0047ECE0;
    inline constexpr Address32 spFunctionEvalSerializerTargetClassID = 0x0047ED10;
    inline constexpr Address32 spFunctionEvalSerializerRead = 0x0047EE00;
    inline constexpr Address32 spFunctionEvalSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spFunctionEvalSerializerWrite = 0x0047F220;
    inline constexpr Address32 spFunctionEvalSerializerVTable = 0x006EB458;
    inline constexpr Address32 spFunctionEvalSerializerInterfaceVTable = 0x006EB44C;
    inline constexpr std::uint32_t spFunctionEvalSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spColorFuncEvalSerializerClassID = 0x2CC46B90;
    inline constexpr std::uint32_t spColorFuncEvalSerializerTargetClassIDValue = 0x0BC70FE7;
    inline constexpr Address32 spColorFuncEvalSerializerRegistration = 0x007612D8;
    inline constexpr Address32 spColorFuncEvalSerializerRegistrationInitializer = 0x006D4180;
    inline constexpr Address32 spColorFuncEvalSerializerFactoryProtectedEntry = 0x0047E240;
    inline constexpr Address32 spColorFuncEvalSerializerDestructor = 0x0047E210;
    inline constexpr Address32 spColorFuncEvalSerializerDeletingDestructor = 0x0047E300;
    inline constexpr Address32 spColorFuncEvalSerializerClone = 0x0047E2B0;
    inline constexpr Address32 spColorFuncEvalSerializerRegistrationGetter = 0x0047E200;
    inline constexpr Address32 spColorFuncEvalSerializerTargetClassID = 0x0047E230;
    inline constexpr Address32 spColorFuncEvalSerializerRead = 0x0047E320;
    inline constexpr Address32 spColorFuncEvalSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spColorFuncEvalSerializerWrite = 0x0047E850;
    inline constexpr Address32 spColorFuncEvalSerializerVTable = 0x006EB13C;
    inline constexpr Address32 spColorFuncEvalSerializerInterfaceVTable = 0x006EB130;
    inline constexpr std::uint32_t spColorFuncEvalSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spColorFuncEvalSerializerDefaultColorARGB = 0xFF000000;
    inline constexpr std::uint32_t spSphereBVSerializerClassID = 0x7294634F;
    inline constexpr std::uint32_t spSphereBVSerializerTargetClassIDValue = 0x390946D2;
    inline constexpr Address32 spSphereBVSerializerRegistration = 0x0075E948;
    inline constexpr Address32 spSphereBVSerializerRegistrationInitializer = 0x006D2E20;
    inline constexpr Address32 spSphereBVSerializerFactoryProtectedEntry = 0x0043A190;
    inline constexpr Address32 spSphereBVSerializerDestructor = 0x0043A170;
    inline constexpr Address32 spSphereBVSerializerDeletingDestructor = 0x0043A250;
    inline constexpr Address32 spSphereBVSerializerClone = 0x0043A200;
    inline constexpr Address32 spSphereBVSerializerRegistrationGetter = 0x0043A160;
    inline constexpr Address32 spSphereBVSerializerRead = 0x0043A2E0;
    inline constexpr Address32 spSphereBVSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spSphereBVSerializerWrite = 0x0043A4D0;
    inline constexpr Address32 spSphereBVSerializerVTable = 0x006DF4F0;
    inline constexpr Address32 spSphereBVSerializerInterfaceVTable = 0x006DF4E4;
    inline constexpr std::uint32_t spSphereBVSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spBoxBVSerializerClassID = 0x48E43495;
    inline constexpr std::uint32_t spBoxBVSerializerTargetClassIDValue = 0x7B4C0876;
    inline constexpr Address32 spBoxBVSerializerRegistration = 0x0075E888;
    inline constexpr Address32 spBoxBVSerializerRegistrationInitializer = 0x006D2DC0;
    inline constexpr Address32 spBoxBVSerializerFactoryProtectedEntry = 0x00439320;
    inline constexpr Address32 spBoxBVSerializerDestructor = 0x00439300;
    inline constexpr Address32 spBoxBVSerializerDeletingDestructor = 0x004393E0;
    inline constexpr Address32 spBoxBVSerializerClone = 0x00439390;
    inline constexpr Address32 spBoxBVSerializerRegistrationGetter = 0x004392F0;
    inline constexpr Address32 spBoxBVSerializerRead = 0x00439470;
    inline constexpr Address32 spBoxBVSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spBoxBVSerializerWrite = 0x004396B0;
    inline constexpr Address32 spBoxBVSerializerVTable = 0x006DF274;
    inline constexpr Address32 spBoxBVSerializerInterfaceVTable = 0x006DF268;
    inline constexpr std::uint32_t spBoxBVSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spOBBBVSerializerClassID = 0x68EA2ED1;
    inline constexpr std::uint32_t spOBBBVSerializerTargetClassIDValue = 0x4DA04889;
    inline constexpr Address32 spOBBBVSerializerRegistration = 0x0075E8E8;
    inline constexpr Address32 spOBBBVSerializerRegistrationInitializer = 0x006D2DF0;
    inline constexpr Address32 spOBBBVSerializerFactoryProtectedEntry = 0x00439A50;
    inline constexpr Address32 spOBBBVSerializerDestructor = 0x00439950;
    inline constexpr Address32 spOBBBVSerializerDeletingDestructor = 0x00439B10;
    inline constexpr Address32 spOBBBVSerializerClone = 0x00439AC0;
    inline constexpr Address32 spOBBBVSerializerRegistrationGetter = 0x00439940;
    inline constexpr Address32 spOBBBVSerializerRead = 0x00439BA0;
    inline constexpr Address32 spOBBBVSerializerFinalize = 0x005A7DB0;
    inline constexpr Address32 spOBBBVSerializerWrite = 0x00439E70;
    inline constexpr Address32 spOBBBVSerializerVTable = 0x006DF390;
    inline constexpr Address32 spOBBBVSerializerInterfaceVTable = 0x006DF384;
    inline constexpr std::uint32_t spOBBBVSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spLightDataSerializerClassID = 0x33EC2F8E;
    inline constexpr Address32 spLightDataSerializerRegistration = 0x0075ED10;
    inline constexpr Address32 spLightDataSerializerRegistrationInitializer = 0x006D3000;
    inline constexpr Address32 spLightDataSerializerFactoryProtectedEntry = 0x0043FFD0;
    inline constexpr Address32 spLightDataSerializerDestructor = 0x0043FFA0;
    inline constexpr Address32 spLightDataSerializerDeletingDestructor = 0x00440090;
    inline constexpr Address32 spLightDataSerializerClone = 0x00440040;
    inline constexpr Address32 spLightDataSerializerRegistrationGetter = 0x0043FF90;
    inline constexpr Address32 spLightDataSerializerLoad = 0x004400B0;
    inline constexpr Address32 spLightDataSerializerTargetClassID = 0x0043FFC0;
    inline constexpr Address32 spLightDataSerializerWrite = 0x00440110;
    inline constexpr Address32 spLightDataSerializerRead = 0x00440640;
    inline constexpr Address32 spLightDataSerializerVTable = 0x006E1974;
    inline constexpr Address32 spLightDataSerializerInterfaceVTable = 0x006E1968;
    inline constexpr std::uint32_t spLightSerializerClassID = 0x06165309;
    inline constexpr Address32 spLightSerializerRegistration = 0x007606A0;
    inline constexpr Address32 spLightSerializerRegistrationInitializer = 0x006D3C10;
    inline constexpr Address32 spLightSerializerFactoryProtectedEntry = 0x00471590;
    inline constexpr Address32 spLightSerializerDestructor = 0x00471560;
    inline constexpr Address32 spLightSerializerDeletingDestructor = 0x00471650;
    inline constexpr Address32 spLightSerializerClone = 0x00471600;
    inline constexpr Address32 spLightSerializerRegistrationGetter = 0x00471550;
    inline constexpr Address32 spLightSerializerTargetClassID = 0x00471580;
    inline constexpr Address32 spLightSerializerRead = 0x00471670;
    inline constexpr Address32 spLightSerializerWrite = 0x00471B00;
    inline constexpr Address32 spLightSerializerWriteThunk = 0x00471AF0;
    inline constexpr Address32 spLightSerializerVTable = 0x006E8EDC;
    inline constexpr Address32 spLightSerializerInterfaceVTable = 0x006E8ED0;
    inline constexpr std::uint32_t spRenderableSerializerClassID = 0x4D694D82;
    inline constexpr Address32 spRenderableSerializerRegistration = 0x00761398;
    inline constexpr Address32 spRenderableSerializerRegistrationInitializer = 0x006D41E0;
    inline constexpr Address32 spRenderableSerializerFactoryProtectedEntry = 0x0047F650;
    inline constexpr Address32 spRenderableSerializerDestructor = 0x0047F620;
    inline constexpr Address32 spRenderableSerializerDeletingDestructor = 0x0047F710;
    inline constexpr Address32 spRenderableSerializerClone = 0x0047F6C0;
    inline constexpr Address32 spRenderableSerializerRegistrationGetter = 0x0047F610;
    inline constexpr Address32 spRenderableSerializerTargetClassID = 0x0047F640;
    inline constexpr Address32 spRenderableSerializerWrite = 0x0047F7B0;
    inline constexpr Address32 spRenderableSerializerWriteThunk = 0x0047F7A0;
    inline constexpr Address32 spRenderableSerializerIndexResourceGraph = 0x0047F730;
    inline constexpr Address32 spRenderableSerializerRead = 0x0047FBA0;
    inline constexpr Address32 spRenderableSerializerVTable = 0x006EB6DC;
    inline constexpr Address32 spRenderableSerializerInterfaceVTable = 0x006EB6D0;
    inline constexpr std::uint32_t spModelSerializerClassID = 0xDB55C34A;
    inline constexpr Address32 spModelSerializerRegistration = 0x00762BC8;
    inline constexpr Address32 spModelSerializerRegistrationInitializer = 0x006D49B0;
    inline constexpr Address32 spModelSerializerFactoryProtectedEntry = 0x004934C0;
    inline constexpr Address32 spModelSerializerDestructor = 0x00493490;
    inline constexpr Address32 spModelSerializerDeletingDestructor = 0x00493580;
    inline constexpr Address32 spModelSerializerClone = 0x00493530;
    inline constexpr Address32 spModelSerializerRegistrationGetter = 0x00493480;
    inline constexpr Address32 spModelSerializerTargetClassID = 0x004934B0;
    inline constexpr Address32 spModelSerializerWrite = 0x00493600;
    inline constexpr Address32 spModelSerializerWriteThunk = 0x004935F0;
    inline constexpr Address32 spModelSerializerIndexResourceGraph = 0x004935A0;
    inline constexpr Address32 spModelSerializerRead = 0x004938F0;
    inline constexpr Address32 spModelSerializerVTable = 0x006ECBE4;
    inline constexpr Address32 spModelSerializerInterfaceVTable = 0x006ECBD8;
    inline constexpr std::uint32_t spMeshDataSerializerClassID = 0x66380037;
    inline constexpr Address32 spMeshDataSerializerRegistration = 0x0075E400;
    inline constexpr Address32 spMeshDataSerializerRegistrationInitializer = 0x006D2BD0;
    inline constexpr Address32 spMeshDataSerializerFactoryProtectedEntry = 0x0042AEF0;
    inline constexpr Address32 spMeshDataSerializerDestructor = 0x0042AEC0;
    inline constexpr Address32 spMeshDataSerializerDeletingDestructor = 0x0042AFB0;
    inline constexpr Address32 spMeshDataSerializerClone = 0x0042AF60;
    inline constexpr Address32 spMeshDataSerializerRegistrationGetter = 0x0042AEB0;
    inline constexpr Address32 spMeshDataSerializerLoad = 0x0042AFD0;
    inline constexpr Address32 spMeshDataSerializerTargetClassID = 0x0042AEE0;
    inline constexpr Address32 spMeshDataSerializerWrite = 0x0042B170;
    inline constexpr Address32 spMeshDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spMeshDataSerializerRead = 0x0042B420;
    inline constexpr Address32 spMeshDataSerializerVTable = 0x006DD4B4;
    inline constexpr Address32 spMeshDataSerializerInterfaceVTable = 0x006DD4A8;
    inline constexpr Address32 spNodeRegistration = 0x0075DD88;
    inline constexpr Address32 spNodeRegistrationInitializer = 0x006D27E0;
    inline constexpr Address32 spNodeFactory = 0x00421E20;
    inline constexpr Address32 spNodeConstructor = 0x00421BA0;
    inline constexpr Address32 spNodeDeletingDestructor = 0x00422220;
    inline constexpr Address32 spNodeClone = 0x00421E80;
    inline constexpr Address32 spNodeCopy = 0x00421F80;
    inline constexpr Address32 spNodeRegistrationGetter = 0x00421D50;
    inline constexpr Address32 spNodeVTable = 0x006DC4F4;
    // Analytical roles. Guest x86 replay resolves the unmodified protected
    // bridges; scene/collision implementations remain separate dependencies.
    inline constexpr Address32 spNodeWorldUpdateProtectedEntry = 0x00421420;
    inline constexpr Address32 spNodeWorldUpdateVisibleTail = 0x0042142E;
    inline constexpr Address32 spNodeWorldUpdateResolvedFlagsRead = 0x00442FA6;
    inline constexpr Address32 spNodeQuaternionResolvedDirtyWrite = 0x004023A9;
    inline constexpr Address32 spNodeAffineResolvedStackSetup = 0x004061E8;
    inline constexpr std::uint32_t spNodeWorldUpdateVirtualByteOffset = 0x30;
    inline constexpr std::uint32_t spNodeAllocationSize = 0xB4;
    inline constexpr std::uint32_t spNodeDefaultFlags = 0x00070A00;
    inline constexpr std::uint32_t spLightClassID = 0x72444900;
    inline constexpr Address32 spLightRegistration = 0x0075E278;
    inline constexpr Address32 spLightRegistrationInitializer = 0x006D2AE0;
    inline constexpr Address32 spLightConstructor = 0x00428CA0;
    inline constexpr Address32 spLightDestructor = 0x00428DB0;
    inline constexpr Address32 spLightDeletingDestructor = 0x00428F70;
    inline constexpr Address32 spLightCloneProtectedEntry = 0x004A1BF0;
    inline constexpr Address32 spLightCopyProtectedEntry = 0x00428EB0;
    inline constexpr Address32 spLightRegistrationGetter = 0x00428D90;
    inline constexpr Address32 spLightUpdate = 0x00428C30;
    inline constexpr Address32 spLightRenderHelper = 0x00428DD0;
    inline constexpr Address32 spLightVTable = 0x006DCC18;
    inline constexpr Address32 spLightSceneInterfaceVTable = 0x006DE98C;
    inline constexpr std::uint32_t spLightDataClassID = 0x5E6402DF;
    inline constexpr Address32 spLightDataRegistration = 0x0075D4E8;
    inline constexpr Address32 spLightDataRegistrationInitializer = 0x006D1D60;
    inline constexpr Address32 spLightDataFactoryProtectedEntry = 0x0041A330;
    inline constexpr Address32 spLightDataDestructor = 0x00435410;
    inline constexpr Address32 spLightDataDeletingDestructor = 0x00435430;
    inline constexpr Address32 spLightDataClone = 0x0041ACA0;
    inline constexpr Address32 spLightDataRegistrationGetter = 0x00435400;
    inline constexpr Address32 spLightDataVTable = 0x006DE990;
    inline constexpr std::uint32_t spMaterialClassID = 0x5C0314C5;
    inline constexpr Address32 spMaterialRegistration = 0x0075DFD0;
    inline constexpr Address32 spMaterialRegistrationInitializer = 0x006D2930;
    inline constexpr Address32 spMaterialDestructor = 0x00423B30;
    inline constexpr Address32 spMaterialClone = 0x004A1BF0;
    inline constexpr Address32 spMaterialCopy = 0x00423880;
    inline constexpr Address32 spMaterialRegistrationGetter = 0x00423830;
    inline constexpr Address32 spMaterialPrimaryVTable = 0x006DC984;
    inline constexpr std::uint32_t spMaterialObservedSize = 0x78;
    inline constexpr std::uint32_t spMaterialDataClassID = 0x6160348B;
    inline constexpr Address32 spMaterialDataRegistration = 0x0075D548;
    inline constexpr Address32 spMaterialDataRegistrationInitializer = 0x006D1D90;
    inline constexpr Address32 spMaterialDataFactoryProtectedEntry = 0x0041A390;
    inline constexpr Address32 spMaterialDataDestructor = 0x004357C0;
    inline constexpr Address32 spMaterialDataClone = 0x0041ACF0;
    inline constexpr Address32 spMaterialDataCopy = 0x005A7DB0;
    inline constexpr Address32 spMaterialDataRegistrationGetter = 0x004356E0;
    inline constexpr Address32 spMaterialDataPrimaryVTable = 0x006DE9FC;
    inline constexpr Address32 spMaterialDataInterfaceVTable = 0x006DE9D0;
    inline constexpr std::uint32_t spMaterialDataObservedSize = 0xBC;
    inline constexpr std::uint32_t spMaterialPassLayerClassID = 0x3A8905A5;
    inline constexpr Address32 spMaterialPassLayerRegistration = 0x0075FE80;
    inline constexpr Address32 spMaterialPassLayerRegistrationInitializer = 0x006D37C0;
    inline constexpr Address32 spMaterialPassLayerFactoryProtectedEntry = 0x0045F610;
    inline constexpr Address32 spMaterialPassLayerDestructor = 0x0045F7D0;
    inline constexpr Address32 spMaterialPassLayerClone = 0x0045F6A0;
    inline constexpr Address32 spMaterialPassLayerCopy = 0x0045F740;
    inline constexpr Address32 spMaterialPassLayerRegistrationGetter = 0x0045F560;
    inline constexpr Address32 spMaterialPassLayerVTable = 0x006E7388;
    inline constexpr std::uint32_t spMaterialPassLayerObservedSize = 0x38;
    inline constexpr std::uint32_t spMaterialTextureLayerClassID = 0x7F577C6D;
    inline constexpr Address32 spMaterialTextureLayerRegistration = 0x0075DF70;
    inline constexpr Address32 spMaterialTextureLayerRegistrationInitializer = 0x006D2900;
    inline constexpr Address32 spMaterialTextureLayerFactoryProtectedEntry = 0x00423470;
    inline constexpr Address32 spMaterialTextureLayerDestructor = 0x00423630;
    inline constexpr Address32 spMaterialTextureLayerClone = 0x004234E0;
    inline constexpr Address32 spMaterialTextureLayerCopy = 0x004235E0;
    inline constexpr Address32 spMaterialTextureLayerRegistrationGetter = 0x00423450;
    inline constexpr Address32 spMaterialTextureLayerVTable = 0x006DC91C;
    inline constexpr std::uint32_t spMaterialTextureLayerObservedSize = 0x14;
    inline constexpr std::uint32_t spStdLayerClassID = 0x234C576B;
    inline constexpr Address32 spStdLayerRegistration = 0x0075FFA8;
    inline constexpr Address32 spStdLayerRegistrationInitializer = 0x006D3850;
    inline constexpr Address32 spStdLayerFactoryProtectedEntry = 0x00460E50;
    inline constexpr Address32 spStdLayerDestructor = 0x00460F00;
    inline constexpr Address32 spStdLayerClone = 0x00460EB0;
    inline constexpr Address32 spStdLayerCopy = 0x00460F20;
    inline constexpr Address32 spStdLayerRegistrationGetter = 0x00460E30;
    inline constexpr Address32 spStdLayerVTable = 0x006E7700;
    inline constexpr std::uint32_t spStdLayerObservedSize = 0x14;
    inline constexpr std::uint32_t spFogClassID = 0x7AC95AEC;
    inline constexpr Address32 spFogRegistration = 0x0075CF48;
    inline constexpr Address32 spFogRegistrationInitializer = 0x006D1A90;
    inline constexpr Address32 spFogFactoryProtectedEntry = 0x00419E90;
    inline constexpr Address32 spFogDestructor = 0x0041A8B0;
    inline constexpr Address32 spFogClone = 0x0041A8E0;
    inline constexpr Address32 spFogCopy = 0x00413120;
    inline constexpr Address32 spFogRegistrationGetter = 0x00419E80;
    inline constexpr Address32 spFogVTable = 0x006DBD04;
    inline constexpr std::uint32_t spFogObservedSize = 0x28;
    inline constexpr std::uint32_t spFogDefaultColorARGB = 0xFF000000;
    inline constexpr std::uint32_t spPlatformSpecificMeshDataClassID = 0x71BE79C5;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistration = 0x00762A48;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistrationInitializer = 0x006D48F0;
    inline constexpr Address32 spPlatformSpecificMeshDataFactory = 0x004920F0;
    inline constexpr Address32 spPlatformSpecificMeshDataDestructor = 0x004920E0;
    inline constexpr Address32 spPlatformSpecificMeshDataDeletingDestructor = 0x004921B0;
    inline constexpr Address32 spPlatformSpecificMeshDataClone = 0x00492160;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistrationGetter = 0x004920D0;
    inline constexpr Address32 spPlatformSpecificMeshDataVTable = 0x006ECB00;
    inline constexpr std::uint32_t spDXMeshDataClassID = 0x3178114C;
    inline constexpr Address32 spDXMeshDataRegistration = 0x00760760;
    inline constexpr Address32 spDXMeshDataRegistrationInitializer = 0x006D3C70;
    inline constexpr Address32 spDXMeshDataFactory = 0x004725C0;
    inline constexpr Address32 spDXMeshDataDestructor = 0x00472680;
    inline constexpr Address32 spDXMeshDataDeletingDestructor = 0x004726F0;
    inline constexpr Address32 spDXMeshDataClone = 0x00472630;
    inline constexpr Address32 spDXMeshDataRegistrationGetter = 0x00472580;
    inline constexpr Address32 spDXMeshDataVTable = 0x006E8F54;
    inline constexpr std::uint32_t spPS2MeshDataClassID = 0x737D740F;
    inline constexpr Address32 spPS2MeshDataRegistration = 0x007608D8;
    inline constexpr Address32 spPS2MeshDataRegistrationInitializer = 0x006D3CA0;
    inline constexpr Address32 spPS2MeshDataFactory = 0x00473FA0;
    inline constexpr Address32 spPS2MeshDataDestructor = 0x00474050;
    inline constexpr Address32 spPS2MeshDataDeletingDestructor = 0x00474A60;
    inline constexpr Address32 spPS2MeshDataClone = 0x00474000;
    inline constexpr Address32 spPS2MeshDataRegistrationGetter = 0x00473160;
    inline constexpr Address32 spPS2MeshDataVTable = 0x006E9454;
    inline constexpr Address32 spPS2MeshDataBuildGrouped = 0x00475880;
    inline constexpr Address32 spPS2MeshDataBuildTriangles = 0x00474E00;
    inline constexpr Address32 spPS2MeshDataInitializeFromBuffers = 0x00474FF0;
    inline constexpr std::uint32_t spPS2MeshDataSerializerClassID = 0x6B0C238F;
    inline constexpr Address32 spPS2MeshDataSerializerRegistration = 0x0075E398;
    inline constexpr Address32 spPS2MeshDataSerializerRegistrationInitializer = 0x006D2BA0;
    inline constexpr Address32 spPS2MeshDataSerializerFactoryProtectedEntry = 0x0042A2A0;
    inline constexpr Address32 spPS2MeshDataSerializerDestructor = 0x0042A240;
    inline constexpr Address32 spPS2MeshDataSerializerDeletingDestructor = 0x0042A360;
    inline constexpr Address32 spPS2MeshDataSerializerClone = 0x0042A310;
    inline constexpr Address32 spPS2MeshDataSerializerRegistrationGetter = 0x0042A230;
    inline constexpr Address32 spPS2MeshDataSerializerTargetClassID = 0x0042A260;
    inline constexpr Address32 spPS2MeshDataSerializerSBOOLoad = 0x0042AFD0;
    inline constexpr Address32 spPS2MeshDataSerializerLoadPlatformSpecific = 0x0042A420;
    inline constexpr Address32 spPS2MeshDataSerializerSerializePlatformSpecific = 0x0042A600;
    inline constexpr Address32 spPS2MeshDataSerializerWrite = 0x0042A700;
    inline constexpr Address32 spPS2MeshDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spPS2MeshDataSerializerRead = 0x0042AB40;
    inline constexpr Address32 spPS2MeshDataSerializerVTable = 0x006DD2DC;
    inline constexpr Address32 spPS2MeshDataSerializerInterfaceVTable = 0x006DD2D0;
    inline constexpr std::uint32_t spPS2MeshDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spPS2MeshDataSerializerNativeLoadFlagMask = 0x02;
    inline constexpr std::uint32_t spDXMeshDataSerializerClassID = 0x77006ABE;
    inline constexpr Address32 spDXMeshDataSerializerRegistration = 0x0075E338;
    inline constexpr Address32 spDXMeshDataSerializerRegistrationInitializer = 0x006D2B70;
    inline constexpr Address32 spDXMeshDataSerializerFactoryProtectedEntry = 0x004297C0;
    inline constexpr Address32 spDXMeshDataSerializerDestructor = 0x00429790;
    inline constexpr Address32 spDXMeshDataSerializerDeletingDestructor = 0x00429880;
    inline constexpr Address32 spDXMeshDataSerializerClone = 0x00429830;
    inline constexpr Address32 spDXMeshDataSerializerRegistrationGetter = 0x00429780;
    inline constexpr Address32 spDXMeshDataSerializerTargetClassID = 0x004297B0;
    inline constexpr Address32 spDXMeshDataSerializerSBOOLoad = 0x0042AFD0;
    inline constexpr Address32 spDXMeshDataSerializerLoadCrossPlatform = 0x0042B0A0;
    inline constexpr Address32 spDXMeshDataSerializerLoadPlatformSpecific = 0x00429A40;
    inline constexpr Address32 spDXMeshDataSerializerSerializePlatformSpecific = 0x004298A0;
    inline constexpr Address32 spDXMeshDataSerializerWrite = 0x00429EA0;
    inline constexpr Address32 spDXMeshDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spDXMeshDataSerializerRead = 0x00429BC0;
    inline constexpr Address32 spDXMeshDataSerializerVTable = 0x006DCCB8;
    inline constexpr Address32 spDXMeshDataSerializerInterfaceVTable = 0x006DCCAC;
    inline constexpr std::uint32_t spDXMeshDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spDXMeshDataSerializerNativeLoadFlagMask = 0x02;
    inline constexpr std::uint32_t spTextureDataSerializerClassID = 0x1C4C75BA;
    inline constexpr std::uint32_t spTextureDataSerializerTargetClassIDValue = 0x78EA082B;
    inline constexpr Address32 spTextureDataSerializerRegistration = 0x0075E528;
    inline constexpr Address32 spTextureDataSerializerRegistrationInitializer = 0x006D2C60;
    inline constexpr Address32 spTextureDataSerializerFactoryProtectedEntry = 0x0042DC30;
    inline constexpr Address32 spTextureDataSerializerDestructor = 0x0042DC00;
    inline constexpr Address32 spTextureDataSerializerDeletingDestructor = 0x0042DCF0;
    inline constexpr Address32 spTextureDataSerializerClone = 0x0042DCA0;
    inline constexpr Address32 spTextureDataSerializerRegistrationGetter = 0x0042DBF0;
    inline constexpr Address32 spTextureDataSerializerLoad = 0x0042DD10;
    inline constexpr Address32 spTextureDataSerializerReadSource = 0x0042EA50;
    inline constexpr Address32 spTextureDataSerializerWriteSource = 0x0042E5F0;
    inline constexpr Address32 spTextureDataSerializerReadCrossPlatform = 0x0042E100;
    inline constexpr Address32 spTextureDataSerializerWriteCrossPlatform = 0x0042DD70;
    inline constexpr Address32 spTextureDataSerializerTargetClassID = 0x0042DC20;
    inline constexpr Address32 spTextureDataSerializerWrite = 0x0042EE10;
    inline constexpr Address32 spTextureDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spTextureDataSerializerRead = 0x0042F180;
    inline constexpr Address32 spTextureDataSerializerVTable = 0x006DDD90;
    inline constexpr Address32 spTextureDataSerializerInterfaceVTable = 0x006DDD84;
    inline constexpr std::uint32_t spTextureDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spTextureDataSerializerCrossPlatformType = 1;
    inline constexpr std::uint32_t spMaterialSerializerClassID = 0x2A14745F;
    inline constexpr Address32 spMaterialSerializerRegistration = 0x00760998;
    inline constexpr Address32 spMaterialSerializerRegistrationInitializer = 0x006D3D00;
    inline constexpr Address32 spMaterialSerializerFactoryProtectedEntry = 0x00477F00;
    inline constexpr Address32 spMaterialSerializerDestructor = 0x00477480;
    inline constexpr Address32 spMaterialSerializerDeletingDestructor = 0x00477EE0;
    inline constexpr Address32 spMaterialSerializerClone = 0x00477F60;
    inline constexpr Address32 spMaterialSerializerRegistrationGetter = 0x004774C0;
    inline constexpr Address32 spMaterialSerializerIndexLayer = 0x004767F0;
    inline constexpr Address32 spMaterialSerializerLoadLayer = 0x00477230;
    inline constexpr Address32 spMaterialSerializerSerializeLayer = 0x00477350;
    inline constexpr Address32 spMaterialSerializerPrimaryVTable = 0x006EA728;
    inline constexpr std::uint32_t spMaterialSerializerObservedSize = 0x3C;
    inline constexpr std::uint32_t spMaterialSerializerDataBlockStateOffset = 0x14;
    inline constexpr std::uint32_t spMaterialDataSerializerClassID = 0x0B251467;
    inline constexpr std::uint32_t spMaterialDataSerializerTargetClassIDValue = 0x6160348B;
    inline constexpr Address32 spMaterialDataSerializerRegistration = 0x0075E648;
    inline constexpr Address32 spMaterialDataSerializerRegistrationInitializer = 0x006D2CF0;
    inline constexpr Address32 spMaterialDataSerializerFactoryProtectedEntry = 0x0042F690;
    inline constexpr Address32 spMaterialDataSerializerDestructor = 0x0042F640;
    inline constexpr Address32 spMaterialDataSerializerDeletingDestructor = 0x0042F750;
    inline constexpr Address32 spMaterialDataSerializerClone = 0x0042F700;
    inline constexpr Address32 spMaterialDataSerializerRegistrationGetter = 0x0042F630;
    inline constexpr Address32 spMaterialDataSerializerLoadSignature = 0x0042F4C0;
    inline constexpr Address32 spMaterialDataSerializerTargetClassID = 0x0042F660;
    inline constexpr Address32 spMaterialDataSerializerPrimaryVTable = 0x006DE5C8;
    inline constexpr Address32 spMaterialDataSerializerInterfaceVTable = 0x006EFD68;
    inline constexpr std::uint32_t spMaterialDataSerializerObservedSize = 0x3C;
    inline constexpr std::uint32_t spDXMaterialDataSerializerClassID = 0x60EE3A89;
    inline constexpr Address32 spDXMaterialDataSerializerRegistration = 0x0075E588;
    inline constexpr Address32 spDXMaterialDataSerializerRegistrationInitializer = 0x006D2C90;
    inline constexpr Address32 spDXMaterialDataSerializerFactoryProtectedEntry = 0x0042F3E0;
    inline constexpr Address32 spDXMaterialDataSerializerDestructor = 0x0042F3C0;
    inline constexpr Address32 spDXMaterialDataSerializerDeletingDestructor = 0x0042F4A0;
    inline constexpr Address32 spDXMaterialDataSerializerClone = 0x0042F450;
    inline constexpr Address32 spDXMaterialDataSerializerRegistrationGetter = 0x0042F3B0;
    inline constexpr Address32 spDXMaterialDataSerializerLoadSignature = 0x0042F4C0;
    inline constexpr Address32 spDXMaterialDataSerializerPrimaryVTable = 0x006DE520;
    inline constexpr Address32 spDXMaterialDataSerializerInterfaceVTable = 0x006DE514;
    inline constexpr std::uint32_t spDXMaterialDataSerializerObservedSize = 0x3C;
    inline constexpr std::uint32_t spPS2MaterialDataSerializerClassID = 0x69327633;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistration = 0x0075E5E8;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistrationInitializer = 0x006D2CC0;
    inline constexpr Address32 spPS2MaterialDataSerializerFactoryProtectedEntry = 0x0042F550;
    inline constexpr Address32 spPS2MaterialDataSerializerDestructor = 0x0042F530;
    inline constexpr Address32 spPS2MaterialDataSerializerDeletingDestructor = 0x0042F610;
    inline constexpr Address32 spPS2MaterialDataSerializerClone = 0x0042F5C0;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistrationGetter = 0x0042F520;
    inline constexpr Address32 spPS2MaterialDataSerializerLoadSignature = 0x0042F4C0;
    inline constexpr Address32 spPS2MaterialDataSerializerPrimaryVTable = 0x006DE574;
    inline constexpr Address32 spPS2MaterialDataSerializerInterfaceVTable = 0x006DE514;
    inline constexpr std::uint32_t spPS2MaterialDataSerializerObservedSize = 0x3C;
    inline constexpr std::uint32_t spDXTextureDataSerializerClassID = 0x1C6D480F;
    inline constexpr std::uint32_t spDXTextureDataSerializerTargetClassIDValue = 0x0B1C67BB;
    inline constexpr Address32 spDXTextureDataSerializerRegistration = 0x0075E460;
    inline constexpr Address32 spDXTextureDataSerializerRegistrationInitializer = 0x006D2C00;
    inline constexpr Address32 spDXTextureDataSerializerFactoryProtectedEntry = 0x0042B660;
    inline constexpr Address32 spDXTextureDataSerializerDestructor = 0x0042B570;
    inline constexpr Address32 spDXTextureDataSerializerDeletingDestructor = 0x0042B720;
    inline constexpr Address32 spDXTextureDataSerializerClone = 0x0042B6D0;
    inline constexpr Address32 spDXTextureDataSerializerRegistrationGetter = 0x0042B560;
    inline constexpr Address32 spDXTextureDataSerializerSerializePlatformSpecific = 0x0042B9E0;
    inline constexpr Address32 spDXTextureDataSerializerLoadPlatformSpecific = 0x0042C3B0;
    inline constexpr Address32 spDXTextureDataSerializerWrite = 0x0042BF40;
    inline constexpr Address32 spDXTextureDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spDXTextureDataSerializerRead = 0x0042C640;
    inline constexpr Address32 spDXTextureDataSerializerTargetClassID = 0x0042B590;
    inline constexpr Address32 spDXTextureDataSerializerVTable = 0x006DD648;
    inline constexpr Address32 spDXTextureDataSerializerInterfaceVTable = 0x006DD63C;
    inline constexpr std::uint32_t spDXTextureDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spDXTextureDataSerializerNativeLoadFlagMask = 0x02;
    inline constexpr std::uint32_t spPS2TextureDataSerializerClassID = 0x43C76799;
    inline constexpr std::uint32_t spPS2TextureDataSerializerTargetClassIDValue = 0x24767C83;
    inline constexpr Address32 spPS2TextureDataSerializerRegistration = 0x0075E4C8;
    inline constexpr Address32 spPS2TextureDataSerializerRegistrationInitializer = 0x006D2C30;
    inline constexpr Address32 spPS2TextureDataSerializerFactoryProtectedEntry = 0x0042C9E0;
    inline constexpr Address32 spPS2TextureDataSerializerDestructor = 0x0042C910;
    inline constexpr Address32 spPS2TextureDataSerializerDeletingDestructor = 0x0042CAA0;
    inline constexpr Address32 spPS2TextureDataSerializerClone = 0x0042CA50;
    inline constexpr Address32 spPS2TextureDataSerializerRegistrationGetter = 0x0042C900;
    inline constexpr Address32 spPS2TextureDataSerializerSerializePlatformSpecific = 0x0042CD90;
    inline constexpr Address32 spPS2TextureDataSerializerLoadPlatformSpecific = 0x0042D6C0;
    inline constexpr Address32 spPS2TextureDataSerializerWrite = 0x0042D1B0;
    inline constexpr Address32 spPS2TextureDataSerializerIndexResourceGraph = 0x005A7DB0;
    inline constexpr Address32 spPS2TextureDataSerializerRead = 0x0042D910;
    inline constexpr Address32 spPS2TextureDataSerializerTargetClassID = 0x0042C930;
    inline constexpr Address32 spPS2TextureDataSerializerVTable = 0x006DDBA8;
    inline constexpr Address32 spPS2TextureDataSerializerInterfaceVTable = 0x006DDB9C;
    inline constexpr std::uint32_t spPS2TextureDataSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spPS2TextureDataSerializerNativeLoadFlagMask = 0x02;
    inline constexpr Address32 spMeshRegistration = 0x0075E090;
    inline constexpr Address32 spMeshRegistrationInitializer = 0x006D2990;
    inline constexpr Address32 spMeshDestructor = 0x00424140;
    inline constexpr Address32 spMeshDeletingDestructor = 0x00424210;
    inline constexpr Address32 spMeshRegistrationGetter = 0x00424190;
    inline constexpr Address32 spMeshBoundsPass = 0x00424220;
    inline constexpr Address32 spMeshVTable = 0x006DC9FC;
    inline constexpr Address32 spMeshSecondaryVTable = 0x006EFFE4;
    inline constexpr std::uint32_t spRenderMeshClassID = 0x67974A9C;
    inline constexpr Address32 spRenderMeshRegistration = 0x00763B40;
    inline constexpr Address32 spRenderMeshRegistrationInitializer = 0x006D5190;
    inline constexpr Address32 spRenderMeshRegistrationGetter = 0x004B1610;
    inline constexpr Address32 spRenderMeshDestructorProtectedEntry = 0x004B1620;
    inline constexpr Address32 spRenderMeshDeletingDestructor = 0x004B1640;
    inline constexpr Address32 spRenderMeshVTable = 0x006EFFEC;
    inline constexpr Address32 spRenderMeshSecondaryVTable = 0x006EFFE4;
    inline constexpr std::uint32_t spDXVertexBufferClassID = 0x37036C17;
    inline constexpr Address32 spDXVertexBufferRegistration = 0x00763C00;
    inline constexpr Address32 spDXVertexBufferRegistrationInitializer = 0x006D51F0;
    inline constexpr Address32 spDXVertexBufferConstructorProtectedEntry = 0x004B1E00;
    inline constexpr Address32 spDXVertexBufferRegistrationGetter = 0x004B1E20;
    inline constexpr Address32 spDXVertexBufferFactoryProtectedEntry = 0x004B1E30;
    inline constexpr Address32 spDXVertexBufferClone = 0x004B1EA0;
    inline constexpr Address32 spDXVertexBufferDestructor = 0x004B1EF0;
    inline constexpr Address32 spDXVertexBufferInitialize = 0x004B1F60;
    inline constexpr Address32 spDXVertexBufferDeletingDestructor = 0x004B1FA0;
    inline constexpr Address32 spDXVertexBufferVTable = 0x006F05C4;
    inline constexpr std::uint32_t spDXVertexBufferObservedSize = 0x20;
    inline constexpr std::uint32_t spDXIndexBufferClassID = 0x23022413;
    inline constexpr Address32 spDXIndexBufferRegistration = 0x00763C60;
    inline constexpr Address32 spDXIndexBufferRegistrationInitializer = 0x006D5220;
    inline constexpr Address32 spDXIndexBufferConstructorProtectedEntry = 0x004B1FC0;
    inline constexpr Address32 spDXIndexBufferRegistrationGetter = 0x004B1FE0;
    inline constexpr Address32 spDXIndexBufferRelease = 0x004B1FF0;
    inline constexpr Address32 spDXIndexBufferFactoryProtectedEntry = 0x004B2010;
    inline constexpr Address32 spDXIndexBufferClone = 0x004B2080;
    inline constexpr Address32 spDXIndexBufferDestructor = 0x004B20D0;
    inline constexpr Address32 spDXIndexBufferInitialize = 0x004B2140;
    inline constexpr Address32 spDXIndexBufferDeletingDestructor = 0x004B2180;
    inline constexpr Address32 spDXIndexBufferVTable = 0x006F05F8;
    inline constexpr std::uint32_t spDXIndexBufferObservedSize = 0x1C;
    inline constexpr std::uint32_t d3dDeviceCreateVertexBufferVTableOffset = 0x68;
    inline constexpr std::uint32_t d3dDeviceCreateIndexBufferVTableOffset = 0x6C;
    inline constexpr Address32 spDXMeshCombinerIsFull = 0x004A95E0;
    inline constexpr Address32 spDXMeshCombinerConstructor = 0x004A9610;
    inline constexpr Address32 spDXMeshCombinerDestructor = 0x004A9640;
    inline constexpr Address32 spDXMeshCombinerInitialize = 0x004A96C0;
    inline constexpr Address32 spDXMeshCombinerCommit = 0x004A98E0;
    inline constexpr Address32 spDXMeshCombinerDeletingDestructor = 0x004A9F30;
    inline constexpr Address32 spDXMeshCombinerVTable = 0x006EF294;
    inline constexpr Address32 spDXMeshCombinerActiveGlobal = 0x00763148;
    inline constexpr std::uint32_t spDXMeshCombinerSize = 0x2C;
    inline constexpr std::uint32_t spDXSharedMeshDataClassID = 0x293A2681;
    inline constexpr std::uint32_t spDXCombinedVBClassID = 0x4B18E622;
    inline constexpr Address32 spDXCombinedVBRegistration = 0x00764408;
    inline constexpr Address32 spDXCombinedVBRegistrationInitializer = 0x006D5550;
    inline constexpr Address32 spDXCombinedVBDestructor = 0x004C0C50;
    inline constexpr Address32 spDXCombinedVBRegistrationGetter = 0x004C0D50;
    inline constexpr Address32 spDXCombinedVBConstructorProtectedEntry = 0x004C0D60;
    inline constexpr Address32 spDXCombinedVBDeletingDestructor = 0x004C0DF0;
    inline constexpr Address32 spDXCombinedVBFactoryProtectedEntry = 0x004C0E10;
    inline constexpr Address32 spDXCombinedVBClone = 0x004C0E70;
    inline constexpr Address32 spDXCombinedVBVTable = 0x006F1E70;
    inline constexpr std::uint32_t spDXCombinedVBObservedSize = 0x3C;
    inline constexpr Address32 spDXSharedMeshDataRegistration = 0x007646A8;
    inline constexpr Address32 spDXSharedMeshDataRegistrationInitializer = 0x006D56F0;
    inline constexpr Address32 spDXSharedMeshDataRegistrationGetter = 0x004C27F0;
    inline constexpr Address32 spDXSharedMeshDataDestructor = 0x004C2800;
    inline constexpr Address32 spDXSharedMeshDataFactoryProtectedEntry = 0x004C28E0;
    inline constexpr Address32 spDXSharedMeshDataClone = 0x004C2950;
    inline constexpr Address32 spDXSharedMeshDataDeletingDestructor = 0x004C29A0;
    inline constexpr Address32 spDXSharedMeshDataInitialize = 0x004C29C0;
    inline constexpr Address32 spDXSharedMeshDataVTable = 0x006F23A0;
    inline constexpr std::uint32_t spDXSharedMeshDataObservedSize = 0x1C;
    inline constexpr std::uint32_t spDXMeshClassID = 0x193B2671;
    inline constexpr Address32 spDXMeshRegistration = 0x00763150;
    inline constexpr Address32 spDXMeshRegistrationInitializer = 0x006D4CB0;
    inline constexpr Address32 spDXMeshFactoryProtectedEntry = 0x004A9E80;
    inline constexpr Address32 spDXMeshConstructorProtectedEntry = 0x004A9940;
    inline constexpr Address32 spDXMeshRegistrationGetter = 0x004A9990;
    inline constexpr Address32 spDXMeshInitializeBuffers = 0x004A99A0;
    inline constexpr Address32 spDXMeshReleaseInterface = 0x004A9C30;
    inline constexpr Address32 spDXMeshInitializeShared = 0x004A9CC0;
    inline constexpr Address32 spDXMeshGetWritableData = 0x004A9E20;
    inline constexpr Address32 spDXMeshClone = 0x004A9EE0;
    inline constexpr Address32 spDXMeshDestructor = 0x004A9F50;
    inline constexpr Address32 spDXMeshInitializeInterface = 0x004AA000;
    inline constexpr Address32 spDXMeshDeletingDestructor = 0x004AA350;
    inline constexpr Address32 spDXMeshVTable = 0x006EF334;
    inline constexpr Address32 spDXMeshInterfaceVTable = 0x006EF32C;
    inline constexpr Address32 spDXMeshComponentFlagsToFVF = 0x004B21E0;
    inline constexpr std::uint32_t spDXMeshObservedSize = 0x88;
    inline constexpr std::uint32_t spDXMeshSerializerClassID = 0xE712BCAD;
    inline constexpr std::uint32_t spDXMeshSerializerTargetClassID = 0x193B2671;
    inline constexpr std::uint32_t spDXMeshSerializerRelationshipClassID = 0x293A2681;
    inline constexpr Address32 spDXMeshSerializerRegistration = 0x00763BA0;
    inline constexpr Address32 spDXMeshSerializerRegistrationInitializer = 0x006D51C0;
    inline constexpr Address32 spDXMeshSerializerRegistrationGetter = 0x004B1660;
    inline constexpr Address32 spDXMeshSerializerDestructor = 0x004B1670;
    inline constexpr Address32 spDXMeshSerializerGetTargetClassID = 0x004B1690;
    inline constexpr Address32 spDXMeshSerializerFactoryProtectedEntry = 0x004B16A0;
    inline constexpr Address32 spDXMeshSerializerClone = 0x004B1710;
    inline constexpr Address32 spDXMeshSerializerDeletingDestructor = 0x004B1760;
    inline constexpr Address32 spDXMeshSerializerRead = 0x004B1780;
    inline constexpr Address32 spDXMeshSerializerWrite = 0x004B1B20;
    inline constexpr Address32 spDXMeshSerializerIndexRelationship = 0x004B1DB0;
    inline constexpr Address32 spDXMeshSerializerInterfaceVTable = 0x006F0028;
    inline constexpr Address32 spDXMeshSerializerVTable = 0x006F0034;
    inline constexpr Address32 spDXMeshSerializerOptimizerGetCombinedVB = 0x004BEDF0;
    inline constexpr Address32 spDXMeshSerializerCombinedVBGetRange = 0x004C07E0;
    inline constexpr std::uint32_t spDXMeshSerializerObservedSize = 0x14;
    inline constexpr std::uint32_t spDXSharedMeshDataSerializerClassID = 0x506F8A8C;
    inline constexpr std::uint32_t spDXSharedMeshDataSerializerTargetClassID = 0x293A2681;
    inline constexpr std::uint32_t spDXSharedMeshDataSerializerSourceClassID = 0x4B18E622;
    inline constexpr Address32 spDXSharedMeshDataSerializerRegistration = 0x007645E8;
    inline constexpr Address32 spDXSharedMeshDataSerializerRegistrationInitializer = 0x006D5690;
    inline constexpr Address32 spDXSharedMeshDataSerializerRegistrationGetter = 0x004C1DA0;
    inline constexpr Address32 spDXSharedMeshDataSerializerDestructor = 0x004C1DB0;
    inline constexpr Address32 spDXSharedMeshDataSerializerGetSourceClassID = 0x004C1DD0;
    inline constexpr Address32 spDXSharedMeshDataSerializerResolveClassID = 0x004C1DE0;
    inline constexpr Address32 spDXSharedMeshDataSerializerFactoryProtectedEntry = 0x004C1DF0;
    inline constexpr Address32 spDXSharedMeshDataSerializerClone = 0x004C1E60;
    inline constexpr Address32 spDXSharedMeshDataSerializerDeletingDestructor = 0x004C1EB0;
    inline constexpr Address32 spDXSharedMeshDataSerializerWrite = 0x004C1ED0;
    inline constexpr Address32 spDXSharedMeshDataSerializerRead = 0x004C1FA0;
    inline constexpr Address32 spDXSharedMeshDataSerializerInterfaceVTable = 0x006F20FC;
    inline constexpr Address32 spDXSharedMeshDataSerializerVTable = 0x006F2108;
    inline constexpr std::uint32_t spMeshDataClassID = 0x33C34CF0;
    inline constexpr Address32 spMeshDataRegistration = 0x0075D428;
    inline constexpr Address32 spMeshDataRegistrationInitializer = 0x006D1D00;
    inline constexpr Address32 spMeshDataFactory = 0x0041A270;
    inline constexpr Address32 spMeshDataDestructor = 0x00434EA0;
    inline constexpr Address32 spMeshDataDeletingDestructor = 0x00434F60;
    inline constexpr Address32 spMeshDataClone = 0x0041AC00;
    inline constexpr Address32 spMeshDataRegistrationGetter = 0x00434E20;
    inline constexpr Address32 spMeshDataInitialize = 0x00434E40;
    inline constexpr Address32 spMeshDataDeepCopy = 0x00434F10;
    inline constexpr Address32 spMeshDataPrimaryVTable = 0x006DE8FC;
    inline constexpr Address32 spMeshDataSecondaryVTable = 0x006DE8F4;
    inline constexpr std::uint32_t spVertexBufferClassID = 0x3C846352;
    inline constexpr Address32 spVertexBufferRegistration = 0x0075FF40;
    inline constexpr Address32 spVertexBufferRegistrationInitializer = 0x006D3820;
    inline constexpr Address32 spVertexBufferFactory = 0x00460130;
    inline constexpr Address32 spVertexBufferRegistrationGetter = 0x0045FE90;
    inline constexpr Address32 spVertexBufferDestructor = 0x00460210;
    inline constexpr Address32 spVertexBufferDeletingDestructor = 0x004604D0;
    inline constexpr Address32 spVertexBufferClone = 0x004601C0;
    inline constexpr Address32 spVertexBufferCopy = 0x0040ECE0;
    inline constexpr Address32 spVertexBufferBuildLayout = 0x0045FEA0;
    inline constexpr Address32 spVertexBufferInitialize = 0x00460010;
    inline constexpr Address32 spVertexBufferInitializeRaw = 0x00460070;
    inline constexpr Address32 spVertexBufferInitializeExternal = 0x004600E0;
    inline constexpr Address32 spVertexBufferCopyBuffer = 0x00460240;
    inline constexpr Address32 spVertexBufferRead = 0x00460300;
    inline constexpr Address32 spVertexBufferWrite = 0x00460400;
    inline constexpr Address32 spVertexBufferRelease = 0x00423590;
    inline constexpr Address32 spVertexBufferVTable = 0x006E75BC;
    inline constexpr std::uint32_t spRenderableClassID = 0x4FDA4542;
    inline constexpr Address32 spRenderableRegistration = 0x0075E030;
    inline constexpr Address32 spRenderableRegistrationInitializer = 0x006D2960;
    inline constexpr Address32 spRenderableRegistrationGetter = 0x00423E20;
    inline constexpr Address32 spRenderableDestructor = 0x00423FB0;
    inline constexpr Address32 spRenderableCopy = 0x00423C70;
    inline constexpr Address32 spRenderableVTable = 0x006DC9B0;
    inline constexpr std::uint32_t spModelClassID = 0x763277DB;
    inline constexpr Address32 spModelRegistration = 0x00760CF8;
    inline constexpr Address32 spModelRegistrationInitializer = 0x006D3EE0;
    inline constexpr Address32 spModelFactory = 0x00479ED0;
    inline constexpr Address32 spModelRegistrationGetter = 0x00479A80;
    inline constexpr Address32 spModelDestructor = 0x00479A90;
    inline constexpr Address32 spModelDeletingDestructor = 0x00479F90;
    inline constexpr Address32 spModelClone = 0x00479F40;
    inline constexpr Address32 spModelCopy = 0x00479E60;
    inline constexpr Address32 spModelSetBaseMesh = 0x00479E20;
    inline constexpr Address32 spModelRender = 0x00479DC0;
    // The vtable-equivalent of the PS2 runtime-mode classifier is a no-op on
    // PC. Do not project the PS2 implementation onto this platform.
    inline constexpr Address32 spModelRuntimeModeSlot = 0x0048EAA0;
    inline constexpr Address32 spModelVTable = 0x006EAA58;
    inline constexpr std::uint32_t spSkinClassID = 0x681F2043;
    inline constexpr Address32 spSkinRegistration = 0x00760520;
    inline constexpr Address32 spSkinRegistrationInitializer = 0x006D3B20;
    inline constexpr Address32 spSkinFactoryProtectedEntry = 0x0046A120;
    inline constexpr Address32 spSkinSetPalette = 0x0046A100;
    inline constexpr Address32 spSkinRegistrationGetter = 0x0046A0F0;
    inline constexpr Address32 spSkinDestructor = 0x0046A1F0;
    inline constexpr Address32 spSkinDeletingDestructor = 0x0046A7A0;
    inline constexpr Address32 spSkinClone = 0x0046A1A0;
    inline constexpr Address32 spSkinCopy = 0x0046A650;
    inline constexpr Address32 spSkinRender = 0x0046A240;
    inline constexpr Address32 spSkinVTable = 0x006E8C5C;
    inline constexpr std::uint32_t spSkinSerializerClassID = 0x120D33C7;
    inline constexpr Address32 spSkinSerializerRegistration = 0x00762950;
    inline constexpr Address32 spSkinSerializerRegistrationInitializer = 0x006D4880;
    inline constexpr Address32 spSkinSerializerFactoryProtectedEntry = 0x00490C50;
    inline constexpr Address32 spSkinSerializerRegistrationGetter = 0x00490C10;
    inline constexpr Address32 spSkinSerializerDestructor = 0x00490C20;
    inline constexpr Address32 spSkinSerializerDeletingDestructor = 0x00490D10;
    inline constexpr Address32 spSkinSerializerClone = 0x00490CC0;
    inline constexpr Address32 spSkinSerializerWrite = 0x00490DA0;
    inline constexpr Address32 spSkinSerializerIndex = 0x00490D30;
    inline constexpr Address32 spSkinSerializerRead = 0x00491170;
    inline constexpr Address32 spSkinSerializerTargetClass = 0x00490C40;
    inline constexpr Address32 spSkinSerializerPrimaryVTable = 0x006EC7CC;
    inline constexpr Address32 spSkinSerializerSecondaryVTable = 0x006EC7C0;
    inline constexpr std::uint32_t spAnimationClassID = 0x56EE563A;
    inline constexpr Address32 spAnimationRegistration = 0x0075D248;
    inline constexpr Address32 spAnimationRegistrationInitializer = 0x006D1C10;
    inline constexpr Address32 spAnimationRegistrationInitializerCallSite = 0x006D1C30;
    inline constexpr Address32 spAnimationFactoryProtectedEntry = 0x0041A090;
    inline constexpr Address32 spAnimationFactoryResolvedBody = 0x013D1E00;
    inline constexpr Address32 spAnimationConstructor = 0x00430290;
    inline constexpr Address32 spAnimationConstructorResolvedBody = 0x013C64B0;
    inline constexpr std::uint32_t spAnimationSize = 0x84;
    inline constexpr Address32 spAnimationTrackAppend = 0x0042FED0;
    inline constexpr Address32 spAnimationTrackAppendResolvedBody = 0x013C91A0;
    inline constexpr Address32 spAnimationClone = 0x0041AA70;
    inline constexpr Address32 spAnimationTrackResize = 0x00430010;
    inline constexpr Address32 spAnimationDestructor = 0x00430130;
    inline constexpr Address32 spAnimationRegistrationGetter = 0x00430280;
    inline constexpr Address32 spAnimationDeletingDestructor = 0x00430380;
    inline constexpr Address32 spAnimationTagInsert = 0x004305F0;
    inline constexpr Address32 spAnimationVTable = 0x006DE6CC;
    inline constexpr std::uint32_t spTrackClassID = 0x60C839C5;
    inline constexpr Address32 spTrackRegistration = 0x00762B68;
    inline constexpr Address32 spTrackFactory = 0x00493070;
    inline constexpr Address32 spTrackVTable = 0x006ECBA4;
    inline constexpr std::uint32_t spTrackSize = 0x14;
    inline constexpr std::uint32_t spAnimTrackClassID = 0x33B61869;
    inline constexpr Address32 spAnimTrackRegistration = 0x00760C98;
    inline constexpr Address32 spAnimTrackFactory = 0x004791B0;
    inline constexpr Address32 spAnimTrackConstructor = 0x00478DE0;
    inline constexpr Address32 spAnimTrackVTable = 0x006EAA24;
    inline constexpr std::uint32_t spAnimTrackSize = 0x44;
    inline constexpr Address32 spAnimTrackDuration = 0x00478E30;
    inline constexpr Address32 spAnimTrackReleaseKeys = 0x00479760;
    inline constexpr Address32 spAnimTrackReleaseKeysResolvedBody = 0x013D2850;
    inline constexpr std::uint32_t spAnimationSerializerClassID = 0xC0ACBFA6;
    inline constexpr Address32 spAnimationSerializerRegistration = 0x0075EC48;
    inline constexpr Address32 spAnimationSerializerRegistrationInitializer = 0x006D2FC0;
    inline constexpr Address32 spAnimationSerializerFactoryProtectedEntry = 0x0043DAB0;
    inline constexpr Address32 spAnimationSerializerRegistrationGetter = 0x0043DA20;
    inline constexpr Address32 spAnimationSerializerTargetClass = 0x0043DA50;
    inline constexpr Address32 spAnimationSerializerWrite = 0x0043DFE0;
    inline constexpr Address32 spAnimationSerializerWriteKeys = 0x0043DDC0;
    inline constexpr Address32 spAnimationSerializerIndex = 0x005A7DB0;
    inline constexpr Address32 spAnimationSerializerRead = 0x0043ECC0;
    inline constexpr Address32 spAnimationSerializerStreamVTable = 0x006E0B00;
    inline constexpr Address32 spAnimationSerializerPrimaryVTable = 0x006E0B0C;
    inline constexpr std::uint32_t spControllerClassID = 0x4FAD24F1;
    inline constexpr Address32 spControllerRegistration = 0x0075DE50;
    inline constexpr Address32 spControllerRegistrationGetter = 0x00423070;
    inline constexpr Address32 spControllerVTable = 0x006DC86C;
    inline constexpr std::uint32_t spSubControllerClassID = 0x062C22ED;
    inline constexpr Address32 spSubControllerRegistration = 0x00760340;
    inline constexpr Address32 spSubControllerRegistrationGetter = 0x00467950;
    inline constexpr Address32 spSubControllerVTable = 0x006E83E4;
    inline constexpr std::uint32_t spTransformTrackEvalClassID = 0x5DAF152D;
    inline constexpr std::uint32_t spTransformEvalClassID = 0x87B0E260;
    inline constexpr Address32 spTransformTrackEvalRegistration = 0x00768E90;
    inline constexpr Address32 spTransformTrackEvalRegistrationInitializer =
        0x006D79C0;
    inline constexpr Address32 spTransformTrackEvalFactoryProtectedEntry =
        0x005FF090;
    inline constexpr Address32 spTransformTrackEvalDestructor = 0x005FEB90;
    inline constexpr Address32 spTransformTrackEvalEvaluate = 0x005FEBB0;
    inline constexpr Address32 spTransformTrackEvalConstructorProtectedEntry =
        0x005FF000;
    inline constexpr Address32 spTransformTrackEvalClone = 0x005FF0F0;
    inline constexpr Address32 spTransformTrackEvalRegistrationGetter =
        0x005FEBA0;
    inline constexpr Address32 spTransformTrackEvalVTable = 0x00711404;
    inline constexpr std::uint32_t spTransformTrackEvalVTableSlotCount = 9;
    inline constexpr Address32 spTransformTrackEvalInsertInput = 0x005FE9C0;
    inline constexpr Address32 spTransformTrackEvalClearInput = 0x005FEB70;
    inline constexpr Address32 spAnimationTrackSamplePRS = 0x00479290;
    inline constexpr Address32 spAnimationTrackFindInterval = 0x00478F90;
    inline constexpr std::uint32_t spNodeControllerClassID = 0x14A9784E;
    inline constexpr Address32 spNodeControllerRegistration = 0x00768EF0;
    inline constexpr Address32 spNodeControllerFactory = 0x005FF550;
    inline constexpr Address32 spNodeControllerConstructor = 0x005FF400;
    inline constexpr Address32 spNodeControllerDestructor = 0x005FF4D0;
    inline constexpr Address32 spNodeControllerApply = 0x005FF1B0;
    inline constexpr Address32 spNodeControllerBlend = 0x005FF250;
    inline constexpr Address32 spNodeControllerVTable = 0x00711444;
    inline constexpr std::uint32_t spActorClassID = 0x19D676E6;
    inline constexpr Address32 spActorRegistration = 0x00766480;
    inline constexpr Address32 spActorVTable = 0x00703F80;
    inline constexpr Address32 spActorTick = 0x005A2380;
    inline constexpr Address32 spActorBindInputsProtectedEntry = 0x005A1C10;
    inline constexpr std::uint32_t spResourceClassID = 0x46F043FE;
    inline constexpr Address32 spResourceRegistration = 0x007603A0;
    inline constexpr Address32 spResourceRegistrationInitializer = 0x006D3A60;
    inline constexpr Address32 spResourceFactory = 0x004679C0;
    inline constexpr Address32 spResourceDestructor = 0x00467960;
    inline constexpr Address32 spResourceDeletingDestructor = 0x00467AF0;
    inline constexpr Address32 spResourceClone = 0x00467A30;
    inline constexpr Address32 spResourceRegistrationGetter = 0x004679B0;
    inline constexpr Address32 spResourceVTable = 0x006E8414;
    inline constexpr std::uint32_t spResourceManagerClassID = 0xA4B9923B;
    inline constexpr Address32 spResourceManagerRegistration = 0x0075FB78;
    inline constexpr Address32 spResourceManagerRegistrationInitializer =
        0x006D3640;
    inline constexpr Address32 spResourceManagerFactoryProtectedEntry =
        0x00458D00;
    inline constexpr Address32 spResourceManagerRegistrationGetter = 0x00458B00;
    inline constexpr Address32 spResourceManagerDestructor = 0x00458AB0;
    inline constexpr Address32 spResourceManagerDeletingDestructor = 0x00458C90;
    inline constexpr Address32 spResourceManagerClone = 0x00458D60;
    inline constexpr Address32 spResourceManagerRegister = 0x00458CB0;
    inline constexpr Address32 spResourceManagerClassify = 0x004584B0;
    inline constexpr Address32 spResourceManagerVTable = 0x006E703C;
    inline constexpr Address32 spResourceManagerSupportVTable = 0x006E7038;
    inline constexpr Address32 spResourceManagerSingleton = 0x0075DB78;
    inline constexpr std::uint32_t spTextureClassID = 0x2F281E13;
    inline constexpr std::uint32_t spTextureRegisteredBaseClassID = 0x44DE07FD;
    inline constexpr Address32 spTextureRegistration = 0x0075DF10;
    inline constexpr Address32 spTextureRegistrationInitializer = 0x006D28D0;
    inline constexpr Address32 spTextureDestructor = 0x00423220;
    inline constexpr Address32 spTextureDeletingDestructor = 0x00423410;
    inline constexpr Address32 spTextureClone = 0x004A1BF0;
    inline constexpr Address32 spTextureRegistrationGetter = 0x00423240;
    inline constexpr Address32 spTextureInitializeBuffer = 0x00423250;
    inline constexpr Address32 spTextureInitializeWrapper = 0x004232E0;
    inline constexpr Address32 spTextureNormalizeDimensions = 0x00423380;
    inline constexpr Address32 spTextureVTable = 0x006DC8D8;
    inline constexpr std::uint32_t spTextureBufferClassID = 0x205B390B;
    inline constexpr Address32 spTextureBufferRegistration = 0x00760938;
    inline constexpr Address32 spTextureBufferRegistrationInitializer = 0x006D3CD0;
    inline constexpr Address32 spTextureBufferFactory = 0x00475DF0;
    inline constexpr Address32 spTextureBufferConstructor = 0x00475E00;
    inline constexpr Address32 spTextureBufferDestructor = 0x00475EC0;
    inline constexpr Address32 spTextureBufferClone = 0x00475E70;
    inline constexpr Address32 spTextureBufferRegistrationGetter = 0x00475DE0;
    inline constexpr Address32 spTextureBufferInitialize = 0x00475F30;
    inline constexpr Address32 spTextureBufferVTable = 0x006E94DC;
    inline constexpr std::uint32_t spTextureDataClassID = 0x78EA082B;
    inline constexpr Address32 spTextureDataRegistration = 0x0075D488;
    inline constexpr Address32 spTextureDataRegistrationInitializer = 0x006D1D30;
    inline constexpr Address32 spTextureDataFactory = 0x0041A2D0;
    inline constexpr Address32 spTextureDataConstructor = 0x00435340;
    inline constexpr Address32 spTextureDataDestructorBody = 0x00435280;
    inline constexpr Address32 spTextureDataDeletingDestructor = 0x004353C0;
    inline constexpr Address32 spTextureDataClone = 0x0041AC50;
    inline constexpr Address32 spTextureDataRegistrationGetter = 0x00435330;
    inline constexpr Address32 spTextureDataCopyTextureBufferInterface = 0x00435060;
    inline constexpr Address32 spTextureDataReleasePayloadsInterface = 0x00435140;
    inline constexpr Address32 spTextureDataInterfaceVTable = 0x006DE940;
    inline constexpr Address32 spTextureDataPrimaryVTable = 0x006DE950;
    inline constexpr std::uint32_t spTextureDataAllocationSize = 0x4A0;
    inline constexpr Address32 spITextureVTable = 0x006F1F40;
    inline constexpr std::uint32_t spIndexBufferClassID = 0x77D5669F;
    inline constexpr Address32 spIndexBufferRegistration = 0x0075FEE0;
    inline constexpr Address32 spIndexBufferRegistrationInitializer = 0x006D37F0;
    inline constexpr Address32 spIndexBufferFactory = 0x0045F9D0;
    inline constexpr Address32 spIndexBufferDestructor = 0x0045FAA0;
    inline constexpr Address32 spIndexBufferDeletingDestructor = 0x0045FD70;
    inline constexpr Address32 spIndexBufferClone = 0x0045FA50;
    inline constexpr Address32 spIndexBufferRegistrationGetter = 0x0045F820;
    inline constexpr Address32 spIndexBufferInitializeIndexCount = 0x0045F930;
    inline constexpr Address32 spIndexBufferInitialize = 0x0045FAD0;
    inline constexpr Address32 spIndexBufferRead = 0x0045FB80;
    inline constexpr Address32 spIndexBufferWrite = 0x0045FC90;
    inline constexpr Address32 spIndexBufferRelease = 0x0045F830;
    inline constexpr Address32 spIndexBufferVTable = 0x006E73B8;
    inline constexpr std::uint32_t spEngineCoreClassID = 0x0E9F6B8C;
    inline constexpr std::uint32_t spGameLevelSerializerClassID = 0x72B27469;
    inline constexpr Address32 spGameLevelSerializerRegistration = 0x00768E30;
    inline constexpr Address32 spGameLevelSerializerRegistrationInitializer = 0x006D7970;
    inline constexpr Address32 spGameLevelSerializerFactory = 0x005FDF80;
    inline constexpr Address32 spGameLevelSerializerDestructor = 0x005FDC90;
    inline constexpr Address32 spGameLevelSerializerDeletingDestructor = 0x005FE040;
    inline constexpr Address32 spGameLevelSerializerClone = 0x005FDFF0;
    inline constexpr Address32 spGameLevelSerializerRegistrationGetter = 0x005FDC80;
    inline constexpr Address32 spGameLevelSerializerParseElement = 0x005FDD50;
    inline constexpr Address32 spGameLevelSerializerRead = 0x005FE080;
    inline constexpr Address32 spGameLevelSerializerVTable = 0x00711308;
    inline constexpr std::uint32_t spGameLevelClassID = 0x4A45115B;
    inline constexpr Address32 spGameLevelRegistration = 0x007663C0;
    inline constexpr Address32 spGameLevelRegistrationInitializer = 0x006D6490;
    inline constexpr Address32 spGameLevelFactory = 0x005A0B80;
    inline constexpr Address32 spGameLevelConstructor = 0x005A0B20;
    inline constexpr Address32 spGameLevelDestructor = 0x005A0A20;
    inline constexpr Address32 spGameLevelDeletingDestructor = 0x005A0AC0;
    inline constexpr Address32 spGameLevelClone = 0x005A0BE0;
    inline constexpr Address32 spGameLevelRegistrationGetter = 0x005A0AB0;
    inline constexpr Address32 spGameLevelVTable = 0x00703E08;
    inline constexpr std::uint32_t spTemplateSerializerClassID = 0x41577707;
    inline constexpr Address32 spTemplateSerializerRegistration = 0x00768D70;
    inline constexpr Address32 spTemplateSerializerRegistrationInitializer = 0x006D7910;
    inline constexpr Address32 spTemplateSerializerFactory = 0x005FC480;
    inline constexpr Address32 spTemplateSerializerConstructor = 0x005FC2A0;
    inline constexpr Address32 spTemplateSerializerDestructor = 0x005FC220;
    inline constexpr Address32 spTemplateSerializerDeletingDestructor = 0x005FC2F0;
    inline constexpr Address32 spTemplateSerializerClone = 0x005FC4E0;
    inline constexpr Address32 spTemplateSerializerRegistrationGetter = 0x005FC290;
    inline constexpr Address32 spTemplateSerializerReadHeader = 0x005FC310;
    inline constexpr Address32 spTemplateSerializerParseElement = 0x005FC530;
    inline constexpr Address32 spTemplateSerializerVTable = 0x00711034;
    inline constexpr std::uint32_t spTemplateObjectClassID = 0x014E1394;
    inline constexpr Address32 spTemplateObjectRegistration = 0x00768DD0;
    inline constexpr Address32 spTemplateObjectRegistrationInitializer = 0x006D7940;
    inline constexpr Address32 spTemplateObjectFactory = 0x005FD970;
    inline constexpr Address32 spTemplateObjectConstructor = 0x005FD810;
    inline constexpr Address32 spTemplateObjectDestructor = 0x005FD920;
    inline constexpr Address32 spTemplateObjectDeletingDestructor = 0x005FDA20;
    inline constexpr Address32 spTemplateObjectClone = 0x005FD9D0;
    inline constexpr Address32 spTemplateObjectCopy = 0x005FD720;
    inline constexpr Address32 spTemplateObjectRegistrationGetter = 0x005FD910;
    inline constexpr Address32 spTemplateObjectReleaseResources = 0x005FD790;
    inline constexpr Address32 spTemplateObjectResolve = 0x005FDA40;
    inline constexpr Address32 spTemplateObjectVTable = 0x00711264;
    inline constexpr std::uint32_t spTemplateInstanceClassID = 0x1F6A7DA5;
    inline constexpr Address32 spTemplateInstanceRegistration = 0x00768D10;
    inline constexpr Address32 spTemplateInstanceRegistrationInitializer = 0x006D78E0;
    inline constexpr Address32 spTemplateInstanceFactory = 0x005FB870;
    inline constexpr Address32 spTemplateInstanceConstructor = 0x005FB6D0;
    inline constexpr Address32 spTemplateInstanceDestructor = 0x005FB100;
    inline constexpr Address32 spTemplateInstanceDeletingDestructor = 0x005FB350;
    inline constexpr Address32 spTemplateInstanceClone = 0x005FB8D0;
    inline constexpr Address32 spTemplateInstanceRegistrationGetter = 0x005FB1C0;
    inline constexpr Address32 spTemplateInstanceVTable = 0x00710F80;
    inline constexpr Address32 spTemplateInstanceNotification = 0x00420B40;
    inline constexpr Address32 spTemplateInstanceRootFactory = 0x00421E20;
    inline constexpr std::uint32_t spTemplateManagerClassID = 0x04BB6643;
    inline constexpr Address32 spTemplateManagerRegistration = 0x007600F0;
    inline constexpr Address32 spTemplateManagerRegistrationInitializer = 0x006D38F0;
    inline constexpr Address32 spTemplateManagerFactory = 0x00462B50;
    inline constexpr Address32 spTemplateManagerVTable = 0x006E77E0;
    inline constexpr Address32 spTemplateManagerSupportVTable = 0x006E77D8;
    inline constexpr Address32 spTemplateManagerRegistrationGetter = 0x00462A30;
    inline constexpr Address32 spTemplateManagerDestructor = 0x004629E0;
    inline constexpr Address32 spTemplateManagerSupportDestructor = 0x004627D0;
    inline constexpr Address32 spTemplateManagerDeletingDestructor = 0x00462A70;
    inline constexpr Address32 spTemplateManagerClone = 0x00462BB0;
    inline constexpr Address32 spTemplateManagerFind = 0x004628F0;
    inline constexpr Address32 spTemplateManagerClear = 0x004628A0;
    inline constexpr Address32 spTemplateManagerSingleton = 0x0075DC64;
    inline constexpr std::uint32_t spEntityManagerClassID = 0x48A15BCB;
    inline constexpr Address32 spEntityManagerRegistration = 0x0075DD28;
    inline constexpr Address32 spEntityManagerRegistrationInitializer = 0x006D27B0;
    inline constexpr Address32 spEntityManagerFactory = 0x00420190;
    inline constexpr Address32 spEntityManagerVTable = 0x006DC458;
    inline constexpr Address32 spEntityManagerRegistrationGetter = 0x00420050;
    inline constexpr Address32 spEntityManagerDeletingDestructor = 0x0041FD30;
    inline constexpr Address32 spEntityManagerClone = 0x004201F0;
    inline constexpr Address32 spEntityManagerAdd = 0x004200D0;
    inline constexpr Address32 spEntityManagerRemove = 0x00420070;
    inline constexpr Address32 spEntityManagerClear = 0x0041FE40;
    inline constexpr Address32 spEntityManagerDispatch = 0x0041FF90;
    inline constexpr Address32 spEntityManagerListGetter = 0x0041FD00;
    inline constexpr Address32 spEntityManagerSingleton = 0x0075CE98;
    inline constexpr std::uint32_t spDebugManagerClassID = 0x37054B40;
    inline constexpr Address32 spDebugManagerRegistration = 0x0075DC08;
    inline constexpr Address32 spDebugManagerRegistrationInitializer = 0x006D2720;
    inline constexpr Address32 spDebugManagerFactory = 0x0041E380;
    inline constexpr Address32 spDebugManagerProtectedConstructor = 0x0041D3A0;
    inline constexpr Address32 spDebugManagerRegistrationGetter = 0x0041D400;
    inline constexpr Address32 spDebugManagerDestructor = 0x0041E590;
    inline constexpr Address32 spDebugManagerDeletingDestructor = 0x0041E6E0;
    inline constexpr Address32 spDebugManagerClone = 0x0041E690;
    inline constexpr Address32 spDebugManagerVTable = 0x006DC3F0;
    inline constexpr Address32 spDebugManagerSupportVTable = 0x006DC3EC;
    inline constexpr Address32 spDebugManagerSingleton = 0x00755278;
    inline constexpr std::uint32_t spFontManagerClassID = 0x1640375E;
    inline constexpr std::uint32_t spPCFontManagerClassID = 0x7B467097;
    inline constexpr std::uint32_t spInputManagerClassID = 0x55A1304D;
    inline constexpr std::uint32_t spDXInputManagerClassID = 0x10F20027;
    inline constexpr Address32 spFontManagerRegistration = 0x0075DCC8;
    inline constexpr Address32 spPCFontManagerRegistration = 0x007647D0;
    inline constexpr Address32 spFontManagerRegistrationInitializer = 0x006D2780;
    inline constexpr Address32 spPCFontManagerRegistrationInitializer = 0x006D5790;
    inline constexpr Address32 spFontManagerRegistrationGetter = 0x0041FB70;
    inline constexpr Address32 spPCFontManagerRegistrationGetter = 0x004C3590;
    inline constexpr Address32 spFontManagerProtectedConstructorEntry = 0x0041FB00;
    inline constexpr Address32 spFontManagerDestructor = 0x0041FB90;
    inline constexpr Address32 spFontManagerInitialize = 0x0041E8B0;
    inline constexpr Address32 spFontManagerSetPrimaryMaterial = 0x0041E870;
    inline constexpr Address32 spFontManagerVTable = 0x006DC424;
    inline constexpr Address32 spFontManagerSupportVTable = 0x006DC420;
    inline constexpr Address32 spFontManagerSingleton = 0x00755270;
    inline constexpr Address32 spPCFontManagerFactory = 0x004C35A0;
    inline constexpr Address32 spPCFontManagerDestructor = 0x004C3620;
    inline constexpr Address32 spPCFontManagerClone = 0x004C3650;
    inline constexpr Address32 spPCFontManagerInitialize = 0x004C3830;
    inline constexpr Address32 spPCFontManagerVTable = 0x006F25C4;
    inline constexpr Address32 spPCFontManagerSupportVTable = 0x006F25C0;
    inline constexpr Address32 spInputManagerRegistration = 0x00764EF0;
    inline constexpr Address32 spDXInputManagerRegistration = 0x00764830;
    inline constexpr Address32 spInputManagerRegistrationInitializer = 0x006D5B20;
    inline constexpr Address32 spDXInputManagerRegistrationInitializer = 0x006D57C0;
    inline constexpr Address32 spInputManagerRegistrationGetter = 0x004CB050;
    inline constexpr Address32 spInputManagerDestructor = 0x004CB0A0;
    inline constexpr Address32 spInputManagerSupportDestructorThunk = 0x004CB060;
    inline constexpr Address32 spInputManagerVTable = 0x006F3064;
    inline constexpr Address32 spInputManagerSupportVTable = 0x006F3060;
    inline constexpr Address32 spInputManagerSingleton = 0x00755288;
    inline constexpr Address32 spDXInputManagerRegistrationGetter = 0x004C3860;
    inline constexpr Address32 spDXInputManagerFactory = 0x004C39D0;
    inline constexpr Address32 spDXInputManagerDestructor = 0x004C3AB0;
    inline constexpr Address32 spDXInputManagerDestructorBody = 0x004C3870;
    inline constexpr Address32 spDXInputManagerClone = 0x004C3A60;
    inline constexpr Address32 spDXInputManagerInitialize = 0x004C3BB0;
    inline constexpr Address32 spDXInputManagerPoll = 0x004C3920;
    inline constexpr Address32 spDXInputManagerVTable = 0x006F2688;
    inline constexpr Address32 spDXInputManagerSupportVTable = 0x006F2684;
    inline constexpr Address32 spDXInputManagerDeviceVTable = 0x006F2668;
    inline constexpr std::uint32_t spDXInputManagerControllerCapacity = 4;
    inline constexpr Address32 spEngineCoreRegistration = 0x0075DBA8;
    inline constexpr Address32 spEngineCoreRegistrationInitializer = 0x006D26F0;
    inline constexpr Address32 spEngineCoreRegistrationGetter = 0x0041C5D0;
    inline constexpr Address32 spEngineCoreFactory = 0x0041CB30;
    inline constexpr Address32 spEngineCoreProtectedConstructorEntry = 0x0041C7E0;
    inline constexpr Address32 spEngineCoreDestructor = 0x0041C500;
    inline constexpr Address32 spEngineCoreDeletingDestructor = 0x0041C640;
    inline constexpr Address32 spEngineCoreClone = 0x0041CB90;
    inline constexpr Address32 spEngineCoreVTable = 0x006DC318;
    inline constexpr Address32 spEngineCoreSupportVTable = 0x006DC310;
    inline constexpr Address32 spEngineCoreSingleton = 0x00755274;
    inline constexpr std::uint32_t spEngineCoreFirstLocalSlot = 0x1C;
    inline constexpr std::uint32_t spEngineCoreLastLocalSlot = 0x60;
    inline constexpr std::uint32_t spEngineCoreLocalSlotCount = 18;

    inline constexpr Address32 spEngineCoreLocalTargets[spEngineCoreLocalSlotCount]{
        0x0041C110, 0x0041B420, 0x0041CBE0, 0x0041B3B0,
        0x0041B6B0, 0x0041BE40, 0x0041C300, 0x0041B5F0,
        0x0041BDF0, 0x0041C210, 0x0041C2A0, 0x004D74A0,
        0x0041BD60, 0x0041B3E0, 0x0041B900, 0x0041BA70,
        0x004D74A0, 0x0041C0B0,
    };
}
