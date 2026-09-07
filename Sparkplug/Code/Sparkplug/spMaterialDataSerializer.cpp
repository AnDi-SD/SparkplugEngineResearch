#include "spMaterialDataSerializer.h"
#include "../SparkplugDX/spDXMaterial.h"
#include "../SparkBase/spStream.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialDataSerializer()
        {
            return std::make_unique<spMaterialDataSerializer>();
        }

        const spRTTIRecord MaterialDataSerializerRecord{
            spMaterialDataSerializer::ClassID,
            spMaterialSerializer::ClassID,
            "spMaterialDataSerializer",
            &spMaterialSerializer::StaticRTTI(),
            &CreateMaterialDataSerializer,
            nullptr,
        };

        const bool MaterialDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialDataSerializerRecord);
    }

    spMaterialDataSerializer::~spMaterialDataSerializer() = default;

    std::unique_ptr<spBaseObject> spMaterialDataSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source,spSerializerObjectHeaderForAnalysis* observedHeader) const
    {
        // Actual PC42F4C0 consumes eight bytes, ignores both words, calls4A9460.
        spSerializerObjectHeaderForAnalysis header;
        if(!source.ReadData(&header,sizeof(header)))return nullptr;
        if(observedHeader)*observedHeader=header;
        try{return std::make_unique<spDXMaterial>();}catch(...){return nullptr;}
    }

    const spRTTIRecord& spMaterialDataSerializer::StaticRTTI() noexcept
    {
        (void)MaterialDataSerializerRegistered;
        return MaterialDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMaterialDataSerializer::vfunc_18() const noexcept
    {
        return MaterialDataSerializerRecord;
    }

    spClassID spMaterialDataSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    bool spMaterialDataSerializer::CanReadIntoObjectForAnalysis(
        const bool hasTargetObject) noexcept
    {
        return hasTargetObject;
    }
}
