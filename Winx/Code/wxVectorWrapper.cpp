#include "wxVectorWrapper.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        std::unique_ptr<spBaseObject> CreateVectorWrapper()
        {
            return std::make_unique<wxVectorWrapper>();
        }

        const spRTTIRecord record{wxVectorWrapper::ClassID, spBaseObject::ClassID,
            "wxVectorWrapper", &spBaseObject::StaticRTTI(), &CreateVectorWrapper, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        bool IsSignalingNaN(const std::uint32_t word) noexcept
        {
            return (word & 0x7F800000u) == 0x7F800000u
                && (word & 0x007FFFFFu) != 0
                && (word & 0x00400000u) == 0;
        }
    }

    wxVectorWrapper::wxVectorWrapper(const CopyPlatformForAnalysis platform) noexcept
        : platform_(platform)
    {
        // PC596DF0/PS2 2A5130 both construct spBaseObject and zero 10/14/18.
    }

    const spRTTIRecord& wxVectorWrapper::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxVectorWrapper::vfunc_18() const noexcept
    {
        return record;
    }

    bool wxVectorWrapper::HasSupportedCopyInputForAnalysis() const noexcept
    {
        // X is an integer MOV in the PC Copy. No host floating-point load is
        // needed for any X pattern. Y/Z are FLD/FSTP: masked/unmasked x87
        // signaling-NaN behavior is outside the recovered portable domain.
        return platform_ == CopyPlatformForAnalysis::PS2
            || (!IsSignalingNaN(words_[1]) && !IsSignalingNaN(words_[2]));
    }

    bool wxVectorWrapper::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxVectorWrapper*>(&destination);
        if (!target || !HasSupportedCopyInputForAnalysis())
            return false; // Portable preflight guards; not native fault behavior.
        if (!spBaseObject::vfunc_14(*target, manager))
            return false;

        // Native Copy leaves destination base fields untouched. PS2 transfers
        // all three FPR words without arithmetic; supported PC inputs produce
        // the same payload. Preserve the target's own platform selector.
        target->words_ = words_;
        return true;
    }

    std::unique_ptr<spBaseObject> wxVectorWrapper::vfunc_10(spCloneManager& manager) const
    {
        if (!HasSupportedCopyInputForAnalysis())
            return nullptr; // No temporary clone mapping for a refused input.
        auto clone = std::make_unique<wxVectorWrapper>(platform_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager))
            return nullptr;
        return clone;
    }
}
