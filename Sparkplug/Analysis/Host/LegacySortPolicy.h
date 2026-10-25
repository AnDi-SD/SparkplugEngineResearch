#pragma once

#include "../PC/GeometryHelper4604F0.h"
#include "../PC/Msvcr71Qsort7107031.h"
#include "Code/Sparkplug/spRenderer.h"
#include "Code/Sparkplug/spScene.h"
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
    static reconstruction::spScene::OcclusionSortForAnalysis OcclusionDispatch() noexcept
    {
        using Entry=reconstruction::spScene::OcclusionEntryForAnalysis;
        static_assert(std::is_trivially_copyable_v<Entry>);
        // Explicit host choice ONLY for >32 Scene entries. Original45E7A0 is
        // compiler introsort, not importedCRTqsort. Equal-key permutations of
        // this versioned adapter are not claimed to match that native branch.
        return {nullptr,[](void*,Entry* entries,std::size_t count){
            return msvcr71_7_10_7031_4::Sort(entries,count,sizeof(Entry),
                [](const void*,const void* a,const void* b){
                    const auto x=static_cast<const Entry*>(a)->orderingKey;
                    const auto y=static_cast<const Entry*>(b)->orderingKey;
                    return x<y?-1:x>y?1:0;
                },nullptr);
        }};
    }
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
    // Explicit host policy for native456090's general-queue caller. The native
    // routine is a different compiler sort: this adapter does NOT establish
    // its equal-key permutation or claim that456090 imported CRT qsort.
    static void General(void*,Renderer::GeneralEntryForAnalysis* records,std::size_t count,
        Renderer::GeneralDispatchForAnalysis::Less less)
    {
        static_assert(std::is_trivially_copyable_v<Renderer::GeneralEntryForAnalysis>);
        if(!less||!msvcr71_7_10_7031_4::Sort(records,count,sizeof(*records),
            [](const void* opaque,const void* a,const void* b){
                const auto less=*static_cast<const Renderer::GeneralDispatchForAnalysis::Less*>(opaque);
                const auto& first=*static_cast<const Renderer::GeneralEntryForAnalysis*>(a);
                const auto& second=*static_cast<const Renderer::GeneralEntryForAnalysis*>(b);
                return less(first,second)?-1:less(second,first)?1:0;
            },&less))throw std::runtime_error("Invalid general queue sort input");
    }
};
}
