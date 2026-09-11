#pragma once

// Byte-exact PS2 evidence for the Code/Sparkplug layer.  The leading two words
// in PS2 vtables are ABI metadata and are retained in the address notes.

#include "SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
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
        std::uint8_t headerStack[0x0C];// 0x00: PS2 sequence state
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

    struct spSerializerRegistrationListLayout final
    {
        std::uint32_t size;            // 0x00
        Address32 sentinelNext;        // 0x04: self-linked when empty
        Address32 sentinelPrevious;    // 0x08: self-linked when empty
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

    struct spSerializerHookLayout final
    {
        spBaseObjectLayout base;       // 0x00: abstract, no additional storage
    };

    struct spPS2SerializerHookLayout final
    {
        spSerializerHookLayout base;   // 0x00: no additional storage
    };

    // Internal helper compiled by spResourceFATSerializer.cpp. The translation
    // unit is exact evidence; the literal C++ type spelling is still absent.
    struct spResourceFATHelperLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t nextResourceID;  // 0x10: initialized to 1
        std::uint8_t fileByID[0x10];   // 0x14: ordered-map ABI
        std::uint8_t resourceByID[0x10]; // 0x24: ordered-map ABI
        std::uint8_t resourceByObject[0x10]; // 0x34: ordered-map ABI
        std::uint8_t orderedFiles[0x10]; // 0x44: sequence ABI
        std::uint8_t orderedResources[0x10]; // 0x54: sequence/cursor ABI
    };

    struct spResourceFATFileEntryLayout final
    {
        Address32 vtable;              // 0x00
        std::uint32_t m_uFileID;       // 0x04: exact spelling in diagnostics
        Address32 m_szFilename;        // 0x08: exact spelling; owned string data
    };

    struct spResourceFATEntryLayout final
    {
        Address32 vtable;              // 0x00
        std::uint32_t m_uID;           // 0x04: exact spelling in diagnostics
        std::uint32_t fileID;          // 0x08: semantic role; source spelling unresolved
        Address32 m_szName;            // 0x0c: exact spelling; owned string data
        std::uint32_t m_ClassID;       // 0x10: exact spelling in diagnostics
        std::uint32_t m_uOffset;       // 0x14: exact spelling in diagnostics
        std::uint32_t m_uSize;         // 0x18: exact spelling in diagnostics
        std::uint8_t payloadWritten;   // 0x1c: one-shot save guard
        std::uint8_t padding1D[3];     // 0x1d
        Address32 object;              // 0x20: materialized runtime object
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

    struct spResourceManagerVectorLayout final
    {
        std::uint32_t capacityOrHighWater; // 0x00: allocator ABI detail
        std::uint32_t count;           // 0x04: exact iteration count
        Address32 storage;             // 0x08: 8-byte cache entries
    };

    // Exact 0x2c allocation from PS2 factory 0x0017DD80.
    struct spResourceManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        std::uint8_t unknown14;        // 0x14
        std::uint8_t reserveEnabled;   // 0x15
        std::uint8_t padding16[2];     // 0x16
        std::uint32_t reserveCount;    // 0x18
        std::int32_t field1C;          // 0x1c: constructor default -1
        spResourceManagerVectorLayout entries; // 0x20
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

    struct spTextureLayout final
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

    struct spTextureDataListLayout final
    {
        std::uint32_t capacityOrHighWater; // +0x00
        std::uint32_t count;           // +0x04
        Address32 storage;             // +0x08
    };

    struct spTextureDataLayout final
    {
        spTextureLayout base;          // 0x00
        spTextureBufferLayout textureBuffer; // 0x38
        std::uint8_t field68;          // 0x68: selects payload cleanup path
        std::uint8_t padding69[3];     // 0x69
        spTextureDataListLayout records6C; // 0x6c: 0x10-byte records
        std::uint8_t field78;          // 0x78
        std::uint8_t padding79[3];     // 0x79
        std::uint8_t platformState7C[0x410]; // 0x7c: roles unresolved
        spTextureDataListLayout records48C; // 0x48c: 0x14-byte records
    };

    struct spPlatformSpecificMeshDataLayout final
    {
        spNamedObjectLayout base;      // 0x00: no class-local storage
    };

    struct spDXMeshDataLayout final
    {
        spPlatformSpecificMeshDataLayout base; // 0x00
        std::uint8_t field14[4];       // 0x14: role unresolved
        Address32 indexBuffer;         // 0x18: owning spIndexBuffer*
        Address32 vertexBuffer;        // 0x1c: owning spVertexBuffer*
        std::uint8_t field20[0x24];    // 0x20: exact extent, roles unresolved
    };

    struct spPS2MeshDataLayout final
    {
        spPlatformSpecificMeshDataLayout base; // 0x00
        std::uint32_t field14;          // 0x14: constructor default 4
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
        Address32 packet;               // 0x40: aligned DMA/VIF/GIF packet
        std::uint32_t packetQwordCount; // 0x44
        std::uint32_t field48[22];      // 0x48: per-component descriptors
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
        Address32 field44;             // 0x44: initialized zero
        Address32 field48;             // 0x48: initialized zero
        Address32 field4C;             // 0x4c: initialized zero
    };

    struct spRenderMeshLayout final
    {
        spMeshLayout base;             // 0x00: no class-local storage
    };

    struct spPS2MeshLayout final
    {
        spRenderMeshLayout base;       // 0x00
        Address32 preparedMeshData;    // 0x50: owned spPS2MeshData*
        Address32 packetEmitter;       // 0x54: owned non-RTTI backend helper
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

    struct spRenderableCallbackListLayout final
    {
        std::uint32_t capacityOrHighWater; // +0x00: exact role not yet named
        std::uint32_t count;           // +0x04: iteration count
        Address32 storage;             // +0x08: 8-byte callback records
    };

    struct spRenderableLayout final
    {
        spNamedObjectLayout base;      // 0x00
        std::uint32_t runtimeMode;      // 0x14: recomputed by derived classes
        std::uint8_t alphaSortEnabled;  // 0x18: constructor default 1
        std::uint8_t padding19[3];      // 0x19
        std::uint32_t priority;         // 0x1c: constructor default 0
        Address32 material;             // 0x20: relationship/ref wrapper
        Address32 fog;                  // 0x24: intrusive relationship
        float field28;                  // 0x28: renderer-global default
        Address32 callback2C;           // 0x2c
        Address32 callback30;           // 0x30
        spRenderableCallbackListLayout callbacks34; // 0x34
        spRenderableCallbackListLayout callbacks40; // 0x40
        std::uint8_t useCallbacks34;    // 0x4c
        std::uint8_t useCallbacks40;    // 0x4d
        std::uint8_t padding4E[2];      // 0x4e
    };

    struct spModelLayout final
    {
        spRenderableLayout base;        // 0x00
        Address32 baseMeshData;         // 0x50: spMeshData relationship
        std::uint32_t projectionGroup;  // 0x54: constructor default 3
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
        Address32 parsedDocument;      // 0x018: transient parser state
        char instanceName[0x40];       // 0x01c
        char assetPath[0x100];         // 0x05c
        float position[3];             // 0x15c
        float rotation[4];             // 0x168: quaternion components
        float scale[3];                // 0x178
    };

    struct spGameLevelLayout final
    {
        spNamedObjectLayout base;      // 0x00
        std::uint32_t instanceCount;   // 0x14
        Address32 sentinelNext;        // 0x18: owned instance list
        Address32 sentinelPrevious;    // 0x1c
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
        float constructedDefaults[15];// 0x174: platform-global defaults
    };

    struct spTemplateInstanceLayout final
    {
        spNamedObjectLayout base;      // 0x00
        Address32 field14;             // 0x14: initialized to zero
        std::uint32_t attachedCount;   // 0x18
        Address32 sentinelNext;        // 0x1c
        Address32 sentinelPrevious;    // 0x20
        Address32 instanceRoot;        // 0x24: ref-counted spNode
    };

    struct spTemplateManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        std::uint32_t templateCount;   // 0x14
        Address32 sentinelNext;        // 0x18
        Address32 sentinelPrevious;    // 0x1c
        Address32 field20;             // 0x20: initialized to zero
    };

    struct spEntityManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 singletonSupportVTable; // 0x10
        std::uint32_t entityCount;     // 0x14
        Address32 sentinelNext;        // 0x18: inline list sentinel
        Address32 sentinelPrevious;    // 0x1c
    };

    struct spDebugManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10: singleton-support subobject
        std::uint32_t cycleIndex;      // 0x14: wraps after 20
        std::uint8_t flags[0x0C];      // 0x18: independent debug toggles
        Address32 resource24;          // 0x24: intrusive renderer object
        Address32 resource28;          // 0x28
        Address32 resource2C;          // 0x2c
        Address32 resource30;          // 0x30
        Address32 resource34;          // 0x34
    };

    struct spFontManagerLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 supportVTable;       // 0x14: singleton-support subobject
        Address32 fontStorage;         // 0x18: dynamic pointer array storage
        std::uint32_t fontCount;       // 0x1c
        std::uint32_t fontCapacity;    // 0x20: container high-water/capacity
        Address32 field24;             // 0x24: initialized to zero
        Address32 field28;             // 0x28: initialized to zero
        Address32 field2C;             // 0x2c: initialized from 0x00476cb8
        Address32 primaryMaterial;         // 0x30: intrusive reference
        Address32 fallbackMaterial;        // 0x34: intrusive reference
    };

    struct spPS2FontManagerLayout final
    {
        spFontManagerLayout base;      // 0x00; leaf adds no storage
    };

    struct spInputManagerObservedPrefixLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 supportVTable;       // 0x14: singleton-support subobject
        Address32 inputInterfaceVTable;// 0x18: PS2 device interface
        std::uint8_t initialized;      // 0x1c
        std::uint8_t padding1D[3];     // 0x1d
    };

    struct spPS2InputManagerLayout final
    {
        spInputManagerObservedPrefixLayout base; // 0x00
        Address32 controllers[2];       // 0x20
        std::uint8_t inputState28[0x18];// 0x28: pad aggregation state
        Address32 auxiliaryDevice40;    // 0x40
        Address32 auxiliaryDevice44;    // 0x44
    };

    // The spPS2Renderer constructor begins its derived writes at +0xcbb0,
    // after spRenderer construction has initialized every byte through
    // +0xcba8. The allocation is 16-byte aligned, fixing the exact common
    // extent. The 29-slot platform interface is rooted at +0x18.
    struct spRendererLayout final
    {
        spCrossPlatformLayout base;     // 0x0000
        Address32 supportVTable;        // 0x0014
        Address32 rendererInterfaceVTable; // 0x0018
        std::uint8_t state1C[0xC034];   // 0x001c
        std::uint8_t renderQueueEnabled;// 0xc050
        std::uint8_t stateC051[0x97F];  // 0xc051
        std::uint32_t renderStateCache[12]; // 0xc9d0: invalidated to ~0u
        std::uint32_t textureStateCache[96];// 0xca00: invalidated to ~0u
        std::uint8_t lifetimeTailCB80[0x30]; // 0xcb80
    };

    struct spPS2RendererLayout final
    {
        spRendererLayout base;          // 0x00000
        std::uint8_t ps2StateCBB0[0xD150]; // 0x0cbb0
    };

    struct spRenderTargetLayout final
    {
        spResourceLayout base;          // 0x00
        Address32 targetInterfaceVTable;// 0x14
        Address32 backingTexture;       // 0x18
        std::uint32_t width;            // 0x1c
        std::uint32_t height;           // 0x20
        std::uint32_t pixelFormat;      // 0x24: eTBPixelFormat
    };

    struct spCubeRenderTargetLayout final
    {
        spRenderTargetLayout base;      // 0x00
        Address32 cubeTexture;          // 0x28
    };

    struct spPS2RenderTargetLayout final
    {
        spRenderTargetLayout base;      // 0x00
        Address32 textureManagerHandle; // 0x28
        std::uint8_t active;            // 0x2c
        std::uint8_t padding2D[3];      // 0x2d
    };

    struct spPS2CubeRenderTargetLayout final
    {
        spCubeRenderTargetLayout base;  // 0x00: leaf adds no storage
    };

    struct spRenderTargetListStateLayout final
    {
        std::uint32_t count;
        Address32 sentinelNext;
        Address32 sentinelPrevious;
    };

    struct spRenderTargetManagerLayout final
    {
        spCrossPlatformLayout base;      // 0x00
        Address32 singletonSupportVTable;// 0x14
        spRenderTargetListStateLayout ordinaryTargets; // 0x18
        spRenderTargetListStateLayout cubeTargets;     // 0x24
        spRenderTargetListStateLayout layerTargets;    // 0x30
        std::uint32_t field3C;           // 0x3c: initialized zero
        std::uint32_t field40;           // 0x40: initialized one
    };

    struct spPS2RenderTargetManagerLayout final
    {
        spRenderTargetManagerLayout base;
    };

    struct spMaterialTextureLayout final
    {
        spBaseObjectLayout base;        // 0x00: direct registered/C++ base
        std::uint32_t textureStates[12];// 0x10: PS2 etsMaxTextureStates
        Address32 fallbackTexture;      // 0x40: intrusive spTexture*
        Address32 animationController; // 0x44: clone-resolved relationship
        float uvTransform[9];           // 0x48: 3x3 static transform
        std::uint8_t hasStaticUV;       // 0x6c
        std::uint8_t padding6D[3];      // 0x6d
        Address32 uvController;         // 0x70: clone-resolved relationship
        std::uint8_t tail74[0x0C];      // 0x74: allocator/alignment tail
    };

    struct spMaterialRenderTargetTextureLayout final
    {
        spMaterialTextureLayout base;  // 0x00
        std::uint32_t maxRecursionLevel;// 0x80: default 1
        std::uint32_t currentRecursionLevel; // 0x84: default 0
        std::uint32_t width;            // 0x88: default 256
        std::uint32_t height;           // 0x8c: default 256
        std::uint32_t pixelFormat;      // 0x90: eTBPixelFormat, default 0
        std::uint32_t targetCapacityOrHighWater; // 0x94
        std::uint32_t targetCount;      // 0x98
        Address32 targets;              // 0x9c
    };

    struct spMaterialCameraViewTextureLayout final
    {
        spMaterialRenderTargetTextureLayout base; // 0x00
        Address32 cameraName;           // 0xa0: owned/shared string entry
        Address32 resolvedCamera;       // 0xa4: runtime camera
        std::uint8_t paddingA8[8];      // 0xa8: aligned allocation tail
    };

    struct spMaterialCubeMapTextureLayout final
    {
        spMaterialRenderTargetTextureLayout base; // 0x00
        Address32 sourceRenderNode;     // 0xa0
        Address32 ownedCamera;          // 0xa4
        std::uint32_t facesPerTick;     // 0xa8: default 6, clamped 1..6
        std::uint32_t currentFace;      // 0xac: default 0
    };

    struct spEngineCoreLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        std::uint8_t initialized;      // 0x14
        std::uint8_t padding15[3];     // 0x15
        Address32 defaultScene;        // 0x18: owned polymorphic object
        Address32 defaultCamera;       // 0x1c: intrusive reference
        std::uint8_t container20[0x0C];// 0x20: compiler-specific container
        Address32 firstFrameCallback;  // 0x2c
        Address32 secondFrameCallback; // 0x30
        Address32 field34;             // 0x34: initialized from 0x00476cf8
        Address32 field38;             // 0x38: manager pointer
        Address32 field3C;             // 0x3c: manager pointer
        Address32 field40;             // 0x40: manager pointer
        Address32 field44;             // 0x44: manager pointer
        Address32 field48;             // 0x48: manager pointer
        std::uint8_t containers4C[0x100]; // 0x4c: two inline containers
        Address32 field14C;            // 0x14c: released during shutdown
    };

    struct spNodeLayout final
    {
        spNamedObjectLayout base;      // 0x00
        std::uint8_t childList[0x0C];  // 0x14: PS2 list head/count state
        float position[3];             // 0x20: local position
        Address32 parent;              // 0x2c: non-owning spNode*
        float scale[3];                // 0x30: local scale
        Address32 sceneLink;           // 0x3c: registered runtime object
        float orientation[9];          // 0x40: local 3x3 orientation
        Address32 collisionData;       // 0x64: dynamic pointer array
        std::uint32_t collisionCount;  // 0x68
        std::uint32_t collisionCapacity; // 0x6c
        float worldPosition[3];        // 0x70: cached
        std::uint32_t padding7C;       // 0x7c
        float worldScale[3];           // 0x80: cached
        std::uint32_t padding8C;       // 0x8c
        float worldOrientation[9];     // 0x90: cached 3x3 orientation
        std::uint32_t flags;           // 0xb4
        std::uint8_t paddingB8[8];     // 0xb8: 16-byte allocation alignment
    };

    // Exact 0x250 allocations are present in both the spCameraData factory
    // and the base portion of the aligned spPS2Camera factory.
    struct spCameraLayout final
    {
        spNodeLayout base;             // 0x000
        float verticalHalfExtent;      // 0x0c0: near*tan(viewAngle/2)
        float verticalHalfExtentCopy;  // 0x0c4
        float nearClipPlane;           // 0x0c8
        float farClipPlane;            // 0x0cc
        float aspectHalfExtent;        // 0x0d0
        std::uint8_t twoDimensional;   // 0x0d4: serialized Is2DMode
        std::uint8_t paddingD5[3];     // 0x0d5
        std::uint8_t stateD8[8];       // 0x0d8: role unresolved
        float viewMatrix[16];          // 0x0e0
        float projectionMatrix[16];    // 0x120
        float viewBasis[9];            // 0x160
        std::uint8_t state184[0x18];   // 0x184: role unresolved
        float viewAngle;               // 0x19c
        float scaledViewAngle;         // 0x1a0
        float pixelAspectRatio;        // 0x1a4
        float derivedBasis[12];        // 0x1a8
        float frustumPlanes[24];       // 0x1d8
        std::uint32_t dirtyFlags;      // 0x238
        std::uint32_t field23C;        // 0x23c: role unresolved
        std::uint32_t backendMode;     // 0x240
        std::uint8_t viewportActive;   // 0x244
        std::uint8_t projectionBranch;// 0x245: not serialized Is2DMode
        std::uint8_t padding246[2];    // 0x246
        float viewportRatio;           // 0x248: height/width
        std::uint8_t padding24C[4];    // 0x24c: aligned extent
    };

    struct spCameraDataLayout final
    {
        spCameraLayout base;           // 0x000: storage-free leaf
    };

    struct spPS2CameraLayout final
    {
        spCameraLayout base;           // 0x000
        std::uint8_t platformMatrices250[0x80]; // 0x250
        float platformFrustumPlanes[24]; // 0x2d0
        std::uint32_t viewportWidth;    // 0x330
        std::uint32_t viewportHeight;   // 0x334
        std::uint8_t tail338[8];        // 0x338
    };

    // Exact 0x1e0 allocation from factory 0x001ab160. Names below stop at the
    // boundaries proven by constructor/destructor and render traversal; the
    // matrix/cache internals remain deliberately opaque.
    struct spRenderNodeLayout final
    {
        spNodeLayout base;             // 0x000
        std::uint32_t fieldC0;         // 0x0c0: initialized zero
        std::uint32_t fieldC4;         // 0x0c4: initialized zero
        Address32 renderSupportVTable; // 0x0c8: secondary subobject
        std::uint32_t renderableCapacityOrHighWater; // 0x0cc
        std::uint32_t renderableCount; // 0x0d0
        Address32 renderableStorage;   // 0x0d4: intrusive spRenderable**
        std::uint8_t aggregateBoundsD8[0x20]; // 0x0d8
        Address32 matrix140;           // 0x0f8: self + 0x140
        Address32 matrix180;           // 0x0fc: self + 0x180
        std::uint8_t renderState100[0x34]; // 0x100
        Address32 self;                // 0x134
        std::uint8_t field138;         // 0x138: initialized zero
        std::uint8_t padding139[3];    // 0x139
        std::uint32_t dirtyFlags;      // 0x13c: initialized zero
        float matrixBlock140[16];      // 0x140
        float matrixBlock180[16];      // 0x180
        float inverseScale[3];         // 0x1c0: constructor default 1,1,1
        std::uint8_t callbackState1CC[0x14]; // 0x1cc
    };

    struct spLightLayout final
    {
        spNodeLayout base;             // 0x00
        Address32 sceneLightVTable;    // 0xc0: embedded support subobject
        Address32 sceneLightFieldC4;   // 0xc4: initialized zero
        Address32 sceneLightFieldC8;   // 0xc8: initialized zero
        std::uint32_t type;            // 0xcc: 0..3 light type
        float colorRGBA[4];            // 0xd0: normalized R,G,B,A
        std::uint8_t attenuation;      // 0xe0: serializer field 3
        std::uint8_t paddingE1[3];     // 0xe1
        float intensity;               // 0xe4: serializer field 4
        std::uint32_t opaqueRuntimeE8; // 0xe8: copied, not initialized/serialized
        float range;                   // 0xec: serializer field 5
        float hotspotAngle;            // 0xf0: serializer field 6
        float falloffAngle;            // 0xf4: serializer field 7
        std::uint8_t projectShadow;    // 0xf8: serializer field 1
        std::uint8_t enabled;          // 0xf9: serializer field 8
        std::uint8_t paddingFA[6];     // 0xfa: 16-byte allocation alignment
    };

    struct spLightDataLayout final
    {
        spLightLayout base;            // 0x00: no derived storage
    };

    struct spMaterialLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t padding10;       // 0x10: not initialized by constructor
        Address32 materialVTable;      // 0x14: secondary material interface
        std::uint8_t padding18[8];     // 0x18: role unresolved
        std::uint32_t renderStates[11];// 0x20: serialized field 0
        std::uint32_t padding4C;       // 0x4c: role unresolved
        std::uint32_t passCount;       // 0x50
        Address32 passes[8];           // 0x54: fixed native capacity
        std::uint8_t renderOverride;   // pre/post-render state-save flag
        std::uint8_t useVertexAlpha;   // serialized field 1
        std::uint8_t padding76[2];
        std::uint32_t opaqueRuntime78; // cached renderer generation in PS2 material
        Address32 materialColorController; // serialized relationship field 6
    };

    struct spMaterialDataLayout final
    {
        spMaterialLayout base;         // 0x00
        float diffuseRGBA[4];          // 0x80
        float ambientRGBA[4];          // 0x90
        float specularRGBA[4];         // 0xa0
        float emissiveRGBA[4];         // 0xb0
        float specularPower;           // 0xc0
        std::uint8_t paddingC4[0x0C];  // exact aligned allocation extent
    };

    struct spPS2MaterialLayout final
    {
        spMaterialLayout base;         // 0x00
        float diffuseRGBA[4];          // 0x80
        float ambientRGBA[4];          // 0x90
        float specularRGBA[4];         // 0xa0
        float emissiveRGBA[4];         // 0xb0
        float specularPower;           // 0xc0
        std::uint8_t paddingC4[0x0C];  // exact aligned allocation extent
    };

    struct spMaterialPassLayerLayout final
    {
        spBaseObjectLayout base;       // 0x00: direct registered base
        std::uint32_t finalBlendOperation; // 0x10: serializer pass payload
        std::uint32_t layerCount;      // 0x14
        Address32 layers[8];           // 0x18: fixed intrusive relationships
    };

    struct spMaterialTextureLayerLayout final
    {
        spBaseObjectLayout base;       // 0x00: direct registered base
        Address32 materialTexture;     // 0x10: owned spMaterialTexture family
    };

    struct spStdLayerLayout final
    {
        spMaterialTextureLayerLayout base; // no derived storage
    };

    struct spFogLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t padding10;       // 0x10: not initialized by constructor
        std::uint32_t type;            // 0x14: 0/1/2/3 = off/exp/exp2/linear
        std::uint32_t colorARGB;       // 0x18
        float start;                   // 0x1c
        float end;                     // 0x20
        float density;                 // 0x24
    };

    struct spSerializerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 serializerVTable;    // 0x10: secondary interface
    };

    struct spNodeSerializerLayout final
    {
        spSerializerLayout base;       // 0x00: no derived storage
    };

    struct spRenderNodeSerializerLayout final
    {
        spNodeSerializerLayout base;   // 0x00: no derived storage
    };

    struct spLightDataSerializerLayout final
    {
        spNodeSerializerLayout base;  // C++ base; RTTI registration is flattened
    };

    struct spLightSerializerLayout final
    {
        spNodeSerializerLayout base;
    };

    struct spCameraSerializerLayout final
    {
        spNodeSerializerLayout base;
    };

    struct spCameraDataSerializerLayout final
    {
        spCameraSerializerLayout base;
    };

    struct spFogSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spMatColorControllerSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spLightControllerSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spAnimTexControllerSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spUVControllerSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spTransFunctionEvalSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spFunctionEvalSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spColorFuncEvalSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spSphereBVSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spBoxBVSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spOBBBVSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spRenderableSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spMaterialSerializerLayout final
    {
        spSerializerLayout base;
        std::uint8_t dataBlockSerializerState[0x28];
    };

    struct spMaterialDataSerializerLayout final
    {
        spMaterialSerializerLayout base;
    };

    struct spDXMaterialDataSerializerLayout final
    {
        spMaterialSerializerLayout base;
    };

    struct spPS2MaterialDataSerializerLayout final
    {
        spMaterialSerializerLayout base;
    };

    struct spModelSerializerLayout final
    {
        spRenderableSerializerLayout base;
    };

    struct spMeshDataSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spPS2MeshDataSerializerLayout final
    {
        spMeshDataSerializerLayout base;
    };

    struct spDXMeshDataSerializerLayout final
    {
        spMeshDataSerializerLayout base;
    };

    struct spTextureDataSerializerLayout final
    {
        spSerializerLayout base;
    };

    struct spPS2TextureDataSerializerLayout final
    {
        spTextureDataSerializerLayout base;
    };

    struct spDXTextureDataSerializerLayout final
    {
        spTextureDataSerializerLayout base;
    };

    static_assert(sizeof(spMeshLayout) == 0x50);
    static_assert(offsetof(spMeshLayout, boundsValid) == 0x28);
    static_assert(offsetof(spMeshLayout, boundsMinimum) == 0x2C);
    static_assert(offsetof(spMeshLayout, field4C) == 0x4C);
    static_assert(sizeof(spRenderMeshLayout) == 0x50);
    static_assert(sizeof(spPS2MeshLayout) == 0x58);
    static_assert(offsetof(spPS2MeshLayout, preparedMeshData) == 0x50);
    static_assert(offsetof(spPS2MeshLayout, packetEmitter) == 0x54);
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
    static_assert(sizeof(spResourceManagerVectorLayout) == 0x0C);
    static_assert(sizeof(spResourceManagerLayout) == 0x2C);
    static_assert(offsetof(spResourceManagerLayout, reserveEnabled) == 0x15);
    static_assert(offsetof(spResourceManagerLayout, reserveCount) == 0x18);
    static_assert(offsetof(spResourceManagerLayout, entries) == 0x20);
    static_assert(sizeof(spTextureBufferLayout) == 0x30);
    static_assert(offsetof(spTextureBufferLayout, buffer) == 0x1C);
    static_assert(offsetof(spTextureBufferLayout, auxiliaryObject) == 0x24);
    static_assert(offsetof(spTextureBufferLayout, initialized) == 0x2C);
    static_assert(sizeof(spTextureDataListLayout) == 0x0C);
    static_assert(sizeof(spTextureDataLayout) == 0x498);
    static_assert(offsetof(spTextureDataLayout, textureBuffer) == 0x38);
    static_assert(offsetof(spTextureDataLayout, field68) == 0x68);
    static_assert(offsetof(spTextureDataLayout, field78) == 0x78);
    static_assert(offsetof(spTextureDataLayout, records48C) == 0x48C);
    static_assert(sizeof(spTextureLayout) == 0x38);
    static_assert(offsetof(spTextureLayout, iTextureVTable) == 0x14);
    static_assert(offsetof(spTextureLayout, width) == 0x28);
    static_assert(offsetof(spTextureLayout, field34) == 0x34);
    static_assert(sizeof(spPlatformSpecificMeshDataLayout) == 0x14);
    static_assert(sizeof(spDXMeshDataLayout) == 0x44);
    static_assert(offsetof(spDXMeshDataLayout, indexBuffer) == 0x18);
    static_assert(offsetof(spDXMeshDataLayout, vertexBuffer) == 0x1C);
    static_assert(sizeof(spPS2MeshDataLayout) == 0x100);
    static_assert(offsetof(spPS2MeshDataLayout, packet) == 0x40);
    static_assert(offsetof(spPS2MeshDataLayout, field48) == 0x48);
    static_assert(offsetof(spPS2MeshDataLayout, fieldA0) == 0xA0);
    static_assert(offsetof(spPS2MeshDataLayout, fieldFC) == 0xFC);
    static_assert(sizeof(spRenderableCallbackListLayout) == 0x0C);
    static_assert(sizeof(spRenderableLayout) == 0x50);
    static_assert(offsetof(spRenderableLayout, material) == 0x20);
    static_assert(offsetof(spRenderableLayout, callbacks40) == 0x40);
    static_assert(sizeof(spModelLayout) == 0x58);
    static_assert(offsetof(spModelLayout, baseMeshData) == 0x50);
    static_assert(offsetof(spModelLayout, projectionGroup) == 0x54);
    static_assert(sizeof(spIndexBufferLayout) == 0x28);
    static_assert(offsetof(spIndexBufferLayout, primitiveCount) == 0x18);
    static_assert(offsetof(spIndexBufferLayout, indexData) == 0x24);
    static_assert(sizeof(spEngineCoreLayout) == 0x150);
    static_assert(sizeof(spGameLevelSerializerLayout) == 0x184);
    static_assert(offsetof(spGameLevelSerializerLayout, instanceName) == 0x1C);
    static_assert(offsetof(spGameLevelSerializerLayout, assetPath) == 0x5C);
    static_assert(offsetof(spGameLevelSerializerLayout, position) == 0x15C);
    static_assert(sizeof(spGameLevelLayout) == 0x2C);
    static_assert(offsetof(spGameLevelLayout, instanceCount) == 0x14);
    static_assert(offsetof(spGameLevelLayout, sentinelNext) == 0x18);
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
    static_assert(offsetof(spTemplateInstanceLayout, attachedCount) == 0x18);
    static_assert(offsetof(spTemplateInstanceLayout, instanceRoot) == 0x24);
    static_assert(sizeof(spTemplateManagerLayout) == 0x24);
    static_assert(offsetof(spTemplateManagerLayout, templateCount) == 0x14);
    static_assert(offsetof(spTemplateManagerLayout, field20) == 0x20);
    static_assert(sizeof(spEntityManagerLayout) == 0x20);
    static_assert(offsetof(spEntityManagerLayout, entityCount) == 0x14);
    static_assert(offsetof(spEntityManagerLayout, sentinelNext) == 0x18);
    static_assert(sizeof(spDebugManagerLayout) == 0x38);
    static_assert(offsetof(spDebugManagerLayout, supportVTable) == 0x10);
    static_assert(offsetof(spDebugManagerLayout, cycleIndex) == 0x14);
    static_assert(offsetof(spDebugManagerLayout, flags) == 0x18);
    static_assert(offsetof(spDebugManagerLayout, resource24) == 0x24);
    static_assert(offsetof(spDebugManagerLayout, resource34) == 0x34);
    static_assert(sizeof(spFontManagerLayout) == 0x38);
    static_assert(offsetof(spFontManagerLayout, supportVTable) == 0x14);
    static_assert(offsetof(spFontManagerLayout, fontCount) == 0x1C);
    static_assert(offsetof(spFontManagerLayout, field2C) == 0x2C);
    static_assert(offsetof(spFontManagerLayout, primaryMaterial) == 0x30);
    static_assert(offsetof(spFontManagerLayout, fallbackMaterial) == 0x34);
    static_assert(sizeof(spPS2FontManagerLayout) == 0x38);
    static_assert(sizeof(spInputManagerObservedPrefixLayout) == 0x20);
    static_assert(offsetof(spInputManagerObservedPrefixLayout,
        inputInterfaceVTable) == 0x18);
    static_assert(offsetof(spInputManagerObservedPrefixLayout,
        initialized) == 0x1C);
    static_assert(sizeof(spPS2InputManagerLayout) == 0x48);
    static_assert(offsetof(spPS2InputManagerLayout, controllers) == 0x20);
    static_assert(sizeof(spRendererLayout) == 0xCBB0);
    static_assert(offsetof(spRendererLayout, rendererInterfaceVTable) == 0x18);
    static_assert(offsetof(spRendererLayout, renderQueueEnabled) == 0xC050);
    static_assert(offsetof(spRendererLayout, renderStateCache) == 0xC9D0);
    static_assert(offsetof(spRendererLayout, textureStateCache) == 0xCA00);
    static_assert(sizeof(spPS2RendererLayout) == 0x19D00);
    static_assert(sizeof(spRenderTargetLayout) == 0x28);
    static_assert(offsetof(spRenderTargetLayout, targetInterfaceVTable) == 0x14);
    static_assert(offsetof(spRenderTargetLayout, width) == 0x1C);
    static_assert(offsetof(spRenderTargetLayout, pixelFormat) == 0x24);
    static_assert(sizeof(spCubeRenderTargetLayout) == 0x2C);
    static_assert(sizeof(spPS2RenderTargetLayout) == 0x30);
    static_assert(offsetof(spPS2RenderTargetLayout, active) == 0x2C);
    static_assert(sizeof(spPS2CubeRenderTargetLayout) == 0x2C);
    static_assert(sizeof(spRenderTargetListStateLayout) == 0x0C);
    static_assert(sizeof(spRenderTargetManagerLayout) == 0x44);
    static_assert(offsetof(spRenderTargetManagerLayout, ordinaryTargets) == 0x18);
    static_assert(offsetof(spRenderTargetManagerLayout, cubeTargets) == 0x24);
    static_assert(offsetof(spRenderTargetManagerLayout, layerTargets) == 0x30);
    static_assert(sizeof(spMaterialTextureLayout) == 0x80);
    static_assert(offsetof(spMaterialTextureLayout, fallbackTexture) == 0x40);
    static_assert(offsetof(spMaterialTextureLayout, uvController) == 0x70);
    static_assert(sizeof(spMaterialRenderTargetTextureLayout) == 0xA0);
    static_assert(offsetof(spMaterialRenderTargetTextureLayout,
        maxRecursionLevel) == 0x80);
    static_assert(offsetof(spMaterialRenderTargetTextureLayout,
        targets) == 0x9C);
    static_assert(sizeof(spMaterialCameraViewTextureLayout) == 0xB0);
    static_assert(sizeof(spMaterialCubeMapTextureLayout) == 0xB0);
    static_assert(offsetof(spMaterialCubeMapTextureLayout,
        facesPerTick) == 0xA8);
    static_assert(offsetof(spPS2InputManagerLayout, auxiliaryDevice40) == 0x40);
    static_assert(offsetof(spEngineCoreLayout, initialized) == 0x14);
    static_assert(offsetof(spEngineCoreLayout, defaultScene) == 0x18);
    static_assert(offsetof(spEngineCoreLayout, defaultCamera) == 0x1C);
    static_assert(offsetof(spEngineCoreLayout, firstFrameCallback) == 0x2C);
    static_assert(offsetof(spEngineCoreLayout, field38) == 0x38);
    static_assert(offsetof(spEngineCoreLayout, containers4C) == 0x4C);
    static_assert(offsetof(spEngineCoreLayout, field14C) == 0x14C);
    static_assert(sizeof(spNodeLayout) == 0xC0);
    static_assert(offsetof(spNodeLayout, position) == 0x20);
    static_assert(offsetof(spNodeLayout, parent) == 0x2C);
    static_assert(offsetof(spNodeLayout, scale) == 0x30);
    static_assert(offsetof(spNodeLayout, sceneLink) == 0x3C);
    static_assert(offsetof(spNodeLayout, orientation) == 0x40);
    static_assert(offsetof(spNodeLayout, collisionCount) == 0x68);
    static_assert(offsetof(spNodeLayout, worldPosition) == 0x70);
    static_assert(offsetof(spNodeLayout, worldOrientation) == 0x90);
    static_assert(offsetof(spNodeLayout, flags) == 0xB4);
    static_assert(sizeof(spCameraLayout) == 0x250);
    static_assert(offsetof(spCameraLayout, nearClipPlane) == 0xC8);
    static_assert(offsetof(spCameraLayout, twoDimensional) == 0xD4);
    static_assert(offsetof(spCameraLayout, viewMatrix) == 0xE0);
    static_assert(offsetof(spCameraLayout, projectionMatrix) == 0x120);
    static_assert(offsetof(spCameraLayout, viewAngle) == 0x19C);
    static_assert(offsetof(spCameraLayout, pixelAspectRatio) == 0x1A4);
    static_assert(offsetof(spCameraLayout, frustumPlanes) == 0x1D8);
    static_assert(offsetof(spCameraLayout, dirtyFlags) == 0x238);
    static_assert(offsetof(spCameraLayout, viewportActive) == 0x244);
    static_assert(offsetof(spCameraLayout, viewportRatio) == 0x248);
    static_assert(sizeof(spCameraDataLayout) == 0x250);
    static_assert(sizeof(spPS2CameraLayout) == 0x340);
    static_assert(offsetof(spPS2CameraLayout, platformMatrices250) == 0x250);
    static_assert(offsetof(spPS2CameraLayout, platformFrustumPlanes) == 0x2D0);
    static_assert(offsetof(spPS2CameraLayout, viewportWidth) == 0x330);
    static_assert(sizeof(spRenderNodeLayout) == 0x1E0);
    static_assert(offsetof(spRenderNodeLayout, renderSupportVTable) == 0xC8);
    static_assert(offsetof(spRenderNodeLayout, renderableCount) == 0xD0);
    static_assert(offsetof(spRenderNodeLayout, renderableStorage) == 0xD4);
    static_assert(offsetof(spRenderNodeLayout, matrix140) == 0xF8);
    static_assert(offsetof(spRenderNodeLayout, self) == 0x134);
    static_assert(offsetof(spRenderNodeLayout, dirtyFlags) == 0x13C);
    static_assert(offsetof(spRenderNodeLayout, matrixBlock140) == 0x140);
    static_assert(offsetof(spRenderNodeLayout, matrixBlock180) == 0x180);
    static_assert(offsetof(spRenderNodeLayout, inverseScale) == 0x1C0);
    static_assert(sizeof(spLightLayout) == 0x100);
    static_assert(offsetof(spLightLayout, sceneLightVTable) == 0xC0);
    static_assert(offsetof(spLightLayout, type) == 0xCC);
    static_assert(offsetof(spLightLayout, colorRGBA) == 0xD0);
    static_assert(offsetof(spLightLayout, attenuation) == 0xE0);
    static_assert(offsetof(spLightLayout, intensity) == 0xE4);
    static_assert(offsetof(spLightLayout, opaqueRuntimeE8) == 0xE8);
    static_assert(offsetof(spLightLayout, range) == 0xEC);
    static_assert(offsetof(spLightLayout, projectShadow) == 0xF8);
    static_assert(offsetof(spLightLayout, enabled) == 0xF9);
    static_assert(sizeof(spLightDataLayout) == 0x100);
    static_assert(sizeof(spMaterialLayout) == 0x80);
    static_assert(offsetof(spMaterialLayout, materialVTable) == 0x14);
    static_assert(offsetof(spMaterialLayout, renderStates) == 0x20);
    static_assert(offsetof(spMaterialLayout, passCount) == 0x50);
    static_assert(offsetof(spMaterialLayout, passes) == 0x54);
    static_assert(offsetof(spMaterialLayout, renderOverride) == 0x74);
    static_assert(offsetof(spMaterialLayout, useVertexAlpha) == 0x75);
    static_assert(offsetof(spMaterialLayout, materialColorController) == 0x7C);
    static_assert(sizeof(spMaterialDataLayout) == 0xD0);
    static_assert(offsetof(spMaterialDataLayout, diffuseRGBA) == 0x80);
    static_assert(offsetof(spMaterialDataLayout, ambientRGBA) == 0x90);
    static_assert(offsetof(spMaterialDataLayout, specularRGBA) == 0xA0);
    static_assert(offsetof(spMaterialDataLayout, emissiveRGBA) == 0xB0);
    static_assert(offsetof(spMaterialDataLayout, specularPower) == 0xC0);
    static_assert(sizeof(spPS2MaterialLayout) == 0xD0);
    static_assert(offsetof(spPS2MaterialLayout, diffuseRGBA) == 0x80);
    static_assert(offsetof(spPS2MaterialLayout, ambientRGBA) == 0x90);
    static_assert(offsetof(spPS2MaterialLayout, specularRGBA) == 0xA0);
    static_assert(offsetof(spPS2MaterialLayout, emissiveRGBA) == 0xB0);
    static_assert(offsetof(spPS2MaterialLayout, specularPower) == 0xC0);
    static_assert(sizeof(spMaterialPassLayerLayout) == 0x38);
    static_assert(offsetof(spMaterialPassLayerLayout,
        finalBlendOperation) == 0x10);
    static_assert(offsetof(spMaterialPassLayerLayout, layerCount) == 0x14);
    static_assert(offsetof(spMaterialPassLayerLayout, layers) == 0x18);
    static_assert(sizeof(spMaterialTextureLayerLayout) == 0x14);
    static_assert(offsetof(spMaterialTextureLayerLayout, materialTexture) == 0x10);
    static_assert(sizeof(spStdLayerLayout) == 0x14);
    static_assert(sizeof(spFogLayout) == 0x28);
    static_assert(offsetof(spFogLayout, type) == 0x14);
    static_assert(offsetof(spFogLayout, density) == 0x24);
    static_assert(sizeof(spSerializerLayout) == 0x14);
    static_assert(offsetof(spSerializerLayout, serializerVTable) == 0x10);
    static_assert(sizeof(spNodeSerializerLayout) == 0x14);
    static_assert(sizeof(spRenderNodeSerializerLayout) == 0x14);
    static_assert(sizeof(spLightDataSerializerLayout) == 0x14);
    static_assert(sizeof(spLightSerializerLayout) == 0x14);
    static_assert(sizeof(spCameraSerializerLayout) == 0x14);
    static_assert(sizeof(spCameraDataSerializerLayout) == 0x14);
    static_assert(sizeof(spFogSerializerLayout) == 0x14);
    static_assert(sizeof(spMatColorControllerSerializerLayout) == 0x14);
    static_assert(sizeof(spLightControllerSerializerLayout) == 0x14);
    static_assert(sizeof(spAnimTexControllerSerializerLayout) == 0x14);
    static_assert(sizeof(spUVControllerSerializerLayout) == 0x14);
    static_assert(sizeof(spTransFunctionEvalSerializerLayout) == 0x14);
    static_assert(sizeof(spFunctionEvalSerializerLayout) == 0x14);
    static_assert(sizeof(spColorFuncEvalSerializerLayout) == 0x14);
    static_assert(sizeof(spSphereBVSerializerLayout) == 0x14);
    static_assert(sizeof(spBoxBVSerializerLayout) == 0x14);
    static_assert(sizeof(spOBBBVSerializerLayout) == 0x14);
    static_assert(sizeof(spRenderableSerializerLayout) == 0x14);
    static_assert(sizeof(spMaterialSerializerLayout) == 0x3C);
    static_assert(offsetof(spMaterialSerializerLayout,
        dataBlockSerializerState) == 0x14);
    static_assert(sizeof(spMaterialDataSerializerLayout) == 0x3C);
    static_assert(sizeof(spDXMaterialDataSerializerLayout) == 0x3C);
    static_assert(sizeof(spPS2MaterialDataSerializerLayout) == 0x3C);
    static_assert(sizeof(spModelSerializerLayout) == 0x14);
    static_assert(sizeof(spMeshDataSerializerLayout) == 0x14);
    static_assert(sizeof(spPS2MeshDataSerializerLayout) == 0x14);
    static_assert(sizeof(spDXMeshDataSerializerLayout) == 0x14);
    static_assert(sizeof(spTextureDataSerializerLayout) == 0x14);
    static_assert(sizeof(spPS2TextureDataSerializerLayout) == 0x14);
    static_assert(sizeof(spDXTextureDataSerializerLayout) == 0x14);
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
    static_assert(sizeof(spSerializerHookLayout) == 0x10);
    static_assert(sizeof(spPS2SerializerHookLayout) == 0x10);
    static_assert(sizeof(spResourceFATHelperLayout) == 0x64);
    static_assert(offsetof(spResourceFATHelperLayout, resourceByID) == 0x24);
    static_assert(offsetof(spResourceFATHelperLayout, resourceByObject) == 0x34);
    static_assert(offsetof(spResourceFATHelperLayout, orderedResources) == 0x54);
    static_assert(sizeof(spResourceFATFileEntryLayout) == 0x0C);
    static_assert(sizeof(spResourceFATEntryLayout) == 0x24);
    static_assert(offsetof(spResourceFATEntryLayout, m_ClassID) == 0x10);
    static_assert(offsetof(spResourceFATEntryLayout, object) == 0x20);

    inline constexpr std::uint32_t spMeshClassID = 0x3F077B6C;
    inline constexpr std::uint32_t spNodeClassID = 0x695C0F65;
    inline constexpr std::uint32_t spRenderNodeClassID = 0x603625D0;
    inline constexpr Address32 spRenderNodeRegistration = 0x004AB290;
    inline constexpr Address32 spRenderNodeRegistrationInitializer = 0x00483F00;
    inline constexpr Address32 spRenderNodeFactory = 0x001AB160;
    inline constexpr Address32 spRenderNodeConstructor = 0x001AAFF0;
    inline constexpr Address32 spRenderNodeDeletingDestructor = 0x001AAEF0;
    inline constexpr Address32 spRenderNodeClone = 0x001AB090;
    inline constexpr Address32 spRenderNodeCopy = 0x001AA230;
    inline constexpr Address32 spRenderNodeRegistrationGetter = 0x001A9DE0;
    inline constexpr Address32 spRenderNodeVTableHeader = 0x00490370;
    inline constexpr Address32 spRenderNodeSupportVTableHeader = 0x004903B0;
    inline constexpr std::uint32_t spRenderNodeAllocationSize = 0x1E0;
    inline constexpr std::uint32_t spRenderNodeRenderableCountOffset = 0xD0;
    inline constexpr std::uint32_t spRenderNodeRenderableStorageOffset = 0xD4;
    inline constexpr std::uint32_t spSceneGraphOptimizerClassID = 0x4FE639C2;
    inline constexpr Address32 spSceneGraphOptimizerRegistration = 0x004A9C10;
    inline constexpr Address32 spSceneGraphOptimizerRegistrationInitializer =
        0x00482F10;
    inline constexpr std::uint32_t spRendererClassID = 0x2D9C0296;
    inline constexpr Address32 spRendererRegistration = 0x004A9C70;
    inline constexpr Address32 spRendererRegistrationInitializer = 0x00482F50;
    inline constexpr Address32 spRendererRegistrationGetter = 0x00179560;
    inline constexpr Address32 spRendererConstructor = 0x0017A350;
    inline constexpr Address32 spRendererDestructor = 0x0017A160;
    inline constexpr Address32 spRendererPrimaryVTableHeader = 0x0048ED50;
    inline constexpr Address32 spRendererSupportVTableHeader = 0x0048ED74;
    inline constexpr Address32 spRendererInterfaceVTableHeader = 0x0048ED80;
    inline constexpr Address32 spRendererInvalidateStateCaches = 0x00179E60;
    inline constexpr std::uint32_t spRendererAllocationSize = 0xCBB0;
    inline constexpr std::uint32_t spRendererInterfaceSlotCount = 29;
    inline constexpr std::uint32_t spRendererRenderStateCount = 12;
    inline constexpr std::uint32_t spRendererTextureStateCount = 96;
    inline constexpr std::uint32_t spPS2RendererClassID = 0x303652B8;
    inline constexpr Address32 spPS2RendererRegistration = 0x004B7BA0;
    inline constexpr Address32 spPS2RendererRegistrationInitializer = 0x00485350;
    inline constexpr Address32 spPS2RendererRegistrationGetter = 0x001F6F40;
    inline constexpr Address32 spPS2RendererConstructor = 0x001FC2E0;
    inline constexpr Address32 spPS2RendererDestructor = 0x001FC1B0;
    inline constexpr Address32 spPS2RendererFactory = 0x001FCB80;
    inline constexpr Address32 spPS2RendererClone = 0x001FCA70;
    inline constexpr Address32 spPS2RendererPrimaryVTableHeader = 0x00491920;
    inline constexpr Address32 spPS2RendererSupportVTableHeader = 0x00491944;
    inline constexpr Address32 spPS2RendererInterfaceVTableHeader = 0x00491950;
    inline constexpr std::uint32_t spPS2RendererAllocationSize = 0x19D00;
    inline constexpr Address32
        spPS2RendererInterfaceThunks[spRendererInterfaceSlotCount]{
            0x001FCD80, 0x001FCD90, 0x001FCD70, 0x001FCD60,
            0x001FCD50, 0x001FCD40, 0x001FCD30, 0x001FCD20,
            0x001FCD10, 0x001FCD00, 0x001FCCF0, 0x001FCCE0,
            0x001FCCD0, 0x001FCCC0, 0x001FCCB0, 0x001FCCA0,
            0x001FCC90, 0x001FCC80, 0x001FCC70, 0x001FCC60,
            0x001FCC50, 0x001FCC40, 0x001FCC30, 0x001FCC10,
            0x001FCC20, 0x001FCC00, 0x001FCBF0, 0x001FCBE0,
            0x001FCBD0,
        };
    inline constexpr Address32
        spPS2RendererInterfaceBodies[spRendererInterfaceSlotCount]{
            0x001FBE40, 0x001FBD90, 0x001FBD80, 0x00201BC0,
            0x00201900, 0x00200BD0, 0x00200470, 0x001FE910,
            0x001FBD30, 0x001FF6A0, 0x001FB8C0, 0x001FF4B0,
            0x001FBA40, 0x001FB260, 0x001F6F50, 0x001FC1A0,
            0x001FA9D0, 0x001FA9C0, 0x001FA9B0, 0x001FA880,
            0x001FA870, 0x001FA860, 0x001FAB60, 0x001FAB00,
            0x001FA9E0, 0x001FB1E0, 0x001FB1F0, 0x001FA850,
            0x001FEDB0,
        };
    // The two target-binding operations are deliberately reversed relative
    // to PC. Slot 1 diagnoses unsupported cube targets in this PS2 build.
    inline constexpr std::uint32_t spPS2RendererBindRenderTargetSlot = 0;
    inline constexpr std::uint32_t spPS2RendererBindCubeRenderTargetSlot = 1;
    inline constexpr std::uint32_t spPS2RendererBeginSceneSlot = 3;
    inline constexpr std::uint32_t spPS2RendererEndSceneSlot = 4;
    inline constexpr std::uint32_t spPS2RendererClearSlot = 5;
    inline constexpr std::uint32_t spPS2RendererSubmitMeshSlot = 9;
    inline constexpr Address32 spPS2RendererSubmitMesh = 0x001FF6A0;
    inline constexpr std::uint32_t spPS2RendererConfigure2DSlot = 10;
    inline constexpr std::uint32_t spPS2RendererSetProjectionMatrixSlot = 12;
    inline constexpr std::uint32_t spPS2RendererSetViewMatrixSlot = 13;
    inline constexpr std::uint32_t spPS2RendererSetWorldMatrixSlot = 14;
    inline constexpr std::uint32_t spPS2RendererSetViewportSlot = 22;
    inline constexpr std::uint32_t spPS2RendererSetTextureTransformSlot = 23;
    inline constexpr Address32 spPS2RendererSetTextureTransform = 0x001FAB00;
    inline constexpr std::uint32_t spPS2RendererSetFogSlot = 26;
    inline constexpr Address32 spPS2RendererSetFog = 0x001FB1F0;
    inline constexpr std::uint32_t spRenderTargetClassID = 0x00D1229C;
    inline constexpr Address32 spRenderTargetRegistration = 0x004ACA50;
    inline constexpr Address32 spRenderTargetRegistrationInitializer = 0x0048461C;
    inline constexpr Address32 spRenderTargetConstructor = 0x001C2830;
    inline constexpr std::uint32_t spRenderTargetAllocationSize = 0x28;
    inline constexpr std::uint32_t spCubeRenderTargetClassID = 0x0F8B095F;
    inline constexpr Address32 spCubeRenderTargetRegistration = 0x004AC9F0;
    inline constexpr Address32 spCubeRenderTargetRegistrationInitializer = 0x004845DC;
    inline constexpr Address32 spCubeRenderTargetConstructor = 0x001C2590;
    inline constexpr std::uint32_t spCubeRenderTargetAllocationSize = 0x2C;
    inline constexpr std::uint32_t spPS2RenderTargetClassID = 0x30A7021D;
    inline constexpr Address32 spPS2RenderTargetRegistration = 0x004B8530;
    inline constexpr Address32 spPS2RenderTargetRegistrationInitializer = 0x00485860;
    inline constexpr Address32 spPS2RenderTargetFactory = 0x00209D10;
    inline constexpr Address32 spPS2RenderTargetInit = 0x002098A0;
    inline constexpr Address32 spPS2RenderTargetReinitTargetsForDeviceReset = 0x00209A20;
    inline constexpr Address32 spPS2RenderTargetReleaseTargetsForDeviceReset = 0x00209820;
    inline constexpr std::uint32_t spPS2RenderTargetAllocationSize = 0x30;
    inline constexpr std::uint32_t spPS2CubeRenderTargetClassID = 0x5AE83884;
    inline constexpr Address32 spPS2CubeRenderTargetRegistration = 0x004B84D0;
    inline constexpr Address32 spPS2CubeRenderTargetRegistrationInitializer = 0x00485820;
    inline constexpr Address32 spPS2CubeRenderTargetFactory = 0x00209750;
    inline constexpr std::uint32_t spPS2CubeRenderTargetAllocationSize = 0x2C;
    inline constexpr std::uint32_t spRenderTargetManagerClassID = 0x546C50F2;
    inline constexpr Address32 spRenderTargetManagerRegistration = 0x004ACAB0;
    inline constexpr Address32 spRenderTargetManagerRegistrationInitializer = 0x0048465C;
    inline constexpr Address32 spRenderTargetManagerConstructor = 0x001C3810;
    inline constexpr std::uint32_t spRenderTargetManagerAllocationSize = 0x44;
    inline constexpr std::uint32_t spPS2RenderTargetManagerClassID = 0x01486E51;
    inline constexpr Address32 spPS2RenderTargetManagerRegistration = 0x004B8470;
    inline constexpr Address32 spPS2RenderTargetManagerRegistrationInitializer = 0x004857E0;
    inline constexpr Address32 spPS2RenderTargetManagerFactory = 0x002094B0;
    inline constexpr std::uint32_t spMaterialTextureClassID = 0x694E6975;
    inline constexpr Address32 spMaterialTextureRegistration = 0x004A9910;
    inline constexpr Address32 spMaterialTextureRegistrationInitializer = 0x00482D10;
    inline constexpr Address32 spMaterialTextureRegistrationGetter = 0x00172F70;
    inline constexpr Address32 spMaterialTextureFactory = 0x00173630;
    inline constexpr std::uint32_t spMaterialTextureAllocationSize = 0x80;
    inline constexpr std::uint32_t spMaterialTextureStateCount = 12;
    inline constexpr std::uint32_t spMaterialRenderTargetTextureClassID = 0x535D1473;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistration = 0x004A98B0;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistrationInitializer = 0x00482CD0;
    inline constexpr Address32 spMaterialRenderTargetTextureRegistrationGetter = 0x00172460;
    inline constexpr Address32 spMaterialRenderTargetTextureConstructor = 0x00172BA0;
    inline constexpr Address32 spMaterialRenderTargetTextureCopyConstructor = 0x001729B0;
    inline constexpr Address32 spMaterialRenderTargetTextureGetTexture = 0x00172630;
    inline constexpr Address32 spMaterialRenderTargetTextureVTableHeader = 0x0048EA60;
    inline constexpr std::uint32_t spMaterialRenderTargetTextureAllocationSize = 0xA0;
    inline constexpr std::uint32_t spMaterialCameraViewTextureClassID = 0x34EF51B9;
    inline constexpr Address32 spMaterialCameraViewTextureRegistration = 0x004A9790;
    inline constexpr Address32 spMaterialCameraViewTextureRegistrationInitializer = 0x00482C10;
    inline constexpr Address32 spMaterialCameraViewTextureFactory = 0x00171340;
    inline constexpr Address32 spMaterialCameraViewTextureVTableHeader = 0x0048E9A0;
    inline constexpr std::uint32_t spMaterialCameraViewTextureAllocationSize = 0xB0;
    inline constexpr std::uint32_t spMaterialCubeMapTextureClassID = 0x1C3B499A;
    inline constexpr Address32 spMaterialCubeMapTextureRegistration = 0x004A97F0;
    inline constexpr Address32 spMaterialCubeMapTextureRegistrationInitializer = 0x00482C50;
    inline constexpr Address32 spMaterialCubeMapTextureFactory = 0x00172010;
    inline constexpr Address32 spMaterialCubeMapTextureRender = 0x00171620;
    inline constexpr Address32 spMaterialCubeMapTextureVTableHeader = 0x0048E9E0;
    inline constexpr std::uint32_t spMaterialCubeMapTextureAllocationSize = 0xB0;
    inline constexpr std::uint32_t spMaterialLayerRenderTargetFieldID = 13;
    inline constexpr std::uint32_t spMaterialLayerCameraFieldID = 14;
    inline constexpr std::uint32_t spMaterialLayerCubeMapFieldID = 15;
    inline constexpr std::uint32_t spMaterialLayerMovieFieldID = 16;
    inline constexpr std::uint32_t spSerializerClassID = 0x42429877;
    inline constexpr Address32 spSerializerRegistration = 0x004A9DF0;
    inline constexpr Address32 spSerializerRegistrationInitializer = 0x00483050;
    inline constexpr Address32 spSerializerConstructor = 0x00181D00;
    inline constexpr Address32 spSerializerDeletingDestructor = 0x00181C90;
    inline constexpr Address32 spSerializerRegistrationGetter = 0x001810E0;
    inline constexpr Address32 spSerializerResolveClassID = 0x001814D0;
    inline constexpr Address32 spSerializerReadObjectReference = 0x001814E0;
    inline constexpr Address32 spSerializerReadObjectHeader = 0x001815B0;
    inline constexpr Address32 spSerializerIndexRelationships = 0x00181720;
    inline constexpr Address32 spSerializerWriteObjectReference = 0x001817E0;
    inline constexpr Address32 spSerializerIndexObject = 0x00181BE0;
    inline constexpr Address32 spSerializerVTableHeader = 0x0048EFB0;
    inline constexpr Address32 spSerializerInterfaceVTableHeader = 0x0048EFD4;
    inline constexpr std::uint32_t spSerializerAllocationSize = 0x14;
    inline constexpr Address32 spDataBlockSerializerSelectSizeCode = 0x0017E740;
    inline constexpr Address32 spDataBlockSerializerWriteTerminator = 0x0017E7C0;
    inline constexpr Address32 spDataBlockSerializerSkipData = 0x0017E830;
    inline constexpr Address32 spDataBlockSerializerReadHeader = 0x0017E890;
    inline constexpr Address32 spDataBlockSerializerWriteHeader = 0x0017EAA0;
    inline constexpr Address32 spDataBlockSerializerSourcePath = 0x00449010;
    inline constexpr std::uint32_t spDataBlockSerializerAllocationSize = 0x28;
    inline constexpr std::uint32_t spSerializerHookClassID = 0x18092F8D;
    inline constexpr Address32 spSerializerHookRegistration = 0x004A9D90;
    inline constexpr Address32 spSerializerHookRegistrationInitializer =
        0x00483010;
    inline constexpr Address32 spSerializerHookRegistrationGetter = 0x0017E680;
    inline constexpr Address32 spSerializerHookDestructor = 0x0017E690;
    inline constexpr Address32 spSerializerHookConstructor = 0x0017E6F0;
    inline constexpr Address32 spSerializerHookClone = 0x0017E730;
    inline constexpr Address32 spSerializerHookVTableHeader = 0x0048EF30;
    inline constexpr std::uint32_t spSerializerHookAllocationSize = 0x10;
    inline constexpr std::uint32_t spPS2SerializerHookClassID = 0x1C0E0F30;
    inline constexpr Address32 spPS2SerializerHookRegistration = 0x004B83B0;
    inline constexpr Address32 spPS2SerializerHookRegistrationInitializer =
        0x00485730;
    inline constexpr Address32 spPS2SerializerHookRegistrationGetter = 0x00208E60;
    inline constexpr Address32 spPS2SerializerHookPlatformHook = 0x00208E70;
    inline constexpr Address32 spPS2SerializerHookDestructor = 0x00208EE0;
    inline constexpr Address32 spPS2SerializerHookClone = 0x00208F40;
    inline constexpr Address32 spPS2SerializerHookNew = 0x00209010;
    inline constexpr Address32 spPS2SerializerHookFactory = 0x00209070;
    inline constexpr Address32 spPS2SerializerHookVTableHeader = 0x00491BB0;
    inline constexpr std::uint32_t spPS2SerializerHookAllocationSize = 0x10;
    inline constexpr std::uint32_t spPS2SerializerHookPlatformValue = 0x08;
    inline constexpr std::uint32_t spSerializerManagerClassID = 0xE422E9EB;
    inline constexpr std::uint32_t spSerializerManagerRegisteredBaseClassID =
        0x415352A1;
    inline constexpr Address32 spSerializerManagerRegistration = 0x004A9E50;
    inline constexpr Address32 spSerializerManagerRegistrationInitializer =
        0x00483090;
    inline constexpr Address32 spSerializerManagerSingleton = 0x0049F9DC;
    inline constexpr Address32 spSerializerManagerRegistrationGetter = 0x00181D70;
    inline constexpr Address32 spSerializerManagerValidateFileHeader = 0x00181D80;
    inline constexpr Address32 spSerializerManagerRegisterSerializer = 0x00182070;
    inline constexpr Address32 spSerializerManagerLoadSceneGraph = 0x00182150;
    inline constexpr Address32 spSerializerManagerLoadResources = 0x00182640;
    inline constexpr Address32 spSerializerManagerFindForObject = 0x001829E0;
    inline constexpr Address32 spSerializerManagerFindForClassID = 0x00182AC0;
    inline constexpr Address32 spSerializerManagerLoadAllFATEntries = 0x00182B90;
    inline constexpr Address32 spSerializerManagerClearRegistrations = 0x00182EC0;
    inline constexpr Address32 spSerializerManagerDestructor = 0x00182FE0;
    inline constexpr Address32 spSerializerManagerConstructor = 0x001830E0;
    inline constexpr Address32 spSerializerManagerClone = 0x00183150;
    inline constexpr Address32 spSerializerManagerFactory = 0x00183260;
    inline constexpr Address32 spSerializerManagerVTableHeader = 0x0048F020;
    inline constexpr std::uint32_t spSerializerManagerAllocationSize = 0x2C;
    inline constexpr Address32 spDXMeshDataSerializerPolicyRead = 0x00161DF0;
    inline constexpr Address32 spMeshDataSerializerPolicyRead = 0x0016255C;
    inline constexpr Address32 spPS2MeshDataSerializerPolicyRead = 0x00162E60;
    inline constexpr Address32 spResourceFATHelperLoadFileIndex = 0x0017F460;
    inline constexpr Address32 spResourceFATHelperLoadIndex = 0x0017F570;
    inline constexpr Address32 spResourceFATHelperFirstEntry = 0x0017F8A0;
    inline constexpr Address32 spResourceFATHelperFindEntryByID = 0x0017F990;
    inline constexpr Address32 spResourceFATHelperFindEntryByObject = 0x0017F930;
    inline constexpr Address32 spResourceFATHelperIndexObject = 0x0017FC60;
    inline constexpr Address32 spResourceFATHelperDestructor = 0x0017FDF0;
    inline constexpr Address32 spResourceFATHelperConstructor = 0x001801C0;
    inline constexpr Address32 spResourceFATHelperVTableHeader = 0x0048EF60;
    inline constexpr Address32 spResourceFATApplyName = 0x001810F0;
    inline constexpr std::uint32_t spResourceFATHelperAllocationSize = 0x64;
    inline constexpr std::uint32_t spNodeSerializerClassID = 0x4545848A;
    inline constexpr Address32 spNodeSerializerRegistration = 0x004AA810;
    inline constexpr Address32 spNodeSerializerRegistrationInitializer = 0x00483710;
    inline constexpr Address32 spNodeSerializerFactory = 0x00197660;
    inline constexpr Address32 spNodeSerializerConstructor = 0x00197540;
    inline constexpr Address32 spNodeSerializerDeletingDestructor = 0x001974D0;
    inline constexpr Address32 spNodeSerializerClone = 0x00197580;
    inline constexpr Address32 spNodeSerializerRegistrationGetter = 0x00196520;
    inline constexpr Address32 spNodeSerializerTargetClassID = 0x001974C0;
    inline constexpr Address32 spNodeSerializerFinalizeRelationships = 0x00196A60;
    inline constexpr Address32 spNodeSerializerRead = 0x00196530;
    inline constexpr Address32 spNodeSerializerWrite = 0x00196B90;
    inline constexpr Address32 spNodeSerializerVTableHeader = 0x0048F950;
    inline constexpr Address32 spNodeSerializerInterfaceVTableHeader = 0x0048F974;
    inline constexpr std::uint32_t spNodeSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spRenderNodeSerializerClassID = 0x66EF6060;
    inline constexpr std::uint32_t spRenderNodeSerializerTargetClassIDValue =
        0x603625D0;
    inline constexpr Address32 spRenderNodeSerializerRegistration = 0x004AA8D0;
    inline constexpr Address32 spRenderNodeSerializerRegistrationInitializer =
        0x00483790;
    inline constexpr Address32 spRenderNodeSerializerFactory = 0x00198420;
    inline constexpr Address32 spRenderNodeSerializerConstructor = 0x00198300;
    inline constexpr Address32 spRenderNodeSerializerDeletingDestructor =
        0x00198290;
    inline constexpr Address32 spRenderNodeSerializerClone = 0x00198340;
    inline constexpr Address32 spRenderNodeSerializerRegistrationGetter =
        0x00197E30;
    inline constexpr Address32 spRenderNodeSerializerTargetClassID = 0x00198280;
    inline constexpr Address32 spRenderNodeSerializerIndexRelationships =
        0x00197FF0;
    inline constexpr Address32 spRenderNodeSerializerRead = 0x00197E40;
    inline constexpr Address32 spRenderNodeSerializerWrite = 0x001980A0;
    inline constexpr Address32 spRenderNodeSerializerVTableHeader = 0x0048FA10;
    inline constexpr Address32 spRenderNodeSerializerInterfaceVTableHeader =
        0x0048FA34;
    inline constexpr std::uint32_t spRenderNodeSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spCameraSerializerClassID = 0x440E53FB;
    inline constexpr std::uint32_t spCameraSerializerTargetClassIDValue = 0x18DF3845;
    inline constexpr Address32 spCameraSerializerRegistration = 0x004AA690;
    inline constexpr Address32 spCameraSerializerRegistrationInitializer = 0x00483610;
    inline constexpr Address32 spCameraSerializerFactory = 0x001919F0;
    inline constexpr Address32 spCameraSerializerConstructor = 0x001918D0;
    inline constexpr Address32 spCameraSerializerDeletingDestructor = 0x00191860;
    inline constexpr Address32 spCameraSerializerClone = 0x00191910;
    inline constexpr Address32 spCameraSerializerRegistrationGetter = 0x00191310;
    inline constexpr Address32 spCameraSerializerTargetClassID = 0x00191850;
    inline constexpr Address32 spCameraSerializerFinalizeRelationships = 0x00196A60;
    inline constexpr Address32 spCameraSerializerRead = 0x00191320;
    inline constexpr Address32 spCameraSerializerWrite = 0x001915C0;
    inline constexpr Address32 spCameraSerializerReadThunk = 0x00191A60;
    inline constexpr Address32 spCameraSerializerFinalizeRelationshipsThunk = 0x00191A70;
    inline constexpr Address32 spCameraSerializerWriteThunk = 0x00191A80;
    inline constexpr Address32 spCameraSerializerVTableHeader = 0x0048F7D0;
    inline constexpr Address32 spCameraSerializerInterfaceVTableHeader = 0x0048F7F4;
    inline constexpr std::uint32_t spCameraSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spCameraDataSerializerClassID = 0x759F1687;
    inline constexpr std::uint32_t spCameraDataSerializerTargetClassIDValue = 0x18DF3845;
    inline constexpr Address32 spCameraDataSerializerRegistration = 0x004AB170;
    inline constexpr Address32 spCameraDataSerializerRegistrationInitializer = 0x00483DD0;
    inline constexpr Address32 spCameraDataSerializerFactory = 0x001A5A20;
    inline constexpr Address32 spCameraDataSerializerDeletingDestructor = 0x001A58D0;
    inline constexpr Address32 spCameraDataSerializerClone = 0x001A5940;
    inline constexpr Address32 spCameraDataSerializerRegistrationGetter = 0x001A57D0;
    inline constexpr Address32 spCameraDataSerializerTargetClassID = 0x001A5840;
    inline constexpr Address32 spCameraDataSerializerLoadSignature = 0x001A5850;
    inline constexpr Address32 spCameraDataSerializerReadWrapper = 0x001A57E0;
    inline constexpr Address32 spCameraDataSerializerWriteWrapper = 0x001A57F0;
    inline constexpr Address32 spCameraDataSerializerReadThunk = 0x001A5A90;
    inline constexpr Address32 spCameraDataSerializerFinalizeRelationshipsThunk = 0x00191A70;
    inline constexpr Address32 spCameraDataSerializerWriteThunk = 0x001A5AA0;
    inline constexpr Address32 spCameraDataSerializerVTableHeader = 0x00490290;
    inline constexpr Address32 spCameraDataSerializerInterfaceVTableHeader = 0x004902B4;
    inline constexpr std::uint32_t spCameraDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spFogSerializerClassID = 0x576A70CA;
    inline constexpr std::uint32_t spFogSerializerTargetClassIDValue = 0x7AC95AEC;
    inline constexpr Address32 spFogSerializerRegistration = 0x004AA3F0;
    inline constexpr Address32 spFogSerializerRegistrationInitializer = 0x00483450;
    inline constexpr Address32 spFogSerializerFactory = 0x0018B3D0;
    inline constexpr Address32 spFogSerializerDeletingDestructor = 0x0018B280;
    inline constexpr Address32 spFogSerializerClone = 0x0018B2F0;
    inline constexpr Address32 spFogSerializerRegistrationGetter = 0x0018ADD0;
    inline constexpr Address32 spFogSerializerTargetClassID = 0x0018B270;
    inline constexpr Address32 spFogSerializerRead = 0x0018ADE0;
    inline constexpr Address32 spFogSerializerWrite = 0x0018B010;
    inline constexpr Address32 spFogSerializerReadThunk = 0x0018B440;
    inline constexpr Address32 spFogSerializerFinalizeRelationshipsThunk = 0x0018B450;
    inline constexpr Address32 spFogSerializerWriteThunk = 0x0018B460;
    inline constexpr Address32 spFogSerializerVTableHeader = 0x0048F540;
    inline constexpr Address32 spFogSerializerInterfaceVTableHeader = 0x0048F564;
    inline constexpr std::uint32_t spFogSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spMatColorControllerSerializerClassID = 0x0F881A36;
    inline constexpr std::uint32_t spMatColorControllerSerializerTargetClassIDValue = 0x4C633E85;
    inline constexpr Address32 spMatColorControllerSerializerRegistration = 0x004AA090;
    inline constexpr Address32 spMatColorControllerSerializerRegistrationInitializer = 0x00483210;
    inline constexpr Address32 spMatColorControllerSerializerFactory = 0x00187A30;
    inline constexpr Address32 spMatColorControllerSerializerDeletingDestructor = 0x001878E0;
    inline constexpr Address32 spMatColorControllerSerializerClone = 0x00187950;
    inline constexpr Address32 spMatColorControllerSerializerRegistrationGetter = 0x00187250;
    inline constexpr Address32 spMatColorControllerSerializerTargetClassID = 0x001878D0;
    inline constexpr Address32 spMatColorControllerSerializerRead = 0x00187260;
    inline constexpr Address32 spMatColorControllerSerializerFinalize = 0x00187520;
    inline constexpr Address32 spMatColorControllerSerializerWrite = 0x00187530;
    inline constexpr Address32 spMatColorControllerSerializerReadThunk = 0x00187AA0;
    inline constexpr Address32 spMatColorControllerSerializerFinalizeThunk = 0x00187AB0;
    inline constexpr Address32 spMatColorControllerSerializerWriteThunk = 0x00187AC0;
    inline constexpr Address32 spMatColorControllerSerializerVTableHeader = 0x0048F230;
    inline constexpr Address32 spMatColorControllerSerializerInterfaceVTableHeader = 0x0048F254;
    inline constexpr std::uint32_t spMatColorControllerSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spLightControllerSerializerClassID = 0x70573E5E;
    inline constexpr std::uint32_t spLightControllerSerializerTargetClassIDValue = 0x10262533;
    inline constexpr std::uint32_t spLightControllerSerializerLightClassID = 0x72444900;
    inline constexpr Address32 spLightControllerSerializerRegistration = 0x004AA030;
    inline constexpr Address32 spLightControllerSerializerRegistrationInitializer = 0x004831D0;
    inline constexpr Address32 spLightControllerSerializerFactory = 0x001871B0;
    inline constexpr Address32 spLightControllerSerializerDeletingDestructor = 0x00187060;
    inline constexpr Address32 spLightControllerSerializerClone = 0x001870D0;
    inline constexpr Address32 spLightControllerSerializerRegistrationGetter = 0x001868B0;
    inline constexpr Address32 spLightControllerSerializerTargetClassID = 0x00187050;
    inline constexpr Address32 spLightControllerSerializerRead = 0x001868C0;
    inline constexpr Address32 spLightControllerSerializerFinalizeRelationships = 0x00186BF0;
    inline constexpr Address32 spLightControllerSerializerWrite = 0x00186C30;
    inline constexpr Address32 spLightControllerSerializerReadThunk = 0x00187220;
    inline constexpr Address32 spLightControllerSerializerFinalizeRelationshipsThunk = 0x00187230;
    inline constexpr Address32 spLightControllerSerializerWriteThunk = 0x00187240;
    inline constexpr Address32 spLightControllerSerializerVTableHeader = 0x0048F1D0;
    inline constexpr Address32 spLightControllerSerializerInterfaceVTableHeader = 0x0048F1F4;
    inline constexpr std::uint32_t spLightControllerSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spLightControllerSerializerDefaultColorARGB = 0x00000000;
    inline constexpr std::uint32_t spAnimTexControllerSerializerClassID = 0x77793754;
    inline constexpr std::uint32_t spAnimTexControllerSerializerTargetClassIDValue = 0x16FB0E47;
    inline constexpr std::uint32_t spAnimTexControllerSerializerTextureClassID = 0x2F281E13;
    inline constexpr Address32 spAnimTexControllerSerializerRegistration = 0x004A9F10;
    inline constexpr Address32 spAnimTexControllerSerializerRegistrationInitializer = 0x00483110;
    inline constexpr Address32 spAnimTexControllerSerializerFactory = 0x00185720;
    inline constexpr Address32 spAnimTexControllerSerializerDeletingDestructor = 0x001855D0;
    inline constexpr Address32 spAnimTexControllerSerializerClone = 0x00185640;
    inline constexpr Address32 spAnimTexControllerSerializerRegistrationGetter = 0x00185030;
    inline constexpr Address32 spAnimTexControllerSerializerTargetClassID = 0x001855C0;
    inline constexpr Address32 spAnimTexControllerSerializerRead = 0x00185040;
    inline constexpr Address32 spAnimTexControllerSerializerFinalizeRelationships = 0x001852B0;
    inline constexpr Address32 spAnimTexControllerSerializerWrite = 0x00185360;
    inline constexpr Address32 spAnimTexControllerSerializerReadThunk = 0x00185790;
    inline constexpr Address32 spAnimTexControllerSerializerFinalizeRelationshipsThunk = 0x001857A0;
    inline constexpr Address32 spAnimTexControllerSerializerWriteThunk = 0x001857B0;
    inline constexpr Address32 spAnimTexControllerSerializerVTableHeader = 0x0048F0B0;
    inline constexpr Address32 spAnimTexControllerSerializerInterfaceVTableHeader = 0x0048F0D4;
    inline constexpr std::uint32_t spAnimTexControllerSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spUVControllerSerializerClassID = 0x591224D0;
    inline constexpr std::uint32_t spUVControllerSerializerTargetClassIDValue = 0x1C0053D6;
    inline constexpr Address32 spUVControllerSerializerRegistration = 0x004AA150;
    inline constexpr Address32 spUVControllerSerializerRegistrationInitializer = 0x00483290;
    inline constexpr Address32 spUVControllerSerializerFactory = 0x001887A0;
    inline constexpr Address32 spUVControllerSerializerDeletingDestructor = 0x00188650;
    inline constexpr Address32 spUVControllerSerializerClone = 0x001886C0;
    inline constexpr Address32 spUVControllerSerializerRegistrationGetter = 0x00188340;
    inline constexpr Address32 spUVControllerSerializerTargetClassID = 0x00188640;
    inline constexpr Address32 spUVControllerSerializerRead = 0x00188350;
    inline constexpr Address32 spUVControllerSerializerFinalize = 0x00188460;
    inline constexpr Address32 spUVControllerSerializerWrite = 0x00188470;
    inline constexpr Address32 spUVControllerSerializerReadThunk = 0x00188810;
    inline constexpr Address32 spUVControllerSerializerFinalizeThunk = 0x00188820;
    inline constexpr Address32 spUVControllerSerializerWriteThunk = 0x00188830;
    inline constexpr Address32 spUVControllerSerializerVTableHeader = 0x0048F2F0;
    inline constexpr Address32 spUVControllerSerializerInterfaceVTableHeader = 0x0048F314;
    inline constexpr std::uint32_t spUVControllerSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerClassID = 0x2AE96657;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerTargetClassIDValue = 0x491432F0;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistration = 0x004AA0F0;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistrationInitializer = 0x00483250;
    inline constexpr Address32 spTransFunctionEvalSerializerFactory = 0x001882A0;
    inline constexpr Address32 spTransFunctionEvalSerializerDeletingDestructor = 0x00188110;
    inline constexpr Address32 spTransFunctionEvalSerializerClone = 0x001881C0;
    inline constexpr Address32 spTransFunctionEvalSerializerRegistrationGetter = 0x00187AD0;
    inline constexpr Address32 spTransFunctionEvalSerializerTargetClassID = 0x00188100;
    inline constexpr Address32 spTransFunctionEvalSerializerRead = 0x00187AE0;
    inline constexpr Address32 spTransFunctionEvalSerializerFinalize = 0x00187EB0;
    inline constexpr Address32 spTransFunctionEvalSerializerWrite = 0x00187EC0;
    inline constexpr Address32 spTransFunctionEvalSerializerReadThunk = 0x00188310;
    inline constexpr Address32 spTransFunctionEvalSerializerFinalizeThunk = 0x00188320;
    inline constexpr Address32 spTransFunctionEvalSerializerWriteThunk = 0x00188330;
    inline constexpr Address32 spTransFunctionEvalSerializerVTableHeader = 0x0048F290;
    inline constexpr Address32 spTransFunctionEvalSerializerInterfaceVTableHeader = 0x0048F2B4;
    inline constexpr std::uint32_t spTransFunctionEvalSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spFunctionEvalSerializerClassID = 0x1D2A151D;
    inline constexpr std::uint32_t spFunctionEvalSerializerTargetClassIDValue = 0x9450E590;
    inline constexpr Address32 spFunctionEvalSerializerRegistration = 0x004A9FD0;
    inline constexpr Address32 spFunctionEvalSerializerRegistrationInitializer = 0x00483190;
    inline constexpr Address32 spFunctionEvalSerializerFactory = 0x00186810;
    inline constexpr Address32 spFunctionEvalSerializerDeletingDestructor = 0x00186680;
    inline constexpr Address32 spFunctionEvalSerializerClone = 0x00186730;
    inline constexpr Address32 spFunctionEvalSerializerRegistrationGetter = 0x00186100;
    inline constexpr Address32 spFunctionEvalSerializerTargetClassID = 0x00186670;
    inline constexpr Address32 spFunctionEvalSerializerRead = 0x00186110;
    inline constexpr Address32 spFunctionEvalSerializerFinalize = 0x00186390;
    inline constexpr Address32 spFunctionEvalSerializerWrite = 0x001863A0;
    inline constexpr Address32 spFunctionEvalSerializerReadThunk = 0x00186880;
    inline constexpr Address32 spFunctionEvalSerializerFinalizeThunk = 0x00186890;
    inline constexpr Address32 spFunctionEvalSerializerWriteThunk = 0x001868A0;
    inline constexpr Address32 spFunctionEvalSerializerVTableHeader = 0x0048F170;
    inline constexpr Address32 spFunctionEvalSerializerInterfaceVTableHeader = 0x0048F194;
    inline constexpr std::uint32_t spFunctionEvalSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spColorFuncEvalSerializerClassID = 0x2CC46B90;
    inline constexpr std::uint32_t spColorFuncEvalSerializerTargetClassIDValue = 0x0BC70FE7;
    inline constexpr Address32 spColorFuncEvalSerializerRegistration = 0x004A9F70;
    inline constexpr Address32 spColorFuncEvalSerializerRegistrationInitializer = 0x00483150;
    inline constexpr Address32 spColorFuncEvalSerializerFactory = 0x00186060;
    inline constexpr Address32 spColorFuncEvalSerializerDeletingDestructor = 0x00185ED0;
    inline constexpr Address32 spColorFuncEvalSerializerClone = 0x00185F80;
    inline constexpr Address32 spColorFuncEvalSerializerRegistrationGetter = 0x001857C0;
    inline constexpr Address32 spColorFuncEvalSerializerTargetClassID = 0x00185EC0;
    inline constexpr Address32 spColorFuncEvalSerializerRead = 0x001857D0;
    inline constexpr Address32 spColorFuncEvalSerializerFinalize = 0x00185AD0;
    inline constexpr Address32 spColorFuncEvalSerializerWrite = 0x00185AE0;
    inline constexpr Address32 spColorFuncEvalSerializerReadThunk = 0x001860D0;
    inline constexpr Address32 spColorFuncEvalSerializerFinalizeThunk = 0x001860E0;
    inline constexpr Address32 spColorFuncEvalSerializerWriteThunk = 0x001860F0;
    inline constexpr Address32 spColorFuncEvalSerializerVTableHeader = 0x0048F110;
    inline constexpr Address32 spColorFuncEvalSerializerInterfaceVTableHeader = 0x0048F134;
    inline constexpr std::uint32_t spColorFuncEvalSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spColorFuncEvalSerializerDefaultColorARGB = 0x00000000;
    inline constexpr std::uint32_t spSphereBVSerializerClassID = 0x7294634F;
    inline constexpr std::uint32_t spSphereBVSerializerTargetClassIDValue = 0x390946D2;
    inline constexpr Address32 spSphereBVSerializerRegistration = 0x004AA330;
    inline constexpr Address32 spSphereBVSerializerRegistrationInitializer = 0x004833D0;
    inline constexpr Address32 spSphereBVSerializerFactory = 0x0018A7C0;
    inline constexpr Address32 spSphereBVSerializerDeletingDestructor = 0x0018A670;
    inline constexpr Address32 spSphereBVSerializerClone = 0x0018A6E0;
    inline constexpr Address32 spSphereBVSerializerRegistrationGetter = 0x0018A2C0;
    inline constexpr Address32 spSphereBVSerializerRead = 0x0018A2D0;
    inline constexpr Address32 spSphereBVSerializerFinalize = 0x0018A460;
    inline constexpr Address32 spSphereBVSerializerWrite = 0x0018A470;
    inline constexpr Address32 spSphereBVSerializerReadThunk = 0x0018A830;
    inline constexpr Address32 spSphereBVSerializerFinalizeThunk = 0x0018A840;
    inline constexpr Address32 spSphereBVSerializerWriteThunk = 0x0018A850;
    inline constexpr Address32 spSphereBVSerializerVTableHeader = 0x0048F490;
    inline constexpr Address32 spSphereBVSerializerInterfaceVTableHeader = 0x0048F4B4;
    inline constexpr std::uint32_t spSphereBVSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spBoxBVSerializerClassID = 0x48E43495;
    inline constexpr std::uint32_t spBoxBVSerializerTargetClassIDValue = 0x7B4C0876;
    inline constexpr Address32 spBoxBVSerializerRegistration = 0x004AA1B0;
    inline constexpr Address32 spBoxBVSerializerRegistrationInitializer = 0x004832D0;
    inline constexpr Address32 spBoxBVSerializerFactory = 0x00188DB0;
    inline constexpr Address32 spBoxBVSerializerDeletingDestructor = 0x00188C60;
    inline constexpr Address32 spBoxBVSerializerClone = 0x00188CD0;
    inline constexpr Address32 spBoxBVSerializerRegistrationGetter = 0x00188840;
    inline constexpr Address32 spBoxBVSerializerRead = 0x00188850;
    inline constexpr Address32 spBoxBVSerializerFinalize = 0x00188A40;
    inline constexpr Address32 spBoxBVSerializerWrite = 0x00188A50;
    inline constexpr Address32 spBoxBVSerializerReadThunk = 0x00188EB0;
    inline constexpr Address32 spBoxBVSerializerFinalizeThunk = 0x00188EC0;
    inline constexpr Address32 spBoxBVSerializerWriteThunk = 0x00188ED0;
    inline constexpr Address32 spBoxBVSerializerVTableHeader = 0x0048F350;
    inline constexpr Address32 spBoxBVSerializerInterfaceVTableHeader = 0x0048F374;
    inline constexpr std::uint32_t spBoxBVSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spOBBBVSerializerClassID = 0x68EA2ED1;
    inline constexpr std::uint32_t spOBBBVSerializerTargetClassIDValue = 0x4DA04889;
    inline constexpr Address32 spOBBBVSerializerRegistration = 0x004AA2D0;
    inline constexpr Address32 spOBBBVSerializerRegistrationInitializer = 0x00483390;
    inline constexpr Address32 spOBBBVSerializerFactory = 0x0018A190;
    inline constexpr Address32 spOBBBVSerializerDeletingDestructor = 0x0018A040;
    inline constexpr Address32 spOBBBVSerializerClone = 0x0018A0B0;
    inline constexpr Address32 spOBBBVSerializerRegistrationGetter = 0x00189950;
    inline constexpr Address32 spOBBBVSerializerRead = 0x00189960;
    inline constexpr Address32 spOBBBVSerializerFinalize = 0x00189BB0;
    inline constexpr Address32 spOBBBVSerializerWrite = 0x00189BC0;
    inline constexpr Address32 spOBBBVSerializerReadThunk = 0x0018A290;
    inline constexpr Address32 spOBBBVSerializerFinalizeThunk = 0x0018A2A0;
    inline constexpr Address32 spOBBBVSerializerWriteThunk = 0x0018A2B0;
    inline constexpr Address32 spOBBBVSerializerVTableHeader = 0x0048F440;
    inline constexpr Address32 spOBBBVSerializerInterfaceVTableHeader = 0x0048F464;
    inline constexpr std::uint32_t spOBBBVSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spLightDataSerializerClassID = 0x33EC2F8E;
    inline constexpr Address32 spLightDataSerializerRegistration = 0x004AAFF0;
    inline constexpr Address32 spLightDataSerializerRegistrationInitializer = 0x00483CD0;
    inline constexpr Address32 spLightDataSerializerFactory = 0x001A4FE0;
    inline constexpr Address32 spLightDataSerializerDeletingDestructor = 0x001A4E90;
    inline constexpr Address32 spLightDataSerializerClone = 0x001A4F00;
    inline constexpr Address32 spLightDataSerializerRegistrationGetter = 0x001A4170;
    inline constexpr Address32 spLightDataSerializerLoad = 0x001A4E00;
    inline constexpr Address32 spLightDataSerializerTargetClassID = 0x001A4E80;
    inline constexpr Address32 spLightDataSerializerWrite = 0x001A4180;
    inline constexpr Address32 spLightDataSerializerRead = 0x001A48C0;
    inline constexpr Address32 spLightDataSerializerVTableHeader = 0x00490110;
    inline constexpr Address32 spLightDataSerializerInterfaceVTableHeader = 0x00490134;
    inline constexpr std::uint32_t spLightDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spLightSerializerClassID = 0x06165309;
    inline constexpr Address32 spLightSerializerRegistration = 0x004AA6F0;
    inline constexpr Address32 spLightSerializerRegistrationInitializer = 0x00483650;
    inline constexpr Address32 spLightSerializerFactory = 0x00192860;
    inline constexpr Address32 spLightSerializerDeletingDestructor = 0x00192710;
    inline constexpr Address32 spLightSerializerClone = 0x00192780;
    inline constexpr Address32 spLightSerializerRegistrationGetter = 0x00191A90;
    inline constexpr Address32 spLightSerializerTargetClassID = 0x00192700;
    inline constexpr Address32 spLightSerializerRead = 0x00191AA0;
    inline constexpr Address32 spLightSerializerWrite = 0x00191FC0;
    inline constexpr Address32 spLightSerializerReadThunk = 0x001928D0;
    inline constexpr Address32 spLightSerializerWriteThunk = 0x001928E0;
    inline constexpr Address32 spLightSerializerVTableHeader = 0x0048F830;
    inline constexpr Address32 spLightSerializerInterfaceVTableHeader = 0x0048F854;
    inline constexpr std::uint32_t spLightSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spRenderableSerializerClassID = 0x4D694D82;
    inline constexpr Address32 spRenderableSerializerRegistration = 0x004AA870;
    inline constexpr Address32 spRenderableSerializerRegistrationInitializer = 0x00483750;
    inline constexpr Address32 spRenderableSerializerFactory = 0x00197D90;
    inline constexpr Address32 spRenderableSerializerConstructor = 0x00197C70;
    inline constexpr Address32 spRenderableSerializerDeletingDestructor = 0x00197C00;
    inline constexpr Address32 spRenderableSerializerClone = 0x00197CB0;
    inline constexpr Address32 spRenderableSerializerRegistrationGetter = 0x001976F0;
    inline constexpr Address32 spRenderableSerializerTargetClassID = 0x00197BF0;
    inline constexpr Address32 spRenderableSerializerWrite = 0x00197700;
    inline constexpr Address32 spRenderableSerializerIndexResourceGraph = 0x00197980;
    inline constexpr Address32 spRenderableSerializerRead = 0x00197A10;
    inline constexpr Address32 spRenderableSerializerWriteThunk = 0x00197E20;
    inline constexpr Address32 spRenderableSerializerIndexResourceGraphThunk = 0x00197E10;
    inline constexpr Address32 spRenderableSerializerReadThunk = 0x00197E00;
    inline constexpr Address32 spRenderableSerializerVTableHeader = 0x0048F9B0;
    inline constexpr Address32 spRenderableSerializerInterfaceVTableHeader = 0x0048F9D4;
    inline constexpr std::uint32_t spRenderableSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spModelSerializerClassID = 0xDB55C34A;
    inline constexpr Address32 spModelSerializerRegistration = 0x004AA7B0;
    inline constexpr Address32 spModelSerializerRegistrationInitializer = 0x004836D0;
    inline constexpr Address32 spModelSerializerFactory = 0x00196480;
    inline constexpr Address32 spModelSerializerConstructor = 0x00196360;
    inline constexpr Address32 spModelSerializerDeletingDestructor = 0x001962F0;
    inline constexpr Address32 spModelSerializerClone = 0x001963A0;
    inline constexpr Address32 spModelSerializerRegistrationGetter = 0x00195E60;
    inline constexpr Address32 spModelSerializerTargetClassID = 0x001962E0;
    inline constexpr Address32 spModelSerializerWrite = 0x00195E70;
    inline constexpr Address32 spModelSerializerIndexResourceGraph = 0x00196090;
    inline constexpr Address32 spModelSerializerRead = 0x00196100;
    inline constexpr Address32 spModelSerializerWriteThunk = 0x00196510;
    inline constexpr Address32 spModelSerializerIndexResourceGraphThunk = 0x00196500;
    inline constexpr Address32 spModelSerializerReadThunk = 0x001964F0;
    inline constexpr Address32 spModelSerializerVTableHeader = 0x0048F8F0;
    inline constexpr Address32 spModelSerializerInterfaceVTableHeader = 0x0048F914;
    inline constexpr std::uint32_t spModelSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spMeshDataSerializerClassID = 0x66380037;
    inline constexpr Address32 spMeshDataSerializerRegistration = 0x004A91F0;
    inline constexpr Address32 spMeshDataSerializerRegistrationInitializer = 0x004827E0;
    inline constexpr Address32 spMeshDataSerializerFactory = 0x00162BB0;
    inline constexpr Address32 spMeshDataSerializerConstructor = 0x00162A90;
    inline constexpr Address32 spMeshDataSerializerDeletingDestructor = 0x00162A20;
    inline constexpr Address32 spMeshDataSerializerClone = 0x00162AD0;
    inline constexpr Address32 spMeshDataSerializerRegistrationGetter = 0x001622D0;
    inline constexpr Address32 spMeshDataSerializerLoad = 0x001622E0;
    inline constexpr Address32 spMeshDataSerializerSerializeCrossPlatform = 0x00162470;
    inline constexpr Address32 spMeshDataSerializerTargetClassID = 0x00162A10;
    inline constexpr Address32 spMeshDataSerializerWrite = 0x00162510;
    inline constexpr Address32 spMeshDataSerializerIndexResourceGraph = 0x00162500;
    inline constexpr Address32 spMeshDataSerializerRead = 0x00162730;
    inline constexpr Address32 spMeshDataSerializerWriteThunk = 0x00162C40;
    inline constexpr Address32 spMeshDataSerializerIndexResourceGraphThunk = 0x00162C30;
    inline constexpr Address32 spMeshDataSerializerReadThunk = 0x00162C20;
    inline constexpr Address32 spMeshDataSerializerVTableHeader = 0x0048E430;
    inline constexpr Address32 spMeshDataSerializerInterfaceVTableHeader = 0x0048E454;
    inline constexpr std::uint32_t spMeshDataSerializerAllocationSize = 0x14;
    inline constexpr Address32 spNodeRegistration = 0x004AB1D0;
    inline constexpr Address32 spNodeRegistrationInitializer = 0x00483E10;
    inline constexpr Address32 spNodeFactory = 0x001A9160;
    inline constexpr Address32 spNodeConstructor = 0x001A8EE0;
    inline constexpr Address32 spNodeDestructor = 0x001A8CF0;
    inline constexpr Address32 spNodeClone = 0x001A9090;
    inline constexpr Address32 spNodeCopy = 0x001A5BE0;
    inline constexpr Address32 spNodeRegistrationGetter = 0x001A5AB0;
    inline constexpr Address32 spNodeGetRoot = 0x001A5AD0;
    inline constexpr Address32 spNodeSetEnabledRecursive = 0x001A5B00;
    inline constexpr Address32 spNodeVTableHeader = 0x004902F0;
    inline constexpr std::uint32_t spNodeAllocationSize = 0xC0;
    inline constexpr std::uint32_t spNodeDefaultFlags = 0x00070A00;
    inline constexpr std::uint32_t spLightClassID = 0x72444900;
    inline constexpr Address32 spLightRegistration = 0x004A9550;
    inline constexpr Address32 spLightRegistrationInitializer = 0x00482A20;
    inline constexpr Address32 spLightConstructor = 0x0016DE40;
    inline constexpr Address32 spLightDestructor = 0x0016DDD0;
    inline constexpr Address32 spLightClone = 0x0016E000;
    inline constexpr Address32 spLightCopy = 0x0016D9F0;
    inline constexpr Address32 spLightRegistrationGetter = 0x0016D9E0;
    inline constexpr Address32 spLightUpdate = 0x0016DAC0;
    inline constexpr Address32 spLightRenderHelper = 0x0016DB30;
    inline constexpr Address32 spLightVTableHeader = 0x0048E7E0;
    inline constexpr Address32 spLightSceneInterfaceVTableHeader = 0x0048E820;
    inline constexpr std::uint32_t spLightDataClassID = 0x5E6402DF;
    inline constexpr Address32 spLightDataRegistration = 0x004A8260;
    inline constexpr Address32 spLightDataRegistrationInitializer = 0x00480E9C;
    inline constexpr Address32 spLightDataFactory = 0x00134BB0;
    inline constexpr Address32 spLightDataConstructor = 0x0016ECC0;
    inline constexpr Address32 spLightDataDeletingDestructor = 0x0016EC50;
    inline constexpr Address32 spLightDataClone = 0x00134AF0;
    inline constexpr Address32 spLightDataRegistrationGetter = 0x00135B00;
    inline constexpr Address32 spLightDataVTableHeader = 0x0048D930;
    inline constexpr Address32 spLightDataSceneInterfaceVTableHeader = 0x0048D970;
    inline constexpr std::uint32_t spLightDataAllocationSize = 0x100;
    inline constexpr std::uint32_t spMaterialClassID = 0x5C0314C5;
    inline constexpr Address32 spMaterialRegistration = 0x004A9610;
    inline constexpr Address32 spMaterialRegistrationInitializer = 0x00482B10;
    inline constexpr Address32 spMaterialConstructor = 0x0016F570;
    inline constexpr Address32 spMaterialDestructor = 0x0016F480;
    inline constexpr Address32 spMaterialClone = 0x0016F650;
    inline constexpr Address32 spMaterialCopy = 0x0016F1B0;
    inline constexpr Address32 spMaterialRegistrationGetter = 0x0016ED00;
    inline constexpr Address32 spMaterialPrimaryVTableHeader = 0x0048E870;
    inline constexpr Address32 spMaterialInterfaceVTableHeader = 0x0048E894;
    inline constexpr std::uint32_t spMaterialAllocationSize = 0x80;
    inline constexpr std::uint32_t spMaterialDataClassID = 0x6160348B;
    inline constexpr Address32 spMaterialDataRegistration = 0x004A82C0;
    inline constexpr Address32 spMaterialDataRegistrationInitializer = 0x00480ED4;
    inline constexpr Address32 spMaterialDataFactory = 0x00134AB0;
    inline constexpr Address32 spMaterialDataConstructor = 0x0016F9E0;
    inline constexpr Address32 spMaterialDataDestructor = 0x0016F970;
    inline constexpr Address32 spMaterialDataClone = 0x001349F0;
    inline constexpr Address32 spMaterialDataCopy = 0x0016F950;
    inline constexpr Address32 spMaterialDataRegistrationGetter = 0x00135AF0;
    inline constexpr Address32 spMaterialDataPrimaryVTableHeader = 0x0048D8A0;
    inline constexpr Address32 spMaterialDataInterfaceVTableHeader = 0x0048D8C4;
    inline constexpr std::uint32_t spMaterialDataAllocationSize = 0xD0;
    inline constexpr Address32 spMaterialDataGetSpecularPower = 0x0016F7B0;
    inline constexpr Address32 spMaterialDataSetSpecularPower = 0x0016F880;
    inline constexpr std::uint32_t spPS2MaterialClassID = 0x0F507BC8;
    inline constexpr Address32 spPS2MaterialRegistration = 0x004B79C0;
    inline constexpr Address32 spPS2MaterialRegistrationInitializer = 0x00485210;
    inline constexpr Address32 spPS2MaterialFactory = 0x001F2FC0;
    inline constexpr Address32 spPS2MaterialDestructor = 0x001F2430;
    inline constexpr Address32 spPS2MaterialClone = 0x001F24A0;
    inline constexpr Address32 spPS2MaterialCopy = 0x001F20B0;
    inline constexpr Address32 spPS2MaterialRegistrationGetter = 0x001F20A0;
    inline constexpr Address32 spPS2MaterialUpdate = 0x001F2360;
    inline constexpr Address32 spMaterialPassLayerUpdate = 0x001700B0;
    inline constexpr Address32 spMaterialTextureLayerUpdate = 0x00170790;
    inline constexpr Address32 spMaterialTextureUpdate = 0x001732E0;
    inline constexpr Address32 spPS2MaterialPrimaryVTableHeader = 0x004916E0;
    inline constexpr Address32 spPS2MaterialInterfaceVTableHeader = 0x00491704;
    inline constexpr std::uint32_t spPS2MaterialAllocationSize = 0xD0;
    inline constexpr std::uint32_t spMaterialPassLayerClassID = 0x3A8905A5;
    inline constexpr Address32 spMaterialPassLayerRegistration = 0x004A9670;
    inline constexpr Address32 spMaterialPassLayerRegistrationInitializer = 0x00482B50;
    inline constexpr Address32 spMaterialPassLayerFactory = 0x00170320;
    inline constexpr Address32 spMaterialPassLayerDestructor = 0x00170140;
    inline constexpr Address32 spMaterialPassLayerClone = 0x00170260;
    inline constexpr Address32 spMaterialPassLayerCopy = 0x0016FF80;
    inline constexpr Address32 spMaterialPassLayerRegistrationGetter = 0x0016FEE0;
    inline constexpr Address32 spMaterialPassLayerVTableHeader = 0x0048E910;
    inline constexpr std::uint32_t spMaterialPassLayerAllocationSize = 0x38;
    inline constexpr std::uint32_t spMaterialTextureLayerClassID = 0x7F577C6D;
    inline constexpr Address32 spMaterialTextureLayerRegistration = 0x004A96D0;
    inline constexpr Address32 spMaterialTextureLayerRegistrationInitializer = 0x00482B90;
    inline constexpr Address32 spMaterialTextureLayerFactory = 0x00170940;
    inline constexpr Address32 spMaterialTextureLayerDestructor = 0x001707A0;
    inline constexpr Address32 spMaterialTextureLayerClone = 0x00170870;
    inline constexpr Address32 spMaterialTextureLayerCopy = 0x001706D0;
    inline constexpr Address32 spMaterialTextureLayerRegistrationGetter = 0x001706C0;
    inline constexpr Address32 spMaterialTextureLayerVTableHeader = 0x0048E940;
    inline constexpr std::uint32_t spMaterialTextureLayerAllocationSize = 0x14;
    inline constexpr std::uint32_t spStdLayerClassID = 0x234C576B;
    inline constexpr Address32 spStdLayerRegistration = 0x004A9730;
    inline constexpr Address32 spStdLayerRegistrationInitializer = 0x00482BD0;
    inline constexpr Address32 spStdLayerFactory = 0x00170D30;
    inline constexpr Address32 spStdLayerDestructor = 0x00170B80;
    inline constexpr Address32 spStdLayerClone = 0x00170C40;
    inline constexpr Address32 spStdLayerCopy = 0x00170B20;
    inline constexpr Address32 spStdLayerRegistrationGetter = 0x00170B10;
    inline constexpr Address32 spStdLayerVTableHeader = 0x0048E970;
    inline constexpr std::uint32_t spStdLayerAllocationSize = 0x14;
    inline constexpr std::uint32_t spStdLayerNestedMaterialTextureSize = 0x80;
    inline constexpr std::uint32_t spFogClassID = 0x7AC95AEC;
    inline constexpr Address32 spFogRegistration = 0x004A7CC0;
    inline constexpr Address32 spFogRegistrationInitializer = 0x00480B5C;
    inline constexpr Address32 spFogFactory = 0x00135820;
    inline constexpr Address32 spFogDestructor = 0x00135C00;
    inline constexpr Address32 spFogClone = 0x00135720;
    inline constexpr Address32 spFogCopy = 0x00105DC0;
    inline constexpr Address32 spFogRegistrationGetter = 0x00135BF0;
    inline constexpr Address32 spFogVTableHeader = 0x0048DCF0;
    inline constexpr std::uint32_t spFogAllocationSize = 0x28;
    inline constexpr std::uint32_t spFogDefaultColorARGB = 0xFF000000;
    inline constexpr std::uint32_t spPlatformSpecificMeshDataClassID = 0x71BE79C5;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistration = 0x004A8FB0;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistrationInitializer = 0x00482720;
    inline constexpr Address32 spPlatformSpecificMeshDataFactory = 0x0015D4F0;
    inline constexpr Address32 spPlatformSpecificMeshDataConstructor = 0x0015D3E0;
    inline constexpr Address32 spPlatformSpecificMeshDataDeletingDestructor = 0x0015D380;
    inline constexpr Address32 spPlatformSpecificMeshDataClone = 0x0015D420;
    inline constexpr Address32 spPlatformSpecificMeshDataRegistrationGetter = 0x0015D370;
    inline constexpr Address32 spPlatformSpecificMeshDataVTable = 0x0048E370;
    inline constexpr std::uint32_t spDXMeshDataClassID = 0x3178114C;
    inline constexpr Address32 spDXMeshDataRegistration = 0x004A8F50;
    inline constexpr Address32 spDXMeshDataRegistrationInitializer = 0x004826E0;
    inline constexpr Address32 spDXMeshDataFactory = 0x0015D0C0;
    inline constexpr Address32 spDXMeshDataConstructor = 0x0015CFB0;
    inline constexpr Address32 spDXMeshDataDestructor = 0x0015CF00;
    inline constexpr Address32 spDXMeshDataClone = 0x0015CFF0;
    inline constexpr Address32 spDXMeshDataRegistrationGetter = 0x0015CEA0;
    inline constexpr Address32 spDXMeshDataInitializeFromMeshData = 0x0015CEB0;
    inline constexpr Address32 spDXMeshDataVTable = 0x0048E340;
    inline constexpr std::uint32_t spPS2MeshDataClassID = 0x737D740F;
    inline constexpr Address32 spPS2MeshDataRegistration = 0x004A9130;
    inline constexpr Address32 spPS2MeshDataRegistrationInitializer = 0x00482760;
    inline constexpr Address32 spPS2MeshDataFactory = 0x001611A0;
    inline constexpr Address32 spPS2MeshDataConstructor = 0x00160FA0;
    inline constexpr Address32 spPS2MeshDataDestructor = 0x00160F00;
    inline constexpr Address32 spPS2MeshDataClone = 0x001610E0;
    inline constexpr Address32 spPS2MeshDataRegistrationGetter = 0x0015D550;
    inline constexpr Address32 spPS2MeshDataVTableHeader = 0x0048E3A0;
    inline constexpr Address32 spPS2MeshDataBuildGrouped = 0x0015F1F0;
    inline constexpr Address32 spPS2MeshDataBuildTriangles = 0x0015F030;
    inline constexpr Address32 spPS2MeshDataBuildPacket = 0x0015F900;
    inline constexpr Address32 spPS2MeshDataExpandVertexBuffer = 0x00160900;
    inline constexpr Address32 spPS2MeshDataInitializeFromBuffers = 0x00160D10;
    inline constexpr std::uint32_t spPS2MeshDataSerializerClassID = 0x6B0C238F;
    inline constexpr Address32 spPS2MeshDataSerializerRegistration = 0x004A9250;
    inline constexpr Address32 spPS2MeshDataSerializerRegistrationInitializer = 0x00482820;
    inline constexpr Address32 spPS2MeshDataSerializerFactory = 0x00163890;
    inline constexpr Address32 spPS2MeshDataSerializerDeletingDestructor = 0x00163740;
    inline constexpr Address32 spPS2MeshDataSerializerClone = 0x001637B0;
    inline constexpr Address32 spPS2MeshDataSerializerRegistrationGetter = 0x00162C50;
    inline constexpr Address32 spPS2MeshDataSerializerSBOOLoad = 0x00162990;
    inline constexpr Address32 spPS2MeshDataSerializerSerializePlatformSpecific = 0x00162C70;
    inline constexpr Address32 spPS2MeshDataSerializerWrite = 0x00162E10;
    inline constexpr Address32 spPS2MeshDataSerializerLoadPlatformSpecific = 0x00163160;
    inline constexpr Address32 spPS2MeshDataSerializerRead = 0x00163350;
    inline constexpr Address32 spPS2MeshDataSerializerTargetClassID = 0x00163730;
    inline constexpr Address32 spPS2MeshDataSerializerIndexResourceGraph = 0x00162C60;
    inline constexpr Address32 spPS2MeshDataSerializerWriteThunk = 0x00163920;
    inline constexpr Address32 spPS2MeshDataSerializerIndexResourceGraphThunk = 0x00163910;
    inline constexpr Address32 spPS2MeshDataSerializerReadThunk = 0x00163900;
    inline constexpr Address32 spPS2MeshDataSerializerVTableHeader = 0x0048E490;
    inline constexpr Address32 spPS2MeshDataSerializerInterfaceVTableHeader = 0x0048E4B4;
    inline constexpr std::uint32_t spPS2MeshDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spPS2MeshDataSerializerNativeLoadFlagMask = 0x08;
    inline constexpr std::uint32_t spDXMeshDataSerializerClassID = 0x77006ABE;
    inline constexpr Address32 spDXMeshDataSerializerRegistration = 0x004A9190;
    inline constexpr Address32 spDXMeshDataSerializerRegistrationInitializer = 0x004827A0;
    inline constexpr Address32 spDXMeshDataSerializerFactory = 0x00162180;
    inline constexpr Address32 spDXMeshDataSerializerDeletingDestructor = 0x00162030;
    inline constexpr Address32 spDXMeshDataSerializerClone = 0x001620A0;
    inline constexpr Address32 spDXMeshDataSerializerRegistrationGetter = 0x00161610;
    inline constexpr Address32 spDXMeshDataSerializerSBOOLoad = 0x00162990;
    inline constexpr Address32 spDXMeshDataSerializerLoadCrossPlatform = 0x001622E0;
    inline constexpr Address32 spDXMeshDataSerializerLoadPlatformSpecific = 0x00161630;
    inline constexpr Address32 spDXMeshDataSerializerSerializePlatformSpecific = 0x001618A0;
    inline constexpr Address32 spDXMeshDataSerializerWrite = 0x00161DA0;
    inline constexpr Address32 spDXMeshDataSerializerIndexResourceGraph = 0x00161620;
    inline constexpr Address32 spDXMeshDataSerializerRead = 0x00161AE0;
    inline constexpr Address32 spDXMeshDataSerializerTargetClassID = 0x00162020;
    inline constexpr Address32 spDXMeshDataSerializerWriteThunk = 0x001622C0;
    inline constexpr Address32 spDXMeshDataSerializerIndexResourceGraphThunk = 0x001622B0;
    inline constexpr Address32 spDXMeshDataSerializerReadThunk = 0x001622A0;
    inline constexpr Address32 spDXMeshDataSerializerVTableHeader = 0x0048E3D0;
    inline constexpr Address32 spDXMeshDataSerializerInterfaceVTableHeader = 0x0048E3F4;
    inline constexpr std::uint32_t spDXMeshDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spDXMeshDataSerializerNativeLoadFlagMask = 0x08;
    inline constexpr std::uint32_t spTextureDataSerializerClassID = 0x1C4C75BA;
    inline constexpr std::uint32_t spTextureDataSerializerTargetClassIDValue = 0x78EA082B;
    inline constexpr Address32 spTextureDataSerializerRegistration = 0x004A9BB0;
    inline constexpr Address32 spTextureDataSerializerRegistrationInitializer = 0x00482ED0;
    inline constexpr Address32 spTextureDataSerializerFactory = 0x00178BA0;
    inline constexpr Address32 spTextureDataSerializerConstructor = 0x00178A80;
    inline constexpr Address32 spTextureDataSerializerDeletingDestructor = 0x00178A10;
    inline constexpr Address32 spTextureDataSerializerClone = 0x00178AC0;
    inline constexpr Address32 spTextureDataSerializerRegistrationGetter = 0x00177830;
    inline constexpr Address32 spTextureDataSerializerLoad = 0x00178980;
    inline constexpr Address32 spTextureDataSerializerReadSource = 0x00177840;
    inline constexpr Address32 spTextureDataSerializerWriteSource = 0x00177B30;
    inline constexpr Address32 spTextureDataSerializerReadCrossPlatform = 0x00177F00;
    inline constexpr Address32 spTextureDataSerializerWriteCrossPlatform = 0x00178200;
    inline constexpr Address32 spTextureDataSerializerTargetClassID = 0x00178A00;
    inline constexpr Address32 spTextureDataSerializerWrite = 0x00178640;
    inline constexpr Address32 spTextureDataSerializerIndexResourceGraph = 0x00178460;
    inline constexpr Address32 spTextureDataSerializerRead = 0x00178470;
    inline constexpr Address32 spTextureDataSerializerWriteThunk = 0x00178CC0;
    inline constexpr Address32 spTextureDataSerializerIndexResourceGraphThunk = 0x00178CB0;
    inline constexpr Address32 spTextureDataSerializerReadThunk = 0x00178CA0;
    inline constexpr Address32 spTextureDataSerializerVTableHeader = 0x0048ECF0;
    inline constexpr Address32 spTextureDataSerializerInterfaceVTableHeader = 0x0048ED14;
    inline constexpr std::uint32_t spTextureDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spTextureDataSerializerCrossPlatformType = 1;
    inline constexpr std::uint32_t spMaterialSerializerClassID = 0x2A14745F;
    inline constexpr Address32 spMaterialSerializerRegistration = 0x004AA750;
    inline constexpr Address32 spMaterialSerializerRegistrationInitializer = 0x00483690;
    inline constexpr Address32 spMaterialSerializerFactory = 0x00195D30;
    inline constexpr Address32 spMaterialSerializerConstructor = 0x00195BF0;
    inline constexpr Address32 spMaterialSerializerDeletingDestructor = 0x00195B60;
    inline constexpr Address32 spMaterialSerializerClone = 0x00195C40;
    inline constexpr Address32 spMaterialSerializerRegistrationGetter = 0x001928F0;
    inline constexpr Address32 spMaterialSerializerWrite = 0x00193060;
    inline constexpr Address32 spMaterialSerializerRead = 0x00193EB0;
    inline constexpr Address32 spMaterialSerializerIndexLayer = 0x001956A0;
    inline constexpr Address32 spMaterialSerializerIndexResourceGraph = 0x001959D0;
    inline constexpr Address32 spMaterialSerializerLoadLayer = 0x00192D90;
    inline constexpr Address32 spMaterialSerializerSerializeLayer = 0x00195300;
    inline constexpr Address32 spMaterialSerializerWriteThunk = 0x00195E50;
    inline constexpr Address32 spMaterialSerializerIndexResourceGraphThunk = 0x00195E40;
    inline constexpr Address32 spMaterialSerializerReadThunk = 0x00195E30;
    inline constexpr Address32 spMaterialSerializerVTableHeader = 0x0048F890;
    inline constexpr Address32 spMaterialSerializerInterfaceVTableHeader = 0x0048F8B4;
    inline constexpr std::uint32_t spMaterialSerializerAllocationSize = 0x3C;
    inline constexpr std::uint32_t spMaterialSerializerDataBlockStateOffset = 0x14;
    inline constexpr std::uint32_t spMaterialDataSerializerClassID = 0x0B251467;
    inline constexpr std::uint32_t spMaterialDataSerializerTargetClassIDValue = 0x6160348B;
    inline constexpr Address32 spMaterialDataSerializerRegistration = 0x004AB0B0;
    inline constexpr Address32 spMaterialDataSerializerRegistrationInitializer = 0x00483D50;
    inline constexpr Address32 spMaterialDataSerializerFactory = 0x001A54F0;
    inline constexpr Address32 spMaterialDataSerializerDeletingDestructor = 0x001A53A0;
    inline constexpr Address32 spMaterialDataSerializerClone = 0x001A5410;
    inline constexpr Address32 spMaterialDataSerializerRegistrationGetter = 0x001A52C0;
    inline constexpr Address32 spMaterialDataSerializerWrite = 0x001A52D0;
    inline constexpr Address32 spMaterialDataSerializerRead = 0x001A52E0;
    inline constexpr Address32 spMaterialDataSerializerLoadSignature = 0x001A5310;
    inline constexpr Address32 spMaterialDataSerializerTargetClassID = 0x001A5390;
    inline constexpr Address32 spMaterialDataSerializerWriteThunk = 0x001A5570;
    inline constexpr Address32 spMaterialDataSerializerIndexResourceGraphThunk = 0x00195E40;
    inline constexpr Address32 spMaterialDataSerializerReadThunk = 0x001A5560;
    inline constexpr Address32 spMaterialDataSerializerVTableHeader = 0x004901D0;
    inline constexpr Address32 spMaterialDataSerializerInterfaceVTableHeader = 0x004901F4;
    inline constexpr std::uint32_t spMaterialDataSerializerAllocationSize = 0x3C;
    inline constexpr std::uint32_t spDXMaterialDataSerializerClassID = 0x60EE3A89;
    inline constexpr Address32 spDXMaterialDataSerializerRegistration = 0x004AB050;
    inline constexpr Address32 spDXMaterialDataSerializerRegistrationInitializer = 0x00483D10;
    inline constexpr Address32 spDXMaterialDataSerializerFactory = 0x001A5250;
    inline constexpr Address32 spDXMaterialDataSerializerDeletingDestructor = 0x001A5100;
    inline constexpr Address32 spDXMaterialDataSerializerClone = 0x001A5170;
    inline constexpr Address32 spDXMaterialDataSerializerRegistrationGetter = 0x001A5070;
    inline constexpr Address32 spDXMaterialDataSerializerLoadSignature = 0x001A5080;
    inline constexpr Address32 spDXMaterialDataSerializerWriteThunk = 0x00195E50;
    inline constexpr Address32 spDXMaterialDataSerializerIndexResourceGraphThunk = 0x00195E40;
    inline constexpr Address32 spDXMaterialDataSerializerReadThunk = 0x00195E30;
    inline constexpr Address32 spDXMaterialDataSerializerVTableHeader = 0x00490170;
    inline constexpr Address32 spDXMaterialDataSerializerInterfaceVTableHeader = 0x00490194;
    inline constexpr std::uint32_t spDXMaterialDataSerializerAllocationSize = 0x3C;
    inline constexpr std::uint32_t spPS2MaterialDataSerializerClassID = 0x69327633;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistration = 0x004AB110;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistrationInitializer = 0x00483D90;
    inline constexpr Address32 spPS2MaterialDataSerializerFactory = 0x001A5760;
    inline constexpr Address32 spPS2MaterialDataSerializerDeletingDestructor = 0x001A5610;
    inline constexpr Address32 spPS2MaterialDataSerializerClone = 0x001A5680;
    inline constexpr Address32 spPS2MaterialDataSerializerRegistrationGetter = 0x001A5580;
    inline constexpr Address32 spPS2MaterialDataSerializerLoadSignature = 0x001A5590;
    inline constexpr Address32 spPS2MaterialDataSerializerWriteThunk = 0x00195E50;
    inline constexpr Address32 spPS2MaterialDataSerializerIndexResourceGraphThunk = 0x00195E40;
    inline constexpr Address32 spPS2MaterialDataSerializerReadThunk = 0x00195E30;
    inline constexpr Address32 spPS2MaterialDataSerializerVTableHeader = 0x00490230;
    inline constexpr Address32 spPS2MaterialDataSerializerInterfaceVTableHeader = 0x00490254;
    inline constexpr std::uint32_t spPS2MaterialDataSerializerAllocationSize = 0x3C;
    inline constexpr std::uint32_t spDXTextureDataSerializerClassID = 0x1C6D480F;
    inline constexpr std::uint32_t spDXTextureDataSerializerTargetClassIDValue = 0x0B1C67BB;
    inline constexpr Address32 spDXTextureDataSerializerRegistration = 0x004A9AF0;
    inline constexpr Address32 spDXTextureDataSerializerRegistrationInitializer = 0x00482E50;
    inline constexpr Address32 spDXTextureDataSerializerFactory = 0x00175D50;
    inline constexpr Address32 spDXTextureDataSerializerDestructor = 0x00175C00;
    inline constexpr Address32 spDXTextureDataSerializerClone = 0x00175C70;
    inline constexpr Address32 spDXTextureDataSerializerRegistrationGetter = 0x00174D60;
    inline constexpr Address32 spDXTextureDataSerializerLoadPlatformSpecific = 0x00174D80;
    inline constexpr Address32 spDXTextureDataSerializerSerializePlatformSpecific = 0x00175060;
    inline constexpr Address32 spDXTextureDataSerializerRead = 0x00175420;
    inline constexpr Address32 spDXTextureDataSerializerWrite = 0x001757B0;
    inline constexpr Address32 spDXTextureDataSerializerTargetClassID = 0x00175BF0;
    inline constexpr Address32 spDXTextureDataSerializerIndexResourceGraph = 0x00174D70;
    inline constexpr Address32 spDXTextureDataSerializerWriteThunk = 0x00176310;
    inline constexpr Address32 spDXTextureDataSerializerIndexResourceGraphThunk = 0x00176300;
    inline constexpr Address32 spDXTextureDataSerializerReadThunk = 0x001762F0;
    inline constexpr Address32 spDXTextureDataSerializerVTableHeader = 0x0048EC30;
    inline constexpr Address32 spDXTextureDataSerializerInterfaceVTableHeader = 0x0048EC54;
    inline constexpr std::uint32_t spDXTextureDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spDXTextureDataSerializerNativeLoadFlagMask = 0x08;
    inline constexpr std::uint32_t spPS2TextureDataSerializerClassID = 0x43C76799;
    inline constexpr std::uint32_t spPS2TextureDataSerializerTargetClassIDValue = 0x24767C83;
    inline constexpr Address32 spPS2TextureDataSerializerRegistration = 0x004A9B50;
    inline constexpr Address32 spPS2TextureDataSerializerRegistrationInitializer = 0x00482E90;
    inline constexpr Address32 spPS2TextureDataSerializerFactory = 0x001771E0;
    inline constexpr Address32 spPS2TextureDataSerializerDestructor = 0x00177090;
    inline constexpr Address32 spPS2TextureDataSerializerClone = 0x00177100;
    inline constexpr Address32 spPS2TextureDataSerializerRegistrationGetter = 0x00176320;
    inline constexpr Address32 spPS2TextureDataSerializerSerializePlatformSpecific = 0x00176330;
    inline constexpr Address32 spPS2TextureDataSerializerWrite = 0x001765D0;
    inline constexpr Address32 spPS2TextureDataSerializerLoadPlatformSpecific = 0x00176A10;
    inline constexpr Address32 spPS2TextureDataSerializerRead = 0x00176CF0;
    inline constexpr Address32 spPS2TextureDataSerializerTargetClassID = 0x00177080;
    inline constexpr Address32 spPS2TextureDataSerializerIndexResourceGraph = 0x001765C0;
    inline constexpr Address32 spPS2TextureDataSerializerWriteThunk = 0x00177820;
    inline constexpr Address32 spPS2TextureDataSerializerIndexResourceGraphThunk = 0x00177810;
    inline constexpr Address32 spPS2TextureDataSerializerReadThunk = 0x00177800;
    inline constexpr Address32 spPS2TextureDataSerializerVTableHeader = 0x0048EC90;
    inline constexpr Address32 spPS2TextureDataSerializerInterfaceVTableHeader = 0x0048ECB4;
    inline constexpr std::uint32_t spPS2TextureDataSerializerAllocationSize = 0x14;
    inline constexpr std::uint32_t spPS2TextureDataSerializerNativeLoadFlagMask = 0x08;
    inline constexpr Address32 spMeshRegistration = 0x004A8CE0;
    inline constexpr Address32 spMeshRegistrationInitializer = 0x00482350;
    inline constexpr Address32 spMeshDestructor = 0x00159E70;
    inline constexpr Address32 spMeshConstructor = 0x00159EF0;
    inline constexpr Address32 spMeshCloneNull = 0x00159F60;
    inline constexpr Address32 spMeshRegistrationGetter = 0x00159AD0;
    inline constexpr Address32 spMeshBoundsPass = 0x00159AE0;
    inline constexpr Address32 spMeshInitialize = 0x00159C80;
    inline constexpr Address32 spMeshVTable = 0x0048E200;
    inline constexpr Address32 spMeshSecondaryVTable = 0x0048E224;
    inline constexpr std::uint32_t spRenderMeshClassID = 0x67974A9C;
    inline constexpr Address32 spRenderMeshRegistration = 0x004A8E90;
    inline constexpr Address32 spRenderMeshRegistrationInitializer = 0x00482660;
    inline constexpr Address32 spRenderMeshRegistrationGetter = 0x0015C340;
    inline constexpr Address32 spRenderMeshDeletingDestructor = 0x0015C350;
    inline constexpr Address32 spRenderMeshConstructor = 0x0015C3C0;
    inline constexpr Address32 spRenderMeshCloneNull = 0x0015C400;
    inline constexpr Address32 spRenderMeshVTable = 0x0048E2D0;
    inline constexpr Address32 spRenderMeshSecondaryVTable = 0x0048E2F4;
    inline constexpr std::uint32_t spPS2MeshClassID = 0x35ED77A5;
    inline constexpr Address32 spPS2MeshRegistration = 0x004B7380;
    inline constexpr Address32 spPS2MeshRegistrationInitializer = 0x00485090;
    inline constexpr Address32 spPS2MeshFactory = 0x001EF6E0;
    inline constexpr Address32 spPS2MeshDestructor = 0x001EF490;
    inline constexpr Address32 spPS2MeshClone = 0x001EF530;
    inline constexpr Address32 spPS2MeshRegistrationGetter = 0x001EF310;
    inline constexpr Address32 spPS2MeshConvertFromBuffers = 0x001EF320;
    inline constexpr Address32 spPS2MeshAttachPreparedData = 0x001EF3C0;
    inline constexpr Address32 spPS2MeshRelease = 0x001EF440;
    inline constexpr Address32 spPS2MeshPrimaryVTableHeader = 0x00491410;
    inline constexpr Address32 spPS2MeshSecondaryVTableHeader = 0x00491434;
    inline constexpr std::uint32_t spPS2MeshAllocationSize = 0x58;
    inline constexpr std::uint32_t spRenderMeshAllocationSize = 0x50;
    inline constexpr std::uint32_t spMeshDataClassID = 0x33C34CF0;
    inline constexpr Address32 spMeshDataRegistration = 0x004A81A0;
    inline constexpr Address32 spMeshDataRegistrationInitializer = 0x00480E2C;
    inline constexpr Address32 spMeshDataFactory = 0x00134DB0;
    inline constexpr Address32 spMeshDataConstructor = 0x0015D330;
    inline constexpr Address32 spMeshDataDestructor = 0x0015D2B0;
    inline constexpr Address32 spMeshDataClone = 0x00134CF0;
    inline constexpr Address32 spMeshDataRegistrationGetter = 0x00135B20;
    inline constexpr Address32 spMeshDataInitialize = 0x0015D220;
    inline constexpr Address32 spMeshDataDeepCopy = 0x0015D1A0;
    inline constexpr Address32 spMeshDataRelease = 0x0015D120;
    inline constexpr Address32 spMeshDataPrimaryVTable = 0x0048D9F0;
    inline constexpr Address32 spMeshDataSecondaryVTable = 0x0048DA14;
    inline constexpr std::uint32_t spVertexBufferClassID = 0x3C846352;
    inline constexpr Address32 spVertexBufferRegistration = 0x004A8EF0;
    inline constexpr Address32 spVertexBufferRegistrationInitializer = 0x004826A0;
    inline constexpr Address32 spVertexBufferFactory = 0x0015CE10;
    inline constexpr Address32 spVertexBufferConstructor = 0x0015CCF0;
    inline constexpr Address32 spVertexBufferDeletingDestructor = 0x0015CC70;
    inline constexpr Address32 spVertexBufferClone = 0x0015CD50;
    inline constexpr Address32 spVertexBufferRegistrationGetter = 0x0015C410;
    inline constexpr Address32 spVertexBufferBuildLayout = 0x0015C8E0;
    inline constexpr Address32 spVertexBufferInitializeExternal = 0x0015C420;
    inline constexpr Address32 spVertexBufferSetComponents = 0x0015C5C0;
    inline constexpr Address32 spVertexBufferRead = 0x0015C610;
    inline constexpr Address32 spVertexBufferInitializeRaw = 0x0015C790;
    inline constexpr Address32 spVertexBufferInitialize = 0x0015C800;
    inline constexpr Address32 spVertexBufferRelease = 0x0015C8B0;
    inline constexpr Address32 spVertexBufferCopyBuffer = 0x0015CB30;
    inline constexpr Address32 spVertexBufferWrite = 0x0015C4C0;
    inline constexpr Address32 spVertexBufferVTable = 0x0048E310;
    inline constexpr std::uint32_t spRenderableClassID = 0x4FDA4542;
    inline constexpr Address32 spRenderableRegistration = 0x004AB230;
    inline constexpr Address32 spRenderableRegistrationInitializer = 0x00483EC0;
    inline constexpr Address32 spRenderableConstructor = 0x001A9AC0;
    inline constexpr Address32 spRenderableDestructor = 0x001A9A00;
    inline constexpr Address32 spRenderableCopy = 0x001A9690;
    inline constexpr Address32 spRenderableRegistrationGetter = 0x001A9270;
    inline constexpr Address32 spRenderableVTable = 0x00490330;
    inline constexpr std::uint32_t spModelClassID = 0x763277DB;
    inline constexpr Address32 spModelRegistration = 0x004A8D40;
    inline constexpr Address32 spModelRegistrationInitializer = 0x00482380;
    inline constexpr Address32 spModelFactory = 0x0015AB90;
    inline constexpr Address32 spModelConstructor = 0x0015AA60;
    inline constexpr Address32 spModelDestructor = 0x0015A9F0;
    inline constexpr Address32 spModelClone = 0x0015AAB0;
    inline constexpr Address32 spModelCopy = 0x0015A4D0;
    inline constexpr Address32 spModelSetBaseMesh = 0x0015A5A0;
    inline constexpr Address32 spModelRender = 0x0015A640;
    inline constexpr Address32 spModelRecomputeRuntimeMode = 0x00159F80;
    inline constexpr Address32 spModelRegistrationGetter = 0x00159F70;
    inline constexpr Address32 spModelVTable = 0x0048E250;
    inline constexpr std::uint32_t spModelDefaultProjectionGroup = 3;
    inline constexpr std::uint32_t spResourceClassID = 0x46F043FE;
    inline constexpr Address32 spResourceRegistration = 0x004A9CD0;
    inline constexpr Address32 spResourceRegistrationInitializer = 0x00482F90;
    inline constexpr Address32 spResourceFactory = 0x0017D050;
    inline constexpr Address32 spResourceConstructor = 0x0017CF40;
    inline constexpr Address32 spResourceDeletingDestructor = 0x0017CEB0;
    inline constexpr Address32 spResourceClone = 0x0017CF80;
    inline constexpr Address32 spResourceRegistrationGetter = 0x0017CEA0;
    inline constexpr Address32 spResourceVTable = 0x0048EEC0;
    inline constexpr std::uint32_t spResourceManagerClassID = 0xA4B9923B;
    inline constexpr Address32 spResourceManagerRegistration = 0x004A9D30;
    inline constexpr Address32 spResourceManagerRegistrationInitializer =
        0x00482FD0;
    inline constexpr Address32 spResourceManagerFactory = 0x0017DD80;
    inline constexpr Address32 spResourceManagerRegistrationGetter = 0x0017D0B0;
    inline constexpr Address32 spResourceManagerConfigure = 0x0017D360;
    inline constexpr Address32 spResourceManagerRemove = 0x0017D3A0;
    inline constexpr Address32 spResourceManagerFind = 0x0017D480;
    inline constexpr Address32 spResourceManagerRegister = 0x0017D670;
    inline constexpr Address32 spResourceManagerLoadResource = 0x0017D7E0;
    inline constexpr Address32 spResourceManagerLoadSceneGraph = 0x0017DA20;
    inline constexpr Address32 spResourceManagerDestructor = 0x0017DB10;
    inline constexpr Address32 spResourceManagerClone = 0x0017DC40;
    inline constexpr Address32 spResourceManagerAsyncBegin = 0x0017D0C0;
    inline constexpr Address32 spResourceManagerAsyncComplete = 0x0017D1D0;
    inline constexpr Address32 spResourceManagerVTableHeader = 0x0048EEE8;
    inline constexpr Address32 spResourceManagerVTable = 0x0048EEF0;
    inline constexpr Address32 spResourceManagerSupportVTable = 0x0048EF14;
    inline constexpr Address32 spResourceManagerSingleton = 0x0049F868;
    inline constexpr std::uint32_t spResourceManagerAllocationSize = 0x2C;
    inline constexpr std::uint32_t spTextureClassID = 0x2F281E13;
    inline constexpr std::uint32_t spTextureRegisteredBaseClassID = 0x44DE07FD;
    inline constexpr Address32 spTextureRegistration = 0x004A99D0;
    inline constexpr Address32 spTextureRegistrationInitializer = 0x00482D90;
    inline constexpr Address32 spTextureConstructor = 0x00173C00;
    inline constexpr Address32 spTextureDeletingDestructor = 0x00173B90;
    inline constexpr Address32 spTextureClone = 0x00173C70;
    inline constexpr Address32 spTextureRegistrationGetter = 0x0015A9E0;
    inline constexpr Address32 spTextureInitializeBuffer = 0x00173AA0;
    inline constexpr Address32 spTextureInitializeWrapper = 0x001739B0;
    inline constexpr Address32 spTextureNormalizeDimensions = 0x00173960;
    inline constexpr Address32 spTextureDimensionExponent = 0x00108EA0;
    inline constexpr Address32 spTextureVTableHeader = 0x0048EB30;
    inline constexpr Address32 spITextureVTableHeader = 0x0048EB54;
    inline constexpr Address32 spCubeTextureConstructor = 0x001738E0;
    inline constexpr Address32 spCubeTextureClone = 0x001F3660;
    inline constexpr std::uint32_t spCubeTextureAllocationSize = 0x38;
    inline constexpr std::uint32_t spTextureBufferClassID = 0x205B390B;
    inline constexpr Address32 spTextureBufferRegistration = 0x004A9A30;
    inline constexpr Address32 spTextureBufferRegistrationInitializer = 0x00482DD0;
    inline constexpr Address32 spTextureBufferFactory = 0x00173FC0;
    inline constexpr Address32 spTextureBufferConstructor = 0x00173EA0;
    inline constexpr Address32 spTextureBufferDeletingDestructor = 0x00173E00;
    inline constexpr Address32 spTextureBufferClone = 0x00173F00;
    inline constexpr Address32 spTextureBufferRegistrationGetter = 0x00173C80;
    inline constexpr Address32 spTextureBufferInitialize = 0x00173C90;
    inline constexpr Address32 spTextureBufferVTableHeader = 0x0048EBB0;
    inline constexpr std::uint32_t spTextureDataClassID = 0x78EA082B;
    inline constexpr Address32 spTextureDataRegistration = 0x004A8200;
    inline constexpr Address32 spTextureDataRegistrationInitializer = 0x00480E64;
    inline constexpr Address32 spTextureDataFactory = 0x00134CB0;
    inline constexpr Address32 spTextureDataConstructor = 0x001790E0;
    inline constexpr Address32 spTextureDataDeletingDestructor = 0x00178F40;
    inline constexpr Address32 spTextureDataClone = 0x00134BF0;
    inline constexpr Address32 spTextureDataRegistrationGetter = 0x00135B10;
    inline constexpr Address32 spTextureDataCopyTextureBuffer = 0x00178CD0;
    inline constexpr Address32 spTextureDataCopyTextureBufferThunk = 0x00135D30;
    inline constexpr Address32 spTextureDataReleasePayloads = 0x00178E60;
    inline constexpr Address32 spTextureDataReleasePayloadsThunk = 0x00135D20;
    inline constexpr Address32 spTextureDataPrimaryVTableHeader = 0x0048D980;
    inline constexpr Address32 spTextureDataInterfaceVTableHeader = 0x0048D9A4;
    inline constexpr std::uint32_t spTextureDataAllocationSize = 0x498;
    inline constexpr std::uint32_t spIndexBufferClassID = 0x77D5669F;
    inline constexpr Address32 spIndexBufferRegistration = 0x004A8C80;
    inline constexpr Address32 spIndexBufferRegistrationInitializer = 0x00482310;
    inline constexpr Address32 spIndexBufferFactory = 0x00159A50;
    inline constexpr Address32 spIndexBufferConstructor = 0x00159910;
    inline constexpr Address32 spIndexBufferDeletingDestructor = 0x00159890;
    inline constexpr Address32 spIndexBufferClone = 0x00159960;
    inline constexpr Address32 spIndexBufferRegistrationGetter = 0x001591F0;
    inline constexpr Address32 spIndexBufferWrite = 0x00159200;
    inline constexpr Address32 spIndexBufferRead = 0x00159330;
    inline constexpr Address32 spIndexBufferInitializeIndexCount = 0x001594C0;
    inline constexpr Address32 spIndexBufferInitialize = 0x001595C0;
    inline constexpr Address32 spIndexBufferPrimitiveCountFromIndices = 0x001596A0;
    inline constexpr Address32 spIndexBufferIndexCountFromPrimitives = 0x001596F0;
    inline constexpr Address32 spIndexBufferRelease = 0x00159740;
    inline constexpr Address32 spIndexBufferCopyBuffer = 0x00159780;
    inline constexpr Address32 spIndexBufferVTable = 0x0048E1D0;
    inline constexpr std::uint32_t spEngineCoreClassID = 0x0E9F6B8C;
    inline constexpr std::uint32_t spGameLevelSerializerClassID = 0x72B27469;
    inline constexpr Address32 spGameLevelSerializerRegistration = 0x004A8C20;
    inline constexpr Address32 spGameLevelSerializerRegistrationInitializer = 0x004822D0;
    inline constexpr Address32 spGameLevelSerializerFactory = 0x00159180;
    inline constexpr Address32 spGameLevelSerializerConstructor = 0x00159060;
    inline constexpr Address32 spGameLevelSerializerDestructor = 0x00159000;
    inline constexpr Address32 spGameLevelSerializerClone = 0x001590A0;
    inline constexpr Address32 spGameLevelSerializerRegistrationGetter = 0x00158960;
    inline constexpr Address32 spGameLevelSerializerReadInstance = 0x00158970;
    inline constexpr Address32 spGameLevelSerializerRead = 0x00158D30;
    inline constexpr Address32 spGameLevelSerializerVTable = 0x0048E1A0;
    inline constexpr std::uint32_t spGameLevelClassID = 0x4A45115B;
    inline constexpr Address32 spGameLevelRegistration = 0x004A8BC0;
    inline constexpr Address32 spGameLevelRegistrationInitializer = 0x00482290;
    inline constexpr Address32 spGameLevelFactory = 0x001588F0;
    inline constexpr Address32 spGameLevelDestructor = 0x00158680;
    inline constexpr Address32 spGameLevelClone = 0x00158810;
    inline constexpr Address32 spGameLevelRegistrationGetter = 0x00158330;
    inline constexpr Address32 spGameLevelAddInstance = 0x00158630;
    inline constexpr Address32 spGameLevelVTable = 0x0048E170;
    inline constexpr std::uint32_t spTemplateSerializerClassID = 0x41577707;
    inline constexpr Address32 spTemplateSerializerRegistration = 0x004A8B60;
    inline constexpr Address32 spTemplateSerializerRegistrationInitializer = 0x00482250;
    inline constexpr Address32 spTemplateSerializerFactory = 0x001582A0;
    inline constexpr Address32 spTemplateSerializerConstructor = 0x00158170;
    inline constexpr Address32 spTemplateSerializerDestructor = 0x001580E0;
    inline constexpr Address32 spTemplateSerializerClone = 0x001581E0;
    inline constexpr Address32 spTemplateSerializerRegistrationGetter = 0x00157500;
    inline constexpr Address32 spTemplateSerializerDeserializeObject = 0x00157520;
    inline constexpr Address32 spTemplateSerializerRead = 0x00157DA0;
    inline constexpr Address32 spTemplateSerializerVTable = 0x0048E140;
    inline constexpr std::uint32_t spTemplateObjectClassID = 0x014E1394;
    inline constexpr Address32 spTemplateObjectRegistration = 0x004A8B00;
    inline constexpr Address32 spTemplateObjectRegistrationInitializer = 0x00482210;
    inline constexpr Address32 spTemplateObjectFactory = 0x001573B0;
    inline constexpr Address32 spTemplateObjectConstructor = 0x001571D0;
    inline constexpr Address32 spTemplateObjectDestructor = 0x00157160;
    inline constexpr Address32 spTemplateObjectClone = 0x001572F0;
    inline constexpr Address32 spTemplateObjectCopy = 0x00157070;
    inline constexpr Address32 spTemplateObjectRegistrationGetter = 0x00156CF0;
    inline constexpr Address32 spTemplateObjectReleaseResources = 0x00156F50;
    inline constexpr Address32 spTemplateObjectResolve = 0x00156D00;
    inline constexpr Address32 spTemplateObjectInitializePair = 0x001574F0;
    inline constexpr Address32 spTemplateObjectVTable = 0x0048E110;
    inline constexpr std::uint32_t spTemplateInstanceClassID = 0x1F6A7DA5;
    inline constexpr Address32 spTemplateInstanceRegistration = 0x004A8A40;
    inline constexpr Address32 spTemplateInstanceRegistrationInitializer = 0x00482170;
    inline constexpr Address32 spTemplateInstanceFactory = 0x00155C90;
    inline constexpr Address32 spTemplateInstanceDestructor = 0x00155940;
    inline constexpr Address32 spTemplateInstanceClone = 0x00155B80;
    inline constexpr Address32 spTemplateInstanceRegistrationGetter = 0x001548F0;
    inline constexpr Address32 spTemplateInstanceVTable = 0x0048E0A8;
    inline constexpr Address32 spTemplateInstanceNotification = 0x00154900;
    inline constexpr Address32 spTemplateInstanceRootFactory = 0x001A9160;
    inline constexpr std::uint32_t spTemplateManagerClassID = 0x04BB6643;
    inline constexpr Address32 spTemplateManagerRegistration = 0x004A8AA0;
    inline constexpr Address32 spTemplateManagerRegistrationInitializer = 0x004821D0;
    inline constexpr Address32 spTemplateManagerFactory = 0x00156C50;
    inline constexpr Address32 spTemplateManagerDestructor = 0x00156A10;
    inline constexpr Address32 spTemplateManagerClone = 0x00156B50;
    inline constexpr Address32 spTemplateManagerRegistrationGetter = 0x00156720;
    inline constexpr Address32 spTemplateManagerVTable = 0x0048E0D0;
    inline constexpr Address32 spTemplateManagerSupportVTable = 0x0048E0F4;
    inline constexpr Address32 spTemplateManagerFind = 0x00156730;
    inline constexpr Address32 spTemplateManagerClear = 0x00156870;
    inline constexpr Address32 spTemplateManagerAdd = 0x001569B0;
    inline constexpr Address32 spTemplateManagerSingleton = 0x0049F9A8;
    inline constexpr std::uint32_t spEntityManagerClassID = 0x48A15BCB;
    inline constexpr Address32 spEntityManagerRegistration = 0x004A8940;
    inline constexpr Address32 spEntityManagerRegistrationInitializer = 0x00482040;
    inline constexpr Address32 spEntityManagerFactory = 0x0014EE90;
    inline constexpr Address32 spEntityManagerConstructor = 0x0014EE20;
    inline constexpr Address32 spEntityManagerDestructor = 0x0014EBD0;
    inline constexpr Address32 spEntityManagerClone = 0x0014ED90;
    inline constexpr Address32 spEntityManagerRegistrationGetter = 0x0014E620;
    inline constexpr Address32 spEntityManagerVTable = 0x0048DF90;
    inline constexpr Address32 spEntityManagerSupportVTable = 0x0048DFB4;
    inline constexpr Address32 spEntityManagerAdd = 0x0014EB70;
    inline constexpr Address32 spEntityManagerRemove = 0x0014EA90;
    inline constexpr Address32 spEntityManagerClear = 0x0014E9A0;
    inline constexpr Address32 spEntityManagerDispatch = 0x0014E670;
    inline constexpr Address32 spEntityManagerListGetter = 0x0014E630;
    inline constexpr Address32 spEntityManagerSingleton = 0x0049F9AC;
    inline constexpr std::uint32_t spDebugManagerClassID = 0x37054B40;
    inline constexpr Address32 spDebugManagerRegistration = 0x004AD1B0;
    inline constexpr Address32 spDebugManagerRegistrationInitializer = 0x00484AB0;
    inline constexpr Address32 spDebugManagerFactory = 0x001D9DF0;
    inline constexpr Address32 spDebugManagerConstructor = 0x001D9C30;
    inline constexpr Address32 spDebugManagerDestructor = 0x001D9AD0;
    inline constexpr Address32 spDebugManagerClone = 0x001D9D30;
    inline constexpr Address32 spDebugManagerRegistrationGetter = 0x001D8560;
    inline constexpr Address32 spDebugManagerAdvanceCycle = 0x001D9A50;
    inline constexpr Address32 spDebugManagerSupportDestructorThunk = 0x001D9E30;
    inline constexpr Address32 spDebugManagerVTable = 0x00490EF0;
    inline constexpr Address32 spDebugManagerSupportVTable = 0x00490F14;
    inline constexpr Address32 spDebugManagerSingleton = 0x0049F854;
    inline constexpr std::uint32_t spFontManagerClassID = 0x1640375E;
    inline constexpr std::uint32_t spPS2FontManagerClassID = 0x31650C4A;
    inline constexpr std::uint32_t spInputManagerClassID = 0x55A1304D;
    inline constexpr std::uint32_t spPS2InputManagerClassID = 0x462B48E1;
    inline constexpr Address32 spFontManagerRegistration = 0x004B9430;
    inline constexpr Address32 spPS2FontManagerRegistration = 0x004B72C0;
    inline constexpr Address32 spFontManagerRegistrationInitializer = 0x00482960;
    inline constexpr Address32 spPS2FontManagerRegistrationInitializer = 0x00485010;
    inline constexpr Address32 spFontManagerRegistrationGetter = 0x001678C0;
    inline constexpr Address32 spPS2FontManagerRegistrationGetter = 0x001EEBD0;
    inline constexpr Address32 spFontManagerAddFont = 0x001685C0;
    inline constexpr Address32 spFontManagerInitialize = 0x00168620;
    inline constexpr Address32 spFontManagerFindFont = 0x001688C0;
    inline constexpr Address32 spFontManagerDestructor = 0x00168970;
    inline constexpr Address32 spFontManagerConstructor = 0x00168AC0;
    inline constexpr Address32 spFontManagerClone = 0x00168BA0;
    inline constexpr Address32 spFontManagerVTable = 0x0048E5D0;
    inline constexpr Address32 spFontManagerSupportVTable = 0x0048E5F4;
    inline constexpr Address32 spFontManagerSingleton = 0x0049F8BC;
    inline constexpr Address32 spPS2FontManagerFactory = 0x001EEDA0;
    inline constexpr Address32 spPS2FontManagerDestructor = 0x001EEBE0;
    inline constexpr Address32 spPS2FontManagerClone = 0x001EEC50;
    inline constexpr Address32 spPS2FontManagerVTable = 0x004913A0;
    inline constexpr Address32 spPS2FontManagerSupportVTable = 0x004913C4;
    inline constexpr Address32 spInputManagerRegistration = 0x004A94F0;
    inline constexpr Address32 spPS2InputManagerRegistration = 0x004B76A0;
    inline constexpr Address32 spInputManagerRegistrationInitializer = 0x004829E0;
    inline constexpr Address32 spPS2InputManagerRegistrationInitializer = 0x00485150;
    inline constexpr Address32 spInputManagerRegistrationGetter = 0x0016D840;
    inline constexpr Address32 spInputManagerConstructor = 0x0016D8F0;
    inline constexpr Address32 spInputManagerDestructor = 0x0016D850;
    inline constexpr Address32 spInputManagerClone = 0x0016D9C0;
    inline constexpr Address32 spInputManagerSupportDestructorThunk = 0x0016D9D0;
    inline constexpr Address32 spInputManagerVTable = 0x0048E740;
    inline constexpr Address32 spInputManagerSupportVTable = 0x0048E764;
    inline constexpr Address32 spInputManagerDeviceVTable = 0x0048E770;
    inline constexpr Address32 spInputManagerSingleton = 0x0049F8B8;
    inline constexpr Address32 spPS2InputManagerRegistrationGetter = 0x001F0D80;
    inline constexpr Address32 spPS2InputManagerFactory = 0x001F1370;
    inline constexpr Address32 spPS2InputManagerDestructor = 0x001F1170;
    inline constexpr Address32 spPS2InputManagerClone = 0x001F1200;
    inline constexpr Address32 spPS2InputManagerInitialize = 0x001F10B0;
    inline constexpr Address32 spPS2InputManagerShutdown = 0x001F0FF0;
    inline constexpr Address32 spPS2InputManagerVTable = 0x00491560;
    inline constexpr Address32 spPS2InputManagerSupportVTable = 0x00491584;
    inline constexpr Address32 spPS2InputManagerDeviceVTable = 0x00491590;
    inline constexpr std::uint32_t spPS2InputManagerControllerCapacity = 2;
    inline constexpr Address32 spEngineCoreRegistration = 0x004A7C60;
    inline constexpr Address32 spEngineCoreRegistrationInitializer = 0x004807B0;
    inline constexpr Address32 spEngineCoreRegistrationGetter = 0x001313E0;
    inline constexpr Address32 spEngineCoreFactory = 0x001335C0;
    inline constexpr Address32 spEngineCoreConstructor = 0x00133380;
    inline constexpr Address32 spEngineCoreDestructor = 0x00133270;
    inline constexpr Address32 spEngineCoreSupportDeletingDestructor = 0x00133490;
    inline constexpr Address32 spEngineCoreClone = 0x00133500;
    inline constexpr Address32 spEngineCoreVTable = 0x0048D340;
    inline constexpr Address32 spEngineCoreSupportVTable = 0x0048D364;
    inline constexpr Address32 spEngineCoreSingleton = 0x0049F850;
    inline constexpr std::uint32_t spEngineCoreFirstSupportSlot = 0x0C;
    inline constexpr std::uint32_t spEngineCoreLastSupportSlot = 0x50;
    inline constexpr std::uint32_t spEngineCoreLocalSlotCount = 18;

    inline constexpr Address32 spEngineCoreLocalTargets[spEngineCoreLocalSlotCount]{
        0x00131E30, 0x00131C90, 0x001318B0, 0x00132F80,
        0x00132B60, 0x00131F90, 0x00133100, 0x00132FD0,
        0x00132360, 0x001317E0, 0x00131750, 0x00132A70,
        0x00132490, 0x00132430, 0x00132A80, 0x00132580,
        0x00132A60, 0x00131EE0,
    };
}
