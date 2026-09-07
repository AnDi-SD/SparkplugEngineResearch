#pragma once
// PC478990/478920, original class identity, analytical finite API.
#include "spColorEval.h"
#include "spFunctionEval.h"
namespace sparkplug::reconstruction
{
    class spColorFuncEval final : public spColorEval
    {
    public:
        static constexpr spClassID ClassID=0x0BC70FE7;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] spFunctionEval& GetFunctionForAnalysis() noexcept{return function_;}
        [[nodiscard]] const spFunctionEval& GetFunctionForAnalysis() const noexcept{return function_;}
        [[nodiscard]] std::uint32_t GetColor1ForAnalysis() const noexcept{return color1_;}
        [[nodiscard]] std::uint32_t GetColor2ForAnalysis() const noexcept{return color2_;}
        void SetColorsForAnalysis(std::uint32_t first,std::uint32_t second) noexcept{color1_=first;color2_=second;}
        [[nodiscard]] bool EvaluateColorForAnalysis(float delta,std::uint32_t& argb) override;
    private:
        std::uint32_t color1_=0xFF000000,color2_=0xFF000000; // confirmed PC defaults; not PS2
        spFunctionEval function_;
    };
}
