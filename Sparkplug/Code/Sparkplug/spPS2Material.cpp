#include "spPS2Material.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2Material()
        {
            return std::make_unique<spPS2Material>();
        }

        const spRTTIRecord PS2MaterialRecord{
            spPS2Material::ClassID,
            spMaterial::ClassID,
            "spPS2Material",
            &spMaterial::StaticRTTI(),
            &CreatePS2Material,
            nullptr,
        };

        const bool PS2MaterialRegistered =
            spRTTIManager::Instance().Register(PS2MaterialRecord);
    }

    spPS2Material::spPS2Material() noexcept = default;
    spPS2Material::~spPS2Material() = default;

    const spRTTIRecord& spPS2Material::StaticRTTI() noexcept
    {
        (void)PS2MaterialRegistered;
        return PS2MaterialRecord;
    }

    std::unique_ptr<spBaseObject> spPS2Material::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2Material>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spPS2Material::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID)
            || !spMaterial::vfunc_14(destination, manager))
        {
            return false;
        }

        auto& material = static_cast<spPS2Material&>(destination);
        material.diffuse_ = diffuse_;
        material.ambient_ = ambient_;
        material.specular_ = specular_;
        material.emissive_ = emissive_;
        material.specularPower_ = specularPower_;
        return true;
    }

    const spRTTIRecord& spPS2Material::vfunc_18() const noexcept
    {
        return PS2MaterialRecord;
    }

    const spPS2Material::ColorRGBA&
    spPS2Material::GetDiffuseColorForAnalysis() const noexcept { return diffuse_; }
    void spPS2Material::SetDiffuseColorForAnalysis(
        const ColorRGBA& value) noexcept { diffuse_ = value; }
    const spPS2Material::ColorRGBA&
    spPS2Material::GetAmbientColorForAnalysis() const noexcept { return ambient_; }
    void spPS2Material::SetAmbientColorForAnalysis(
        const ColorRGBA& value) noexcept { ambient_ = value; }
    const spPS2Material::ColorRGBA&
    spPS2Material::GetSpecularColorForAnalysis() const noexcept { return specular_; }
    void spPS2Material::SetSpecularColorForAnalysis(
        const ColorRGBA& value) noexcept { specular_ = value; }
    const spPS2Material::ColorRGBA&
    spPS2Material::GetEmissiveColorForAnalysis() const noexcept { return emissive_; }
    void spPS2Material::SetEmissiveColorForAnalysis(
        const ColorRGBA& value) noexcept { emissive_ = value; }
    float spPS2Material::GetSpecularPowerForAnalysis() const noexcept
    {
        return specularPower_;
    }
    void spPS2Material::SetSpecularPowerForAnalysis(
        const float value) noexcept { specularPower_ = value; }
}
