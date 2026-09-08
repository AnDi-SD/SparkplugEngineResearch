#pragma once
#include "spRenderableSerializer.h"
#include "spParticleSystem.h"

namespace sparkplug::reconstruction
{
    class spParticleSystemSerializer final : public spRenderableSerializer
    {
    public:
        static constexpr spClassID ClassID=0x047F310F;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        spClassID GetTargetClassIDForAnalysis() const noexcept override {return spParticleSystem::ClassID;}
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
    private:
        bool WriteFields(spSerializerManager*,spStream&,const spBaseObject&,std::string*) const;
    };
}
