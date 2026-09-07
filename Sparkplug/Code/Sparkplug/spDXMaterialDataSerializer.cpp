#include "spDXMaterialDataSerializer.h"
#include "spMaterialDataSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXMaterialDataSerializer()
        {
            return std::make_unique<spDXMaterialDataSerializer>();
        }

        const spRTTIRecord DXMaterialDataSerializerRecord{
            spDXMaterialDataSerializer::ClassID,
            spMaterialSerializer::ClassID,
            "spDXMaterialDataSerializer",
            &spMaterialSerializer::StaticRTTI(),
            &CreateDXMaterialDataSerializer,
            nullptr,
        };

        const bool DXMaterialDataSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXMaterialDataSerializerRecord);
    }

    spDXMaterialDataSerializer::~spDXMaterialDataSerializer() = default;

    std::unique_ptr<spBaseObject> spDXMaterialDataSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source,spSerializerObjectHeaderForAnalysis* observedHeader) const
    {
        // Both native primary tables point to the SAME42F4C0 implementation.
        return spMaterialDataSerializer{}.ReadObjectHeaderAndCreateForAnalysis(source,observedHeader);
    }

    const spRTTIRecord& spDXMaterialDataSerializer::StaticRTTI() noexcept
    {
        (void)DXMaterialDataSerializerRegistered;
        return DXMaterialDataSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spDXMaterialDataSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXMaterialDataSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spDXMaterialDataSerializer::vfunc_18() const noexcept
    {
        return DXMaterialDataSerializerRecord;
    }
}
