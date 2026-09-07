#pragma once

// Inferred declaration path. Native RTTI and the single relationship member
// are shared by PC and PS2; factory bodies differ by platform protection.

#include "../SparkBase/spBaseObject.h"

#include <memory>
#include <array>

namespace sparkplug::reconstruction
{
    class spMaterialTexture;

    class spMaterialTextureLayer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x7F577C6D;

        spMaterialTextureLayer() noexcept;
        ~spMaterialTextureLayer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] const std::unique_ptr<spMaterialTexture>&
            GetMaterialTextureForAnalysis() const noexcept;
        void SetMaterialTextureForAnalysis(
            std::unique_ptr<spMaterialTexture> texture) noexcept;
        // PC423590→49E4D0, two arguments. Stage word is unused; caller's
        // writable output receives nine raw states. NULL texture host guard.
        [[nodiscard]] bool CopyTextureStatesForAnalysis(std::uint32_t stage,
            std::array<std::uint32_t,9>& output) const noexcept;
        using UVSubmitForAnalysis=bool (*)(void*,std::uint32_t,const std::array<float,9>&);
        // PC423460 tail-calls MaterialTexture467B70. NULL is a host guard.
        [[nodiscard]] bool UpdateForRenderForAnalysis(std::uint32_t stage,UVSubmitForAnalysis,void*);

    private:
        // PC423530 directly deletes the nested object, no intrusive retain.
        std::unique_ptr<spMaterialTexture> materialTexture_;
    };
}
