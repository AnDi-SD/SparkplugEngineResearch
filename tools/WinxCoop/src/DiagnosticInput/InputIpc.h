#pragma once
// Our isolated diagnostic IPC5. This does not alter RuntimeContract IPC3.
// Windows little-endian x86/x64; no pointers, handles or C++ owners cross IPC.
#include <cstddef>
#include <cstdint>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace winxcoop::diagnostic_input
{
    constexpr std::uint32_t IpcMagic = 0x35494357, IpcVersion = 5, RingCapacity = 64;
    constexpr std::uint32_t MaximumCredits = 8, LeaseMilliseconds = 500, KeepaliveMilliseconds = 100;
    constexpr std::uint32_t MinimumGrantRenewalMilliseconds = 20, MaximumGrantHistory = 32;
    enum class Role : std::uint32_t { Host = 1, Guest = 2 };
    enum class BridgeState : std::uint32_t { Waiting = 1, Connected = 2, Ready = 3, Paused = 4, Closed = 5 };
    enum class Availability : std::uint32_t { Waiting = 0, Ready = 1, Paused = 2, Retired = 3 };
    enum class Terminal : std::uint32_t
    {
        None = 0, Identity = 1, InvalidRecord = 2, Sequence = 3, Overflow = 4,
        Transport = 5, PeerClosed = 6, ActorClosed = 7, RunLimit = 8,
        Clock = 9, CounterExhausted = 10, LeaseExpired = 11, NativeSource = 12
    };

    struct alignas(8) Header final
    {
        std::uint32_t magic{}, version{}, bytes{}, capacity{}; // 00..0C
        std::uint64_t nonce{};                              // 10
        std::uint32_t ownerPid{}, role{};                    // 18,1C
        std::uint64_t ownerCreationFileTime{};               // 20
        std::uint64_t sessionId{}, scenarioId{};             // 28,30
        std::uint32_t levelEpoch{}, bodyGeneration{};         // 38,3C
    };
    // Bridge is the sole writer. Publish odd/even sequence around bytes04..3F.
    // connectionToken is host-issued and identical at both endpoints; local
    // transport connectionId is deliberately not an IPC/native body identity.
    struct alignas(8) BridgeStatus final
    {
        std::uint32_t publication{}, state{};                // 00,04
        std::uint64_t connectionToken{};                     // 08
        std::uint32_t inputEpoch{}, grant{};                 // 10,14
        std::uint64_t leaseDeadlineMilliseconds{};            // 18 (HOST clock; opaque at Guest)
        std::uint32_t maximumSourceSequence{}, acknowledgedSourceSequence{}; //20,24
        std::uint32_t neutralRequired{}, terminalReason{};    //28,2C
        std::uint64_t bridgeTickMilliseconds{}, reserved{};   //30,38
    };
    // ActorProbe is the sole writer. Set participant identity once, then keep
    // it unchanged. readinessRevision increments on every readiness transition
    // or local timeout re-arm request. Do NOT bump it merely because bridge
    // inputEpoch changed: the controller's internal epoch-neutral fence must
    // not feed back as another native readiness transition. appliedEpoch/source ACK is independent
    // of the ring read cursor and is published ONLY after native WorldRET apply
    // (or deliberate suppression); never acknowledge merely peeking a slot.
    struct alignas(8) ActorStatus final
    {
        std::uint32_t publication{}, availability{};          //00,04
        std::uint64_t participantCreationFileTime{};          //08
        std::uint32_t participantPid{}, appliedEpoch{};       //10,14
        std::uint32_t appliedSourceSequence{}, readinessRevision{}; //18,1C
        std::uint64_t observedAtMilliseconds{};               //20 (LOCAL clock)
        std::uint32_t terminalReason{}, reserved0{};          //28,2C
        std::uint64_t reserved1{}, reserved2{};               //30,38
    };
    struct alignas(8) InputRecord final
    {
        std::uint32_t inputEpoch{}, sourceSequence{};         //00,04 (sequence resets to1 per epoch)
        std::uint64_t captureOrdinal{}, captureMilliseconds{}; //08,10 (source clock diagnostic only)
        std::uint64_t hostIngressMilliseconds{};              //18 (Guest MUST publish0)
        float magnitude{}, relativeDirection{}, referenceHeading{}; //20,24,28
        std::uint32_t buttons{};                             //2C bit0=jump-held, bit1=ordinary-attack-held; other bits0
        std::uint32_t grant{}, reserved{};                    //30,34
        std::uint64_t leaseDeadlineMilliseconds{};            //38 echoed HOST deadline; never translate
    };
    struct alignas(8) InputRing final
    {
        // Sole producer writes writeCursor and producerTerminal. Sole consumer
        // writes readCursor. Counters never wrap; full/invalid means terminal
        // fence, never overwrite. A consumer commits only after next-owner
        // acceptance. Guest producer=ActorProbe; Host producer=InputBridge.
        std::uint32_t writeCursor{}, readCursor{}, producerTerminal{}, reserved{};
        std::uint64_t reservedHeader[6]{};
        InputRecord records[RingCapacity]{};                 //40
    };
    struct alignas(8) Shared final
    {
        Header header;             //0000
        BridgeStatus bridge;       //0040
        ActorStatus actor;         //0080
        InputRing input;           //00C0
    };
    static_assert(sizeof(Header) == 64 && sizeof(BridgeStatus) == 64 && sizeof(ActorStatus) == 64);
    static_assert(sizeof(InputRecord) == 64 && sizeof(InputRing) == 4160 && sizeof(Shared) == 4352);
    static_assert(offsetof(Shared, bridge) == 64 && offsetof(Shared, actor) == 128 && offsetof(Shared, input) == 192);
    static_assert(offsetof(InputRecord, hostIngressMilliseconds) == 24 && offsetof(InputRecord, magnitude) == 32);
    static_assert(offsetof(InputRecord, grant) == 48 && offsetof(InputRecord, leaseDeadlineMilliseconds) == 56);
    static_assert(std::is_trivially_copyable_v<Shared> && alignof(Shared) == 8);

#ifdef _WIN32
    inline std::uint32_t Acquire(const std::uint32_t& word) noexcept
    { return static_cast<std::uint32_t>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(&word)), 0, 0)); }
    inline void Release(std::uint32_t& word, std::uint32_t value) noexcept
    { InterlockedExchange(reinterpret_cast<volatile LONG*>(&word), static_cast<LONG>(value)); }
    inline void Latch(std::uint32_t& word, Terminal reason) noexcept
    { InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&word), static_cast<LONG>(reason), 0); }
    template<class T> bool ReadStatus(const T& slot, T& copy) noexcept
    {
        static_assert(sizeof(T) == 64);
        for(unsigned attempt = 0; attempt < 3; ++attempt)
        {
            const auto before = Acquire(slot.publication); if(before & 1) continue;
            MemoryBarrier(); CopyMemory(&copy, &slot, sizeof(T)); MemoryBarrier();
            if(before == Acquire(slot.publication)) return true;
        }
        return false;
    }
    template<class T> bool PublishStatus(T& slot, T value) noexcept
    {
        const auto before = Acquire(slot.publication);
        if((before & 1) || before > 0xFFFFFFFCu) return false;
        Release(slot.publication, before + 1); MemoryBarrier();
        CopyMemory(reinterpret_cast<unsigned char*>(&slot) + 4, reinterpret_cast<const unsigned char*>(&value) + 4, sizeof(T) - 4);
        MemoryBarrier(); Release(slot.publication, before + 2); return true;
    }
#endif
}
