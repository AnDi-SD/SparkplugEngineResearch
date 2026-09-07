#pragma once

// Byte-exact evidence from the shipped 32-bit PC executable.  This is kept
// separate from the portable reconstruction and from the PS2 ABI table.

#include <cstddef>
#include <cstdint>

namespace sparkplug::evidence::pc
{
    using Address32 = std::uint32_t;

    struct spBaseObjectLayout final
    {
        Address32 vtableAddress;       // 0x00
        Address32 field04;             // 0x04: optional reverse-reference list
        std::uint16_t referenceCount;  // 0x08: intrusive strong-reference count
        std::uint8_t padding0A[2];     // 0x0a: alignment padding
        std::uint32_t field0C;         // 0x0c: copied state; role unknown
    };

    struct spNamedObjectLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 sharedNameEntry;     // 0x10
    };

    // PC allocation20 and lookup414420/contains4423F0 consumer layout.
    // The whole protected constructor/registration path remains unexecuted.
    struct spRTTIManagerObservedLayout final
    {
        spBaseObjectLayout base;
        std::uint32_t support10;       // 0x10: exact construction not closed
        std::uint32_t allocatorState;  // 0x14
        Address32 registrationHead;    // 0x18: allocated RB sentinel
        std::uint32_t registrationCount; // 0x1c
    };
    static_assert(sizeof(spRTTIManagerObservedLayout) == 0x20);
    static_assert(offsetof(spRTTIManagerObservedLayout, registrationHead) == 0x18);

    struct spCrossPlatformLayout final
    {
        spNamedObjectLayout base;      // 0x00; no additional storage observed
    };

    struct spAppLayout final
    {
        spCrossPlatformLayout base;    // 0x00; actual C++ construction base
        Address32 supportVTable;       // 0x14: singleton-support subobject
        std::uint8_t stateFlag;        // 0x18: constructor value zero
        std::uint8_t padding19[3];     // 0x19
        Address32 ownedText;           // 0x1c: nullable owned C string
    };

    // The protected PC factory hides the direct spError allocation.  Every
    // field through +0x24 is independently accessed, so 0x28 is the exact
    // observed prefix; PS2 supplies the direct sizeof proof.
    struct spErrorObservedPrefixLayout final
    {
        spBaseObjectLayout base;       // 0x00
        std::uint32_t errorCode;       // 0x10
        std::uint32_t severity;        // 0x14: 0 information .. 3 fatal
        Address32 sourceFile;          // 0x18: borrowed string
        std::uint32_t sourceLine;      // 0x1c
        Address32 message;             // 0x20: manager data-stack pointer
        Address32 nextError;           // 0x24: intrusive temporary chain
    };

    // spPCErrorManager allocates exactly 0x1024 and adds no storage to this
    // common prefix.  The common class itself is abstract.
    struct spErrorManagerLayout final
    {
        spBaseObjectLayout base;       // 0x0000
        Address32 supportVTable;       // 0x0010: singleton-support subobject
        std::uint8_t dataStack[0x1000];// 0x0014
        std::uint32_t dataUsed;        // 0x1014
        Address32 errorHead;           // 0x1018
        Address32 handler;             // 0x101c: lazily resolved callback
        std::uint8_t handlerResolved;  // 0x1020
        std::uint8_t padding1021[3];   // 0x1021
    };

    struct spPCErrorManagerLayout final
    {
        spErrorManagerLayout base;     // 0x0000; stateless platform leaf
    };

    struct spSubscriptionManagerLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10: singleton-support subobject
        std::uint8_t subscriptionTree[0x0C]; // 0x14: compiler container ABI
    };

    // The protected constructor entry does not expose an allocation size.
    // This is therefore the byte-exact prefix touched by spPCApp itself, not
    // a claim that sizeof(spPCApp) is exactly 0x84.
    struct spPCAppObservedPrefixLayout final
    {
        spAppLayout base;              // 0x00
        Address32 moduleHandle;        // 0x20: HINSTANCE
        Address32 mainWindow;          // 0x24: HWND
        std::uint8_t opaque28[0x28];   // 0x28: roles not closed
        Address32 windowClassName;     // 0x50
        std::uint32_t field54;         // 0x54: role unknown
        std::int32_t windowX;          // 0x58
        std::int32_t windowY;          // 0x5c
        std::int32_t windowWidth;      // 0x60: constructor value 0x400
        std::int32_t windowHeight;     // 0x64: constructor value 0x300
        std::int32_t adjustedLeft;     // 0x68
        std::int32_t adjustedTop;      // 0x6c
        std::int32_t adjustedRight;    // 0x70
        std::int32_t adjustedBottom;   // 0x74
        std::uint32_t windowStyle;     // 0x78: constructor value 0x00ca0000
        std::uint32_t field7C;         // 0x7c: constructor value zero
        std::uint32_t field80;         // 0x80: constructor value zero
    };

    struct spStreamLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        std::uint32_t logicalOrigin;   // 0x14; analytical name
        Address32 ownedStreamName;     // 0x18; nullable heap C string
    };

    struct spMemoryStreamLayout final
    {
        spStreamLayout base;           // 0x00
        std::uint32_t capacity;        // 0x1c
        std::uint32_t logicalSize;     // 0x20
        std::uint32_t growthQuantum;   // 0x24; defaults to 5000
        std::uint8_t resizeEnabled;    // 0x28
        std::uint8_t padding29[3];     // 0x29
        std::uint32_t absolutePosition;// 0x2c
        Address32 buffer;              // 0x30
        std::uint8_t ownsBuffer;       // 0x34; controls Close/destruction
        std::uint8_t padding35[3];     // 0x35
    };

    struct spFileStreamLayout final
    {
        spStreamLayout base;           // 0x00; no additional storage observed
    };

    struct spPCFileStreamLayout final
    {
        spFileStreamLayout base;       // 0x00
        Address32 fileHandle;          // 0x1c: Win32 HANDLE, zero when closed
    };

    struct spAsyncFileStreamManagerLayout final
    {
        spCrossPlatformLayout base;    // 0x00
        Address32 supportVTable;       // 0x14: polymorphic singleton support
    };

    struct spPCAsyncFileStreamManagerLayout final
    {
        spAsyncFileStreamManagerLayout base; // 0x00; stateless platform leaf
    };

    // Serialized in PCK files before the first two offsets are fixed up to
    // pointers.  The LSN/byte-offset equality is also verified across the PS2
    // package corpus.
    struct spPCKFileRecordLayout final
    {
        Address32 directory;           // 0x00: pointer after fixup
        Address32 fileName;            // 0x04: pointer after fixup
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
        std::uint32_t physicalOrigin;  // 0x14: package inside outer source
        std::uint32_t physicalSize;    // 0x18
    };

    // The protected PC factory hides its allocation instruction, so 0x3c is
    // an exact observed extent rather than a directly recovered sizeof.  Two
    // compiler-specific 16-byte vectors account for +0x14 and +0x2c.
    struct spPCKManagerObservedLayout final
    {
        spBaseObjectLayout base;       // 0x00
        Address32 supportVTable;       // 0x10
        std::uint32_t packageVectorField14; // 0x14: original role unknown
        Address32 packageBegin;        // 0x18
        Address32 packageEnd;          // 0x1c
        Address32 packageCapacityEnd;  // 0x20
        std::uint32_t field24;         // 0x24: role unknown
        std::uint8_t trackOpenedNames; // 0x28: analytical role
        std::uint8_t trackingPaused;   // 0x29: analytical role
        std::uint8_t padding2A[2];     // 0x2a
        std::uint32_t nameVectorField2C; // 0x2c: original role unknown
        Address32 nameBegin;           // 0x30
        Address32 nameEnd;             // 0x34
        Address32 nameCapacityEnd;     // 0x38
    };

    static_assert(sizeof(spBaseObjectLayout) == 0x10);
    static_assert(offsetof(spBaseObjectLayout, field04) == 0x04);
    static_assert(offsetof(spBaseObjectLayout, referenceCount) == 0x08);
    static_assert(offsetof(spBaseObjectLayout, padding0A) == 0x0A);
    static_assert(offsetof(spBaseObjectLayout, field0C) == 0x0C);
    static_assert(sizeof(spNamedObjectLayout) == 0x14);
    static_assert(sizeof(spCrossPlatformLayout) == 0x14);
    static_assert(sizeof(spAppLayout) == 0x20);
    static_assert(offsetof(spAppLayout, supportVTable) == 0x14);
    static_assert(offsetof(spAppLayout, stateFlag) == 0x18);
    static_assert(offsetof(spAppLayout, ownedText) == 0x1C);
    static_assert(sizeof(spErrorObservedPrefixLayout) == 0x28);
    static_assert(offsetof(spErrorObservedPrefixLayout, errorCode) == 0x10);
    static_assert(offsetof(spErrorObservedPrefixLayout, severity) == 0x14);
    static_assert(offsetof(spErrorObservedPrefixLayout, message) == 0x20);
    static_assert(offsetof(spErrorObservedPrefixLayout, nextError) == 0x24);
    static_assert(sizeof(spErrorManagerLayout) == 0x1024);
    static_assert(offsetof(spErrorManagerLayout, dataStack) == 0x14);
    static_assert(offsetof(spErrorManagerLayout, dataUsed) == 0x1014);
    static_assert(offsetof(spErrorManagerLayout, errorHead) == 0x1018);
    static_assert(offsetof(spErrorManagerLayout, handler) == 0x101C);
    static_assert(offsetof(spErrorManagerLayout, handlerResolved) == 0x1020);
    static_assert(sizeof(spPCErrorManagerLayout) == 0x1024);
    static_assert(sizeof(spSubscriptionManagerLayout) == 0x20);
    static_assert(offsetof(spSubscriptionManagerLayout, subscriptionTree) == 0x14);
    static_assert(sizeof(spPCAppObservedPrefixLayout) == 0x84);
    static_assert(offsetof(spPCAppObservedPrefixLayout, moduleHandle) == 0x20);
    static_assert(offsetof(spPCAppObservedPrefixLayout, mainWindow) == 0x24);
    static_assert(offsetof(spPCAppObservedPrefixLayout, windowClassName) == 0x50);
    static_assert(offsetof(spPCAppObservedPrefixLayout, windowX) == 0x58);
    static_assert(offsetof(spPCAppObservedPrefixLayout, windowWidth) == 0x60);
    static_assert(offsetof(spPCAppObservedPrefixLayout, windowStyle) == 0x78);
    static_assert(offsetof(spPCAppObservedPrefixLayout, field80) == 0x80);
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
    static_assert(sizeof(spPCFileStreamLayout) == 0x20);
    static_assert(offsetof(spPCFileStreamLayout, fileHandle) == 0x1C);
    static_assert(sizeof(spAsyncFileStreamManagerLayout) == 0x18);
    static_assert(offsetof(spAsyncFileStreamManagerLayout, supportVTable) == 0x14);
    static_assert(sizeof(spPCAsyncFileStreamManagerLayout) == 0x18);
    static_assert(sizeof(spPCKFileRecordLayout) == 0x14);
    static_assert(offsetof(spPCKFileRecordLayout, logicalSector) == 0x08);
    static_assert(offsetof(spPCKFileRecordLayout, byteOffset) == 0x0C);
    static_assert(offsetof(spPCKFileRecordLayout, byteCount) == 0x10);
    static_assert(sizeof(spPCKPackageRecordLayout) == 0x1C);
    static_assert(offsetof(spPCKPackageRecordLayout, files) == 0x0C);
    static_assert(offsetof(spPCKPackageRecordLayout, physicalOrigin) == 0x14);
    static_assert(offsetof(spPCKPackageRecordLayout, physicalSize) == 0x18);
    static_assert(sizeof(spPCKManagerObservedLayout) == 0x3C);
    static_assert(offsetof(spPCKManagerObservedLayout, supportVTable) == 0x10);
    static_assert(offsetof(spPCKManagerObservedLayout, packageBegin) == 0x18);
    static_assert(offsetof(spPCKManagerObservedLayout, trackOpenedNames) == 0x28);
    static_assert(offsetof(spPCKManagerObservedLayout, nameBegin) == 0x30);

    inline constexpr std::uint32_t spBaseObjectClassID = 0x415352A1;
    inline constexpr std::uint32_t spCrossPlatformClassID = 0x20A72504;
    inline constexpr std::uint32_t spAppClassID = 0x391B146A;
    inline constexpr std::uint32_t spPCAppClassID = 0x7635EFDE;
    inline constexpr std::uint32_t spErrorClassID = 0x789B29B9;
    inline constexpr std::uint32_t spErrorManagerClassID = 0x660E40D8;
    inline constexpr std::uint32_t spPCErrorManagerClassID = 0x12162D8E;
    inline constexpr std::uint32_t spSubscriptionManagerClassID = 0xE4567D00;
    inline constexpr std::uint32_t spStreamClassID = 0x6CC80D8A;
    inline constexpr std::uint32_t spMemoryStreamClassID = 0x57177DB5;
    inline constexpr std::uint32_t spFileStreamClassID = 0x5E0623EC;
    inline constexpr std::uint32_t spSocketStreamClassID = 0x1ED8677D;
    inline constexpr std::uint32_t spPCFileStreamClassID = 0x5EDF341C;
    inline constexpr std::uint32_t spAsyncFileStreamManagerClassID = 0x7EA51364;
    inline constexpr std::uint32_t spPCAsyncFileStreamManagerClassID = 0x15B533A4;
    inline constexpr std::uint32_t spPCKManagerClassID = 0x2B9D1649;
    inline constexpr Address32 spBaseObjectRegistration = 0x00755310;
    inline constexpr Address32 spCrossPlatformRegistration = 0x0075AC00;
    inline constexpr Address32 spAppRegistration = 0x00764E30;
    inline constexpr Address32 spPCAppRegistration = 0x00764710;
    inline constexpr Address32 spErrorRegistration = 0x0075A9A8;
    inline constexpr Address32 spErrorManagerRegistration = 0x0075A428;
    inline constexpr Address32 spPCErrorManagerRegistration = 0x00764770;
    inline constexpr Address32 spSubscriptionManagerRegistration = 0x0075A548;
    inline constexpr Address32 spStreamRegistration = 0x0075AB18;
    inline constexpr Address32 spMemoryStreamRegistration = 0x00760280;
    inline constexpr Address32 spFileStreamRegistration = 0x0084D590;
    inline constexpr Address32 spSocketStreamRegistration = 0x00762C88;
    inline constexpr Address32 spPCFileStreamRegistration = 0x0084D470;
    inline constexpr Address32 spAsyncFileStreamManagerRegistration = 0x0084D530;
    inline constexpr Address32 spPCAsyncFileStreamManagerRegistration = 0x0084D410;
    inline constexpr Address32 spPCKManagerRegistration = 0x0075FD60;
    inline constexpr Address32 spBaseObjectVTable = 0x006DAEB8;
    inline constexpr Address32 spCrossPlatformVTable = 0x006DB8FC;
    inline constexpr Address32 spAppVTable = 0x006F2F08;
    inline constexpr Address32 spAppSupportVTable = 0x006F2F04;
    inline constexpr Address32 spPCAppVTable = 0x006F2430;
    inline constexpr Address32 spPCAppSupportVTable = 0x006F2428;
    inline constexpr Address32 spErrorVTable = 0x006DB7BC;
    inline constexpr Address32 spErrorManagerVTable = 0x006DB63C;
    inline constexpr Address32 spErrorManagerSupportVTable = 0x006DB638;
    inline constexpr Address32 spPCErrorManagerVTable = 0x006F255C;
    inline constexpr Address32 spPCErrorManagerSupportVTable = 0x006F2558;
    inline constexpr Address32 spSubscriptionManagerVTable = 0x006DB788;
    inline constexpr Address32 spSubscriptionManagerSupportVTable = 0x006DB784;
    inline constexpr Address32 spStreamVTable = 0x006DB868;
    inline constexpr Address32 spMemoryStreamVTable = 0x006E7E50;
    inline constexpr Address32 spFileStreamVTable = 0x00729270;
    inline constexpr Address32 spPCFileStreamVTable = 0x007290B8;
    inline constexpr Address32 spAsyncFileStreamManagerVTable = 0x0072922C;
    inline constexpr Address32 spAsyncFileStreamManagerSupportVTable = 0x00729228;
    inline constexpr Address32 spPCAsyncFileStreamManagerVTable = 0x00729078;
    inline constexpr Address32 spPCAsyncFileStreamManagerSupportVTable = 0x00729074;
    inline constexpr Address32 spPCKManagerVTable = 0x006E71C8;
    inline constexpr Address32 spPCKManagerSupportVTable = 0x006E71C4;

    // Analytical flag names.  Both platform implementations test these exact
    // bits, but the original enum type and enumerator spellings are unknown.
    inline constexpr std::uint32_t spStreamOpenRead = 0x01;
    inline constexpr std::uint32_t spStreamOpenWrite = 0x02;
    inline constexpr std::uint32_t spStreamOpenReadWrite = 0x04;
    inline constexpr std::uint32_t spStreamOpenAppend = 0x08;

    // PC vtables do not contain the two leading PS2 ABI words.  These virtual
    // targets therefore begin at slot +0x00 rather than PS2 slot +0x08.
    inline constexpr Address32 spBaseObjectDeletingDestructor = 0x00411CC0;
    inline constexpr Address32 spBaseObjectNonDeletingDestructor = 0x004102B0;
    inline constexpr Address32 spBaseObjectEmptyNotificationHandler = 0x005B7A00;
    inline constexpr Address32 spBaseObjectCloneConstruction = 0x004A1BF0;
    inline constexpr Address32 spBaseObjectCloneCopy = 0x0040ECE0;
    inline constexpr Address32 spBaseObjectRegistrationGetter = 0x0040E930;
    inline constexpr Address32 spBaseObjectExactTypeCheck = 0x00408350;
    inline constexpr Address32 spBaseObjectIsKindOf = 0x00408370;
    inline constexpr Address32 spBaseObjectConstructor = 0x0040E910;

    // The protected PC image still exposes these exact vtable targets and the
    // registration immediate.  PS2 supplies the directly disassemblable
    // behavior counterpart used by the portable reconstruction.
    inline constexpr Address32 spCrossPlatformDeletingDestructor = 0x00417B00;
    inline constexpr Address32 spCrossPlatformRegistrationGetter = 0x00417AF0;
    inline constexpr Address32 spAppConstructorReadableTail = 0x004019B0;
    inline constexpr Address32 spAppDefaultEmptyString = 0x004CA080;
    inline constexpr Address32 spAppSupportDestructor = 0x004CA100;
    inline constexpr Address32 spAppSupportDeletingDestructor = 0x004CA120;
    inline constexpr Address32 spAppRegistrationGetter = 0x004CA180;
    inline constexpr Address32 spAppSupportDestructorThunk = 0x004CA190;
    inline constexpr Address32 spAppDestructor = 0x004CA1A0;
    inline constexpr Address32 spAppDeletingDestructor = 0x004CA220;
    inline constexpr Address32 spAppGlobal = 0x00764708;
    inline constexpr std::uint32_t spAppFirstPCPureSlot = 0x20;
    inline constexpr std::uint32_t spAppLastPCPureSlot = 0x2C;
    inline constexpr Address32 spPCAppConstructorEntry = 0x00447E50;
    inline constexpr Address32 spPCAppConstructorReadableTail = 0x00447EA0;
    inline constexpr Address32 spPCAppRegistrationGetter = 0x004C2C40;
    inline constexpr Address32 spPCAppDestructor = 0x004C2C50;
    inline constexpr Address32 spPCAppRun = 0x004C2C70;
    inline constexpr Address32 spPCAppShutdown = 0x004C2D10;
    inline constexpr Address32 spPCAppDefaultWindowProcedure = 0x004C2D20;
    inline constexpr Address32 spPCAppSupportDestructorThunk = 0x004C2D30;
    inline constexpr Address32 spPCAppDeletingDestructor = 0x004C2D40;
    inline constexpr Address32 spPCAppUpdate = 0x004C2D60;
    inline constexpr Address32 spPCAppCreateMainWindow = 0x004C3130;
    inline constexpr Address32 spPCAppInitialize = 0x004C3230;
    inline constexpr Address32 spPCAppTopLevelSequence = 0x004C3040;
    inline constexpr Address32 spErrorConstructor = 0x00416750;
    inline constexpr Address32 spErrorInitialize = 0x00416790;
    inline constexpr Address32 spErrorDestructor = 0x00416780;
    inline constexpr Address32 spErrorRegistrationGetter = 0x00416BB0;
    inline constexpr Address32 spErrorFactoryEntry = 0x00416BC0;
    inline constexpr Address32 spErrorClone = 0x00416C40;
    inline constexpr Address32 spErrorDescribe = 0x004167B0;
    inline constexpr Address32 spErrorDescribeSeverity = 0x004169A0;
    inline constexpr Address32 spErrorDescribeSource = 0x00416A80;
    inline constexpr Address32 spErrorName = 0x00416A70;
    inline constexpr Address32 spErrorManagerRegistrationInitializer = 0x006D1560;
    inline constexpr Address32 spErrorManagerConstructorEntry = 0x00413680;
    inline constexpr Address32 spErrorManagerDestructor = 0x004136F0;
    inline constexpr Address32 spErrorManagerRegistrationGetter = 0x004136E0;
    inline constexpr Address32 spErrorManagerFormatAndRoute = 0x004139C0;
    inline constexpr Address32 spErrorManagerHandle = 0x00413A90;
    inline constexpr Address32 spErrorManagerResolveHandlerPure = 0x0060DB76;
    inline constexpr Address32 spErrorManagerClear = 0x004137C0;
    inline constexpr Address32 spErrorManagerFormatChain = 0x004137F0;
    inline constexpr Address32 spErrorManagerInvokeHandler = 0x00413A40;
    inline constexpr Address32 spErrorManagerGlobal = 0x00755264;
    inline constexpr Address32 spPCErrorManagerRegistrationInitializer = 0x006D5760;
    inline constexpr Address32 spPCErrorManagerRegistrationGetter = 0x004C33A0;
    inline constexpr Address32 spPCErrorManagerSupportDestructorThunk = 0x004C33B0;
    inline constexpr Address32 spPCErrorManagerFactory = 0x004C33C0;
    inline constexpr Address32 spPCErrorManagerDeletingDestructor = 0x004C3430;
    inline constexpr Address32 spPCErrorManagerDestructor = 0x004C3450;
    inline constexpr Address32 spPCErrorManagerClone = 0x004C3460;
    inline constexpr Address32 spPCErrorManagerHandler = 0x004C34B0;
    inline constexpr Address32 spPCErrorManagerResolveHandler = 0x004C3580;
    inline constexpr Address32 spSubscriptionManagerRegistrationInitializer = 0x006D15F0;
    inline constexpr Address32 spSubscriptionManagerRegistrationGetter = 0x004164E0;
    inline constexpr Address32 spSubscriptionManagerConstructorEntry = 0x00416500;
    inline constexpr Address32 spSubscriptionManagerDestructorEntry = 0x00416450;
    inline constexpr Address32 spSubscriptionManagerDeletingDestructor = 0x00416590;
    inline constexpr Address32 spSubscriptionManagerFactory = 0x004165B0;
    inline constexpr Address32 spSubscriptionManagerClone = 0x00416610;
    inline constexpr Address32 spSubscriptionManagerDispatch = 0x00415A20;
    inline constexpr Address32 spSubscriptionManagerUnsubscribe = 0x004163A0;
    inline constexpr Address32 spSubscriptionManagerGlobal = 0x0075537C;
    inline constexpr Address32 spStreamConstructorReadableTail = 0x0040502B;
    inline constexpr Address32 spStreamNonDeletingDestructor = 0x00416D40;
    inline constexpr Address32 spStreamCopyWholeStream = 0x00416D70;
    inline constexpr Address32 spStreamReadString = 0x00416DC0;
    inline constexpr Address32 spStreamWriteString = 0x00416E80;
    inline constexpr Address32 spStreamSetOwnedName = 0x00416F90;
    inline constexpr Address32 spStreamRegistrationGetter = 0x00417060;
    inline constexpr Address32 spStreamDeletingDestructor = 0x00417070;
    inline constexpr Address32 spStreamPureCall = 0x0060DB76;
    inline constexpr Address32 spStreamDefaultGetBuffer = 0x004A1BF0;
    inline constexpr Address32 spMemoryStreamFactory = 0x00465560;
    inline constexpr Address32 spMemoryStreamRegistrationGetter = 0x00465480;
    inline constexpr Address32 spMemoryStreamOpenWithMode = 0x00465490;
    inline constexpr Address32 spMemoryStreamOpen = 0x004654A0;
    inline constexpr Address32 spMemoryStreamClose = 0x004654D0;
    inline constexpr Address32 spMemoryStreamResizeAndSetSize = 0x00465500;
    inline constexpr Address32 spMemoryStreamReleaseBuffer = 0x00465540;
    inline constexpr Address32 spMemoryStreamReset = 0x00465550;
    inline constexpr Address32 spMemoryStreamClone = 0x004655E0;
    inline constexpr Address32 spMemoryStreamNonDeletingDestructor = 0x00465630;
    inline constexpr Address32 spMemoryStreamDeletingDestructor = 0x00465730;
    inline constexpr Address32 spMemoryStreamSeek = 0x00465750;
    inline constexpr Address32 spMemoryStreamReadData = 0x00465820;
    inline constexpr Address32 spMemoryStreamPrepareWrite = 0x00465910;
    inline constexpr Address32 spMemoryStreamWriteData = 0x00465A50;
    inline constexpr Address32 spMemoryStreamWriteFromStream = 0x00465AA0;
    inline constexpr Address32 spMemoryStreamGetSize = 0x00465AE0;
    inline constexpr Address32 spMemoryStreamGetCurrentPosition = 0x00465B60;
    inline constexpr Address32 spMemoryStreamGetBuffer = 0x004CF2F0;
    inline constexpr Address32 spFileStreamConstructor = 0x006BE780;
    inline constexpr Address32 spFileStreamRegistrationGetter = 0x006BE7A0;
    inline constexpr Address32 spFileStreamNonDeletingDestructor = 0x006BE7B0;
    inline constexpr Address32 spFileStreamOpenRead = 0x006BE7C0;
    inline constexpr Address32 spFileStreamDeletingDestructor = 0x006BE7D0;
    inline constexpr std::uint32_t spFileStreamFirstPureSlot = 0x20;
    inline constexpr std::uint32_t spFileStreamLastPureSlot = 0x3C;
    inline constexpr Address32 spSocketStreamFactory = 0x00499410;
    inline constexpr Address32 spPCFileStreamRegistrationGetter = 0x006BD570;
    inline constexpr Address32 spPCFileStreamFactory = 0x006BD580;
    inline constexpr Address32 spPCFileStreamClone = 0x006BD5F0;
    inline constexpr Address32 spPCFileStreamOpen = 0x006BD710;
    inline constexpr Address32 spPCFileStreamClose = 0x006BD980;
    inline constexpr Address32 spPCFileStreamSeek = 0x006BDA00;
    inline constexpr Address32 spPCFileStreamReadData = 0x006BDB60;
    inline constexpr Address32 spPCFileStreamWriteData = 0x006BDCF0;
    inline constexpr Address32 spPCFileStreamWriteFromStream = 0x006BDE30;
    inline constexpr Address32 spPCFileStreamGetSize = 0x006BDFC0;
    inline constexpr Address32 spPCFileStreamGetCurrentPosition = 0x006BE100;
    inline constexpr Address32 spPCFileStreamNonDeletingDestructor = 0x006BE240;
    inline constexpr Address32 spPCFileStreamDeletingDestructor = 0x006BE2A0;
    inline constexpr Address32 spAsyncFileStreamManagerConstructor = 0x006BE6E0;
    inline constexpr Address32 spAsyncFileStreamManagerDestructor = 0x006BE730;
    inline constexpr Address32 spAsyncFileStreamManagerDeletingDestructor = 0x006BE760;
    inline constexpr Address32 spAsyncFileStreamManagerRegistrationGetter = 0x006BE710;
    inline constexpr Address32 spAsyncFileStreamManagerSupportDestructorThunk = 0x006BE720;
    inline constexpr Address32 spAsyncFileStreamManagerPureRequest = 0x0060DB76;
    inline constexpr Address32 spAsyncFileStreamManagerNoOpUpdate = 0x0048EAA0;
    inline constexpr Address32 spAsyncFileStreamManagerGlobal = 0x0075DB94;
    inline constexpr std::uint32_t spAsyncFileStreamManagerRequestSlot = 0x1C;
    inline constexpr std::uint32_t spAsyncFileStreamManagerUpdateSlot = 0x20;
    inline constexpr Address32 spPCAsyncFileStreamManagerFactory = 0x006BD490;
    inline constexpr Address32 spPCAsyncFileStreamManagerRegistrationGetter = 0x006BD3D0;
    inline constexpr Address32 spPCAsyncFileStreamManagerRequest = 0x006BD400;
    inline constexpr Address32 spPCAsyncFileStreamManagerClone = 0x006BD500;
    inline constexpr Address32 spPCAsyncFileStreamManagerDestructor = 0x006BD3E0;
    inline constexpr Address32 spPCAsyncFileStreamManagerDeletingDestructor = 0x006BD550;
    inline constexpr Address32 spPCAsyncFileStreamManagerSupportDestructorThunk = 0x006BD480;
    inline constexpr Address32 spPCKManagerRegistrationGetter = 0x0045C430;
    inline constexpr Address32 spPCKManagerSupportDestructorThunk = 0x0045C440;
    inline constexpr Address32 spPCKManagerFactory = 0x0045C4E0;
    inline constexpr Address32 spPCKManagerClone = 0x0045C540;
    inline constexpr Address32 spPCKManagerAddPackage = 0x0045C590;
    inline constexpr Address32 spPCKManagerResolve = 0x0045B8A0;
    inline constexpr Address32 spPCKManagerResolveInPackage = 0x0045B1D0;
    inline constexpr Address32 spPCKManagerIsTrackingActive = 0x0045B660;
    inline constexpr Address32 spPCKManagerGlobal = 0x0075DB9C;
    inline constexpr std::uint32_t spStreamFirstPureSlot = 0x1C;
    inline constexpr std::uint32_t spStreamLastPureSlot = 0x3C;
    inline constexpr std::uint32_t spStreamReadDataSlot = 0x30;
    inline constexpr std::uint32_t spStreamWriteFromStreamSlot = 0x34;
    inline constexpr std::uint32_t spStreamWriteDataSlot = 0x38;
    inline constexpr std::uint32_t spStreamGetSizeSlot = 0x3C;
    inline constexpr std::uint32_t spStreamDefaultBufferSlot = 0x40;
}
