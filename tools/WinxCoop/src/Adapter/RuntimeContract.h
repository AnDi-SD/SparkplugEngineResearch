#pragma once
// WinxCoop's local IPC only. No native pointer, handle or C++ container crosses
// the process boundary. Both processes run on the same Windows computer.
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace winxcoop::adapter
{
    constexpr std::uint32_t IpcMagic = 0x50494357; // WCIP
    constexpr std::uint32_t IpcVersion = 3;
    enum class RuntimeMode : std::uint32_t { Observe, PassiveActor };
    enum class RuntimePhase : std::uint32_t { Empty, Attached, WaitingForLevel, Ready, Stopped, Failed };
    enum class RuntimeError : std::uint32_t { None, Identity, Image, Signature, Memory, Thread, NativeException, Ipc, Media, Command, Actor, Unload, Owner };
    enum class RuntimeOperation : std::uint32_t { None, Create, Apply, Remove };
    enum class RuntimeAnimation : std::uint32_t { Idle, Walk, Jump };
    enum class RuntimeAckStatus : std::uint32_t { Empty, Applied, Failed };
    struct Transform final
    {
        float position[3]{};
        float scale[3]{};
        float orientation[9]{};
    };
    struct RuntimeSnapshot final
    {
        RuntimePhase phase{};
        RuntimeError error{};
        std::uint32_t gamePid{}, mainThreadId{}, worldUpdates{}, levelId{};
        std::uint64_t monotonicMilliseconds{};
        Transform local{};
        std::uint32_t localComponentCount{}, remoteCreated{}, remoteRemoved{};
        // Input interpretation must be qualified before a producer sets this.
        std::uint32_t inputQualified{};
        std::int16_t inputX{}, inputZ{};
        std::uint32_t inputButtons{};
        RuntimeMode mode{};
        std::uint64_t session{};
        std::uint32_t levelEpoch{}, lastCommandSequence{}, remoteEntity{}, actorError{}, nativeCalls{};
        std::uint32_t unloadGuardHits{}, unloadCleanups{}, epochRetired{}, mediaQualified{};
        RuntimeAnimation localAnimation{};
        std::uint32_t localAnimationQualified{}, localFsmState{};
        Transform remoteDisplay{};
        std::uint32_t remoteDisplaySamples{}, remoteDisplayMaxGapMs{};
        float remoteDisplayMaxStep{};
        // Raw stock wxGameTimer observation. World updates alone do not prove
        // gameplay advances while the original window is out of focus.
        std::uint32_t gameTimerPaused{}, gameTimerActive{}, gameTimerCurrentRaw{}, gameTimerDeltaRaw{};
    };
    struct RuntimeCommand final
    {
        std::uint32_t bytes{}, version{};
        std::uint64_t session{};
        std::uint32_t levelEpoch{}, sequence{}, entity{};
        RuntimeOperation operation{};
        RuntimeAnimation animation{};
        std::uint32_t reserved{};
        Transform remote{};
    };
    struct RuntimeAck final
    {
        std::uint32_t bytes{}, version{};
        std::uint64_t session{};
        std::uint32_t levelEpoch{}, sequence{}, entity{};
        RuntimeOperation operation{};
        RuntimeAckStatus status{};
        RuntimeError error{};
        std::uint32_t worldUpdates{}, actorError{};
    };
    template<class T> struct alignas(8) Mailbox final
    {
        volatile LONG sequence{};
        LONG reserved{};
        T value{};
    };
    struct alignas(8) RuntimeShared final
    {
        std::uint32_t magic{}, version{}, bytes{}, launcherPid{};
        std::uint64_t nonce{};
        Mailbox<RuntimeSnapshot> snapshot;
        Mailbox<RuntimeCommand> command;
        Mailbox<RuntimeAck> acknowledgement;
    };
    struct RuntimeStart final
    {
        std::uint32_t bytes{}, version{}, gamePid{}, mainThreadId{};
        std::uint64_t nonce{};
        RuntimeMode mode{};
        std::uint32_t levelEpoch{};
        std::uint64_t session{};
        wchar_t mappingName[128]{};
        // PassiveActor only. Absolute ASCII drive path; pins the five qualified
        // Bloom assets before the first command. Observer leaves these zero.
        char mediaDirectory[MAX_PATH]{};
    };
    static_assert(sizeof(wchar_t) == 2, "Windows UTF16 ABI");
    static_assert(sizeof(Transform) == 60, "IPC transform ABI");
    static_assert(sizeof(RuntimeCommand) == 104, "IPC command ABI");
    static_assert(sizeof(RuntimeAck) == 48, "IPC acknowledgement ABI");
    static_assert(sizeof(RuntimeStart) == 560, "IPC start ABI");
    static_assert(sizeof(RuntimeSnapshot) == 264, "IPC snapshot ABI");
    static_assert(sizeof(RuntimeShared) == 464, "IPC shared ABI");
    static_assert(offsetof(RuntimeCommand, remote) == 40, "IPC command layout");
    static_assert(std::is_trivially_copyable_v<RuntimeShared>, "IPC has no process-local owners");

    // Command is stop-and-wait, not a latest-state mailbox: the host publishes
    // sequence 1, then waits for an identical session/epoch/sequence/entity/op
    // ACK before sequence+1. No wrap, skipped sequence, retry with changed bytes,
    // or second outstanding command is allowed. Runtime consumes one command
    // only at its original main-thread world boundary. ACK means that bounded
    // native operation completed; Apply admits a visual interpolation target.
    // It does not grant gameplay authority or acknowledge physical simulation.
    // Input is deliberately unqualified here. Future input delivery needs an
    // ordered native-observation FIFO, never this snapshot/command mailbox.

    // One writer per mailbox. A reader makes a bounded attempt and otherwise
    // retains its previous sample; the game thread never waits for the UI.
    template<class T> void Publish(Mailbox<T>& slot, const T& value) noexcept
    {
        InterlockedIncrement(&slot.sequence);
        CopyMemory(&slot.value, &value, sizeof(value));
        MemoryBarrier();
        InterlockedIncrement(&slot.sequence);
    }
    template<class T> bool TryRead(const Mailbox<T>& slot, T& result) noexcept
    {
        auto* sequence = const_cast<volatile LONG*>(&slot.sequence);
        for (int attempt = 0; attempt != 3; ++attempt)
        {
            const LONG before = InterlockedCompareExchange(sequence, 0, 0);
            if (before & 1) continue;
            T copy{};
            CopyMemory(&copy, &slot.value, sizeof(copy));
            MemoryBarrier();
            if (before == InterlockedCompareExchange(sequence, 0, 0)) { result = copy; return true; }
        }
        return false;
    }
}
