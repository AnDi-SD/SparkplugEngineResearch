#pragma once

// Inferred declaration path. The shipped executables preserve the class name
// and exact runtime layout, but not an original header pathname.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spMaterial : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x5C0314C5;
        static constexpr std::size_t RenderStateCount = 11;
        static constexpr std::size_t MaximumPassCount = 8;
        using RenderStates = std::array<std::uint32_t, RenderStateCount>;

        ~spMaterial() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] const RenderStates& GetRenderStatesForAnalysis()
            const noexcept;
        [[nodiscard]] std::uint32_t GetRenderStateForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] bool SetRenderStateForAnalysis(
            std::size_t index, std::uint32_t value) noexcept;

        [[nodiscard]] std::size_t GetPassCountForAnalysis() const noexcept;
        [[nodiscard]] spBaseObject* GetPassForAnalysis(
            std::size_t index) const noexcept;
        [[nodiscard]] bool SetPassForAnalysis(
            std::size_t index, spBaseObject* pass) noexcept;

        [[nodiscard]] bool UsesVertexAlphaForAnalysis() const noexcept;
        void SetUsesVertexAlphaForAnalysis(bool value) noexcept;

        // +0x74 participates in the pre/post-render state-save protocol, but
        // its original name and broader semantics are still unresolved.
        [[nodiscard]] bool GetRenderOverrideFlagForAnalysis() const noexcept;
        void SetRenderOverrideFlagForAnalysis(bool value) noexcept;

        [[nodiscard]] std::uint32_t GetOpaqueRuntimeFieldForAnalysis()
            const noexcept;
        void SetOpaqueRuntimeFieldForAnalysis(std::uint32_t value) noexcept;

        [[nodiscard]] spBaseObject* GetMaterialColorControllerForAnalysis()
            const noexcept;
        void SetMaterialColorControllerForAnalysis(spBaseObject* controller)
            noexcept;

    protected:
        spMaterial() noexcept;

    private:
        RenderStates renderStates_{
            0u, 0u, 1u, 2u, 1u, 1u, 3u, 0u, 4u, 1u, 6u};
        std::array<spBaseObject*, MaximumPassCount> passes_{};
        std::size_t passCount_ = 0;
        bool renderOverrideFlag_ = false;
        bool useVertexAlpha_ = false;
        std::uint32_t opaqueRuntimeField_ = 0;
        spBaseObject* materialColorController_ = nullptr;
    };
}
