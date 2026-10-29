#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "RuntimeContract.h"
#include <memory>
#include <string>

namespace winxcoop::adapter
{
    enum class OwnedGamePolicy { DiagnosticProbe, GameHost };
    enum class GameHostPhase : std::uint32_t { Empty, Starting, Bootstrapping, Running, Stopping, Stopped, Failed };
    enum class GameHostStopReason : std::uint32_t { None, Requested, StopEvent, OwnerExit, TimeLimit, RuntimeRetired, GameExit, Error };
    constexpr std::uint32_t GameHostStatusMagic = 0x48474357; // WCGH
    constexpr std::uint32_t GameHostStatusVersion = 1;

    struct alignas(8) GameHostStatusSnapshot final
    {
        GameHostPhase phase{};
        std::uint32_t helperPid{}, gamePid{}, mainThreadId{}, bootstrapStage{}, error{};
        std::uint64_t monotonicMilliseconds{};
        std::uint32_t fullContextRestored{}, worldContextPreserved{}, runtimeReady{}, stopRequested{};
        std::uint32_t gameExitCodeKnown{}, gameExitCode{}, exitEventContinued{}, cleanupConfirmed{}, forcedTermination{};
        GameHostStopReason stopReason{};
        char errorText[192]{};
    };
    struct alignas(8) GameHostStatusShared final
    {
        std::uint32_t magic{}, version{}, bytes{}, ownerPid{};
        std::uint64_t nonce{}, ownerCreationTime{};
        Mailbox<GameHostStatusSnapshot> snapshot;
    };
    static_assert(sizeof(GameHostStatusSnapshot) == 264, "GameHost status snapshot ABI");
    static_assert(sizeof(GameHostStatusShared) == 304, "GameHost status mapping ABI");
    static_assert(offsetof(GameHostStatusShared, snapshot) == 32, "GameHost status layout");
    static_assert(std::is_trivially_copyable_v<GameHostStatusShared>, "GameHost status is pointer-free");

    struct ExternalRuntimeBinding final
    {
        // Owner initializes both headers and zeros every mailbox before launch.
        // Names are distinct Local\\ objects, 7..127 ASCII characters, using
        // letters/digits/dot/underscore/hyphen after the namespace prefix.
        // ownerCreationTime is the owner's GetProcessTimes FILETIME bit pattern.
        std::wstring mappingName, statusMappingName, stopEventName;
        std::uint64_t nonce{}, session{}, ownerCreationTime{};
        std::uint32_t ownerPid{}, levelEpoch{};
    };
    struct OwnedGameConfiguration final
    {
        std::wstring game, adapter, report, media, mode = L"observe";
        std::uint32_t seconds{};
        OwnedGamePolicy policy = OwnedGamePolicy::DiagnosticProbe;
        ExternalRuntimeBinding external;
    };
    struct OwnedGameResult final
    {
        bool successful{}, loadComplete{}, startComplete{}, contextRestored{}, worldContextPreserved{};
        bool cleanupConfirmed{}, exitCodeKnown{}, exitEventContinued{}, forcedTermination{}, limitReached{};
        std::uint32_t gamePid{}, mainThreadId{}, exitCode{};
        GameHostStopReason stopReason{};
        std::string error;
    };

    // One sequential owner thread. This engine creates exactly one child and
    // never attaches to an existing game. The GameHost policy only reads the
    // GUI's runtime mapping; the GUI remains its sole command writer. Registry
    // preparation belongs to the existing outer helper, outside this module.
    class OwnedGameProcess final
    {
    public:
        explicit OwnedGameProcess(OwnedGameConfiguration configuration);
        ~OwnedGameProcess();
        OwnedGameProcess(const OwnedGameProcess&) = delete;
        OwnedGameProcess& operator=(const OwnedGameProcess&) = delete;
        bool Start() noexcept;
        // A single bounded debug event, at most 50 ms. False requires Finish.
        bool Pump(std::uint32_t waitMilliseconds = 50) noexcept;
        // May be signaled from another thread; the owner performs all cleanup.
        void RequestStop() noexcept;
        OwnedGameResult Finish();
    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;
    };

    // Both entry points use this same owner/bootstrap implementation. Probe
    // keeps its existing .exe.args and command-line diagnostic contract.
    int RunAdapterProbeMain() noexcept;
    int RunGameHostMain() noexcept;
}
