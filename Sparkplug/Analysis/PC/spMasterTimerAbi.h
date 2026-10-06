#pragma once
#include "spTimerAbi.h"

namespace sparkplug::analysis::pc
{
    struct spMasterTimerState32
    {
        spTimerState32 timer;
        std::uint32_t singletonInterfaceVtable;
    };
    static_assert(sizeof(spMasterTimerState32) == 0x28);
    static_assert(offsetof(spMasterTimerState32, singletonInterfaceVtable) == 0x24);
    inline constexpr std::uint32_t MasterTimerFactory = 0x004505C0;
    inline constexpr std::uint32_t MasterTimerClone = 0x00450640;
    inline constexpr std::uint32_t MasterTimerDeletingDestructor = 0x00450690;
    inline constexpr std::uint32_t MasterTimerSecondaryDeleteThunk = 0x00450580;
    inline constexpr std::uint32_t MasterTimerVtable = 0x006E693C;
    inline constexpr std::uint32_t MasterTimerSecondaryVtable = 0x006E6938;
    inline constexpr std::uint32_t MasterTimerInstance = 0x0075DB74;
}
