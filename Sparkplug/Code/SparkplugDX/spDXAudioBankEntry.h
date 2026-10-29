#pragma once
#include "../Sparkplug/spAudioBankEntry.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spDXAudioBankEntry final : public spAudioBankEntry
    {
    public:
        static constexpr spClassID ClassID=0x2CD93375;
        struct DxStateForAnalysis final
        {
            std::array<std::uint32_t,11> rawWords34To5C{};
            std::uint32_t rawWord64=0,rawWord68=0,rawWord94=0,rawWordE4=0,rawWordF4=0;
        };
        explicit spDXAudioBankEntry(CapturedEmptyProfileForAnalysis);
        ~spDXAudioBankEntry() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] const DxStateForAnalysis& GetDxStateForAnalysis() const noexcept{return dxState_;}

    private:
        DxStateForAnalysis dxState_{};
        // Native98/E0 and all unwritten ranges have no fabricated source
        // value/storage. The marker limits lifetime to the observed branch.
    };
}
