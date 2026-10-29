#pragma once
#include "wxGameFlowStateAbi.h"
namespace winx::evidence::pc
{
    struct wxCinematicGameFlowStateLayout final
    {
        wxGameFlowStateLayout base;
        std::uint8_t unwrittenPath3C[64];
        std::uint32_t field7C,field80;
        std::uint8_t field84,unwritten85[3];
        std::uint32_t field88;
        std::uint8_t flags8C[82],fieldDE,unwrittenDF,fieldE0,unwrittenE1[3];
    };
    static_assert(sizeof(wxCinematicGameFlowStateLayout)==0xE4);
    static_assert(offsetof(wxCinematicGameFlowStateLayout,field7C)==0x7C);
    static_assert(offsetof(wxCinematicGameFlowStateLayout,field84)==0x84);
    static_assert(offsetof(wxCinematicGameFlowStateLayout,flags8C)==0x8C);
    static_assert(offsetof(wxCinematicGameFlowStateLayout,fieldDE)==0xDE);
    static_assert(offsetof(wxCinematicGameFlowStateLayout,fieldE0)==0xE0);
    inline constexpr std::uint32_t wxCinematicGameFlowStateFactory=0x0059E6D0;
    inline constexpr std::uint32_t wxCinematicGameFlowStateConstructor=0x0059E2B0;
    inline constexpr std::uint32_t wxCinematicGameFlowStateVTable=0x00703CA4;
    inline constexpr std::uint32_t wxCinematicGameFlowStateRTTI=0x007662F0;
    inline constexpr std::uint32_t wxCinematicGameFlowStateNotify=0x0059E630;
    inline constexpr std::uint32_t wxCinematicGameFlowStateReset=0x0059E3E0;
    inline constexpr std::uint32_t wxCinematicGameFlowStateEntry=0x0059E410;
    inline constexpr std::uint32_t wxCinematicGameFlowStateExit=0x0059E450;
}
