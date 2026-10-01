#include "wxSpiritAwayState.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSpiritAwayState(){return std::make_unique<wxSpiritAwayState>();}
        const spRTTIRecord record{wxSpiritAwayState::ClassID,wxCharacterState::ClassID,
            "wxSpiritAwayState",&wxCharacterState::StaticRTTI(),&CreateSpiritAwayState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& wxSpiritAwayState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxSpiritAwayState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxSpiritAwayState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxSpiritAwayState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;return clone;
    }
}
