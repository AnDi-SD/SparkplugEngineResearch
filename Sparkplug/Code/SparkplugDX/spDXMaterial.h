#pragma once
// Class/direct base/factory are original PC evidence. Header path is inferred.
#include "../Sparkplug/spMaterial.h"

namespace sparkplug::reconstruction
{
    class spDXMaterial final : public spMaterial
    {
    public:
        static constexpr spClassID ClassID=0x797B39EC;
        spDXMaterial() noexcept = default;
        ~spDXMaterial() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        [[nodiscard]] const ColorRGBA& GetAmbientColorForAnalysis() const noexcept override{return ambient_;}
        void SetAmbientColorForAnalysis(const ColorRGBA& value) noexcept override{ambient_=value;}
        [[nodiscard]] const ColorRGBA& GetDiffuseColorForAnalysis() const noexcept override{return diffuse_;}
        void SetDiffuseColorForAnalysis(const ColorRGBA& value) noexcept override{diffuse_=value;}
        [[nodiscard]] const ColorRGBA& GetSpecularColorForAnalysis() const noexcept override{return specular_;}
        void SetSpecularColorForAnalysis(const ColorRGBA& value) noexcept override{specular_=value;}
        [[nodiscard]] const ColorRGBA& GetEmissiveColorForAnalysis() const noexcept override{return emissive_;}
        void SetEmissiveColorForAnalysis(const ColorRGBA& value) noexcept override{emissive_=value;}
        [[nodiscard]] float GetSpecularPowerForAnalysis() const noexcept override{return power_;}
        void SetSpecularPowerForAnalysis(float value) noexcept override{power_=value;powerInitialized_=true;}
        [[nodiscard]] bool HasInitializedSpecularPowerForAnalysis() const noexcept override{return powerInitialized_;}
        // PC secondary slot0/4A9530; caller supplies Renderer+40 frame stamp.
        // bool is a host guard result, not the native void return signature.
        [[nodiscard]] bool UpdateColorForFrameForAnalysis(std::uint32_t frame,bool force,bool* evaluated=nullptr);
    private:
        ColorRGBA diffuse_{1,1,1,1},ambient_{0,0,0,0},specular_{1,1,1,1},emissive_{0,0,0,0};
        // Native BC-byte ctor does not write +B8. Safe host storage is zero,
        // but writer rejects it until initialized; zero is NOT native default.
        float power_=0;
        bool powerInitialized_=false;
    };
}
