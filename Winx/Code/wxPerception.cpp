#include "wxPerception.h"
#include "Analysis/PC/wxPerceptionNumeric.h"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePerception()
        {
            return std::make_unique<wxPerception>();
        }
        const spRTTIRecord record{wxPerception::ClassID, spBaseObject::ClassID,
            "wxPerception", &spBaseObject::StaticRTTI(), &CreatePerception, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        std::array<int, 3> ExactCoordinates(const std::array<float, 3>& position)
        {
            std::array<int, 3> result{};
            for (std::size_t axis = 0; axis != 3; ++axis)
            {
                const float value = position[axis];
                if (!std::isfinite(value) || std::abs(value) > 1024.0f ||
                    std::trunc(value) != value || (value == 0 && std::signbit(value)))
                {
                    throw std::domain_error("wxPerception arithmetic requires exact integer coordinates in [-1024,1024]");
                }
                result[axis] = static_cast<int>(value);
            }
            return result;
        }
    }

    wxPerception::~wxPerception() { ResetForAnalysis(); }
    const spRTTIRecord& wxPerception::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }
    const spRTTIRecord& wxPerception::vfunc_18() const noexcept { return record; }

    void wxPerception::BindForAnalysis(wxPerceptionHost& host, void* owner,
        std::vector<void*> orderedTargets)
    {
        if (owner_ != nullptr || !targets_.empty())
            throw std::logic_error("wxPerception analytical binding requires ResetForAnalysis first");
        for (const auto* target : orderedTargets)
            if (target == nullptr) throw std::invalid_argument("wxPerception target is null");
        host_ = &host;
        owner_ = owner;
        targets_ = std::move(orderedTargets);
    }

    void wxPerception::ResetForAnalysis() noexcept
    {
        // PC4F18E0: list clear precedes owner detach; scalar fields survive.
        targets_.clear();
        if (owner_ != nullptr) host_->DetachOwnerForAnalysis(*this, owner_);
        owner_ = nullptr;
    }

    void* wxPerception::FindNearestForAnalysis(const wxPerceptionNumericProfileForAnalysis profile)
    {
        if (profile != wxPerceptionNumericProfileForAnalysis::ExactSmallInteger &&
            profile != wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64)
            throw std::invalid_argument("wxPerception numeric profile is unknown");
        if (owner_ == nullptr) return nullptr;
        const auto origin = host_->ReadNodePositionForAnalysis(owner_);
        std::array<int, 3> integerOrigin{};
        if (profile == wxPerceptionNumericProfileForAnalysis::ExactSmallInteger)
            integerOrigin = ExactCoordinates(origin);
        float minimum = std::numeric_limits<float>::max();
        void* selected = nullptr;
        for (auto* target : targets_)
        {
            const auto position = host_->ReadNodePositionForAnalysis(target);
            state_.scratchWords30 = std::array{
                std::bit_cast<std::uint32_t>(position[0]),
                std::bit_cast<std::uint32_t>(position[1]),
                std::bit_cast<std::uint32_t>(position[2])};
            if (profile == wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64)
            {
                if (winx::analysis::pc::UpdatePerceptionMinimumForAnalysis(origin, position, minimum))
                    selected = target;
            }
            else
            {
                const auto coordinate = ExactCoordinates(position);
                std::array<int, 3> delta{};
                for (std::size_t axis = 0; axis != 3; ++axis)
                    delta[axis] = coordinate[axis] - integerOrigin[axis];
                // The EE y*y+x*x+z*z order has the same exact result here.
                const int square = delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0];
                const float distance = static_cast<float>(square);
                if (distance <= minimum)
                {
                    minimum = distance;
                    selected = target;
                }
            }
        }
        return selected;
    }

    std::unique_ptr<spBaseObject> wxPerception::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxPerception>();
        clone->host_ = host_;
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxPerception::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxPerception*>(&destination);
        if (target == nullptr) return false; // portable type guard
        if (!targets_.empty() || !target->targets_.empty() ||
            state_.opaque68 != 0 || state_.opaque6C != 0)
            throw std::logic_error("wxPerception populated-container or opaque-reference Copy is unqualified");
        if (owner_ != nullptr && target->host_ == nullptr)
            throw std::logic_error("wxPerception copied owner requires a destination host");

        target->ResetForAnalysis();
        if (!spBaseObject::vfunc_14(destination, manager)) return false;
        target->owner_ = owner_;
        auto& state = target->state_;
        state.words24 = state_.words24;
        state.scratchWords30 = state_.scratchWords30;
        state.word3C = state_.word3C;
        state.byte40 = state_.byte40;
        state.word44 = state_.word44;
        state.byte48 = state_.byte48;
        state.word54 = state_.word54;
        state.word50 = state_.word50;
        state.word4C = state_.word4C;
        state.opaque68 = state_.opaque68;
        state.opaque6C = state_.opaque6C;
        state.byte70 = state_.byte70;
        state.word74 = state_.word74;
        // +58/+5C/+60/+64/+65/+78 are not copied by either original tail.
        return true;
    }
}
