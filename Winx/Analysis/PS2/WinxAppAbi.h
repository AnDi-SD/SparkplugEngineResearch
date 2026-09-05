#pragma once

#include "Analysis/PS2/SparkBaseAbi.h"

#include <cstddef>
#include <cstdint>

#include "Analysis/PS2/SparkplugAbi.h"

namespace winx::evidence::ps2
{
    using Address32 = std::uint32_t;

    struct wxEngineCoreLayout final
    {
        sparkplug::evidence::ps2::spEngineCoreLayout base;
    };

    static_assert(sizeof(wxEngineCoreLayout) == 0x150);

    inline constexpr std::uint32_t wxEngineCoreClassID = 0x34B85918;
    inline constexpr Address32 wxEngineCoreRegistration = 0x00472778;
    inline constexpr Address32 wxEngineCoreRegistrationInitializer = 0x0048B7F0;
    inline constexpr Address32 wxEngineCoreFactory = 0x003E7D60;
    inline constexpr Address32 wxEngineCoreConstructor = 0x00285480;
    inline constexpr Address32 wxEngineCoreRegistrationGetter = 0x003F88B0;
    inline constexpr Address32 wxEngineCoreDestructor = 0x00285410;
    inline constexpr Address32 wxEngineCoreSupportDestructorThunk = 0x003F9B10;
    inline constexpr Address32 wxEngineCoreClone = 0x003E7CA0;
    inline constexpr Address32 wxEngineCoreVTable = 0x00495AF0;
    inline constexpr Address32 wxEngineCoreSupportVTable = 0x00495B14;
    inline constexpr Address32 wxEngineCoreOnlyOverride = 0x00285200;
    inline constexpr std::uint32_t wxEngineCoreOverrideOrdinal = 10;

    struct wxPS2AppLayout final
    {
        sparkplug::evidence::ps2::spPS2AppLayout base; // 0x00
        Address32 field20;             // 0x20: global service +0x50
        Address32 field24;             // 0x24: global service +0x50
        Address32 field28;             // 0x28: global service +0x50
        Address32 field2C;             // 0x2c: global service +0x50
        std::uint8_t opaque30[0x18];   // 0x30: no direct wxPS2App accesses found
    };

    static_assert(sizeof(wxPS2AppLayout) == 0x48);
    static_assert(offsetof(wxPS2AppLayout, field20) == 0x20);
    static_assert(offsetof(wxPS2AppLayout, field2C) == 0x2C);

    inline constexpr std::uint32_t wxPS2AppClassID = 0x36973698;
    inline constexpr Address32 wxPS2AppRegistration = 0x004C4DF0;
    inline constexpr Address32 wxPS2AppRegistrationInitializer = 0x00487B10;
    inline constexpr Address32 wxPS2AppVTable = 0x00495130;
    inline constexpr Address32 wxPS2AppSupportVTable = 0x00495154;
    inline constexpr Address32 wxPS2AppRegistrationGetter = 0x003E4FE0;
    inline constexpr Address32 wxPS2AppNotificationHandler = 0x003E4FF0;
    inline constexpr Address32 wxPS2AppCreateForEntry = 0x003E5040;
    inline constexpr Address32 wxPS2AppWindowTitle = 0x003E50A0;
    inline constexpr Address32 wxPS2AppUpdate = 0x003E50B0;
    inline constexpr Address32 wxPS2AppShutdown = 0x003E5310;
    inline constexpr Address32 wxPS2AppInitialize = 0x003E53D0;
    inline constexpr Address32 wxPS2AppDeletingDestructor = 0x003E5AB0;
    inline constexpr Address32 wxPS2AppClone = 0x003E5B20;
    inline constexpr Address32 wxPS2AppFactory = 0x003E5C00;
    inline constexpr Address32 wxPS2AppSupportDestructorThunk = 0x003E5C70;
}
