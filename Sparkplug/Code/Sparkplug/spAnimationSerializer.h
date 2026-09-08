#pragma once

// Exact original PC translation unit:
// Z:\Sparkplug\Code\Sparkplug\spAnimationSerializer.cpp
// Header/API names inferred. Strict portable field reader uses the existing
// spDataBlockSerializer/spStream core; it is not a full FFPS/FAT load/save layer.
#include "spAnimation.h"
#include "spSerializer.h"
#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace sparkplug::reconstruction
{
    class spAnimationSerializer final : public spSerializer
    {
      public:
        static constexpr spClassID ClassID = 0xC0ACBFA6;
        static constexpr spClassID TargetClassID = spAnimation::ClassID;
        static constexpr std::uint32_t MaximumFieldBytesForAnalysis = 2U * 1024U * 1024U;
        static constexpr std::uint32_t MaximumTracksForAnalysis = 4096;
        using BindingResolverForAnalysis =
            std::function<std::optional<std::int32_t>(std::string_view)>;
        struct ReadObservationsForAnalysis
        {
            std::array<std::optional<std::uint32_t>, 7> declaredPools{};
            std::array<std::uint32_t, 7> usedPools{};
            std::optional<std::uint32_t> trackReserveHint;
            std::vector<std::uint32_t> unknownFields;
        };
        // Host integration policy, not an original field/constructor argument.
        // blwalk.san omits pool declarations; owned portable vectors can read
        // those bounded keys without claiming the original allocation path.
        enum class KeyPoolPolicyForAnalysis { RequireDeclared, AllowMissingWithOwnedKeys };
        explicit spAnimationSerializer(KeyPoolPolicyForAnalysis policy = KeyPoolPolicyForAnalysis::RequireDeclared);
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept
        {
            return TargetClassID;
        }

        // Reads fields AFTER the [classID,SBOO] prefix. Strict bounds, terminator,
        // finite/key invariants and transaction ownership are host safeguards;
        // native partial-read paths can return success or retain partial objects.
        // Optional resolver is a non-owning lookup, not the native name registry.
        // It is invoked only after all field validation succeeds.
        [[nodiscard]] std::unique_ptr<spAnimation> ReadFieldsForAnalysis(
            spStream& source, std::uint32_t byteCount,
            const BindingResolverForAnalysis& resolveBinding = {},
            ReadObservationsForAnalysis* observations = nullptr,
            std::string* error = nullptr) const;

        // Same field core plus owned per-track registry references. All field
        // validation precedes binding. Failed acquisition releases prior leases;
        // consumed stream position/monotonic ID allocation are not rolled back.
        [[nodiscard]] std::unique_ptr<spAnimation> ReadFieldsWithBindingsForAnalysis(
            spStream& source, std::uint32_t byteCount, spAnimationManager& manager,
            ReadObservationsForAnalysis* observations = nullptr,
            std::string* error = nullptr) const;

        // PC43DFE0/43DDC0: duration/reserve/pool counters, PRS2/3/4 then
        // track name1, tags5, counter patch-back and terminator. Emits fields
        // only, no FFPS/FAT or class/SBOO envelope. Preflight bounds, finite
        // validated snapshots and refusal of missing native descriptors are
        // host safety. I/O failure retains partial destination, not rollback.
        // Semantic/canonical writer: unknown skipped fields are NOT preserved.
        [[nodiscard]] bool WriteFieldsForAnalysis(
            spStream& destination, const spAnimation& animation,
            std::string* error = nullptr) const;

        [[nodiscard]] bool IndexRelationshipsForAnalysis(spBaseObject& object) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(
            spStream& destination, const spBaseObject& object, std::string* error) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(
            spSerializerReadContextForAnalysis& context, spStream& source,
            std::uint32_t byteCount, spBaseObject& object, std::string* error) const override;

      private:
        KeyPoolPolicyForAnalysis keyPoolPolicy_;
        // Populates a newly factory-created object without replacing its
        // address; generic reference readers publish that address first.
        [[nodiscard]] bool ReadFieldsIntoForAnalysis(spStream& source,
            std::uint32_t byteCount, spAnimation& target,
            const BindingResolverForAnalysis& resolveBinding,
            ReadObservationsForAnalysis* observations, std::string* error) const;
    };
} // namespace sparkplug::reconstruction
