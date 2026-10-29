#pragma once
#include "../Adapter/OwnedGameProcess.h"
#include <cstdint>
#include <memory>
#include <string>

namespace winxcoop::launcher
{
    enum class GameProcessClientPhase { Idle, Starting, WaitingForLevel, Ready, Stopping, Closed, Failed };
    struct GameProcessClientConfiguration final
    {
        std::wstring game, media, adapter, gameHost, outer, caseDirectory;
        std::uint64_t session{};
        std::uint32_t levelEpoch{}, seconds{};
    };
    struct GameProcessClientIdentity final
    {
        std::uint32_t ownerPid{}, outerPid{}, helperPid{}, gamePid{}, mainThreadId{};
        std::uint64_t ownerCreationTime{}, nonce{}, session{};
        std::uint32_t levelEpoch{};
    };
    struct GameProcessClientResult final
    {
        bool completed{}, successful{}, outerStarted{}, outerExited{}, outerExitCodeKnown{};
        std::uint32_t outerExitCode{};
        bool outerReportValid{}, outerProbeCompleted{}, mediaRestoreAttempted{}, mediaRestoreVerified{};
        bool mediaLeaseAcquired{}, mediaLeaseInherited{}, mediaLeaseReleased{};
        bool outerChildStopped{}, outerChildTimedOut{}, outerTerminatedChild{};
        bool helperExitCodeKnown{}, helperLimitReached{}, commandUnconfirmed{};
        std::uint32_t helperExitCode{};
        bool helperReportValid{}, helperCompleted{}, gameExitCodeKnown{}, gameExitEventContinued{}, gameCleanupConfirmed{}, gameForcedTermination{};
        std::uint32_t gameExitCode{}, commandsPublished{}, commandsAcknowledged{};
        adapter::GameHostStopReason stopReason{};
        std::string error;
    };

    // GUI-side ownership only: one immutable generation, one command writer.
    // No native bootstrap, game attachment or registry preparation is present.
    // Call Start/Pump/Submit on one GUI thread. Pump performs no waits and never
    // reads a report until the held outer process has exited. Keep this object
    // alive and pump until Finished before treating rollback as confirmed.
    class GameProcessClient final
    {
    public:
        explicit GameProcessClient(GameProcessClientConfiguration configuration);
        ~GameProcessClient();
        GameProcessClient(const GameProcessClient&) = delete;
        GameProcessClient& operator=(const GameProcessClient&) = delete;
        bool Start() noexcept;
        void Pump() noexcept;
        bool Submit(adapter::RuntimeOperation operation, std::uint32_t entity,
            const adapter::Transform& transform = {}, adapter::RuntimeAnimation animation = adapter::RuntimeAnimation::Idle) noexcept;
        void RequestStop() noexcept;
        GameProcessClientPhase Phase() const noexcept;
        bool Ready() const noexcept;
        bool HasRuntimeSnapshot() const noexcept;
        bool CommandPending() const noexcept;
        bool Finished() const noexcept;
        const adapter::RuntimeSnapshot& Snapshot() const noexcept;
        const adapter::RuntimeAck& Acknowledgement() const noexcept;
        const adapter::GameHostStatusSnapshot& HelperStatus() const noexcept;
        const GameProcessClientIdentity& Identity() const noexcept;
        const GameProcessClientResult& Result() const noexcept;
        const std::string& Failure() const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;
    };
}
