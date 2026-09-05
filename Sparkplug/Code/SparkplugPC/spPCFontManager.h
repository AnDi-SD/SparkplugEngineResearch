#pragma once

// The original translation unit path is present verbatim in WinxClub.exe:
// Z:\Sparkplug\Code\SparkplugPC\spPCFontManager.cpp

#include "../Sparkplug/spFontManager.h"

namespace sparkplug::reconstruction
{
    class spPCFontManager final : public spFontManager
    {
    public:
        static constexpr spClassID ClassID = 0x7B467097;

        spPCFontManager() noexcept = default;
        ~spPCFontManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] bool InitializeForAnalysis() override;

        [[nodiscard]] bool IsPlatformBufferReadyForAnalysis() const noexcept;

    private:
        bool platformBufferReady_ = false;
    };
}
