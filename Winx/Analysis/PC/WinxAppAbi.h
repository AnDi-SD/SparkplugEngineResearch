#pragma once

#include <cstddef>
#include <cstdint>

#include "Analysis/PC/SparkplugAbi.h"

namespace winx::evidence::pc
{
    using Address32 = std::uint32_t;

    // The protected PC factory does not expose its allocation instruction.
    // Destructor and every override touch no derived storage, so this is the
    // exact observed prefix, not a direct sizeof claim.
    struct wxEngineCoreObservedPrefixLayout final
    {
        sparkplug::evidence::pc::spEngineCoreLayout base;
    };

    static_assert(sizeof(wxEngineCoreObservedPrefixLayout) == 0x158);

    inline constexpr std::uint32_t wxEngineCoreClassID = 0x34B85918;
    inline constexpr Address32 wxEngineCoreRegistration = 0x007545A8;
    inline constexpr Address32 wxEngineCoreRegistrationInitializer = 0x006D0DF0;
    inline constexpr Address32 wxEngineCoreFactoryEntry = 0x004073A0;
    inline constexpr Address32 wxEngineCoreRegistrationGetter = 0x005739D0;
    inline constexpr Address32 wxEngineCoreDestructor = 0x005739E0;
    inline constexpr Address32 wxEngineCoreSupportDestructorThunk = 0x00573A20;
    inline constexpr Address32 wxEngineCoreDeletingDestructor = 0x00573A30;
    inline constexpr Address32 wxEngineCoreClone = 0x0040D300;
    inline constexpr Address32 wxEngineCoreVTable = 0x007005F8;
    inline constexpr Address32 wxEngineCoreSupportVTable = 0x007005F4;
    inline constexpr Address32 wxEngineCoreOnlyOverride = 0x00573A50;
    inline constexpr std::uint32_t wxEngineCoreOverrideOrdinal = 10;

    // Exact factory allocation.  Offsets are represented only where a direct
    // wxPCApp method uses them; the base/derived boundary is not guessed.
    struct wxPCAppAllocationLayout final
    {
        std::uint8_t opaque00[0x84];
        std::int32_t field84;
        std::int32_t field88;
        std::int32_t field8C;
        std::int32_t field90;
        std::uint8_t field94;
        std::uint8_t field95;
        std::uint8_t shutdownFlag;
        std::uint8_t opaque97[0xC9];
        Address32 field160;
        Address32 field164;
        Address32 field168;
        std::uint8_t opaque16C[0x160];
    };

    static_assert(sizeof(wxPCAppAllocationLayout) == 0x2CC);
    static_assert(offsetof(wxPCAppAllocationLayout, field84) == 0x84);
    static_assert(offsetof(wxPCAppAllocationLayout, shutdownFlag) == 0x96);
    static_assert(offsetof(wxPCAppAllocationLayout, field160) == 0x160);

    inline constexpr std::uint32_t wxPCAppClassID = 0x707D09F3;
    inline constexpr Address32 wxPCAppRegistration = 0x007552A8;
    inline constexpr Address32 wxPCAppRegistrationInitializer = 0x006D1450;
    inline constexpr Address32 wxPCAppVTable = 0x006DADD0;
    inline constexpr Address32 wxPCAppSupportVTable = 0x006DADCC;
    inline constexpr Address32 wxPCAppRegistrationGetter = 0x0040DE10;
    inline constexpr Address32 wxPCAppDestructor = 0x0040DE20;
    inline constexpr Address32 wxPCAppSupportDestructorThunk = 0x0040E070;
    inline constexpr Address32 wxPCAppFactory = 0x0040E080;
    inline constexpr Address32 wxPCAppClone = 0x0040E110;
    inline constexpr Address32 wxPCAppDeletingDestructor = 0x0040E160;
    inline constexpr Address32 wxPCAppShutdown = 0x0040E180;
    inline constexpr Address32 wxPCAppConfigureWindow = 0x0040E1D0;
    inline constexpr Address32 wxPCAppWindowTitle = 0x0040E250;
    inline constexpr Address32 wxPCAppInitialize = 0x0040E2F0;
    inline constexpr Address32 wxPCAppUpdate = 0x0040E610;
    inline constexpr Address32 wxPCAppProcessWindowMessage = 0x0040E760;
    inline constexpr Address32 wxPCAppNotificationHandler = 0x0040E8C0;
}
