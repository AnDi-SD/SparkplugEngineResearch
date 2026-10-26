#pragma once
#include "spEntityAbi.h"

namespace sparkplug::evidence::ps2
{
    struct spGUIObjectLayout final
    {
        spEntityLayout entity;
        std::uint8_t enabled,padding29[3];
        std::uint32_t savedCapacity,savedCount,savedData; // +2C/+30/+34
    };
    struct spWidgetLayout final
    {
        spGUIObjectLayout gui;
        std::uint8_t visible,highlight,padding3A[2];
        std::uint32_t selected,nodes[3],index; // +3C/+40..48/+4C
    };
    struct spButtonLayout final
    {
        spWidgetLayout widget;
        std::uint32_t ownedString;
        std::uint8_t pressed,padding55[3];
        std::uint32_t pressedNode;
    };
    static_assert(sizeof(spGUIObjectLayout)==0x38);
    static_assert(sizeof(spWidgetLayout)==0x50);
    static_assert(sizeof(spButtonLayout)==0x5c);
    static_assert(offsetof(spGUIObjectLayout,savedData)==0x34);
    static_assert(offsetof(spWidgetLayout,index)==0x4c);
    static_assert(offsetof(spButtonLayout,pressedNode)==0x58);
    inline constexpr std::uint32_t GUIObjectConstructor=0x00165860;
    inline constexpr std::uint32_t GUIObjectTable=0x0048e560;
    inline constexpr std::uint32_t GUIObjectSetEnabled=0x00165240;
    inline constexpr std::uint32_t GUIObjectSetNode=0x00165330;
    inline constexpr std::uint32_t GUIObjectCopy=0x00165620;
    inline constexpr std::uint32_t WidgetConstructor=0x0016c1f0;
    inline constexpr std::uint32_t WidgetTable=0x0048d810;
    inline constexpr std::uint32_t WidgetSelect=0x0016bbc0;
    inline constexpr std::uint32_t WidgetCopy=0x0016c0f0;
    inline constexpr std::uint32_t ButtonConstructor=0x00169210;
    inline constexpr std::uint32_t ButtonTable=0x0048d7d0;
    inline constexpr std::uint32_t ButtonSelect=0x00168d40;
    inline constexpr std::uint32_t ButtonCopy=0x001690b0;
}
