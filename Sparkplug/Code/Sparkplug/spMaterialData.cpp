#include "spMaterialData.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialData()
        {
            return std::make_unique<spMaterialData>();
        }

        const spRTTIRecord MaterialDataRecord{
            spMaterialData::ClassID,
            spMaterial::ClassID,
            "spMaterialData",
            &spMaterial::StaticRTTI(),
            &CreateMaterialData,
            nullptr,
        };

        const bool MaterialDataRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialDataRecord);
    }

    spMaterialData::spMaterialData() noexcept = default;

    spMaterialData::~spMaterialData() = default;

    const spRTTIRecord& spMaterialData::StaticRTTI() noexcept
    {
        (void)MaterialDataRegistered;
        return MaterialDataRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialData::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialData>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spMaterialData::vfunc_14(
        spBaseObject& destination,
        spCloneManager&) const
    {
        // Both native builds deliberately use a leaf copy stub which only
        // reports success. Consequently Clone produces constructor defaults,
        // not a deep copy of material state or colors.
        return destination.IsKindOf(ClassID);
    }

    const spRTTIRecord& spMaterialData::vfunc_18() const noexcept
    {
        return MaterialDataRecord;
    }

    const spMaterialData::ColorRGBA&
    spMaterialData::GetAmbientColorForAnalysis() const noexcept
    {
        return ambient_;
    }

    void spMaterialData::SetAmbientColorForAnalysis(
        const ColorRGBA& value) noexcept
    {
        ambient_ = value;
    }

    const spMaterialData::ColorRGBA&
    spMaterialData::GetDiffuseColorForAnalysis() const noexcept
    {
        return diffuse_;
    }

    void spMaterialData::SetDiffuseColorForAnalysis(
        const ColorRGBA& value) noexcept
    {
        diffuse_ = value;
    }

    const spMaterialData::ColorRGBA&
    spMaterialData::GetSpecularColorForAnalysis() const noexcept
    {
        return specular_;
    }

    void spMaterialData::SetSpecularColorForAnalysis(
        const ColorRGBA& value) noexcept
    {
        specular_ = value;
    }

    const spMaterialData::ColorRGBA&
    spMaterialData::GetEmissiveColorForAnalysis() const noexcept
    {
        return emissive_;
    }

    void spMaterialData::SetEmissiveColorForAnalysis(
        const ColorRGBA& value) noexcept
    {
        emissive_ = value;
    }

    float spMaterialData::GetSpecularPowerForAnalysis() const noexcept
    {
        return specularPower_;
    }

    void spMaterialData::SetSpecularPowerForAnalysis(
        const float value) noexcept
    {
        specularPower_ = value;
    }
}
