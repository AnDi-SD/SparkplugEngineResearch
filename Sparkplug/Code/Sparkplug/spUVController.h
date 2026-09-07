#pragma once
// Actual PC class identity; portable ownership/guards, not native ABI layout.
#include "spRenderController.h"
#include "spTransFunctionEval.h"
namespace sparkplug::reconstruction
{
    class spMaterialTexture;
    class spUVController final : public spRenderController
    {
    public:
        static constexpr spClassID ClassID=0x1C0053D6;
        using Matrix3=std::array<float,9>;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;} // clone graph pending
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
        void BindMaterialForAnalysis(spMaterialTexture*) noexcept;
        // Host-only stale-backlink guard; do not dereference a dying holder.
        void DetachMaterialForAnalysis() noexcept{material_=nullptr;}
        [[nodiscard]] spMaterialTexture* GetMaterialForAnalysis() const noexcept{return material_;}
        [[nodiscard]] const Matrix3& GetSavedTransformForAnalysis() const noexcept{return saved_;}
        [[nodiscard]] spTransFunctionEval& GetTransformForAnalysis() noexcept{return transform_;}
        [[nodiscard]] const spTransFunctionEval& GetTransformForAnalysis() const noexcept{return transform_;}
        [[nodiscard]] bool UpdateForRenderForAnalysis() override;
    private:
        spMaterialTexture* material_=nullptr;
        Matrix3 saved_{1,0,0,0,1,0,0,0,1};
        spTransFunctionEval transform_;
    };
}
