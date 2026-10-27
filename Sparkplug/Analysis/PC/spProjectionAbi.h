#pragma once
#include "SparkplugAbi.h"
#include <cstddef>

namespace sparkplug::evidence::pc
{
    struct spProjectionObservedLayout final
    {
        spRenderableLayout base;
        std::uint8_t flags58[3];
        std::uint8_t unassigned5B;
        std::uint32_t words5C[4];
        std::uint32_t word6C;
        float sphere70[4];
        std::uint32_t unassigned80;
        Address32 bound84; // constructor-null; borrowed RenderNode from serializer field5
        std::uint32_t unassigned88;
        Address32 opaqueListHead8C;
        std::uint32_t opaqueRecordCount90;
        Address32 pcFx94;
        std::uint32_t word98;
    };
    struct spBoxProjectionObservedLayout final
    {
        spProjectionObservedLayout base;
        float center9C[3];
        float sizeA8[3];
        std::uint32_t unassignedB4;
    };
    struct spPyramidProjectionObservedLayout final
    {
        spProjectionObservedLayout base;
        std::uint8_t flip9C;
        std::uint8_t constructorUnassigned9D[3];
        float centerA0[3];
        float sizeAC[3];
        std::uint32_t wordB8;
        std::uint8_t constructorUnassignedBC[16];
        std::uint8_t byteCC;
        std::uint8_t constructorUnassignedCD[3];
    };
    static_assert(sizeof(spProjectionObservedLayout)==0x9C);
    static_assert(offsetof(spProjectionObservedLayout,sphere70)==0x70);
    static_assert(offsetof(spProjectionObservedLayout,pcFx94)==0x94);
    static_assert(sizeof(spBoxProjectionObservedLayout)==0xB8);
    static_assert(offsetof(spBoxProjectionObservedLayout,center9C)==0x9C);
    static_assert(offsetof(spBoxProjectionObservedLayout,sizeA8)==0xA8);
    static_assert(sizeof(spPyramidProjectionObservedLayout)==0xD0);
    static_assert(offsetof(spPyramidProjectionObservedLayout,flip9C)==0x9C);
    static_assert(offsetof(spPyramidProjectionObservedLayout,centerA0)==0xA0);
    static_assert(offsetof(spPyramidProjectionObservedLayout,sizeAC)==0xAC);
    static_assert(offsetof(spPyramidProjectionObservedLayout,wordB8)==0xB8);
    static_assert(offsetof(spPyramidProjectionObservedLayout,byteCC)==0xCC);
}
