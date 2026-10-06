#pragma once

// PC identity and own lifetime are executable-backed. The original header,
// API names and meaning of the payload are unknown. Portable storage is not
// the original multiple-inheritance ABI; raw setters below are host fixtures.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction
{
    class spSystemSettings final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x6A265B0E;
        using OpaqueBytesForAnalysis = std::array<std::uint8_t, 255>;
        spSystemSettings() noexcept;
        ~spSystemSettings() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native singleton support is a secondary subobject at +0x10. This
        // returns its normalized primary object view, without owning it.
        [[nodiscard]] static spSystemSettings* InstanceForAnalysis() noexcept { return instance_; }
        [[nodiscard]] const OpaqueBytesForAnalysis& GetOpaqueBytesForAnalysis() const noexcept { return bytes_; }
        [[nodiscard]] std::optional<std::uint8_t> GetTrailingByteForAnalysis() const noexcept { return trailing_; }
        void SetOpaqueBytesForAnalysis(const OpaqueBytesForAnalysis& bytes,
            std::optional<std::uint8_t> trailing = std::nullopt) noexcept
        { bytes_ = bytes; trailing_ = trailing; }

    private:
        static spSystemSettings* instance_;
        OpaqueBytesForAnalysis bytes_{};
        // Factory does not write native +0x113. Disengaged represents unknown
        // allocator bytes, avoiding an indeterminate C++ read or invented zero.
        std::optional<std::uint8_t> trailing_;
    };
}
