#include "wxTextController.h"
#include <algorithm>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        const spRTTIRecord record{wxTextController::ClassID,spBaseObject::ClassID,
            "wxTextController",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        constexpr std::size_t OwnIndex(std::size_t nativeOffset) noexcept{return (nativeOffset-0x10)/4;}
    }
    wxTextController::wxTextController(const CapturedOwnWordsForAnalysis& captured) noexcept:own_(captured){}
    wxTextController::~wxTextController(){ResetForAnalysis();}
    const spRTTIRecord& wxTextController::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxTextController::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxTextController::vfunc_10(spCloneManager&) const
    {return {};}// Native Clone/constructor unassigned own-state profile is not reproduced.
    void wxTextController::ResetForAnalysis() noexcept
    {
        std::fill_n(own_.begin()+OwnIndex(0x220),54,0u);
        std::fill_n(own_.begin()+OwnIndex(0x118),54,0u);
        for(std::size_t i=0;i<6;++i)
        {
            own_[OwnIndex(0x100)+i]=0;
            own_[OwnIndex(0x208)+i]=0;
            own_[OwnIndex(0x1F0)+i]=0;
            own_[OwnIndex(0x10)+i]=0;
        }
        std::fill_n(own_.begin()+OwnIndex(0x28),54,0u);
        // Native does not clear its final18 words at2F8..33C. These remain
        // captured raw data; no pointer or vector meaning is invented.
    }
}
