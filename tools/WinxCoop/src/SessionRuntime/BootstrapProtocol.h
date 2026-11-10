#pragma once
// WinxCoop's bounded pre-entry byte transfer. Packet/MemoryStream remain the
// sole wire codec; these records never attest native game readiness.
#include "Networking/SessionCore.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace winxcoop::bootstrap
{
    constexpr std::uint32_t ApplicationClass = 0x31524257, ProtocolVersion = 1;
    constexpr std::uint32_t SeedWitnessBytes = 512, MaximumCheckpointBytes = 65536;
    constexpr std::uint32_t MaximumBootstrapBytes = SeedWitnessBytes + MaximumCheckpointBytes;
    constexpr std::uint32_t ChunkHeaderBytes = 28;
    constexpr std::uint32_t MaximumChunkBytes = networking::TcpPacketFramer::MaximumPayloadSize - ChunkHeaderBytes;
    static_assert(MaximumChunkBytes == 228);
    using Digest = std::array<std::uint8_t, 32>;
    enum class Role : std::uint32_t { Host = 1, Guest = 2 };
    enum class Kind : std::uint32_t { Hello = 1, Offer = 2, Chunk = 3, Commit = 4, BytesAck = 5, Stop = 6, OwnerReady = 7, OwnerNotice = 8 };
    enum class Terminal : std::uint32_t { None, LocalStop, Clock, Transport, Timeout, Profile,
        Protocol, Sequence, Backpressure, Digest, Argument, Internal };
    enum class Phase : std::uint32_t { Idle, Listening, Connecting, Handshake, ProfileMatched,
        Transferring, BootstrapReady, Running, Closed, Failed };
    struct Assignment final
    {
        std::uint64_t sessionId{}, scenarioId{}, inputToken{}, worldToken{};
        std::uint64_t hostInputNonce{}, hostWorldNonce{}, guestInputNonce{}, guestWorldNonce{};
        std::uint32_t levelEpoch{}, generation{}, level{}, entry{};
        std::uint32_t controlPort{}, inputPort{}, worldPort{}, checkpointBytes{};
        Digest payloadDigest{};
    };
    [[nodiscard]] bool operator==(const Assignment&, const Assignment&) noexcept;
    [[nodiscard]] bool Empty(const Digest&) noexcept;
    [[nodiscard]] bool ValidAssignment(const Assignment&) noexcept;
    [[nodiscard]] bool ValidPayloadBytes(std::uint32_t) noexcept;
    // Uses the OS SHA256 provider, not a second cryptographic implementation.
    [[nodiscard]] Digest ComputeDigest(const void*, std::size_t);
    struct ReadyReceipt final
    {
        Role role = static_cast<Role>(0);
        std::uint32_t fullSnapshotApplied{}, freshNeutralInput{};
        std::uint64_t sessionId{};
        std::uint32_t levelEpoch{}, generation{};
        std::uint64_t revision{}, nativeTick{};
    };
    [[nodiscard]] bool operator==(const ReadyReceipt&, const ReadyReceipt&) noexcept;
    [[nodiscard]] bool ValidReady(const ReadyReceipt&) noexcept;
    enum class NoticeKind : std::uint32_t { GuestPrepared = 1, HostRequestIssued = 2, HostInputFenced = 3,
        HostSessionEnded = 4, GuestLeaveRequested = 5 };
    struct OwnerNotice final
    {
        Role role = static_cast<Role>(0);
        NoticeKind kind = static_cast<NoticeKind>(0);
        std::uint64_t sessionId{};
        std::uint32_t oldEpoch{}, oldGeneration{};
        std::uint64_t operationId{};
    };
    [[nodiscard]] bool operator==(const OwnerNotice&, const OwnerNotice&) noexcept;
    [[nodiscard]] bool ValidNotice(const OwnerNotice&) noexcept;
    struct Message final
    {
        Kind kind = Kind::Hello;
        std::uint64_t transferId{};
        Digest profile{}, digest{};
        Assignment assignment;
        ReadyReceipt ready;
        OwnerNotice notice;
        std::uint32_t offset{}, totalBytes{};
        Terminal terminal = Terminal::None;
        std::vector<std::uint8_t> chunk;
    };
    [[nodiscard]] bool ValidMessage(const Message&) noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Encode(const Message&);
    [[nodiscard]] std::optional<Message> Decode(const networking::SessionMessage&);
}
