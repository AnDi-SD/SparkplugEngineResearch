#pragma once
// Original PC runtime serializer identity; path inferred, API names analytical.
#include "../Sparkplug/spSerializer.h"
namespace sparkplug::reconstruction
{
    class spDXTextureSerializer final : public spSerializer
    {
    public:
        static constexpr spClassID ClassID=0x196D44FE;
        spDXTextureSerializer() noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
    };
}
