#pragma once
// Confirmed PC storage, never the portable host C++ ABI. Kept in a small
// dependency file so controller refinements need not rebuild unrelated tests.
#include "SparkplugAbi.h"
#include <array>
namespace sparkplug::evidence::pc
{
    struct spRenderControllerLayout final
    {
        spControllerLayout base;
        float appliedTime;     //1C
        float accumulatedTime; //20
    };
    struct spTextureTrackLayout final
    {
        spTrackLayout base;
        Address32 times;       //14
        Address32 textures;    //18, intrusive pointer array with count cookie
        std::uint32_t count;   //1C
    };
    struct spAnimTexControllerLayout final
    {
        spRenderControllerLayout base;
        Address32 material;    //24, borrowed
        spTextureTrackLayout track; //28..47
        float playbackTime;    //48
    };
    struct spUVControllerObservedLayout final
    {
        spRenderControllerLayout base;
        Address32 material;    //24, borrowed
        std::array<float,9> savedMatrix; //28..4B
        std::array<std::uint8_t,0x1B0> transformEvaluator; //4C..1FB, seven functions
    };
    static_assert(sizeof(spRenderControllerLayout)==0x24);
    static_assert(sizeof(spTextureTrackLayout)==0x20);
    static_assert(sizeof(spAnimTexControllerLayout)==0x4C);
    static_assert(offsetof(spAnimTexControllerLayout,track)==0x28);
    static_assert(offsetof(spAnimTexControllerLayout,playbackTime)==0x48);
    static_assert(sizeof(spUVControllerObservedLayout)==0x1FC);
    static_assert(offsetof(spUVControllerObservedLayout,transformEvaluator)==0x4C);
    inline constexpr Address32 spRenderControllerRegistration=0x75DEB0;
    inline constexpr Address32 spRenderControllerAccumulateTime=0x423190;
    inline constexpr Address32 spRenderControllerConsumeTime=0x4231A0;
    inline constexpr Address32 spRenderControllerCopy=0x4231D0;
    inline constexpr Address32 spAnimTexControllerFactory=0x41A030;
    inline constexpr Address32 spAnimTexControllerUpdate=0x42FC60;
    inline constexpr Address32 spTextureTrackSelect=0x478C60;
    inline constexpr Address32 spUVControllerFactory=0x41A210;
    inline constexpr Address32 spUVControllerBindMaterial=0x4346C0;
    inline constexpr Address32 spUVControllerUpdate=0x434820;
}
