#include "spCinematic.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    spCinematic::spCinematic() noexcept=default;
    spCinematic::~spCinematic()=default;
    const spRTTIRecord& spCinematic::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,"spCinematic",
            &spBaseObject::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spCinematic::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spCinematic::vfunc_10(spCloneManager&) const
    {throw std::logic_error("Cinematic Clone is outside the recovered empty lifetime profile");}
    bool spCinematic::vfunc_14(spBaseObject&,spCloneManager&) const
    {
        // Do not inherit Named's RTTI-sensitive wrapper or infer new Copy
        // evidence from the physical prefix. Native primary0C is separate.
        throw std::logic_error("Cinematic Copy remains unqualified");
    }
}
