#pragma once

// Inferred portable declaration. The native class identity and three-word
// payload are measured separately for PC and PS2; this is not either C++ ABI.
#include "Code/SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxVectorWrapper final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x1F793E0D;
        using WordsForAnalysis = std::array<std::uint32_t, 3>;
        // This selector belongs to the portable reconstruction, not the
        // native object. PS2 only transfers raw FPR words; PC loads Y/Z into
        // x87. Unqualified PC signaling-NaN inputs are refused explicitly.
        enum class CopyPlatformForAnalysis { PC, PS2 };

        explicit wxVectorWrapper(CopyPlatformForAnalysis platform = CopyPlatformForAnalysis::PC) noexcept;
        wxVectorWrapper(const wxVectorWrapper&) = delete;
        wxVectorWrapper& operator=(const wxVectorWrapper&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;

        [[nodiscard]] const WordsForAnalysis& GetWordsForAnalysis() const noexcept { return words_; }
        // Fixture/adapter access, not a recovered original setter name.
        void SetWordsForAnalysis(const WordsForAnalysis& words) noexcept { words_ = words; }
        [[nodiscard]] CopyPlatformForAnalysis GetCopyPlatformForAnalysis() const noexcept { return platform_; }
        [[nodiscard]] bool HasSupportedCopyInputForAnalysis() const noexcept;

    private:
        WordsForAnalysis words_{};
        CopyPlatformForAnalysis platform_;
    };
}
