#pragma once

// Inferred portable declaration. PC43AD20 and the registered serializer
// establish this sequential reader; original header/TU path is unknown.
#include "spProjectionSerializer.h"

namespace sparkplug::reconstruction
{
    class spBoxProjection;
    class spBoxProjectionSerializer : public spProjectionSerializer
    {
    public:
        static constexpr spClassID ClassID=0x5730539D;
        static constexpr spClassID TargetClassID=0x32BB2F56;
        spBoxProjectionSerializer() noexcept=default;
        ~spBoxProjectionSerializer() override;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
        bool IndexNullRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionNullIndexContextForAnalysis&,spBaseObject&) const override;
        bool IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionPreindexedIndexContextForAnalysis&,spBaseObject&) const override;
        bool IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionFogOnlyIndexContextForAnalysis&,spBaseObject&) const override;
        bool IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionEmptyMaterialIndexContextForAnalysis&,spBaseObject&) const override;
    private:
        bool WriteBoxFieldsForAnalysis(spSerializerManager*,spStream&,
            const spBoxProjection&,std::string*) const;
    };
}
