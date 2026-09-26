#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction { class spCloneManager; }
namespace winx::reconstruction
{
    class wxAlfeaDoorTrigger;
    struct wxAlfeaDoorMessageForAnalysis;

    // Our view of the inherited fields touched by this leaf. This is neither
    // wxPivotingDoor's full implementation nor its native memory layout.
    struct wxPivotingDoorStateForAnalysis final
    {
        void* node = nullptr;                 // PC +18
        std::uint8_t autoDisable = 0;         // +125
        std::uint8_t enabled = 0, moving = 0; // +168/+169
        float angle = 0;                     // +16C
        std::uint32_t duration = 0, started = 0; // +170/+174
        float targetAngle = 0, initialAngle = 0; // +17C/+180
        std::uint8_t interactionBlocked = 0;  // +188
        std::uint32_t phase = 0;              // +18C
        std::uint8_t notifyPlayer = 0;        // +1A8
        std::array<char, 32> linkedNodeName{}; // +1A9..1C8 (native storage)
    };

    class wxAlfeaDoorTriggerHost
    {
    public:
        virtual ~wxAlfeaDoorTriggerHost() = default;
        // Original ctor leaves +1D0 untouched. The factory requires an explicit
        // allocation-storage word from the host; it is not a game default.
        virtual std::uint32_t InitialDoorIdStorageForAnalysis() = 0;
        virtual void ConstructPivotingDoorForAnalysis(wxAlfeaDoorTrigger&, wxPivotingDoorStateForAnalysis&) = 0;
        virtual void DestroyPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept = 0;
        virtual bool CopyPivotingDoorForAnalysis(const wxAlfeaDoorTrigger&, wxAlfeaDoorTrigger&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual bool CleanupPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept = 0;
        virtual bool UpdatePivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept = 0;
        virtual bool CanInteractPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept = 0;
        virtual void EnterPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept = 0;
        virtual void NotifyPivotingDoorForAnalysis(wxAlfeaDoorTrigger&, const wxAlfeaDoorMessageForAnalysis&) noexcept = 0;
        virtual void SubscribeForAnalysis(std::uint32_t, wxAlfeaDoorTrigger&) noexcept = 0;
        virtual void UnsubscribeForAnalysis(std::uint32_t, wxAlfeaDoorTrigger&) noexcept = 0;
        virtual std::uint32_t GetMillisecondsForAnalysis() noexcept = 0;
        virtual std::int32_t GetProgress514ForAnalysis() noexcept = 0;
        virtual bool GetGameFlagForAnalysis(std::uint32_t) noexcept = 0;
        virtual void* GetPlayerTransformForAnalysis() noexcept = 0;
        virtual std::array<float, 3> GetForwardForAnalysis(void* transform) noexcept = 0;
        virtual std::array<float, 3> GetPositionForAnalysis(void* transform) noexcept = 0;
        virtual std::array<float, 3> GetNodePositionForAnalysis(void* node) noexcept = 0;
        // Shared engine math, PC 41D2D0 and 4DAF00; not reimplemented per tool.
        virtual void NormalizeForAnalysis(std::array<float, 3>&) noexcept = 0;
        virtual double AngleBetweenForAnalysis(const std::array<float, 3>&, const std::array<float, 3>&) noexcept = 0;
        virtual void NotifyPlayerForAnalysis(wxAlfeaDoorTrigger&, std::uint32_t code,
            std::uint32_t value18, std::uint32_t value1C) noexcept = 0;
        virtual void* FindSceneNodeForAnalysis(const char*, bool, bool) noexcept = 0;
        virtual void EnableNodeComponentForAnalysis(void*, std::size_t index) noexcept = 0;
        virtual void SendGroupMessageForAnalysis(wxAlfeaDoorTrigger&, std::uint32_t code,
            std::uint32_t group, std::uint32_t id, std::uint8_t value) noexcept = 0;
        virtual void SetNodeYawForAnalysis(void*, float angle) noexcept = 0;
        virtual void MarkNodeDirtyForAnalysis(void*, std::uint32_t bits) noexcept = 0;
        virtual void RegisterEnumPropertyForAnalysis(const char* label, bool isDoorId,
            const char* const* choices, std::size_t count) = 0;
        virtual bool RegisterPivotingDoorPropertiesForAnalysis() = 0;
    };
}
