#include "spController.h"
#include "spAnimationManager.h"
#include <stdexcept>
namespace sparkplug::reconstruction
{
    spController::spController()
    {
        if (auto* manager = spAnimationManager::GetInstance())
            if (!manager->RegisterControllerForAnalysis(*this))
                throw std::length_error("Controller registry exceeds host analysis bound");
    }
    spController::~spController()
    {
        if (manager_)
            manager_->UnregisterControllerForAnalysis(*this);
    }
    bool spController::vfunc_14(spBaseObject& target, spCloneManager& manager) const
    {
        auto* controller = dynamic_cast<spController*>(&target);
        if (!controller || !spBaseObject::vfunc_14(target, manager))
            return false;
        // Original 0x423100 -> 0x419AA0 copies only +0x10, never intrusive links.
        controller->enabled_ = enabled_;
        return true;
    }
    const spRTTIRecord& spController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,        spSubController::ClassID,
                                         "spController", &spSubController::StaticRTTI(),
                                         nullptr,        nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spController::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
} // namespace sparkplug::reconstruction
