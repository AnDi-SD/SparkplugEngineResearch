#pragma once
// Original PC class identity; inferred source path. Actual 41A580/437550 defaults
// and 437610 deleting ownership are confirmed by the bounded 10 September probe.
// Clone/copy remain outside that constructor/loader proof.
#include "spRenderController.h"
#include "spColorFuncEval.h"
#include "spMaterial.h"
namespace sparkplug::reconstruction
{
    class spMaterialColorController final : public spRenderController
    {
    public:
        static constexpr spClassID ClassID=0x4C633E85;
        using Colors=std::array<spMaterial::ColorRGBA,4>;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;} // clone / copy frontier
        void BindMaterialForAnalysis(spMaterial*) noexcept; // actual423650
        void DetachMaterialForAnalysis() noexcept{material_=nullptr;} // host stale-backlink guard
        [[nodiscard]] spMaterial* GetMaterialForAnalysis() const noexcept{return material_;}
        [[nodiscard]] const Colors& GetSavedColorsForAnalysis() const noexcept{return saved_;}
        [[nodiscard]] std::array<spColorFuncEval,4>& GetColorsForAnalysis() noexcept{return colors_;}
        [[nodiscard]] const std::array<spColorFuncEval,4>& GetColorsForAnalysis() const noexcept{return colors_;}
        [[nodiscard]] spFunctionEval& GetAlphaForAnalysis() noexcept{return alpha_;}
        [[nodiscard]] const spFunctionEval& GetAlphaForAnalysis() const noexcept{return alpha_;}
        [[nodiscard]] bool UpdateForRenderForAnalysis() override; // actual4373E0, finite host API
    private:
        spMaterial* material_=nullptr; // borrowed, actual consumer+24
        // Original 13B9129/13B912C/13B9138/13B9144 write each saved alpha 1; the earlier
        // declared consumer state incorrectly left these four words zero.
        Colors saved_{{{0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1}}}; // +28
        std::array<spColorFuncEval,4> colors_; // A,D,S,E at68/B8/108/158
        spFunctionEval alpha_; //1A8
    };
}
