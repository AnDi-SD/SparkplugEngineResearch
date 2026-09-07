#include "spAnimationManager.h"
#include "spController.h"
#include "spTransformTrackEval.h"
#include <cmath>
#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    spAnimationManager* spAnimationManager::instance_ = nullptr;
    spAnimationManager::spAnimationManager()
        : bindingLifetime_(
              std::make_shared<BindingLifetimeForAnalysis>(BindingLifetimeForAnalysis{this}))
    {
        instance_ = this;
    }
    spAnimationManager::~spAnimationManager()
    {
        bindingLifetime_->owner = nullptr; // host leases cannot call a dying manager
        instance_ = nullptr; // native is unconditional, even for a noncurrent instance
        // Native assumes all controllers are destroyed first. Host-only detach
        // prevents dangling registry links if that lifetime precondition is broken.
        while (head_)
            UnregisterControllerForAnalysis(*head_);
    }
    const spRTTIRecord& spAnimationManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,
                                         spBaseObject::ClassID,
                                         "spAnimationManager",
                                         &spBaseObject::StaticRTTI(),
                                         +[]() -> std::unique_ptr<spBaseObject> {
                                             return std::make_unique<spAnimationManager>();
                                         },
                                         nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    spAnimationManager* spAnimationManager::GetInstance() noexcept
    {
        return instance_;
    }
    const spRTTIRecord& spAnimationManager::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spAnimationManager::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spAnimationManager>();
        manager.RegisterClone(*this, *result);
        return vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }
    bool spAnimationManager::vfunc_14(spBaseObject& target, spCloneManager& manager) const
    {
        return target.IsKindOf(ClassID) && spBaseObject::vfunc_14(target, manager);
    }
    std::int32_t spAnimationManager::BindNameForAnalysis(std::string_view name)
    {
        if (name.size() > 4095 || name.find('\0') != std::string_view::npos)
            return -1;
        auto found = names_.find(name);
        if (found != names_.end())
        {
            if (found->second.references == std::numeric_limits<std::uint32_t>::max())
                return -1;
            ++found->second.references;
            return found->second.slot;
        }
        if (names_.size() >= 4096 ||
            nextSlot_ > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
            return -1;
        const auto slot = static_cast<std::int32_t>(nextSlot_);
        names_.emplace(std::string(name), NameBindingForAnalysis{slot, 1});
        ++nextSlot_;
        return slot;
    }
    void spAnimationManager::UnbindNameForAnalysis(std::string_view name)
    {
        auto found = names_.find(name);
        if (found != names_.end() && --found->second.references == 0)
            names_.erase(found);
    }
    std::optional<spAnimationManager::NameBindingForAnalysis> spAnimationManager::
        FindNameForAnalysis(std::string_view name) const
    {
        const auto found = names_.find(name);
        return found == names_.end() ? std::nullopt : std::optional(found->second);
    }
    std::size_t spAnimationManager::GetNameCountForAnalysis() const noexcept
    {
        return names_.size();
    }
    std::uint32_t spAnimationManager::GetNextSlotForAnalysis() const noexcept
    {
        return nextSlot_;
    }
    spAnimationManager::NameBindingLeaseForAnalysis::NameBindingLeaseForAnalysis(
        std::weak_ptr<BindingLifetimeForAnalysis> lifetime, std::string name,
        std::int32_t slot) noexcept
        : lifetime_(std::move(lifetime)), name_(std::move(name)), slot_(slot)
    {
    }
    spAnimationManager::NameBindingLeaseForAnalysis::~NameBindingLeaseForAnalysis()
    {
        ResetForAnalysis();
    }
    spAnimationManager::NameBindingLeaseForAnalysis::NameBindingLeaseForAnalysis(
        NameBindingLeaseForAnalysis&& other) noexcept
        : lifetime_(std::move(other.lifetime_)), name_(std::move(other.name_)),
          slot_(std::exchange(other.slot_, -1))
    {
    }
    spAnimationManager::NameBindingLeaseForAnalysis& spAnimationManager::
        NameBindingLeaseForAnalysis::operator=(NameBindingLeaseForAnalysis&& other) noexcept
    {
        if (this != &other)
        {
            ResetForAnalysis();
            lifetime_ = std::move(other.lifetime_);
            name_ = std::move(other.name_);
            slot_ = std::exchange(other.slot_, -1);
        }
        return *this;
    }
    void spAnimationManager::NameBindingLeaseForAnalysis::ResetForAnalysis() noexcept
    {
        if (slot_ != -1)
            if (const auto live = lifetime_.lock(); live && live->owner)
                live->owner->UnbindNameForAnalysis(name_);
        lifetime_.reset();
        name_.clear();
        slot_ = -1;
    }
    bool spAnimationManager::NameBindingLeaseForAnalysis::BelongsToForAnalysis(
        const spAnimationManager& manager) const noexcept
    {
        const auto live = lifetime_.lock();
        return slot_ != -1 && live && live->owner == &manager;
    }
    std::optional<spAnimationManager::NameBindingLeaseForAnalysis> spAnimationManager::
        AcquireNameBindingForAnalysis(std::string_view name)
    {
        if (name.size() > 4095 || name.find('\0') != std::string_view::npos)
            return std::nullopt;
        std::string ownedName(name); // allocate before acquiring the reference
        const auto slot = BindNameForAnalysis(ownedName);
        if (slot == -1)
            return std::nullopt;
        return NameBindingLeaseForAnalysis(bindingLifetime_, std::move(ownedName), slot);
    }
    std::int32_t spAnimationManager::AttachEvaluatorForAnalysis(spTransformEval& evaluator,
                                                                std::string_view name)
    {
        auto* trackEvaluator = dynamic_cast<spTransformTrackEval*>(&evaluator);
        if (!trackEvaluator)
            return 0; // native type-rejection sentinel, valid IDs start at one
        const auto slot = BindNameForAnalysis(name);
        if (slot != -1)
            trackEvaluator->SetBoundSlotForAnalysis(slot);
        return slot;
    }
    void spAnimationManager::DetachEvaluatorForAnalysis(std::string_view name)
    {
        UnbindNameForAnalysis(name);
    }
    bool spAnimationManager::RegisterControllerForAnalysis(spController& controller) noexcept
    {
        if (controller.manager_ || controllerCount_ >= 4096)
            return false; // host-only duplicate/membership/capacity guard
        controller.manager_ = this;
        controller.next_ = nullptr;
        controller.previous_ = tail_;
        if (tail_)
            tail_->next_ = &controller;
        else
            head_ = &controller;
        tail_ = &controller;
        ++controllerCount_;
        return true;
    }
    void spAnimationManager::UnregisterControllerForAnalysis(spController& controller) noexcept
    {
        if (controller.manager_ != this)
            return;
        if (tail_ == &controller)
            tail_ = controller.previous_;
        if (head_ == &controller)
            head_ = controller.next_;
        if (controller.next_)
            controller.next_->previous_ = controller.previous_;
        if (controller.previous_)
            controller.previous_->next_ = controller.next_;
        if (frameCursor_ == &controller)
            frameCursor_ = controller.next_; // host-only self-removal safety
        controller.manager_ = nullptr;
        controller.previous_ = controller.next_ = nullptr; // native leaves old links
        --controllerCount_;
    }
    bool spAnimationManager::AdvanceFrameForAnalysis(float capturedDelta)
    {
        if (!std::isfinite(capturedDelta) || inFrame_)
            return false;
        inFrame_ = true;
        ++frame_;
        frameCursor_ = head_;
        std::size_t visits = 0;
        try
        {
            while (frameCursor_ && ++visits <= 4096)
            {
                auto* current = frameCursor_;
                if (current->IsEnabledForAnalysis())
                    current->ApplyForAnalysis(capturedDelta);
                if (frameCursor_ == current)
                    frameCursor_ = current->next_; // original reads next after virtual call
            }
        }
        catch (...)
        {
            frameCursor_ = nullptr;
            inFrame_ = false;
            throw;
        }
        const bool complete = frameCursor_ == nullptr;
        frameCursor_ = nullptr;
        inFrame_ = false;
        return complete;
    }
    std::uint32_t spAnimationManager::GetFrameForAnalysis() const noexcept
    {
        return frame_;
    }
    std::size_t spAnimationManager::GetControllerCountForAnalysis() const noexcept
    {
        return controllerCount_;
    }
} // namespace sparkplug::reconstruction
