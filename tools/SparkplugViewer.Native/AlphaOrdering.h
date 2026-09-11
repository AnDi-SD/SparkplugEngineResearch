#pragma once
// HOST frame-batch transport. The original sphere/priority fields are supplied
// by loaded classes; metric, comparator and chosen CRT are shared code. This
// does not impersonate the original 2048-entry queue or visibility scheduler.
#include "ViewerBridge.h"
#include "Analysis/PC/spRendererQueueMath.h"
#include "Analysis/PC/Msvcr71Qsort7107031.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace spvhost {
inline std::vector<SpvAlphaOutput> OrderAlpha(const SpvAlphaInput* input,std::uint32_t count,
    const float* view,std::uint32_t depthOnly,std::uint32_t priorityBase)
{
    namespace math=sparkplug::evidence::pc::renderer_queue_math;
    const auto need=[](bool ok,const char* error){if(!ok)throw std::runtime_error(error);};
    need(count<=65536&&(!count||input)&&view&&depthOnly<=1,"Invalid host alpha batch");
    for(unsigned i=0;i<16;++i)need(std::isfinite(view[i]),"Non-finite alpha view");
    math::Matrix4 camera;std::copy_n(view,16,camera.begin());
    struct Record {math::AlphaKey key;std::uint32_t token;};
    std::vector<Record> records;records.reserve(count);
    for(unsigned i=0;i<count;++i) {
        const auto& item=input[i];need(item.particle<=1,"Invalid exact-particle flag");
        for(auto value:item.center)need(std::isfinite(value),"Non-finite alpha sphere center");
        for(auto value:item.world)need(std::isfinite(value),"Non-finite alpha support matrix");
        math::Matrix4 world;std::copy_n(item.world,16,world.begin());
        records.push_back({math::BuildAlphaKey({item.center[0],item.center[1],item.center[2]},world,camera,
            depthOnly!=0,item.priority,priorityBase,item.particle!=0),item.token});
    }
    need(sparkplug::host::msvcr71_7_10_7031_4::Sort(records.data(),records.size(),sizeof(Record),
        [](const void*,const void* a,const void* b){return math::CompareAlpha(static_cast<const Record*>(a)->key,
            static_cast<const Record*>(b)->key);},nullptr),"Versioned alpha sort refused input");
    std::vector<SpvAlphaOutput> output;output.reserve(count);
    for(const auto& item:records)output.push_back({item.token,item.key.priority,item.key.exactParticleSystem?1u:0u,item.key.distanceSquared});
    return output;
}
static_assert(sizeof(SpvAlphaInfo)==28&&sizeof(SpvAlphaInput)==88&&sizeof(SpvAlphaOutput)==16);
}
