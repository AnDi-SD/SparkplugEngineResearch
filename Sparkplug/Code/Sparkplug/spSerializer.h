#pragma once

// Exact original source path recovered from the PC executable:
// Z:\Sparkplug\Code\Sparkplug\spSerializer.cpp

#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spStream;

    struct spSerializerObjectHeaderForAnalysis final
    {
        std::uint32_t classID = 0;
        std::uint32_t marker = 0x4F4F4253; // bytes "SBOO"
    };

    static_assert(sizeof(spSerializerObjectHeaderForAnalysis) == 0x08);

    class spSerializer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x42429877;

        ~spSerializer() override;

        spSerializer(const spSerializer&) = delete;
        spSerializer& operator=(const spSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // The native base registration has no factory and its clone slot
        // returns null. Concrete per-object serializers provide allocation.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x004671E0 and PS2 0x001814D0 both return their class-ID
        // argument unchanged. The virtual hook lets specialised serializers
        // remap legacy IDs, although no such override is claimed here.
        [[nodiscard]] virtual spClassID ResolveClassIDForAnalysis(
            spClassID serializedClassID) const noexcept;

        // Native PC 0x00467550 / PS2 0x001815B0 read the full 8-byte
        // [classID, "SBOO"] record, but only classID participates in control
        // flow. The marker is exposed to strict callers without pretending
        // that the shipped loader validated it here.
        [[nodiscard]] virtual std::unique_ptr<spBaseObject>
            ReadObjectHeaderAndCreateForAnalysis(
                spStream& source,
                spSerializerObjectHeaderForAnalysis* observedHeader = nullptr) const;

        [[nodiscard]] static bool HasCanonicalObjectMarkerForAnalysis(
            const spSerializerObjectHeaderForAnalysis& header) noexcept;

    protected:
        spSerializer() noexcept;
    };
}
