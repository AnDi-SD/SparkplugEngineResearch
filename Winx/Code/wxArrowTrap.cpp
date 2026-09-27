#include "wxArrowTrap.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxArrowTrapHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateArrowTrap()
        {
            if (!factoryHost) throw std::logic_error("wxArrowTrap requires a factory host");
            return std::make_unique<wxArrowTrap>(*factoryHost);
        }
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord record{wxArrowTrap::ClassID, 0x796A1869,
            "wxArrowTrap", &wxEntityRecord, &CreateArrowTrap, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxArrowTrap::wxArrowTrap(wxArrowTrapHost& host) : host_(host)
    {
        host_.ConstructEntityForAnalysis(*this);
        state_ = StateForAnalysis{};
    }
    wxArrowTrap::~wxArrowTrap()
    {
        for (std::size_t i = 0; i != 3; ++i)
        {
            state_.arrows[i] = state_.emitters[i] = nullptr;
            if (state_.ownedComponents[i])
            {
                host_.DestroyComponentForAnalysis(state_.ownedComponents[i]);
                state_.ownedComponents[i] = nullptr;
            }
        }
        host_.DestroyEntityForAnalysis(*this);
    }
    void wxArrowTrap::SetFactoryHostForAnalysis(wxArrowTrapHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxArrowTrap::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxArrowTrap::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxArrowTrap::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxArrowTrap>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxArrowTrap::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxArrowTrap*>(&destination);
        if (!target) return false; // native call assumes a valid destination
        if (!host_.CopyEntityForAnalysis(*this, *target, manager)) return false;
        target->state_.speed = state_.speed;
        target->state_.pauseMilliseconds = state_.pauseMilliseconds;
        target->state_.entityWord84 = state_.entityWord84;
        return true;
    }
    bool wxArrowTrap::vfunc_2C_InRangeForAnalysis(const wxArrowPosition& point) noexcept
    {
        const auto origin = host_.PositionForAnalysis(host_.RootNodeForAnalysis(*this));
        const float dx = origin[0] - point[0];
        const float dy = origin[1] - point[1];
        const float dz = origin[2] - point[2];
        state_.inRange = static_cast<std::uint8_t>(!(dx * dx + dy * dy + dz * dz > 2000000.0f));
        return true;
    }
    void* wxArrowTrap::FindRenderableForAnalysis(void* node) const
    {
        if (host_.HasRenderDataForAnalysis(node)) return node;
        for (void* child : host_.ChildrenForAnalysis(node))
            if (void* found = FindRenderableForAnalysis(child)) return found;
        return nullptr;
    }
    void wxArrowTrap::EnableEmitterForAnalysis(std::size_t channel)
    {
        void* emitter = state_.emitters[channel];
        if (!emitter) return;
        if (void* component = host_.ComponentForAnalysis(emitter))
        {
            host_.EnableComponentFlagForAnalysis(component, 2);
            return;
        }
        void* component = host_.CreateComponentForAnalysis();
        state_.ownedComponents[channel] = component;
        host_.SetComponentWordForAnalysis(component, 0x200);
        host_.AttachComponentForAnalysis(emitter, component);
    }
    void wxArrowTrap::SetupForAnalysis()
    {
        auto& s = state_;
        if (!s.pauseMilliseconds) s.pauseMilliseconds = 1;
        s.intervalSeconds = static_cast<float>(s.pauseMilliseconds) * 0.001f;
        s.timer[host_.RandomForAnalysis() & 1u] = s.intervalSeconds * 0.5f;
        void* root = host_.RootNodeForAnalysis(*this);
        std::size_t foundArrows = 0;
        for (void* child : host_.ChildrenForAnalysis(root))
        {
            const auto name = host_.NameForAnalysis(child);
            if (foundArrows < 2 && name.find("fleche_") != name.npos)
            {
                s.arrows[foundArrows] = child;
                s.arrowPositions[foundArrows] = host_.PositionForAnalysis(child);
                ++foundArrows;
            }
            else if (name.find("arrowstart_") != name.npos)
            {
                s.arrows[2] = child;
                s.startPosition = host_.PositionForAnalysis(child);
            }
        }
        s.active = static_cast<std::uint8_t>(s.arrows[0] && s.arrows[1] && s.arrows[2]);
        if (s.active)
        {
            for (std::size_t i = 0; i != 2; ++i)
            {
                s.emitters[i] = FindRenderableForAnalysis(s.arrows[0]);
                if (!s.emitters[i]) s.active = 0;
            }
            EnableEmitterForAnalysis(0);
            EnableEmitterForAnalysis(1);
        }
        host_.AfterSetupForAnalysis(*this, root);
        host_.SetEntityTimeForAnalysis(*this, host_.MillisecondsForAnalysis());
    }
    void wxArrowTrap::WaitForAnalysis(std::size_t channel) noexcept
    {
        auto& s = state_;
        s.timer[channel] += host_.DeltaSecondsForAnalysis();
        if (!(s.timer[channel] >= s.intervalSeconds)) return;
        s.phase[channel] = 1;
        s.timer[channel] = 0;
        host_.NotifyForAnalysis(*this, 0x27D1, 0x11, 0xBB, 0);
    }
    void wxArrowTrap::FlyForAnalysis(std::size_t channel) noexcept
    {
        auto& s = state_;
        if (s.inRange) host_.UpdateTargetTransformForAnalysis(*this, s.emitters[channel]);
        s.timer[channel] += host_.DeltaSecondsForAnalysis();
        const float distance = s.timer[channel] * s.speed;
        wxArrowPosition delta{};
        for (std::size_t j = 0; j != 3; ++j)
            delta[j] = s.arrowPositions[channel][j] - s.startPosition[j];
        const float squared = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
        if (distance * distance < squared)
        {
            host_.NormalizeForAnalysis(delta);
            host_.SetPositionAndDirtyForAnalysis(s.arrows[channel],
                {s.startPosition[0] + delta[0] * distance,
                 s.arrowPositions[channel][1],
                 s.startPosition[2] + delta[2] * distance});
        }
        else
        {
            host_.SetPositionAndDirtyForAnalysis(s.arrows[channel], s.arrowPositions[channel]);
            s.phase[channel] = 0;
            s.timer[channel] = 0;
        }
    }
    void wxArrowTrap::UpdateForAnalysis() noexcept
    {
        if (!state_.active) return;
        if (state_.inRange) host_.UpdateEntityTransformForAnalysis(*this);
        for (std::size_t i = 0; i != 2; ++i)
        {
            if (state_.phase[i] == 0) WaitForAnalysis(i);
            else if (state_.phase[i] == 1) FlyForAnalysis(i);
        }
    }
    void wxArrowTrap::vfunc_0C(const void* notification) noexcept
    {
        switch (static_cast<const wxArrowTrapMessageForAnalysis*>(notification)->code)
        {
        case 0x1C: SetupForAnalysis(); break;
        case 0x1E: UpdateForAnalysis(); break;
        default: break;
        }
    }
    bool wxArrowTrap::RegisterPropertiesForAnalysis(wxArrowTrapHost& host)
    {
        host.RegisterFloatPropertyForAnalysis("Speed (cm/s)", 0x128);
        host.RegisterWordPropertyForAnalysis("Pause between arrow shot (ms)", 0x12C);
        (void)host.RegisterEntityPropertiesForAnalysis();
        return true;
    }
}
