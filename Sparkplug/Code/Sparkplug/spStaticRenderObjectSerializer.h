#pragma once

// PC44FE90 reader /450140 writer /44FE30 reference indexing. Names and field
// IDs also occur in the PS2 registration; execution evidence here is PC only.
#include "spSerializer.h"

namespace sparkplug::reconstruction
{
    class spStaticRenderObject;
    class spStaticRenderObjectSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x20757934;
        static constexpr spClassID TargetClassID = 0x56D67170;
        spStaticRenderObjectSerializer() noexcept = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept { return TargetClassID; }
        [[nodiscard]] static bool ReadMatrixFieldForAnalysis(std::uint32_t field, spStream&,
            spStaticRenderObject&);
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&, spStream&,
            std::uint32_t, spBaseObject&, std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&, spBaseObject&) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string*) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&, spStream&,
            const spBaseObject&, std::string*) const override;
    private:
        [[nodiscard]] bool WriteFields(spSerializerManager*, spStream&, const spBaseObject&, std::string*) const;
    };
}
