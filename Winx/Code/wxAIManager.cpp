#include "wxAIManager.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAIManagerHost* factoryHost = nullptr;
        wxAIManager* instance = nullptr;
        std::unique_ptr<spBaseObject> CreateManager()
        {
            if (!factoryHost) throw std::logic_error("wxAIManager requires a factory host");
            return std::make_unique<wxAIManager>(*factoryHost);
        }
        const spRTTIRecord record{wxAIManager::ClassID, spBaseObject::ClassID,
            "wxAIManager", &spBaseObject::StaticRTTI(), &CreateManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxAIManager::wxAIManager(wxAIManagerHost& host) noexcept
        : host_(host), cursor20_(members_.end()), cursor24_(members_.end())
    {
        instance = this;
        host_.SubscribeForAnalysis(11, *this);
    }

    wxAIManager::~wxAIManager()
    {
        ClearForAnalysis();
        // The original clears its global unconditionally, even if another
        // instance (e.g. a clone) replaced it in the meantime.
        instance = nullptr;
    }

    void wxAIManager::SetFactoryHostForAnalysis(wxAIManagerHost* host) noexcept { factoryHost = host; }
    wxAIManager* wxAIManager::GetInstanceForAnalysis() noexcept { return instance; }
    const spRTTIRecord& wxAIManager::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAIManager::vfunc_18() const noexcept { return record; }

    std::unique_ptr<spBaseObject> wxAIManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxAIManager>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    void wxAIManager::AddMemberForAnalysis(void* object)
    {
        if (cleared_) throw std::logic_error("wxAIManager cannot be repopulated after Clear");
        if (std::find(members_.begin(), members_.end(), object) == members_.end())
            members_.push_back(object);
    }

    void wxAIManager::RemoveMemberForAnalysis(void* object) noexcept
    {
        const auto found = std::find(members_.begin(), members_.end(), object);
        if (found == members_.end()) return;
        const auto next = std::next(found);
        if (cursor20_ == found) cursor20_ = next;
        if (cursor24_ == found) cursor24_ = next;
        members_.erase(found);
    }

    void wxAIManager::ClearForAnalysis() noexcept
    {
        // Native destruction visits the front repeatedly; members are owned.
        while (!members_.empty())
        {
            if (members_.front()) host_.DestroyMemberForAnalysis(members_.front());
            members_.pop_front();
        }
        // Native cursor words are left stale. Keep them unobserved until reuse;
        // portable end avoids dangling C++ iterators after this explicit clear.
        cursor20_ = cursor24_ = members_.end();
        cleared_ = true;
        host_.UnsubscribeForAnalysis(11, *this);
        state_.field3C = nullptr;
    }

    void wxAIManager::ProcessThreeForAnalysis() noexcept
    {
        const auto initialCount = members_.size();
        std::byte scratch[36]; // original stack buffer is not initialized
        for (std::size_t i = 0; i < 3 && i < initialCount; ++i)
        {
            if (members_.empty()) continue;
            if (cursor24_ == members_.end()) cursor24_ = members_.begin();
            void* object = *cursor24_++;
            if (object && host_.QueryMemberForAnalysis(object, scratch))
                host_.ExecuteMemberForAnalysis(object);
        }
    }

    bool wxAIManager::UpdateForAnalysis() noexcept
    {
        if (host_.IsUpdateBlockedForAnalysis()) return false;
        ProcessThreeForAnalysis();
        UpdateLevel14ForAnalysis();
        UpdateLevel6ForAnalysis();
        return true;
    }

    void wxAIManager::BroadcastFlagForAnalysis(std::uint32_t flag) noexcept
    {
        for (void* object : members_) host_.SetMemberFlagForAnalysis(object, flag);
    }

    void wxAIManager::vfunc_0C(const void* notification) noexcept
    {
        const auto& message = *static_cast<const wxAIManagerMessageForAnalysis*>(notification);
        switch (message.code)
        {
        case 0x2712: ResetLevelForAnalysis(message.value); break;
        case 0x27A3: HandleFallTimerForAnalysis(message.value); break;
        case 0x27B6: BroadcastFlagForAnalysis(message.value); break;
        default: break;
        }
    }

    void wxAIManager::ResetLevelForAnalysis(std::uint32_t level) noexcept
    {
        state_.triggered30 = false;
        state_.counter2C = state_.counter28 = state_.counter34 = 0;
        state_.pending44 = false;
        if (level == 6) state_.respawnPoint = host_.FindSceneNodeForAnalysis("RespawnPoint", true, false);
        // counter38, field3C, and (for other levels) respawnPoint survive.
    }

    void wxAIManager::OpenDoorPairForAnalysis(const char* first, const char* second) noexcept
    {
        for (auto code : {0x2733u, 0x2736u, 0x2732u})
        {
            host_.SendMessageForAnalysis(*this, code, 6, first, 0);
            host_.SendMessageForAnalysis(*this, code, 6, second, 0);
        }
    }
    void wxAIManager::CountDoorPair03And05ForAnalysis() noexcept
    {
        if (++state_.counter28 >= 2) OpenDoorPairForAnalysis("door_03", "door_05");
    }
    void wxAIManager::CountDoorPair01And06ForAnalysis() noexcept
    {
        if (++state_.counter2C >= 3) OpenDoorPairForAnalysis("door_01", "door_06");
    }
    void wxAIManager::CountLevel21ForAnalysis() noexcept
    {
        if (host_.GetLevelForAnalysis() == 21 && ++state_.counter34 >= 2)
            host_.SetGameByte2CB5ForAnalysis(true);
    }
    void wxAIManager::CountLevel18ForAnalysis() noexcept
    {
        if (!host_.GetGameFlagForAnalysis(0x79) && host_.GetLevelForAnalysis() == 18
            && ++state_.counter38 == 2)
        {
            state_.counter38 = 0;
            host_.CallGame2798A0ForAnalysis(3, true);
        }
    }

    void wxAIManager::UpdateLevel14ForAnalysis() noexcept
    {
        if (host_.GetLevelForAnalysis() != 14 || state_.triggered30
            || host_.GetProgress514ForAnalysis() >= 5) return;
        const auto p = host_.GetPlayerPositionForAnalysis();
        // PC x87 keeps X/Y differences extended, but spills Z to float.
        const double x = 107.0 - p[0], y = 1075.0 - p[1];
        const float z = 764.0f - p[2];
        if (x*x + y*y + double(z)*z < 22500.0)
        {
            host_.SendMessageForAnalysis(*this, 0x2735, 6, "door_06", 0);
            host_.SendMessageForAnalysis(*this, 0x2733, 6, "door_06", 1);
            host_.SendMessageForAnalysis(*this, 0x2733, 6, "door_01", 1);
            state_.triggered30 = true;
        }
    }

    void wxAIManager::UpdateLevel6ForAnalysis() noexcept
    {
        if (host_.GetLevelForAnalysis() == 6 && !state_.pending44
            && host_.GetPlayerPositionForAnalysis()[1] < -240.0f)
        {
            host_.ScheduleFallForAnalysis(200, *this);
            state_.pending44 = true;
        }
    }

    void wxAIManager::HandleFallTimerForAnalysis(std::uint32_t value) noexcept
    {
        if ((value & 0xFF) != 0)
        {
            if (state_.pending44) state_.pending44 = false;
            return;
        }
        host_.MovePlayerForAnalysis(host_.GetNodePositionForAnalysis(state_.respawnPoint));
        host_.SetPlayerRotationForAnalysis(host_.GetNodeRotationForAnalysis(state_.respawnPoint));
        host_.ResetCameraForAnalysis();
        host_.ScheduleRecoveryForAnalysis(1500, *this);
        host_.SetPlayerCounterForAnalysis(host_.GetPlayerCounterForAnalysis() - 1u);
        host_.SendNumericMessageForAnalysis(*this, 0x2716, 4, 4, 0);
    }

    void wxAIManager::SetBattleCageForAnalysis(bool enabled) noexcept
    {
        // PC entry uses a protected singleton load. Callee sequence is also
        // independently visible on PS2; original protection startup is external.
        if (host_.GetLevelForAnalysis() != 30) return;
        void* cage = host_.FindSceneNodeForAnalysis("battle_cage", true, true);
        if (!cage) return;
        host_.SetNodeEnabledForAnalysis(cage, enabled, true);
        void* collision = host_.FindChildForAnalysis(cage, "collision", true, false);
        if (!collision) return;
        void* object = host_.FirstNodeObjectForAnalysis(collision);
        if (object) host_.SetCollisionByteForAnalysis(object, static_cast<std::uint8_t>(enabled));
    }

    std::size_t wxAIManager::GetCursor20ForAnalysis() const noexcept
    { return static_cast<std::size_t>(std::distance(members_.cbegin(), std::list<void*>::const_iterator(cursor20_))); }
    std::size_t wxAIManager::GetCursor24ForAnalysis() const noexcept
    { return static_cast<std::size_t>(std::distance(members_.cbegin(), std::list<void*>::const_iterator(cursor24_))); }
}
