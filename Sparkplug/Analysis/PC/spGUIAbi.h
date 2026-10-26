#pragma once
#include "spEntityAbi.h"

namespace sparkplug::evidence::pc
{
    struct spGUIObjectLayout final
    {
        spEntityLayout entity;
        std::uint8_t enabled; // +28, raw byte default1
        std::uint8_t padding29[3]; // not written
        std::uint32_t allocator2C; // not written
        std::uint32_t savedBegin,savedEnd,savedLimit; // +30/+34/+38
    };
    struct spWidgetLayout final
    {
        spGUIObjectLayout gui;
        std::uint8_t visible,highlight; // +3C/+3D, 1/0
        std::uint8_t padding3E[2];
        std::uint32_t selected; // +40, borrowed
        std::uint32_t nodes[3]; // +44 normal,+48 disabled,+4C highlighted
        std::uint32_t index; // +50, initially0; pressed selection preserves it
    };
    struct spButtonLayout final
    {
        spWidgetLayout widget;
        std::uint32_t ownedString; // +54, ctor alloc2 bytes and writes byte0 only
        std::uint8_t pressed; // +58
        std::uint8_t padding59[3];
        std::uint32_t pressedNode; // +5C, borrowed
    };
    static_assert(sizeof(spGUIObjectLayout)==0x3c);
    static_assert(sizeof(spWidgetLayout)==0x54);
    static_assert(sizeof(spButtonLayout)==0x60);
    static_assert(offsetof(spGUIObjectLayout,savedBegin)==0x30);
    static_assert(offsetof(spWidgetLayout,index)==0x50);
    static_assert(offsetof(spButtonLayout,pressedNode)==0x5c);
    inline constexpr std::uint32_t GUIObjectFactory=0x004296b0;
    inline constexpr std::uint32_t GUIObjectTable=0x006dcc70;
    inline constexpr std::uint32_t GUIObjectRecord=0x0075e2d8;
    inline constexpr std::uint32_t GUIObjectSetEnabled=0x004290b0;
    inline constexpr std::uint32_t GUIObjectSetNode=0x004294b0;
    inline constexpr std::uint32_t GUIObjectNotify=0x004293b0;
    inline constexpr std::uint32_t WidgetFactory=0x0041a450;
    inline constexpr std::uint32_t WidgetTable=0x006dea6c;
    inline constexpr std::uint32_t WidgetRecord=0x0075d608;
    inline constexpr std::uint32_t WidgetSelect=0x004358c0;
    inline constexpr std::uint32_t WidgetSetEnabled=0x004358a0;
    inline constexpr std::uint32_t WidgetSetVisible=0x004358b0;
    inline constexpr std::uint32_t ButtonFactory=0x0041a4b0;
    inline constexpr std::uint32_t ButtonTable=0x006deac0;
    inline constexpr std::uint32_t ButtonRecord=0x0075d668;
    inline constexpr std::uint32_t ButtonSelect=0x00435cb0;
}
