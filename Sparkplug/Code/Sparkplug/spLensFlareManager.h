#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // Portable capture of the original PC borrowed intrusive list.
    class spLensFlareManager
    {
    public:
        static constexpr std::uint32_t ClassID = 0x782E7D46;

        class CapturedFlareForAnalysis
        {
        public:
            CapturedFlareForAnalysis(std::uint32_t address, std::uint32_t classId) noexcept
                : address_(address), classId_(classId) {}
            CapturedFlareForAnalysis(const CapturedFlareForAnalysis&) = delete;
            CapturedFlareForAnalysis& operator=(const CapturedFlareForAnalysis&) = delete;

            [[nodiscard]] std::uint32_t GetAddressForAnalysis() const noexcept { return address_; }
            [[nodiscard]] std::uint32_t GetPreviousForAnalysis() const noexcept;
            [[nodiscard]] std::uint32_t GetNextForAnalysis() const noexcept;

        private:
            friend class spLensFlareManager;
            std::uint32_t address_;
            std::uint32_t classId_;
            CapturedFlareForAnalysis* previous_ = nullptr;
            CapturedFlareForAnalysis* next_ = nullptr;
            spLensFlareManager* owner_ = nullptr; // portable admission bookkeeping
        };

        spLensFlareManager() = default;
        spLensFlareManager(const spLensFlareManager&) = delete;
        spLensFlareManager& operator=(const spLensFlareManager&) = delete;

        // Use one stable record per captured flare address across lists. Records
        // and their manager must remain alive until the links are detached.
        [[nodiscard]] bool AppendFlareForAnalysis(CapturedFlareForAnalysis& flare) noexcept;
        [[nodiscard]] bool UnlinkFlareForAnalysis(CapturedFlareForAnalysis& flare) noexcept;

        [[nodiscard]] std::uint32_t GetFirstFlareForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetLastFlareForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFlareCountForAnalysis() const noexcept { return count_; }

    private:
        CapturedFlareForAnalysis* first_ = nullptr;
        CapturedFlareForAnalysis* last_ = nullptr;
        std::uint32_t count_ = 0;
    };
}
