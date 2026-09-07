#pragma once
// Original abstract RTTI39C85969, inferred header path, PC dependency prefix.
#include "spEvaluator.h"
#include <cstdint>
namespace sparkplug::reconstruction
{
    class spColorEval : public spEvaluator
    {
    public:
        static constexpr spClassID ClassID=0x39C85969;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] virtual bool EvaluateColorForAnalysis(float delta,std::uint32_t& argb)=0;
    protected:
        spColorEval() noexcept=default;
    };
}
