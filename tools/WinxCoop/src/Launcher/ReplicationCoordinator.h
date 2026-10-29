#pragma once
#include "PrototypeProfile.h"
#include "GameProcessClient.h"
#include "../Networking/WindowsTcpTransport.h"
#include <cstdint>
#include <memory>
#include <string>

namespace winxcoop::launcher
{
    struct ReplicationMetrics final
    {
        std::uint64_t gameStarts{}, gameReady{}, nativeFrames{}, sampledStates{}, deferredSamples{}, invalidSamples{};
        std::uint64_t messagesSent{}, messagesReceived{}, wrongConnections{}, staleMessages{}, protocolFailures{};
        std::uint64_t observationsSent{}, observationsReceived{}, observationsAccepted{}, observationsRejected{}, acknowledgementsReceived{};
        std::uint64_t remoteStates{}, visualStatesCoalesced{}, nativeCreatesAcknowledged{}, nativeAppliesAcknowledged{}, nativeRemovesAcknowledged{};
        std::uint64_t nativeCommands{}, nativeAcknowledgements{}, nativeAckLatencyTotalMs{}, nativeAckLatencyMaxMs{};
        std::uint64_t observationRttSamples{}, observationRttTotalMs{}, observationRttMaxMs{}, gameCompletions{}, gameSuccessfulCompletions{};
        std::uint64_t gameNativeCleanupCompletions{}, gameNoOuterLaunchCompletions{};
        std::uint64_t nativeAnimationSamples{}, derivedAnimationSamples{}, nativeRemoteCreated{}, nativeRemoteRemoved{};
        std::uint64_t remoteDisplaySamples{}, remoteDisplayMaxGapMs{};
        std::uint64_t cooperativeStarts{}, cooperativeRunningMs{};
        std::uint64_t gameSoftLimitStops{}, gameSoftLimitFallbackStops{};
        std::uint32_t lastGameHelperLimitSeconds{}, lastGameSoftLimitSeconds{};
        float remoteDisplayMaxStep{};
    };
    // Single GUI-thread owner for carrier application messages, V3 decisions,
    // stock local pose observation and the facade's immutable ACK command driver.
    // Network generations never replace the private per-game native generation.
    class ReplicationCoordinator final
    {
    public:
        explicit ReplicationCoordinator(PrototypeProfile);
        ~ReplicationCoordinator();
        ReplicationCoordinator(const ReplicationCoordinator&) = delete;
        ReplicationCoordinator& operator=(const ReplicationCoordinator&) = delete;
        bool Attach(networking::WindowsTcpTransport&, networking::SessionRole);
        // Call before releasing the transport. Host game may remain running;
        // guest retires its native generation after removing the old visual.
        void Detach() noexcept;
        // Per-game duration contract: helper hard bound=max(30, profile.seconds)
        // (profile is5..3600); soft run=min(profile.seconds, hard-15), reserving
        // at least15 seconds. Examples:5=>soft5/hard30;29=>15/30;900=>885/900;
        // 3600=>3585/3600. Soft run never extends the requested user bound.
        // The timer starts conservatively
        // before facade Start, includes bootstrap and never resets on reconnect.
        // Retirement closes only the peer, waits for the existing immutable ACK
        // then Remove ACK and requests owned stop. The host lobby stays reusable
        // after actual final cleanup. Missing ACK/unload remains unqualified.
        bool StartHostGame(std::uint64_t nowMilliseconds);
        // Transport::Pump runs first on the same UI thread.
        void Pump(std::uint64_t nowMilliseconds) noexcept;
        void RequestShutdown() noexcept;
        bool Finished() const noexcept;
        bool CanStartHostGame() const noexcept;
        bool GameActive() const noexcept;
        bool CanAttach(networking::SessionRole) const noexcept;
        const std::wstring& Status() const noexcept;
        const std::string& Failure() const noexcept;
        const ReplicationMetrics& Metrics() const noexcept;
        const GameProcessClientResult& LastGameResult() const noexcept;
        // Actual final facade report, including rollback/forced-exit evidence.
        // Cleanup is confirmed only after every owned game has a final report.
        bool CleanupConfirmed() const noexcept;
        bool HasGameResult() const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;
    };
}
