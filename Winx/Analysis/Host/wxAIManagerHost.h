#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    class wxAIManager;

    // Our adapter for still-external game systems, not recovered inheritance.
    // Pointers are borrowed except members explicitly adopted by the manager.
    // The host must outlive the manager; no successful default implementations.
    class wxAIManagerHost
    {
    public:
        virtual ~wxAIManagerHost() = default;
        virtual void SubscribeForAnalysis(std::uint32_t group, wxAIManager&) noexcept = 0;
        virtual void UnsubscribeForAnalysis(std::uint32_t group, wxAIManager&) noexcept = 0;
        virtual void DestroyMemberForAnalysis(void*) noexcept = 0;
        // Member PC slots 2C, 40, 4C. The scratch record has unknown fields.
        virtual bool QueryMemberForAnalysis(void*, std::byte* scratch36) noexcept = 0;
        virtual void ExecuteMemberForAnalysis(void*) noexcept = 0;
        virtual void SetMemberFlagForAnalysis(void*, std::uint32_t) noexcept = 0;
        virtual bool IsUpdateBlockedForAnalysis() noexcept = 0;
        virtual std::uint32_t GetLevelForAnalysis() noexcept = 0;
        virtual std::int32_t GetProgress514ForAnalysis() noexcept = 0;
        virtual bool GetGameFlagForAnalysis(std::uint32_t key) noexcept = 0;
        virtual void SetGameByte2CB5ForAnalysis(bool) noexcept = 0;
        virtual void CallGame2798A0ForAnalysis(std::uint32_t, bool) noexcept = 0;
        virtual std::array<float, 3> GetPlayerPositionForAnalysis() noexcept = 0;
        virtual void* FindSceneNodeForAnalysis(const char*, bool, bool) noexcept = 0;
        virtual void* FindChildForAnalysis(void*, const char*, bool, bool) noexcept = 0;
        virtual void SetNodeEnabledForAnalysis(void*, bool, bool) noexcept = 0;
        virtual void* FirstNodeObjectForAnalysis(void*) noexcept = 0;
        virtual void SetCollisionByteForAnalysis(void*, std::uint8_t) noexcept = 0;
        virtual std::array<float, 3> GetNodePositionForAnalysis(void*) noexcept = 0;
        virtual std::array<float, 9> GetNodeRotationForAnalysis(void*) noexcept = 0;
        virtual void MovePlayerForAnalysis(const std::array<float, 3>&) noexcept = 0;
        // Stores nine words, ORs transform flags with 1, then calls slot 30(0).
        virtual void SetPlayerRotationForAnalysis(const std::array<float, 9>&) noexcept = 0;
        virtual void ResetCameraForAnalysis() noexcept = 0;
        // Distinct native timer calls: PC 593B00 and 593A60, respectively.
        virtual void ScheduleFallForAnalysis(std::uint32_t milliseconds, wxAIManager&) noexcept = 0;
        virtual void ScheduleRecoveryForAnalysis(std::uint32_t milliseconds, wxAIManager&) noexcept = 0;
        virtual std::uint32_t GetPlayerCounterForAnalysis() noexcept = 0;
        virtual void SetPlayerCounterForAnalysis(std::uint32_t) noexcept = 0;
        // PC spBaseObject::SendMessage, four argument words after the receiver.
        virtual void SendMessageForAnalysis(wxAIManager&, std::uint32_t code,
            std::uint32_t type, const char* target, std::uint32_t value) noexcept = 0;
        virtual void SendNumericMessageForAnalysis(wxAIManager&, std::uint32_t code,
            std::uint32_t type, std::uint32_t target, std::uint32_t value) noexcept = 0;
    };
}
