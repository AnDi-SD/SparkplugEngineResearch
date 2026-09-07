#pragma once
// Original PC storage only. The portable reconstruction is not this ABI.
#include "spMaterialControllerAbi.h"
namespace sparkplug::evidence::pc
{
    struct spFunctionEvalObservedLayout final
    {
        std::array<std::uint8_t,0x10> evaluatorBase;
        float time,frequency,reciprocal,amplitude,xOffset,yOffset,pitch,limit;
        std::uint8_t clampEnabled;
        std::array<std::uint8_t,3> untouchedPadding;
        std::uint32_t type;
    };
    struct spTransFunctionEvalObservedLayout final
    {
        std::array<std::uint8_t,0x10> transformEvaluatorBase;
        std::array<spFunctionEvalObservedLayout,3> translation;
        std::array<spFunctionEvalObservedLayout,3> scale;
        std::array<float,3> pivot;
        std::array<float,3> axis;
        spFunctionEvalObservedLayout rotation;
    };
    struct spUVControllerFunctionLayout final
    {
        spRenderControllerLayout base;
        Address32 material;
        std::array<float,9> savedMatrix;
        spTransFunctionEvalObservedLayout transform;
    };
    static_assert(sizeof(spFunctionEvalObservedLayout)==0x38);
    static_assert(offsetof(spFunctionEvalObservedLayout,time)==0x10);
    static_assert(offsetof(spFunctionEvalObservedLayout,clampEnabled)==0x30);
    static_assert(offsetof(spFunctionEvalObservedLayout,type)==0x34);
    static_assert(sizeof(spTransFunctionEvalObservedLayout)==0x1B0);
    static_assert(offsetof(spTransFunctionEvalObservedLayout,scale)==0xB8);
    static_assert(offsetof(spTransFunctionEvalObservedLayout,pivot)==0x160);
    static_assert(offsetof(spTransFunctionEvalObservedLayout,axis)==0x16C);
    static_assert(offsetof(spTransFunctionEvalObservedLayout,rotation)==0x178);
    static_assert(sizeof(spUVControllerFunctionLayout)==0x1FC);
    static_assert(offsetof(spUVControllerFunctionLayout,transform)==0x4C);
    inline constexpr Address32 spTransFunctionEvalFactory=0x47D2E0;
    inline constexpr Address32 spTransFunctionEvalPRS=0x47CD10;
    inline constexpr Address32 spTransFunctionEvalMatrix=0x47CEE0;
}
