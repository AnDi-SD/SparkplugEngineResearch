#pragma once

#include "../PC/GeometryHelper4604F0.h"
#include "../PC/Msvcr71Qsort7107031.h"
#include "Code/Sparkplug/spRenderer.h"
#include <stdexcept>
#include <type_traits>

namespace sparkplug::host
{
// Explicit modern-host choice for both original callers of IAT006D9360.
// Callers opt into this version; original classes keep their dispatch seams.
struct LegacySortPolicy final
{
    using Geometry=evidence::pc::GeometryHelper4604F0;
    using Renderer=reconstruction::spRenderer;
    static Geometry::SortDispatchForAnalysis GeometryDispatch() noexcept
    {
        return {nullptr,[](void*,std::uint16_t* records,std::size_t count,std::size_t width,
            Geometry::ComparatorForAnalysis compare,const void* context){
            struct Input{Geometry::ComparatorForAnalysis compare;const void* context;};
            const Input input{compare,context};
            return width==sizeof(std::uint16_t)&&compare&&msvcr71_7_10_7031_4::Sort(records,count,width,
                [](const void* opaque,const void* a,const void* b){
                    const auto& value=*static_cast<const Input*>(opaque);
                    return value.compare(value.context,static_cast<const std::uint16_t*>(a),static_cast<const std::uint16_t*>(b));
                },&input);
        }};
    }
    static void Alpha(void*,Renderer::AlphaEntryForAnalysis* records,std::size_t count,
        Renderer::AlphaDispatchForAnalysis::Compare compare)
    {
        static_assert(std::is_trivially_copyable_v<Renderer::AlphaEntryForAnalysis>);
        if(!compare||!msvcr71_7_10_7031_4::Sort(records,count,sizeof(*records),
            [](const void* opaque,const void* a,const void* b){
                auto compare=*static_cast<const Renderer::AlphaDispatchForAnalysis::Compare*>(opaque);
                return compare(*static_cast<const Renderer::AlphaEntryForAnalysis*>(a),*static_cast<const Renderer::AlphaEntryForAnalysis*>(b));
            },&compare))throw std::runtime_error("Invalid versioned Alpha sort input");
    }
};
}
