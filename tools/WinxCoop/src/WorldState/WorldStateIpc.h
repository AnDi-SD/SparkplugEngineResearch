#pragma once
// WinxCoop-owned WorldState1, isolated from P/V3/IPC3 and DiagnosticInput5.
// Explicit little-endian Windows x86/x64 shared-memory ABI, no native pointers.
#include <cstddef>
#include <cstdint>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace winxcoop::world_state
{
    constexpr std::uint32_t IpcMagic = 0x31534357, IpcVersion = 1;
    constexpr std::uint32_t MaximumSnapshotBytes = 16384, MaximumBodies = 2, MaximumEntities = 64;
    constexpr std::uint32_t SharedProfileBytes = 411, EntryWitnessBytes = 64;
    constexpr std::uint32_t DefaultActorLeaseMilliseconds = 2000;
    enum class Role : std::uint32_t { Host = 1, Guest = 2 };
    enum class Phase : std::uint32_t { Waiting = 1, Connected = 2, Ready = 3, Closed = 4 };
    enum class Availability : std::uint32_t { Waiting = 0, Ready = 1, Retired = 2 };
    enum class Terminal : std::uint32_t
    { None = 0, Identity = 1, InvalidRecord = 2, Sequence = 3, Transport = 4,
      PeerClosed = 5, ActorClosed = 6, RunLimit = 7, Clock = 8, CounterExhausted = 9,
      ActorLeaseExpired = 10, NativeApply = 11 };

    struct alignas(8) Header final
    {
        std::uint32_t magic{}, version{}, bytes{}, maximumSnapshotBytes{};
        std::uint64_t nonce{};
        std::uint32_t ownerPid{}, role{};
        std::uint64_t ownerCreationFileTime{}, sessionId{}, scenarioId{};
        std::uint32_t levelEpoch{}, generation{};
    };
    // Bridge only writer. Ready means an initial complete snapshot has been
    // applied by the Guest native owner and its exact revision ACK observed.
    struct alignas(8) BridgeStatus final
    {
        std::uint32_t publication{}, phase{};        //00,04
        std::uint64_t connectionToken{};            //08
        std::uint32_t syncEpoch{}, terminalReason{};//10,14
        std::uint64_t sentRevision{}, appliedRevision{}, bridgeTickMilliseconds{}; //18,20,28
        std::uint64_t receivedRevision{}, reserved{}; //30,38
    };
    // Actor only writer. Availability is scene/canonical-world readiness,
    // independent of Input5 readiness and ordinary Diary/native pause.
    // Guest ACK is published only after a complete stopped native apply.
    struct alignas(8) ActorStatus final
    {
        std::uint32_t publication{}, availability{}; //00,04
        std::uint32_t participantPid{}, readinessRevision{}; //08,0C
        std::uint64_t participantCreationFileTime{}, observedAtMilliseconds{}; //10,18 LOCAL clock
        std::uint32_t appliedEpoch{}, terminalReason{}; //20,24
        std::uint64_t appliedRevision{}, reserved1{}, reserved2{}; //28,30,38
    };
    // Host Actor writes Host slot; Guest Bridge writes Guest slot. Payload is
    // the serialized Snapshot scalar ABI described in WorldStateProtocol.h.
    // It is never a memcpy of Snapshot/std::vector or a raw game object.
    struct alignas(8) SnapshotSlot final
    {
        std::uint32_t publication{}, bytes{};        //00,04
        std::uint64_t revision{}, nativeTick{};      //08,10
        std::uint32_t flags{}, reserved0{};          //18,1C
        std::uint64_t reserved[4]{};                 //20..3F
        std::uint8_t payload[MaximumSnapshotBytes]{};//40
    };
    struct alignas(8) Shared final
    { Header header; BridgeStatus bridge; ActorStatus actor; SnapshotSlot snapshot; };
    static_assert(sizeof(Header) == 64 && sizeof(BridgeStatus) == 64 && sizeof(ActorStatus) == 64);
    static_assert(sizeof(SnapshotSlot) == 16448 && sizeof(Shared) == 16640);
    static_assert(offsetof(Shared, bridge) == 64 && offsetof(Shared, actor) == 128 && offsetof(Shared, snapshot) == 192);
    static_assert(offsetof(Shared, snapshot) + offsetof(SnapshotSlot, payload) == 256);
    static_assert(offsetof(ActorStatus, appliedRevision) == 40 && offsetof(BridgeStatus, receivedRevision) == 48);
    static_assert(std::is_trivially_copyable_v<Shared> && alignof(Shared) == 8);
#ifdef _WIN32
    inline std::uint32_t Acquire(const std::uint32_t& v) noexcept
    { return static_cast<std::uint32_t>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(&v)), 0, 0)); }
    inline void Release(std::uint32_t& v, std::uint32_t value) noexcept
    { InterlockedExchange(reinterpret_cast<volatile LONG*>(&v), static_cast<LONG>(value)); }
    template<class T> bool ReadCoherent(const T& source, T& target) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        for(unsigned i=0; i<3; ++i)
        {
            const auto before=Acquire(source.publication); if(before&1u) continue;
            MemoryBarrier(); CopyMemory(&target,&source,sizeof(T)); MemoryBarrier();
            if(before==Acquire(source.publication)) return true;
        }
        return false;
    }
    template<class T> bool PublishCoherent(T& slot,const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto before=Acquire(slot.publication);
        if((before&1u)||before>0xFFFFFFFCu) return false;
        Release(slot.publication,before+1); MemoryBarrier();
        CopyMemory(reinterpret_cast<std::uint8_t*>(&slot)+4,reinterpret_cast<const std::uint8_t*>(&value)+4,sizeof(T)-4);
        MemoryBarrier(); Release(slot.publication,before+2); return true;
    }
#endif
}
