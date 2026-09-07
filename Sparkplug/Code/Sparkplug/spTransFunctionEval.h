#pragma once
// Original registered name. Header/member/API placement is analytical.
#include "spTransformEval.h"
#include "spFunctionEval.h"
namespace sparkplug::reconstruction
{
    class spTransFunctionEval final : public spTransformEval
    {
    public:
        static constexpr spClassID ClassID=0x491432F0;
        using Matrix4=std::array<float,16>;
        spTransFunctionEval() noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;} // standalone clone not yet checked
        [[nodiscard]] SampleForAnalysis EvaluateForAnalysis(float delta) override;
        [[nodiscard]] bool EvaluateSampleForAnalysis(float delta,SampleForAnalysis&,spFunctionEval::RandomStateForAnalysis* random=nullptr);
        [[nodiscard]] bool EvaluateMatrixForAnalysis(float delta,Matrix4&,spFunctionEval::RandomStateForAnalysis* random=nullptr);
        [[nodiscard]] auto& GetFunctionsForAnalysis() noexcept{return functions_;}
        [[nodiscard]] const auto& GetFunctionsForAnalysis() const noexcept{return functions_;}
        [[nodiscard]] const Vector3& GetPivotForAnalysis() const noexcept{return pivot_;}
        [[nodiscard]] const Vector3& GetAxisForAnalysis() const noexcept{return axis_;}
        void SetPivotForAnalysis(const Vector3& value) noexcept{pivot_=value;}
        void SetAxisForAnalysis(const Vector3& value) noexcept{axis_=value;}
    private:
        std::array<spFunctionEval,7> functions_; // Tx,Ty,Tz,Sx,Sy,Sz,rotation
        Vector3 pivot_{};
        Vector3 axis_{0,0,1};
    };
}
