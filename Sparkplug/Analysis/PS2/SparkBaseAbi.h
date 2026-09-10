#pragma once

// Byte-exact 32-bit layout evidence.  These are inspection structures, not the
// portable reconstruction classes from Code/SparkBase/spBaseObject.h.

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    using Address32 = std::uint32_t;

    struct spBaseObjectLayout final
    {
        Address32 vtableAddress;       // 0x00
        Address32 field04;             // 0x04: optional reverse-reference list;
                                       //       original member name unknown
        std::uint16_t referenceCount;  // 0x08: intrusive strong-reference count;
                                       //       original member name unknown
        std::uint8_t padding0A[2];     // 0x0a: alignment padding; neither native
                                       //       constructor writes these bytes
        std::uint32_t field0C;         // 0x0c: zero-initialized and copied by
                                       //       native copy paths; role unknown
    };

    // field04 points to this list header.  The embedded next/previous values
    // initially point at the sentinel at header + 0x04.
    struct spReverseReferenceListLayout final
    {
        std::uint32_t count;            // 0x00
        Address32 sentinelNext;         // 0x04
        Address32 sentinelPrevious;     // 0x08
    };

    struct spReverseReferenceNodeLayout final
    {
        Address32 next;                 // 0x00
        Address32 previous;             // 0x04
        Address32 referringObject;      // 0x08
    };

    // Passed to vtable slot +0x0C while changes are propagated to objects in
    // the reverse-reference list.  The type and member names are analytical;
    // sub_00100520 constructs all eight words and sub_00100610 forwards an
    // existing record unchanged.
    struct spReverseReferenceNotificationLayout final
    {
        std::uint32_t eventCode;         // 0x00
        std::uint32_t field04;           // 0x04: zero in known constructors
        std::uint32_t field08;           // 0x08: zero in known constructors
        std::uint32_t field0C;           // 0x0c: zero in known constructors
        Address32 sourceObject;          // 0x10: target whose state changed
        Address32 referringObject;       // 0x14: current owner or zero
        std::uint32_t field18;           // 0x18: caller-supplied context
        std::uint32_t field1C;           // 0x1c: caller-supplied context
    };

    struct spNamedObjectLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 sharedNameEntry;     // 0x10; refcount byte at entry + 0x08
    };

    struct spCrossPlatformLayout final
    {
        spNamedObjectLayout base;      // 0x00; no additional instance fields
    };

    struct spAppLayout final
    {
        spCrossPlatformLayout base;    // 0x00; actual C++ construction base
        Address32 supportVTable;       // 0x14: singleton-support subobject
        std::uint8_t stateFlag;        // 0x18: constructor value zero
        std::uint8_t padding19[3];     // 0x19
        Address32 ownedText;           // 0x1c: nullable owned C string
    };

    struct spErrorLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t errorCode;       // 0x10
        std::uint32_t severity;        // 0x14: 0 information .. 3 fatal
        Address32 sourceFile;          // 0x18
        std::uint32_t sourceLine;      // 0x1c
        Address32 message;             // 0x20: manager data-stack pointer
        Address32 nextError;           // 0x24
    };

    struct spErrorManagerLayout final
    {
        spBaseObjectLayout base;       // 0x000
        Address32 supportVTable;       // 0x010
        std::uint8_t dataStack[0x400]; // 0x014
        std::uint32_t dataUsed;        // 0x414
        Address32 errorHead;           // 0x418
        Address32 handler;             // 0x41c
        std::uint8_t handlerResolved;  // 0x420
        std::uint8_t padding421[3];    // 0x421
    };

    struct spPS2ErrorManagerLayout final
    {
        spErrorManagerLayout base;     // 0x000; stateless platform leaf
    };

    struct spSubscriptionManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10: singleton-support subobject
        std::uint8_t subscriptionTree[0x0C]; // 0x14: PS2 container ABI
    };

    // Constructor 0x001E7730 writes no fields beyond the complete spApp base.
    struct spPS2AppLayout final
    {
        spAppLayout base;              // 0x00
    };

    struct spStreamLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        std::uint32_t logicalOrigin;   // 0x14: subtracted from absolute position;
                                       //       original member name unknown
        Address32 ownedStreamName;     // 0x18: nullable heap C string used in
                                       //       stream errors; original name unknown
    };

    struct spMemoryStreamLayout final
    {
        spStreamLayout base;            // 0x00
        std::uint32_t capacity;         // 0x1c
        std::uint32_t logicalSize;      // 0x20
        std::uint32_t growthQuantum;    // 0x24; native default 0x1388 / 5000
        std::uint8_t resizeEnabled;     // 0x28
        std::uint8_t padding29[3];      // 0x29
        std::uint32_t absolutePosition; // 0x2c
        Address32 buffer;               // 0x30
        std::uint8_t ownsBuffer;        // 0x34; only Close tests this flag
        std::uint8_t padding35[3];      // 0x35
    };

    struct spFileStreamLayout final
    {
        spStreamLayout base;            // 0x00; no additional storage observed
    };

    struct spPS2FileBufferLayout final
    {
        std::uint32_t fileOffset;        // 0x00: absolute start represented by buffer
        std::uint32_t cursorOffset;      // 0x04: current offset inside buffer
        Address32 data;                  // 0x08: 64-byte-aligned data pointer
        std::uint8_t ready;              // 0x0c: wait is required while zero
        std::uint8_t padding0D[3];       // 0x0d
    };

    // Embedded backend called "ShellFile" by spPS2FileStream's own log text.
    // Member spellings remain analytical even though the type label survives.
    struct ShellFileLayout final
    {
        char path[0x80];                 // 0x00
        std::uint32_t openMode;          // 0x80: backend mode 1/2/3
        std::int32_t handle;             // 0x84
        std::uint32_t position;          // 0x88
        std::uint32_t size;              // 0x8c
        std::uint32_t startSector;       // 0x90: analytical role
        std::uint8_t backendKind;        // 0x94
        std::uint8_t padding95[3];       // 0x95
    };

    struct spPS2FileStreamLayout final
    {
        spFileStreamLayout base;         // 0x00
        std::uint32_t openMode;          // 0x1c
        std::uint8_t resolvedFromPCK;    // 0x20: analytical role
        std::uint8_t pathBackendKind;    // 0x21: host0:/atfile distinction
        std::uint8_t padding22[2];       // 0x22
        std::uint32_t physicalSize;      // 0x24: ShellFile/backing-file size
        std::uint8_t isOpen;             // 0x28
        std::uint8_t padding29[3];       // 0x29
        std::uint32_t logicalSizeOverride; // 0x2c: PCK entry size, zero if absent
        std::uint32_t bufferSize;        // 0x30: 0x4000 or 0x20000
        std::uint32_t absolutePosition;  // 0x34
        std::uint8_t buffersInitialized; // 0x38: analytical role
        std::uint8_t padding39[3];       // 0x39
        ShellFileLayout shellFile;       // 0x3c: platform file backend
        std::uint32_t activeBufferIndex; // 0xd4: 0 or 1
        spPS2FileBufferLayout buffers[2];// 0xd8
        std::uint8_t asyncManagerPath;   // 0xf8: selects shared-manager methods
        std::uint8_t paddingF9[3];       // 0xf9
        std::uint8_t fastSourceKind;     // 0xfc: analytical role
        std::uint8_t paddingFD[3];       // 0xfd
        std::uint32_t fastLsnOrHandle;   // 0x100: analytical role
        std::uint32_t fastOrigin;        // 0x104
        std::uint32_t fastPhysicalSize;  // 0x108
        Address32 auxiliaryPath;         // 0x10c: optional owned string
        Address32 physicalPath;          // 0x110: optional owned string
    };

    struct spAsyncFileStreamManagerLayout final
    {
        spCrossPlatformLayout base;     // 0x00
        Address32 supportVTable;        // 0x14: polymorphic singleton support
    };

    struct spPS2AsyncFileRequestLayout final
    {
        char streamName[0x100];          // 0x000
        std::uint32_t commandHandle;     // 0x100
        Address32 destination;           // 0x104: spMemoryStream*
        Address32 completionCallback;    // 0x108
        Address32 completionContext;     // 0x10c
        std::uint32_t field110;          // 0x110: never read by known update path
        std::uint32_t logicalSector;      // 0x114: LSN in surviving log text
        std::uint32_t byteCount;          // 0x118
    };

    struct spPS2AsyncFileStreamManagerLayout final
    {
        spAsyncFileStreamManagerLayout base; // 0x0000
        std::uint8_t constructed;         // 0x0018: set to one after initialization
        std::uint8_t padding19[3];        // 0x0019
        spPS2AsyncFileRequestLayout requests[50]; // 0x001c, 0x3778 bytes
        std::uint32_t queuedCount;        // 0x3794: analytical; no writer found
        std::uint32_t completionWatchdog; // 0x3798: set to 0x14 by update
        std::uint8_t readyState;          // 0x379c: analytical
        std::uint8_t padding379D[3];      // 0x379d
    };

    struct spPCKFileRecordLayout final
    {
        Address32 directory;           // 0x00: pointer after string fixup
        Address32 fileName;            // 0x04: pointer after string fixup
        std::uint32_t logicalSector;   // 0x08: byteOffset / 0x800
        std::uint32_t byteOffset;      // 0x0c
        std::uint32_t byteCount;       // 0x10
    };

    struct spPCKPackageRecordLayout final
    {
        Address32 packageName;         // 0x00: owned C string
        std::uint32_t priority;        // 0x04: descending search order
        std::uint32_t fileCount;       // 0x08
        Address32 files;               // 0x0c: 0x14-byte records
        Address32 stringBlock;         // 0x10: owned storage
        std::uint32_t physicalOrigin;  // 0x14
        std::uint32_t physicalSize;    // 0x18
    };

    struct spPCKManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        std::uint32_t packageCapacity; // 0x14
        std::uint32_t packageCount;    // 0x18
        Address32 packages;            // 0x1c: 0x1c-byte records
        std::uint32_t field20;         // 0x20: role unknown
        std::uint8_t trackOpenedNames; // 0x24: analytical role
        std::uint8_t trackingPaused;   // 0x25: analytical role
        std::uint8_t padding26[2];     // 0x26
        std::uint32_t nameCapacity;    // 0x28
        std::uint32_t nameCount;       // 0x2c
        Address32 names;               // 0x30: owned C-string pointers
    };

    struct spPS2HelperLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        std::uint32_t mode;            // 0x14: 0/2 host path, other disc path
        std::uint32_t field18;         // 0x18: set by sub_001E93B0
        char pathPrefix[0x100];        // 0x1c: constructor value "host0:"
    };

    struct spPS2IOPModuleManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        Address32 moduleRoot;          // 0x14: owned C string
        std::uint8_t field18;          // 0x18: set by sub_001E9B20
        std::uint8_t padding19[3];     // 0x19
        std::uint32_t moduleCount;     // 0x1c: linked-list header count
        Address32 firstModule;         // 0x20: sentinel next
        Address32 lastModule;          // 0x24: sentinel previous
    };

    struct spPS2IOPModuleListNodeLayout final
    {
        Address32 next;                // 0x00
        Address32 previous;            // 0x04
        Address32 moduleName;          // 0x08: owned C string
    };

    // Embedded in every RTTI registration at +0x50.  The original type name
    // is not known; this evidence name describes its observed behavior.
    struct spPropertyGroupLayout final
    {
        Address32 ownerRegistration;   // 0x00: containing RTTI registration
        std::uint32_t propertyCount;   // 0x04
        Address32 firstProperty;       // 0x08: contiguous 0x58-byte records
    };

    struct spRTTIRegistrationLayout final
    {
        std::uint32_t classID;         // 0x00
        std::uint32_t baseClassID;     // 0x04
        char className[0x40];          // 0x08; copied as a zero-terminated string
        Address32 baseRegistration;    // 0x48
        Address32 factoryCallback;     // 0x4c
        spPropertyGroupLayout properties; // 0x50
        std::uint32_t field5C;         // 0x5c: role still unresolved
    };

    // Analytical evidence name.  Manager constructors use this polymorphic
    // secondary base to install and clear their process-wide instance.  Its
    // exact original (probably template-specialization) name is unknown.
    struct SingletonSupportSubobjectLayout final
    {
        Address32 vtableAddress;        // 0x00
    };

    // These systems derive from spBaseObject and the singleton-support base.
    struct spCloneManagerLayout final
    {
        spBaseObjectLayout base;        // 0x00
        SingletonSupportSubobjectLayout singletonSupport; // 0x10
        std::uint32_t cloneDepth;       // 0x14
    };

    struct spRTTIManagerLayout final
    {
        spBaseObjectLayout base;        // 0x00
        SingletonSupportSubobjectLayout singletonSupport; // 0x10
        std::uint8_t registrationTree[0x10]; // 0x14
    };

    struct spPropertySystemLayout final
    {
        spBaseObjectLayout base;        // 0x00
        SingletonSupportSubobjectLayout singletonSupport; // 0x10
        std::uint32_t propertyCapacity; // 0x14; analytical field name
        std::uint32_t propertyCount;    // 0x18; analytical field name
        Address32 properties;           // 0x1c; array of 0x58-byte records
    };

    // Compiler-specific callable representation used by PS2 property records.
    // The original typedef/class name is not known; this evidence name states
    // the observed ABI rather than guessing the source declaration.
    struct Ps2MemberFunctionDescriptorLayout final
    {
        std::int32_t thisAdjustment;                    // 0x00
        std::int32_t virtualTableByteOffsetOrDirectMarker; // 0x04; <0 is direct,
                                                           //       -1 observed
        Address32 functionOrVTablePointerOffset;        // 0x08
    };

    // Analytical evidence name: no original class/type name is known yet.
    struct PropertyRecord58Layout final
    {
        Address32 comparisonVTable;      // 0x00: type-specific equality strategy
        Address32 propertyName;          // 0x04: zero-terminated string
        std::uint32_t propertyType;      // 0x08: parser dispatch value
        std::uint32_t field0C;           // 0x0c: role still unresolved
        std::uint32_t field10;           // 0x10: role still unresolved
        std::uint32_t accessorFlags;     // 0x14: bit 0 passes propertyName
        Ps2MemberFunctionDescriptorLayout getter; // 0x18
        Ps2MemberFunctionDescriptorLayout setter; // 0x24
        std::uint8_t field30[0x0C];       // 0x30: type-specific union; see docs
        std::uint8_t field3C[0x0C];       // 0x3c: descriptor-like, unresolved
        Address32 field48;               // 0x48: optional stream-path callback
        std::uint32_t field4C;            // 0x4c: role still unresolved
        std::uint8_t typeSpecificPayload[0x08]; // 0x50
    };

    static_assert(sizeof(spBaseObjectLayout) == 0x10);
    static_assert(offsetof(spBaseObjectLayout, field04) == 0x04);
    static_assert(offsetof(spBaseObjectLayout, referenceCount) == 0x08);
    static_assert(offsetof(spBaseObjectLayout, padding0A) == 0x0A);
    static_assert(offsetof(spBaseObjectLayout, field0C) == 0x0C);

    static_assert(sizeof(spReverseReferenceListLayout) == 0x0C);
    static_assert(offsetof(spReverseReferenceListLayout, sentinelNext) == 0x04);
    static_assert(sizeof(spReverseReferenceNodeLayout) == 0x0C);
    static_assert(offsetof(spReverseReferenceNodeLayout, referringObject) == 0x08);
    static_assert(sizeof(spReverseReferenceNotificationLayout) == 0x20);
    static_assert(offsetof(spReverseReferenceNotificationLayout, sourceObject) == 0x10);
    static_assert(offsetof(spReverseReferenceNotificationLayout, referringObject) == 0x14);

    static_assert(sizeof(spNamedObjectLayout) == 0x14);
    static_assert(offsetof(spNamedObjectLayout, sharedNameEntry) == 0x10);
    static_assert(sizeof(spCrossPlatformLayout) == 0x14);
    static_assert(sizeof(spAppLayout) == 0x20);
    static_assert(offsetof(spAppLayout, supportVTable) == 0x14);
    static_assert(offsetof(spAppLayout, stateFlag) == 0x18);
    static_assert(offsetof(spAppLayout, ownedText) == 0x1C);
    static_assert(sizeof(spErrorLayout) == 0x28);
    static_assert(offsetof(spErrorLayout, errorCode) == 0x10);
    static_assert(offsetof(spErrorLayout, severity) == 0x14);
    static_assert(offsetof(spErrorLayout, message) == 0x20);
    static_assert(offsetof(spErrorLayout, nextError) == 0x24);
    static_assert(sizeof(spErrorManagerLayout) == 0x424);
    static_assert(offsetof(spErrorManagerLayout, dataStack) == 0x14);
    static_assert(offsetof(spErrorManagerLayout, dataUsed) == 0x414);
    static_assert(offsetof(spErrorManagerLayout, errorHead) == 0x418);
    static_assert(offsetof(spErrorManagerLayout, handler) == 0x41C);
    static_assert(offsetof(spErrorManagerLayout, handlerResolved) == 0x420);
    static_assert(sizeof(spPS2ErrorManagerLayout) == 0x424);
    static_assert(sizeof(spSubscriptionManagerLayout) == 0x20);
    static_assert(offsetof(spSubscriptionManagerLayout, subscriptionTree) == 0x14);
    static_assert(sizeof(spPS2AppLayout) == 0x20);
    static_assert(sizeof(spStreamLayout) == 0x1C);
    static_assert(offsetof(spStreamLayout, logicalOrigin) == 0x14);
    static_assert(offsetof(spStreamLayout, ownedStreamName) == 0x18);
    static_assert(sizeof(spMemoryStreamLayout) == 0x38);
    static_assert(offsetof(spMemoryStreamLayout, capacity) == 0x1C);
    static_assert(offsetof(spMemoryStreamLayout, logicalSize) == 0x20);
    static_assert(offsetof(spMemoryStreamLayout, growthQuantum) == 0x24);
    static_assert(offsetof(spMemoryStreamLayout, resizeEnabled) == 0x28);
    static_assert(offsetof(spMemoryStreamLayout, absolutePosition) == 0x2C);
    static_assert(offsetof(spMemoryStreamLayout, buffer) == 0x30);
    static_assert(offsetof(spMemoryStreamLayout, ownsBuffer) == 0x34);
    static_assert(sizeof(spFileStreamLayout) == 0x1C);
    static_assert(sizeof(spPS2FileBufferLayout) == 0x10);
    static_assert(offsetof(spPS2FileBufferLayout, data) == 0x08);
    static_assert(offsetof(spPS2FileBufferLayout, ready) == 0x0C);
    static_assert(sizeof(ShellFileLayout) == 0x98);
    static_assert(offsetof(ShellFileLayout, openMode) == 0x80);
    static_assert(offsetof(ShellFileLayout, position) == 0x88);
    static_assert(offsetof(ShellFileLayout, size) == 0x8C);
    static_assert(offsetof(ShellFileLayout, backendKind) == 0x94);
    static_assert(sizeof(spPS2FileStreamLayout) == 0x114);
    static_assert(offsetof(spPS2FileStreamLayout, openMode) == 0x1C);
    static_assert(offsetof(spPS2FileStreamLayout, physicalSize) == 0x24);
    static_assert(offsetof(spPS2FileStreamLayout, isOpen) == 0x28);
    static_assert(offsetof(spPS2FileStreamLayout, logicalSizeOverride) == 0x2C);
    static_assert(offsetof(spPS2FileStreamLayout, bufferSize) == 0x30);
    static_assert(offsetof(spPS2FileStreamLayout, absolutePosition) == 0x34);
    static_assert(offsetof(spPS2FileStreamLayout, shellFile) == 0x3C);
    static_assert(offsetof(spPS2FileStreamLayout, activeBufferIndex) == 0xD4);
    static_assert(offsetof(spPS2FileStreamLayout, buffers) == 0xD8);
    static_assert(offsetof(spPS2FileStreamLayout, asyncManagerPath) == 0xF8);
    static_assert(offsetof(spPS2FileStreamLayout, fastSourceKind) == 0xFC);
    static_assert(offsetof(spPS2FileStreamLayout, fastLsnOrHandle) == 0x100);
    static_assert(offsetof(spPS2FileStreamLayout, auxiliaryPath) == 0x10C);
    static_assert(offsetof(spPS2FileStreamLayout, physicalPath) == 0x110);
    static_assert(sizeof(spAsyncFileStreamManagerLayout) == 0x18);
    static_assert(offsetof(spAsyncFileStreamManagerLayout, supportVTable) == 0x14);
    static_assert(sizeof(spPS2AsyncFileRequestLayout) == 0x11C);
    static_assert(offsetof(spPS2AsyncFileRequestLayout, commandHandle) == 0x100);
    static_assert(offsetof(spPS2AsyncFileRequestLayout, destination) == 0x104);
    static_assert(offsetof(spPS2AsyncFileRequestLayout, completionCallback) == 0x108);
    static_assert(offsetof(spPS2AsyncFileRequestLayout, logicalSector) == 0x114);
    static_assert(offsetof(spPS2AsyncFileRequestLayout, byteCount) == 0x118);
    static_assert(sizeof(spPS2AsyncFileStreamManagerLayout) == 0x37A0);
    static_assert(offsetof(spPS2AsyncFileStreamManagerLayout, requests) == 0x1C);
    static_assert(offsetof(spPS2AsyncFileStreamManagerLayout, queuedCount) == 0x3794);
    static_assert(offsetof(spPS2AsyncFileStreamManagerLayout,
        completionWatchdog) == 0x3798);
    static_assert(offsetof(spPS2AsyncFileStreamManagerLayout, readyState) == 0x379C);
    static_assert(sizeof(spPCKFileRecordLayout) == 0x14);
    static_assert(offsetof(spPCKFileRecordLayout, logicalSector) == 0x08);
    static_assert(offsetof(spPCKFileRecordLayout, byteOffset) == 0x0C);
    static_assert(offsetof(spPCKFileRecordLayout, byteCount) == 0x10);
    static_assert(sizeof(spPCKPackageRecordLayout) == 0x1C);
    static_assert(offsetof(spPCKPackageRecordLayout, files) == 0x0C);
    static_assert(offsetof(spPCKPackageRecordLayout, physicalOrigin) == 0x14);
    static_assert(offsetof(spPCKPackageRecordLayout, physicalSize) == 0x18);
    static_assert(sizeof(spPCKManagerLayout) == 0x34);
    static_assert(offsetof(spPCKManagerLayout, supportVTable) == 0x10);
    static_assert(offsetof(spPCKManagerLayout, packageCapacity) == 0x14);
    static_assert(offsetof(spPCKManagerLayout, packageCount) == 0x18);
    static_assert(offsetof(spPCKManagerLayout, packages) == 0x1C);
    static_assert(offsetof(spPCKManagerLayout, trackOpenedNames) == 0x24);
    static_assert(offsetof(spPCKManagerLayout, names) == 0x30);
    static_assert(sizeof(spPS2HelperLayout) == 0x11C);
    static_assert(offsetof(spPS2HelperLayout, supportVTable) == 0x10);
    static_assert(offsetof(spPS2HelperLayout, mode) == 0x14);
    static_assert(offsetof(spPS2HelperLayout, field18) == 0x18);
    static_assert(offsetof(spPS2HelperLayout, pathPrefix) == 0x1C);
    static_assert(sizeof(spPS2IOPModuleManagerLayout) == 0x28);
    static_assert(offsetof(spPS2IOPModuleManagerLayout, supportVTable) == 0x10);
    static_assert(offsetof(spPS2IOPModuleManagerLayout, moduleRoot) == 0x14);
    static_assert(offsetof(spPS2IOPModuleManagerLayout, field18) == 0x18);
    static_assert(offsetof(spPS2IOPModuleManagerLayout, moduleCount) == 0x1C);
    static_assert(offsetof(spPS2IOPModuleManagerLayout, firstModule) == 0x20);
    static_assert(sizeof(spPS2IOPModuleListNodeLayout) == 0x0C);
    static_assert(offsetof(spPS2IOPModuleListNodeLayout, moduleName) == 0x08);

    static_assert(sizeof(spPropertyGroupLayout) == 0x0C);
    static_assert(offsetof(spPropertyGroupLayout, propertyCount) == 0x04);
    static_assert(offsetof(spPropertyGroupLayout, firstProperty) == 0x08);

    static_assert(sizeof(spRTTIRegistrationLayout) == 0x60);
    static_assert(offsetof(spRTTIRegistrationLayout, className) == 0x08);
    static_assert(offsetof(spRTTIRegistrationLayout, baseRegistration) == 0x48);
    static_assert(offsetof(spRTTIRegistrationLayout, factoryCallback) == 0x4C);
    static_assert(offsetof(spRTTIRegistrationLayout, properties) == 0x50);

    static_assert(sizeof(SingletonSupportSubobjectLayout) == 0x04);

    static_assert(sizeof(spCloneManagerLayout) == 0x18);
    static_assert(offsetof(spCloneManagerLayout, singletonSupport) == 0x10);
    static_assert(offsetof(spCloneManagerLayout, cloneDepth) == 0x14);

    static_assert(sizeof(spRTTIManagerLayout) == 0x24);
    static_assert(offsetof(spRTTIManagerLayout, singletonSupport) == 0x10);
    static_assert(offsetof(spRTTIManagerLayout, registrationTree) == 0x14);

    static_assert(sizeof(spPropertySystemLayout) == 0x20);
    static_assert(offsetof(spPropertySystemLayout, singletonSupport) == 0x10);
    static_assert(offsetof(spPropertySystemLayout, propertyCapacity) == 0x14);
    static_assert(offsetof(spPropertySystemLayout, propertyCount) == 0x18);
    static_assert(offsetof(spPropertySystemLayout, properties) == 0x1C);
    static_assert(sizeof(Ps2MemberFunctionDescriptorLayout) == 0x0C);
    static_assert(offsetof(Ps2MemberFunctionDescriptorLayout,
        virtualTableByteOffsetOrDirectMarker) == 0x04);
    static_assert(offsetof(Ps2MemberFunctionDescriptorLayout,
        functionOrVTablePointerOffset) == 0x08);
    static_assert(sizeof(PropertyRecord58Layout) == 0x58);
    static_assert(offsetof(PropertyRecord58Layout, propertyName) == 0x04);
    static_assert(offsetof(PropertyRecord58Layout, propertyType) == 0x08);
    static_assert(offsetof(PropertyRecord58Layout, accessorFlags) == 0x14);
    static_assert(offsetof(PropertyRecord58Layout, getter) == 0x18);
    static_assert(offsetof(PropertyRecord58Layout, setter) == 0x24);
    static_assert(offsetof(PropertyRecord58Layout, field30) == 0x30);
    static_assert(offsetof(PropertyRecord58Layout, field3C) == 0x3C);
    static_assert(offsetof(PropertyRecord58Layout, field48) == 0x48);
    static_assert(offsetof(PropertyRecord58Layout, typeSpecificPayload) == 0x50);

    inline constexpr std::uint32_t spBaseObjectClassID = 0x415352A1;
    inline constexpr std::uint32_t spNamedObjectClassID = 0x44DE07FD;
    inline constexpr std::uint32_t spCrossPlatformClassID = 0x20A72504;
    inline constexpr std::uint32_t spAppClassID = 0x391B146A;
    inline constexpr std::uint32_t spPS2AppClassID = 0x354B1350;
    inline constexpr std::uint32_t spErrorClassID = 0x789B29B9;
    inline constexpr std::uint32_t spErrorManagerClassID = 0x660E40D8;
    inline constexpr std::uint32_t spPS2ErrorManagerClassID = 0x226A416D;
    inline constexpr std::uint32_t spSubscriptionManagerClassID = 0xE4567D00;
    inline constexpr std::uint32_t spStreamClassID = 0x6CC80D8A;
    inline constexpr std::uint32_t spMemoryStreamClassID = 0x57177DB5;
    inline constexpr std::uint32_t spFileStreamClassID = 0x5E0623EC;
    inline constexpr std::uint32_t spPS2FileStreamClassID = 0x12FDDAB3;
    inline constexpr std::uint32_t spAsyncFileStreamManagerClassID = 0x7EA51364;
    inline constexpr std::uint32_t spPS2AsyncFileStreamManagerClassID = 0x57746EF7;
    inline constexpr std::uint32_t spPCKManagerClassID = 0x2B9D1649;
    inline constexpr std::uint32_t spPS2HelperClassID = 0x7E3C519B;
    inline constexpr std::uint32_t spPS2IOPModuleManagerClassID = 0x59264170;
    inline constexpr std::uint32_t spCloneManagerClassID = 0xC4419F78;
    inline constexpr std::uint32_t spRTTIManagerClassID = 0x5EA0637A;
    inline constexpr std::uint32_t spPropertySystemClassID = 0x584F73E4;

    inline constexpr Address32 spBaseObjectVTable = 0x0048C5E0;
    inline constexpr Address32 spNamedObjectVTable = 0x0048C690;
    inline constexpr Address32 spCrossPlatformVTable = 0x0048C660;
    inline constexpr Address32 spBaseObjectRegistration = 0x0049FF60;
    inline constexpr Address32 spNamedObjectRegistration = 0x004A02C0;
    inline constexpr Address32 spCrossPlatformRegistration = 0x004A0260;
    inline constexpr Address32 spAppRegistration = 0x0049FF00;
    inline constexpr Address32 spPS2AppRegistration = 0x004B0EC0;
    inline constexpr Address32 spErrorRegistration = 0x004A0350;
    inline constexpr Address32 spErrorManagerRegistration = 0x004A08C0;
    inline constexpr Address32 spPS2ErrorManagerRegistration = 0x004B8730;
    inline constexpr Address32 spSubscriptionManagerRegistration = 0x004A2470;
    inline constexpr Address32 spStreamRegistration = 0x004A2730;
    inline constexpr Address32 spMemoryStreamRegistration = 0x004A2670;
    inline constexpr Address32 spFileStreamRegistration = 0x004A2610;
    inline constexpr Address32 spPS2FileStreamRegistration = 0x004AD2D0;
    inline constexpr Address32 spAsyncFileStreamManagerRegistration = 0x004A25B0;
    inline constexpr Address32 spPS2AsyncFileStreamManagerRegistration = 0x004AD270;
    inline constexpr Address32 spPCKManagerRegistration = 0x004A26D0;
    inline constexpr Address32 spPS2HelperRegistration = 0x004B6E40;
    inline constexpr Address32 spPS2IOPModuleManagerRegistration = 0x004B6EA0;
    inline constexpr Address32 spCloneManagerRegistration = 0x004A0200;
    inline constexpr Address32 spPropertySystemRegistration = 0x004A24F0;
    inline constexpr Address32 spRTTIManagerRegistration = 0x004A2550;

    inline constexpr Address32 spCloneManagerVTable = 0x0048C620;
    inline constexpr Address32 spAppVTable = 0x0048C580;
    inline constexpr Address32 spAppSupportVTable = 0x0048C5A4;
    inline constexpr Address32 spPS2AppVTable = 0x00491030;
    inline constexpr Address32 spPS2AppSupportVTable = 0x00491054;
    inline constexpr Address32 spErrorVTable = 0x0048C6E8;
    inline constexpr Address32 spErrorManagerVTable = 0x0048C720;
    inline constexpr Address32 spErrorManagerSupportVTable = 0x0048C744;
    inline constexpr Address32 spPS2ErrorManagerVTable = 0x00491E60;
    inline constexpr Address32 spPS2ErrorManagerSupportVTable = 0x00491E84;
    inline constexpr Address32 spSubscriptionManagerVTable = 0x0048C860;
    inline constexpr Address32 spSubscriptionManagerSupportVTable = 0x0048C884;
    inline constexpr Address32 spStreamVTable = 0x0048CA90;
    inline constexpr Address32 spMemoryStreamVTable = 0x0048C9F0;
    inline constexpr Address32 spFileStreamVTable = 0x0048C9A0;
    inline constexpr Address32 spPS2FileStreamVTable = 0x00490FA0;
    inline constexpr Address32 spAsyncFileStreamManagerVTable = 0x0048C950;
    inline constexpr Address32 spAsyncFileStreamManagerSupportVTable = 0x0048C974;
    inline constexpr Address32 spPS2AsyncFileStreamManagerVTable = 0x00490F60;
    inline constexpr Address32 spPS2AsyncFileStreamManagerSupportVTable = 0x00490F84;
    inline constexpr Address32 spPCKManagerVTable = 0x0048CA40;
    inline constexpr Address32 spPCKManagerSupportVTable = 0x0048CA64;
    inline constexpr Address32 spPS2HelperVTable = 0x00491080;
    inline constexpr Address32 spPS2HelperSupportVTable = 0x004910A4;
    inline constexpr Address32 spPS2IOPModuleManagerVTable = 0x004910C0;
    inline constexpr Address32 spPS2IOPModuleManagerSupportVTable = 0x004910E4;
    inline constexpr Address32 spPropertySystemVTable = 0x0048C8E0;
    inline constexpr Address32 spRTTIManagerVTable = 0x0048C910;
    inline constexpr Address32 spCloneMap = 0x004A01F0;

    // Analytical flag names.  Both platform implementations test these exact
    // bits, but the original enum type and enumerator spellings are unknown.
    inline constexpr std::uint32_t spStreamOpenRead = 0x01;
    inline constexpr std::uint32_t spStreamOpenWrite = 0x02;
    inline constexpr std::uint32_t spStreamOpenReadWrite = 0x04;
    inline constexpr std::uint32_t spStreamOpenAppend = 0x08;

    inline constexpr Address32 spBaseObjectDestructor = 0x00102B50;
    inline constexpr Address32 spBaseObjectConstructor = 0x00102BF0;
    inline constexpr Address32 spBaseObjectNotifyReverseReferences = 0x00100520;
    inline constexpr Address32 spBaseObjectForwardReverseReferenceNotification = 0x00100610;
    inline constexpr Address32 spBaseObjectEmptyNotificationHandler = 0x00100810;
    inline constexpr Address32 spBaseObjectRemoveReverseReference = 0x00100820;
    inline constexpr Address32 spBaseObjectAddReverseReference = 0x00100860;
    inline constexpr Address32 spReverseReferenceListDestructor = 0x001008E0;
    inline constexpr Address32 spBaseObjectTextPropertyParser = 0x00100960;
    inline constexpr Address32 spBaseObjectApplyXmlPropertyQueue = 0x00100D10;
    inline constexpr Address32 spBaseObjectCopyPropertyStream = 0x001012F0;
    inline constexpr Address32 spBaseObjectReadPropertyStream = 0x00101E80;
    inline constexpr Address32 spBaseObjectCreateFromXmlStream = 0x00102840;
    inline constexpr Address32 spBaseObjectCreateFromBinaryStream = 0x00102A00;
    inline constexpr Address32 spNamedObjectDestructor = 0x00105EE0;
    inline constexpr Address32 spNamedObjectFactory = 0x00106070;
    inline constexpr Address32 spCrossPlatformRegistrationGetter = 0x00105CF0;
    inline constexpr Address32 spCrossPlatformDestructor = 0x00105D00;
    inline constexpr Address32 spCrossPlatformConstructor = 0x00105D60;
    inline constexpr Address32 spCrossPlatformNullClone = 0x00105DA0;
    inline constexpr Address32 spAppRegistrationGetter = 0x00100000;
    inline constexpr Address32 spAppDefaultEmptyString = 0x00100170;
    inline constexpr Address32 spAppDestructor = 0x00100180;
    inline constexpr Address32 spAppConstructor = 0x00100230;
    inline constexpr Address32 spAppSupportDeletingDestructor = 0x001002A0;
    inline constexpr Address32 spAppNullClone = 0x001002F0;
    inline constexpr Address32 spAppSupportDestructorThunk = 0x00100300;
    inline constexpr Address32 spAppGlobal = 0x0049F800;
    inline constexpr Address32 spPS2AppRegistrationGetter = 0x001E75A0;
    inline constexpr Address32 spErrorConstructor = 0x00107560;
    inline constexpr Address32 spErrorInitialize = 0x001074E0;
    inline constexpr Address32 spErrorDestructor = 0x00107500;
    inline constexpr Address32 spErrorRegistrationGetter = 0x00107030;
    inline constexpr Address32 spErrorFactory = 0x001076A0;
    inline constexpr Address32 spErrorClone = 0x001075B0;
    inline constexpr Address32 spErrorDescribe = 0x00107250;
    inline constexpr Address32 spErrorDescribeSeverity = 0x00107170;
    inline constexpr Address32 spErrorDescribeSource = 0x00107040;
    inline constexpr Address32 spErrorName = 0x00107160;
    inline constexpr Address32 spErrorManagerRegistrationInitializer = 0x0047F540;
    inline constexpr Address32 spErrorManagerConstructor = 0x00107CB0;
    inline constexpr Address32 spErrorManagerDestructor = 0x00107C20;
    inline constexpr Address32 spErrorManagerRegistrationGetter = 0x00107710;
    inline constexpr Address32 spErrorManagerFormatAndRoute = 0x00107840;
    inline constexpr Address32 spErrorManagerHandle = 0x00107720;
    inline constexpr Address32 spErrorManagerResolveHandlerPure = 0;
    inline constexpr Address32 spErrorManagerClear = 0x00107B50;
    inline constexpr Address32 spErrorManagerGlobal = 0x0049F80C;
    inline constexpr Address32 spPS2ErrorManagerRegistrationInitializer = 0x004859B0;
    inline constexpr Address32 spPS2ErrorManagerRegistrationGetter = 0x0020DA20;
    inline constexpr Address32 spPS2ErrorManagerDestructor = 0x0020D9B0;
    inline constexpr Address32 spPS2ErrorManagerClone = 0x0020DA60;
    inline constexpr Address32 spPS2ErrorManagerResolveHandler = 0x0020DA30;
    inline constexpr Address32 spPS2ErrorManagerFormatAdapter = 0x0020DA40;
    inline constexpr Address32 spPS2ErrorManagerNoOpHandler = 0x0020DA50;
    inline constexpr Address32 spPS2ErrorManagerSupportDestructorThunk = 0x0020DC20;
    inline constexpr Address32 spPS2ErrorManagerFactory = 0x0020DBB0;
    inline constexpr Address32 spPS2ErrorManagerDirectCreate = 0x0020DB40;
    inline constexpr Address32 spSubscriptionManagerRegistrationInitializer = 0x0047F730;
    inline constexpr Address32 spSubscriptionManagerRegistrationGetter = 0x0010DE80;
    inline constexpr Address32 spSubscriptionManagerDispatch = 0x0010DE90;
    inline constexpr Address32 spSubscriptionManagerSubscribe = 0x0010E3B0;
    inline constexpr Address32 spSubscriptionManagerUnsubscribe = 0x0010E210;
    inline constexpr Address32 spSubscriptionManagerDestructor = 0x0010E4C0;
    inline constexpr Address32 spSubscriptionManagerClone = 0x0010E630;
    inline constexpr Address32 spSubscriptionManagerFactory = 0x0010E750;
    inline constexpr Address32 spSubscriptionManagerSupportDestructorThunk = 0x0010FDD0;
    inline constexpr Address32 spSubscriptionManagerGlobal = 0x0049F808;
    inline constexpr Address32 spPS2AppEntrySequence = 0x001E75B0;
    inline constexpr Address32 spPS2AppShutdown = 0x001E7650;
    inline constexpr Address32 spPS2AppRun = 0x001E7660;
    inline constexpr Address32 spPS2AppInitialize = 0x001E76B0;
    inline constexpr Address32 spPS2AppDestructor = 0x001E76C0;
    inline constexpr Address32 spPS2AppConstructor = 0x001E7730;
    inline constexpr Address32 spPS2AppNullClone = 0x001E7770;
    inline constexpr Address32 spPS2AppSupportDestructorThunk = 0x001E7780;
    inline constexpr Address32 spStreamRegistrationGetter = 0x001148B0;
    inline constexpr Address32 spStreamSetOwnedName = 0x001148C0;
    inline constexpr Address32 spStreamWriteString = 0x00114AF0;
    inline constexpr Address32 spStreamReadString = 0x00114D80;
    inline constexpr Address32 spStreamDefaultGetBuffer = 0x00114E50;
    inline constexpr Address32 spStreamCopyWholeStream = 0x00114E60;
    inline constexpr Address32 spStreamDestructor = 0x00114ED0;
    inline constexpr Address32 spStreamConstructor = 0x00114F50;
    inline constexpr Address32 spStreamNullClone = 0x00114F90;
    inline constexpr Address32 spMemoryStreamFactory = 0x00112D40;
    inline constexpr Address32 spMemoryStreamRegistrationGetter = 0x00112250;
    inline constexpr Address32 spMemoryStreamResizeAndSetSize = 0x00112260;
    inline constexpr Address32 spMemoryStreamGetCurrentPosition = 0x001122C0;
    inline constexpr Address32 spMemoryStreamGetSize = 0x001123A0;
    inline constexpr Address32 spMemoryStreamWriteFromStream = 0x00112470;
    inline constexpr Address32 spMemoryStreamWriteData = 0x00112500;
    inline constexpr Address32 spMemoryStreamPrepareWrite = 0x00112580;
    inline constexpr Address32 spMemoryStreamReadData = 0x001127B0;
    inline constexpr Address32 spMemoryStreamSeek = 0x00112940;
    inline constexpr Address32 spMemoryStreamClose = 0x00112B00;
    inline constexpr Address32 spMemoryStreamOpen = 0x00112B50;
    inline constexpr Address32 spMemoryStreamOpenWithMode = 0x00112BA0;
    inline constexpr Address32 spMemoryStreamGetBuffer = 0x00112BC0;
    inline constexpr Address32 spMemoryStreamDeletingDestructor = 0x00112BD0;
    inline constexpr Address32 spMemoryStreamClone = 0x00112C50;
    inline constexpr Address32 spFileStreamRegistrationGetter = 0x00112170;
    inline constexpr Address32 spFileStreamOpenRead = 0x00112180;
    inline constexpr Address32 spFileStreamDeletingDestructor = 0x001121A0;
    inline constexpr Address32 spFileStreamConstructor = 0x00112200;
    inline constexpr Address32 spFileStreamNullClone = 0x00112240;
    inline constexpr std::uint32_t spFileStreamFirstPureSlot = 0x28;
    inline constexpr std::uint32_t spFileStreamLastPureSlot = 0x44;
    inline constexpr Address32 spPS2FileStreamFactory = 0x001DC680;
    inline constexpr Address32 spAsyncFileStreamManagerConstructor = 0x001120A0;
    inline constexpr Address32 spAsyncFileStreamManagerDeletingDestructor = 0x00112010;
    inline constexpr Address32 spAsyncFileStreamManagerNullClone = 0x00112150;
    inline constexpr Address32 spAsyncFileStreamManagerRegistrationGetter = 0x00111FF0;
    inline constexpr Address32 spAsyncFileStreamManagerSupportDestructorThunk = 0x00112160;
    inline constexpr Address32 spAsyncFileStreamManagerNoOpUpdate = 0x00112000;
    inline constexpr Address32 spAsyncFileStreamManagerGlobal = 0x0049F828;
    inline constexpr std::uint32_t spAsyncFileStreamManagerRequestSlot = 0x30;
    inline constexpr std::uint32_t spAsyncFileStreamManagerUpdateSlot = 0x34;
    inline constexpr Address32 spPS2AsyncFileStreamManagerUpdate = 0x001DA390;
    inline constexpr Address32 spPS2AsyncFileStreamManagerRequest = 0x001DA4B0;
    inline constexpr Address32 spPS2AsyncFileStreamManagerDeletingDestructor = 0x001DA680;
    inline constexpr Address32 spPS2AsyncFileStreamManagerClone = 0x001DA6F0;
    inline constexpr Address32 spPS2AsyncFileStreamManagerCreate = 0x001DA800;
    inline constexpr Address32 spPS2AsyncFileStreamManagerFactory = 0x001DA890;
    inline constexpr Address32 spPS2AsyncFileStreamManagerSupportDestructorThunk = 0x001DA920;
    inline constexpr Address32 spPS2FileStreamRegistrationGetter = 0x001DA930;
    inline constexpr Address32 spPS2FileStreamGetCurrentPosition = 0x001DAAC0;
    inline constexpr Address32 spPS2FileStreamGetSize = 0x001DAAF0;
    inline constexpr Address32 spPS2FileStreamWriteFromStream = 0x001DAB30;
    inline constexpr Address32 spPS2FileStreamWriteData = 0x001DABC0;
    inline constexpr Address32 spPS2FileStreamReadAsyncManagerPath = 0x001DAED0;
    inline constexpr Address32 spPS2FileStreamReadData = 0x001DB170;
    inline constexpr Address32 spPS2FileStreamSeekAsyncManagerPath = 0x001DB350;
    inline constexpr Address32 spPS2FileStreamSeek = 0x001DB7D0;
    inline constexpr Address32 spPS2FileStreamClose = 0x001DBB00;
    inline constexpr Address32 spPS2FileStreamOpenAsyncManagerPath = 0x001DBC10;
    inline constexpr Address32 spPS2FileStreamOpen = 0x001DC020;
    inline constexpr Address32 spPS2FileStreamOpenRead = 0x001DC440;
    inline constexpr Address32 spPS2FileStreamDeletingDestructor = 0x001DC460;
    inline constexpr Address32 spPS2FileStreamConstructor = 0x001DC4E0;
    inline constexpr Address32 spPS2FileStreamClone = 0x001DC580;
    inline constexpr Address32 spPS2FileStreamAllocate = 0x001DC640;
    inline constexpr Address32 spPCKManagerRegistrationGetter = 0x00112ED0;
    inline constexpr Address32 spPCKManagerRecordOpenedName = 0x00112EE0;
    inline constexpr Address32 spPCKManagerIsTrackingActive = 0x00112FC0;
    inline constexpr Address32 spPCKManagerResolve = 0x00112FF0;
    inline constexpr Address32 spPCKManagerResolveInPackage = 0x00113120;
    inline constexpr Address32 spPCKManagerRemovePackage = 0x00113500;
    inline constexpr Address32 spPCKManagerAddPackage = 0x00113650;
    inline constexpr Address32 spPCKManagerDeletingDestructor = 0x00113A50;
    inline constexpr Address32 spPCKManagerConstructor = 0x00113B90;
    inline constexpr Address32 spPCKManagerClone = 0x00113D90;
    inline constexpr Address32 spPCKManagerFactory = 0x00113E50;
    inline constexpr Address32 spPCKManagerGlobal = 0x0049F844;
    inline constexpr Address32 spPS2HelperRegistrationGetter = 0x001E91A0;
    inline constexpr Address32 spPS2HelperResolvePath = 0x001E91B0;
    inline constexpr Address32 spPS2HelperSetMode = 0x001E93A0;
    inline constexpr Address32 spPS2HelperPlatformReset = 0x001E93B0;
    inline constexpr Address32 spPS2HelperDeletingDestructor = 0x001E95B0;
    inline constexpr Address32 spPS2HelperSupportDeletingDestructor = 0x001E9640;
    inline constexpr Address32 spPS2HelperClone = 0x001E9690;
    inline constexpr Address32 spPS2HelperFactory = 0x001E97A0;
    inline constexpr Address32 spPS2HelperSupportDestructorThunk = 0x001E9840;
    inline constexpr Address32 spPS2HelperGlobal = 0x0049F840;
    inline constexpr Address32 spPS2IOPModuleManagerRegistrationGetter = 0x001E9850;
    inline constexpr Address32 spPS2IOPModuleManagerSetRoot = 0x001E9860;
    inline constexpr Address32 spPS2IOPModuleManagerLoad = 0x001E98C0;
    inline constexpr Address32 spPS2IOPModuleManagerSetField18 = 0x001E9B20;
    inline constexpr Address32 spPS2IOPModuleManagerDeletingDestructor = 0x001E9B30;
    inline constexpr Address32 spPS2IOPModuleManagerSupportDeletingDestructor =
        0x001E9C80;
    inline constexpr Address32 spPS2IOPModuleManagerClone = 0x001E9CD0;
    inline constexpr Address32 spPS2IOPModuleManagerContainerConstructor = 0x001E9E20;
    inline constexpr Address32 spPS2IOPModuleManagerFactory = 0x001E9E50;
    inline constexpr Address32 spPS2IOPModuleManagerSupportDestructorThunk = 0x001E9F20;
    inline constexpr Address32 spPS2IOPModuleManagerGlobal = 0x0049FBF8;
    inline constexpr std::uint32_t spStreamFirstPureSlot = 0x24;
    inline constexpr std::uint32_t spStreamLastPureSlot = 0x44;
    inline constexpr std::uint32_t spStreamReadDataSlot = 0x38;
    inline constexpr std::uint32_t spStreamWriteDataSlot = 0x3C;
    inline constexpr std::uint32_t spStreamWriteFromStreamSlot = 0x40;
    inline constexpr std::uint32_t spStreamGetSizeSlot = 0x44;
    inline constexpr std::uint32_t spStreamDefaultBufferSlot = 0x48;
    inline constexpr Address32 spCloneManagerFactory = 0x001053A0;
    inline constexpr Address32 spPropertyRecordConstructor = 0x00110DB0;
    inline constexpr Address32 spPropertyGroupFindByIndex = 0x00110E50;
    inline constexpr Address32 spPropertyGroupFindByName = 0x00110E70;
    inline constexpr Address32 spPropertyGroupAppend = 0x00110FE0;
    inline constexpr Address32 spPropertyGroupConstructor = 0x00111000;
    inline constexpr Address32 spPropertySystemInitialize = 0x00111020;
    inline constexpr Address32 spDerivedPropertyRecordConstructor = 0x001112F0;
    inline constexpr Address32 spPropertySystemFactory = 0x00111560;
    inline constexpr Address32 spRTTIManagerFactory = 0x00111AF0;
    inline constexpr Address32 spPropertyTextParser = spBaseObjectTextPropertyParser;
    inline constexpr Address32 ps2InvokeMemberFunctionObjectA0 = 0x003FE4C0;
    inline constexpr Address32 ps2InvokeMemberFunctionObjectA1 = 0x003FE500;
    inline constexpr Address32 ps2MemberFunctionDescriptorHasTarget = 0x003FE540;
}
