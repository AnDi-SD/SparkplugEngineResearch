#pragma once

// Inferred declaration path. The shipped executables preserve the class name
// and exact runtime layout, but not an original header pathname.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Physical PC prefix is NamedObject; native engine RTTI remains BaseObject.
    class spMaterial : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x5C0314C5;
        static constexpr std::size_t RenderStateCount = 11;
        static constexpr std::size_t MaximumPassCount = 8;
        using RenderStates = std::array<std::uint32_t, RenderStateCount>;
        using ColorRGBA = std::array<float,4>;

        // Portable spelling of the confirmed secondary color interface. This
        // declaration does not claim a host vtable is native ABI-compatible.
        [[nodiscard]] virtual const ColorRGBA& GetAmbientColorForAnalysis() const noexcept = 0;
        virtual void SetAmbientColorForAnalysis(const ColorRGBA&) noexcept = 0;
        [[nodiscard]] virtual const ColorRGBA& GetDiffuseColorForAnalysis() const noexcept = 0;
        virtual void SetDiffuseColorForAnalysis(const ColorRGBA&) noexcept = 0;
        [[nodiscard]] virtual const ColorRGBA& GetSpecularColorForAnalysis() const noexcept = 0;
        virtual void SetSpecularColorForAnalysis(const ColorRGBA&) noexcept = 0;
        [[nodiscard]] virtual const ColorRGBA& GetEmissiveColorForAnalysis() const noexcept = 0;
        virtual void SetEmissiveColorForAnalysis(const ColorRGBA&) noexcept = 0;
        [[nodiscard]] virtual float GetSpecularPowerForAnalysis() const noexcept = 0;
        virtual void SetSpecularPowerForAnalysis(float) noexcept = 0;
        [[nodiscard]] virtual bool HasInitializedSpecularPowerForAnalysis() const noexcept {return true;}

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
            std::size_t index, std::shared_ptr<spBaseObject> pass) noexcept;

        [[nodiscard]] bool UsesVertexAlphaForAnalysis() const noexcept;
        void SetUsesVertexAlphaForAnalysis(bool value) noexcept;
        [[nodiscard]] std::uint8_t GetVertexAlphaByteForAnalysis() const noexcept { return useVertexAlpha_; }
        void SetVertexAlphaByteForAnalysis(std::uint8_t value) noexcept { useVertexAlpha_=value; }

        // PC +0x6C / PS2 +0x74 participates in the render state-save protocol;
        // its original name and broader semantics are still unresolved.
        [[nodiscard]] bool GetRenderOverrideFlagForAnalysis() const noexcept;
        void SetRenderOverrideFlagForAnalysis(bool value) noexcept;
        [[nodiscard]] std::uint8_t GetRenderOverrideByteForAnalysis() const noexcept{return renderOverrideFlag_;}
        void SetRenderOverrideByteForAnalysis(std::uint8_t value) noexcept{renderOverrideFlag_=value;}

        [[nodiscard]] std::uint32_t GetOpaqueRuntimeFieldForAnalysis()
            const noexcept;
        void SetOpaqueRuntimeFieldForAnalysis(std::uint32_t value) noexcept;

        [[nodiscard]] spBaseObject* GetMaterialColorControllerForAnalysis()
            const noexcept;
        void SetMaterialColorControllerForAnalysis(std::shared_ptr<spBaseObject> controller)
            noexcept;

    protected:
        spMaterial() noexcept;

    private:
        RenderStates renderStates_{
            0u, 0u, 1u, 2u, 1u, 1u, 3u, 0u, 4u, 1u, 6u};
        std::array<std::shared_ptr<spBaseObject>, MaximumPassCount> passes_{};
        std::size_t passCount_ = 0;
        std::uint8_t renderOverrideFlag_ = 0;
        std::uint8_t useVertexAlpha_ = 0; // native serializer preserves the raw byte
        std::uint32_t opaqueRuntimeField_ = 0;
        std::shared_ptr<spBaseObject> materialColorController_;
    };
}
