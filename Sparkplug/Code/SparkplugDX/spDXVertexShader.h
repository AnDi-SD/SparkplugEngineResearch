#pragma once
// Original class7A6743A9, abstract registration; path inferred.
#include "spDXShader.h"
namespace sparkplug::reconstruction
{
    class spDXVertexShader : public spDXShader
    {
    public:
        static constexpr spClassID ClassID=0x7A6743A9;
        ~spDXVertexShader() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    protected:
        spDXVertexShader() noexcept=default;
    };
}
