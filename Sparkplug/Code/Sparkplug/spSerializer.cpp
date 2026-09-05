#include "spSerializer.h"

#include "../SparkBase/spStream.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord SerializerRecord{
            spSerializer::ClassID,
            spBaseObject::ClassID,
            "spSerializer",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool SerializerRegistered =
            spRTTIManager::Instance().Register(SerializerRecord);
    }

    spSerializer::spSerializer() noexcept = default;

    spSerializer::~spSerializer() = default;

    const spRTTIRecord& spSerializer::StaticRTTI() noexcept
    {
        (void)SerializerRegistered;
        return SerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSerializer::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    bool spSerializer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spSerializer::vfunc_18() const noexcept
    {
        return SerializerRecord;
    }

    spClassID spSerializer::ResolveClassIDForAnalysis(
        const spClassID serializedClassID) const noexcept
    {
        return serializedClassID;
    }

    std::unique_ptr<spBaseObject>
    spSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source,
        spSerializerObjectHeaderForAnalysis* const observedHeader) const
    {
        spSerializerObjectHeaderForAnalysis header{};
        if (!source.ReadData(&header, sizeof(header)))
        {
            if (observedHeader != nullptr)
            {
                *observedHeader = header;
            }
            return nullptr;
        }

        if (observedHeader != nullptr)
        {
            *observedHeader = header;
        }

        // Deliberately no marker comparison: neither shipped body reads the
        // second word after ReadData. Strict tools can call the helper below.
        const auto resolvedClassID = ResolveClassIDForAnalysis(header.classID);
        return spRTTIManager::Instance().Create(resolvedClassID);
    }

    bool spSerializer::HasCanonicalObjectMarkerForAnalysis(
        const spSerializerObjectHeaderForAnalysis& header) noexcept
    {
        return header.marker == 0x4F4F4253;
    }
}
