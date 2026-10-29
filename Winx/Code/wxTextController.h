#pragma once

#include "Code/SparkBase/spBaseObject.h"
#include <array>

namespace winx::reconstruction
{
    // Inferred declaration/path. Own field meanings remain unknown. This
    // adapter accepts captured PC words; it is not the original factory or ABI.
    class wxTextController final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x9F1535F7;
        static constexpr std::size_t OwnWordCountForAnalysis=204;
        using CapturedOwnWordsForAnalysis=std::array<std::uint32_t,OwnWordCountForAnalysis>;
        wxTextController(const wxTextController&)=delete;
        wxTextController& operator=(const wxTextController&)=delete;
        explicit wxTextController(const CapturedOwnWordsForAnalysis& captured) noexcept;
        ~wxTextController() override;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // PC5A8490; native source-level method name is not recovered.
        void ResetForAnalysis() noexcept;
        const CapturedOwnWordsForAnalysis& GetOwnWordsForAnalysis() const noexcept{return own_;}
    private:
        CapturedOwnWordsForAnalysis own_;
    };
}
