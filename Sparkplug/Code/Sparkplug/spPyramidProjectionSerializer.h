#pragma once

// Inferred portable declaration. Registered PC serializer43B240/reader43B320
// establish this sequential reader; original header/TU paths are unknown.
#include "spProjectionSerializer.h"

namespace sparkplug::reconstruction
{
    class spPyramidProjection;
    class spPyramidProjectionSerializer : public spProjectionSerializer
    {
    public:
        static constexpr spClassID ClassID=0x22F43149;
        static constexpr spClassID TargetClassID=0x1CCA7732;
        spPyramidProjectionSerializer() noexcept=default;
        ~spPyramidProjectionSerializer() override;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        // Stable raw25-byte own section follows the qualified parent writer.
        // Index and unsupported relationship/derived profiles remain refused.
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
    private:
        bool WritePyramidFieldsForAnalysis(spSerializerManager*,spStream&,
            const spPyramidProjection&,std::string*) const;
    };
}
