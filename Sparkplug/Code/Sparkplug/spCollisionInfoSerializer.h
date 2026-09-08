#pragma once
// Inferred path. PC438A80/438E20; identity6D2DB0, PS24835C0.
#include "spSerializer.h"
namespace sparkplug::reconstruction
{
    class spCollisionInfo;
    class spCollisionInfoSerializer final : public spSerializer
    {
    public:
        static constexpr spClassID ClassID=0x33380E8C;
        static constexpr spClassID TargetClassID=0x47A97C0E;
        enum class Field : std::uint32_t{Primitive=0,Group=1,Transform=2};
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& stream,
            std::uint32_t size,spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
            spBaseObject& object) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,
            const spBaseObject& object,std::string* error) const override;
    private:
        bool WriteFields(spSerializerManager* manager,spStream& stream,const spBaseObject& object,std::string* error) const;
    };
}
