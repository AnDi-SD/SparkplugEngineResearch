#include "wxAlfeaDoorTrigger.h"
#include <cmath>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAlfeaDoorTriggerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateTrigger()
        {
            if (!factoryHost) throw std::logic_error("wxAlfeaDoorTrigger requires a factory host");
            return std::make_unique<wxAlfeaDoorTrigger>(*factoryHost, factoryHost->InitialDoorIdStorageForAnalysis());
        }
        // Registration ancestry only. These records are not registered as
        // portable factories, nor do they claim implemented C++ parent classes.
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID, "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1, "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord genericRecord{0x38F1ED51, 0x796A1869, "wxGenericTrigger", &wxEntityRecord, nullptr, nullptr};
        const spRTTIRecord pivotRecord{0x4AC3694A, 0x38F1ED51, "wxPivotingDoor", &genericRecord, nullptr, nullptr};
        const spRTTIRecord record{wxAlfeaDoorTrigger::ClassID, 0x4AC3694A, "wxAlfeaDoorTrigger", &pivotRecord, &CreateTrigger, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxAlfeaDoorTrigger::wxAlfeaDoorTrigger(wxAlfeaDoorTriggerHost& host, std::uint32_t storage)
        : host_(host), doorId_(storage)
    {
        host_.ConstructPivotingDoorForAnalysis(*this, base_);
        base_.autoDisable = 0;
        host_.SubscribeForAnalysis(23, *this);
    }
    wxAlfeaDoorTrigger::~wxAlfeaDoorTrigger()
    {
        (void)wxAlfeaDoorTrigger::vfunc_40_CleanupForAnalysis();
        host_.DestroyPivotingDoorForAnalysis(*this);
    }
    void wxAlfeaDoorTrigger::SetFactoryHostForAnalysis(wxAlfeaDoorTriggerHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxAlfeaDoorTrigger::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAlfeaDoorTrigger::vfunc_18() const noexcept { return record; }

    std::unique_ptr<spBaseObject> wxAlfeaDoorTrigger::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxAlfeaDoorTrigger>(host_, host_.InitialDoorIdStorageForAnalysis());
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxAlfeaDoorTrigger::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxAlfeaDoorTrigger*>(&destination);
        if (!target) return false; // portable type guard; native expects a valid destination
        if (!host_.CopyPivotingDoorForAnalysis(*this, *target, manager)) return false;
        target->group_ = group_;
        target->doorId_ = doorId_;
        target->flags_.locked = flags_.locked;
        target->flags_.queued = flags_.queued;
        // The third byte (instant) is deliberately NOT copied.
        return true; // native trailing clone-manager marker is an empty PC hook
    }
    bool wxAlfeaDoorTrigger::vfunc_40_CleanupForAnalysis() noexcept
    {
        host_.UnsubscribeForAnalysis(23, *this);
        (void)host_.CleanupPivotingDoorForAnalysis(*this);
        return true;
    }
    bool wxAlfeaDoorTrigger::vfunc_48_UpdateForAnalysis() noexcept
    {
        if (flags_.queued)
        {
            vfunc_58_AnimateForAnalysis();
            flags_.queued = 0;
            base_.moving = 1;
        }
        (void)host_.UpdatePivotingDoorForAnalysis(*this);
        return true;
    }
    void wxAlfeaDoorTrigger::QueueOpenForAnalysis(std::uint32_t id, std::uint32_t instant) noexcept
    {
        if (id != doorId_) return;
        base_.notifyPlayer = 0;
        flags_.queued = flags_.locked = 1;
        flags_.instant = static_cast<std::uint8_t>(instant);
    }
    bool wxAlfeaDoorTrigger::SpecialGateForAnalysis() noexcept
    {
        return doorId_ == 2 && host_.GetProgress514ForAnalysis() == 2
            && !host_.GetGameFlagForAnalysis(0x19);
    }
    void wxAlfeaDoorTrigger::vfunc_5C_EnterForAnalysis() noexcept
    {
        if (!flags_.locked && !base_.interactionBlocked && !SpecialGateForAnalysis())
            host_.EnterPivotingDoorForAnalysis(*this);
    }
    bool wxAlfeaDoorTrigger::vfunc_54_CanInteractForAnalysis() noexcept
    {
        if (!host_.CanInteractPivotingDoorForAnalysis(*this)) return false;
        void* transform = host_.GetPlayerTransformForAnalysis();
        auto forward = host_.GetForwardForAnalysis(transform);
        forward[1] = 0;
        host_.NormalizeForAnalysis(forward);
        const auto player = host_.GetPositionForAnalysis(transform);
        const auto node = host_.GetNodePositionForAnalysis(base_.node);
        std::array<float, 3> direction{node[0] - player[0], 0, node[2] - player[2]};
        host_.NormalizeForAnalysis(direction);
        return host_.AngleBetweenForAnalysis(forward, direction) < 2.0;
    }
    void wxAlfeaDoorTrigger::vfunc_0C(const void* notification) noexcept
    {
        const auto& m = *static_cast<const wxAlfeaDoorMessageForAnalysis*>(notification);
        switch (m.code)
        {
        case 0x27AC:
            if (m.value18 == group_) base_.interactionBlocked = static_cast<std::uint8_t>(m.value1C);
            break;
        case 0x27AD: QueueOpenForAnalysis(m.value18, m.value1C); break;
        case 0x27AE:
            if (m.sender != this && m.value18 == doorId_ && !base_.moving)
            {
                flags_.instant = static_cast<std::uint8_t>(m.value1C);
                vfunc_58_AnimateForAnalysis();
                base_.moving = 1;
            }
            break;
        case 0x27AF:
            if (m.value18 == doorId_ && base_.angle != 0.0f)
            {
                base_.enabled = base_.moving = 1;
                base_.angle = base_.targetAngle;
                base_.phase = 3;
                base_.started = host_.GetMillisecondsForAnalysis();
            }
            break;
        default: host_.NotifyPivotingDoorForAnalysis(*this, m); break;
        }
    }
    void wxAlfeaDoorTrigger::vfunc_58_AnimateForAnalysis() noexcept
    {
        const auto now = host_.GetMillisecondsForAnalysis();
        const std::uint32_t elapsed = now - base_.started;
        if (base_.phase == 0)
        {
            base_.started = now;
            base_.angle = 0;
            base_.phase = 1;
            if (base_.notifyPlayer) host_.NotifyPlayerForAnalysis(*this, 0x27D1, 50, 0);
            else base_.notifyPlayer = 1;
            // Native code tests the address of the inline array, not *name.
            void* linked = host_.FindSceneNodeForAnalysis(base_.linkedNodeName.data(), true, false);
            if (linked)
            {
                host_.EnableNodeComponentForAnalysis(linked, 0);
                host_.EnableNodeComponentForAnalysis(linked, 1);
            }
            host_.SendGroupMessageForAnalysis(*this, 0x27AE, 23, doorId_, flags_.instant);
        }
        else if (base_.phase == 1)
        {
            base_.angle = flags_.instant ? base_.targetAngle : static_cast<float>(
                (static_cast<double>(elapsed) / base_.duration) * base_.targetAngle);
            if (std::fabs(base_.targetAngle) <= std::fabs(base_.angle))
            {
                base_.angle = base_.targetAngle;
                base_.phase = 2;
            }
        }
        else
        {
            bool finished;
            if (base_.phase == 2)
            {
                base_.angle = base_.targetAngle;
                flags_.locked = 1;
                finished = true;
            }
            else
            {
                base_.angle = static_cast<float>((1.0 - static_cast<double>(elapsed) / base_.duration) * base_.targetAngle);
                finished = elapsed > base_.duration; // strict, unsigned
                if (finished) base_.angle = 0;
            }
            if (finished)
            {
                base_.moving = 0;
                base_.phase = 0;
                if (base_.autoDisable) base_.enabled = 0;
            }
        }
        host_.SetNodeYawForAnalysis(base_.node, base_.initialAngle + base_.angle);
        host_.MarkNodeDirtyForAnalysis(base_.node, 1);
    }

    bool wxAlfeaDoorTrigger::RegisterPropertiesForAnalysis(wxAlfeaDoorTriggerHost& host)
    {
        static const char* const groups[]{"FaragondaDoor", "LoomaDoor", "Others", "Girl Appartment", "BallroomAnteroom"};
        static const char* const ids[]{"FaragondaDoor", "LoomaDoor", "GirlAppartment", "BloomDoor", "StellaDoor", "MusaDoor", "LibraryDoor", "SimulationDoor", "BallRoomDoor", "EastAnteroomDoor", "WestAnteroomDoor"};
        host.RegisterEnumPropertyForAnalysis("Door Group : ", false, groups, 5);
        host.RegisterEnumPropertyForAnalysis("Door ID : ", true, ids, 11);
        (void)host.RegisterPivotingDoorPropertiesForAnalysis();
        return true;
    }
}
