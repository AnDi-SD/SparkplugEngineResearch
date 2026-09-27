#include "wxBacoAIBehavior.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateBacoAIBehavior()
        {
            return std::make_unique<wxBacoAIBehavior>();
        }
        const spRTTIRecord bacoRecord{wxBacoAIBehavior::ClassID,
            wxBaseAIBehavior::ClassID, "wxBacoAIBehavior",
            &wxBaseAIBehavior::StaticRTTI(), &CreateBacoAIBehavior, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(
            bacoRecord);
    }

    const spRTTIRecord& wxBacoAIBehavior::StaticRTTI() noexcept
    {
        (void)registered;
        return bacoRecord;
    }
    const spRTTIRecord& wxBacoAIBehavior::vfunc_18() const noexcept
    {
        return bacoRecord;
    }
    std::unique_ptr<spBaseObject> wxBacoAIBehavior::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBacoAIBehavior>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }
    bool wxBacoAIBehavior::vfunc_14(
        spBaseObject& destination, spCloneManager& manager) const
    {
        auto* const bacoDestination = dynamic_cast<wxBacoAIBehavior*>(&destination);
        if (bacoDestination == nullptr)
        {
            return false;
        }
        // Default clone is observed. The derived copy's behavior with changed
        // fields has not been measured; retain constructor values in the clone.
        return CopyEmptyBaseForAnalysis(*bacoDestination, manager);
    }

    void wxBacoAIBehavior::SelectDefaultActionForAnalysis(
        const std::uint32_t parameter) noexcept
    {
        SwitchToActionForAnalysis(FindActionForAnalysis(0), parameter);
    }
}
