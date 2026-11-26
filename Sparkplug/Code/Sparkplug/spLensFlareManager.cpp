#include "spLensFlareManager.h"
#include "spLensFlare.h"

namespace sparkplug::reconstruction
{
    std::uint32_t spLensFlareManager::CapturedFlareForAnalysis::GetPreviousForAnalysis() const noexcept
    {
        return previous_ ? previous_->address_ : 0;
    }

    std::uint32_t spLensFlareManager::CapturedFlareForAnalysis::GetNextForAnalysis() const noexcept
    {
        return next_ ? next_->address_ : 0;
    }

    std::uint32_t spLensFlareManager::GetFirstFlareForAnalysis() const noexcept
    {
        return first_ ? first_->address_ : 0;
    }

    std::uint32_t spLensFlareManager::GetLastFlareForAnalysis() const noexcept
    {
        return last_ ? last_->address_ : 0;
    }

    bool spLensFlareManager::AppendFlareForAnalysis(CapturedFlareForAnalysis& flare) noexcept
    {
        if (flare.address_ == 0 || flare.classId_ != spLensFlare::ClassID || flare.owner_)
            return false;
        for (const auto* linked = first_; linked; linked = linked->next_)
        {
            if (linked->address_ == flare.address_)
                return false;
        }

        // PC 4CE9D0. The preceding admission checks belong to the facade.
        if (!first_)
        {
            first_ = &flare;
            last_ = &flare;
            flare.previous_ = nullptr;
        }
        else
        {
            last_->next_ = &flare;
            flare.previous_ = last_;
            last_ = &flare;
        }
        flare.next_ = nullptr;
        ++count_;
        flare.owner_ = this;
        return true;
    }

    bool spLensFlareManager::UnlinkFlareForAnalysis(CapturedFlareForAnalysis& flare) noexcept
    {
        if (flare.owner_ != this)
            return false;

        // PC 4CE970. The owner check keeps this portable list coherent.
        if (first_ == &flare)
            first_ = flare.next_;
        if (last_ == &flare)
            last_ = flare.previous_;
        if (flare.previous_)
            flare.previous_->next_ = flare.next_;
        if (flare.next_)
            flare.next_->previous_ = flare.previous_;
        flare.previous_ = nullptr;
        flare.next_ = nullptr;
        --count_;
        flare.owner_ = nullptr;
        return true;
    }
}
