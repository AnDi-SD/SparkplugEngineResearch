#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "BootstrapProtocol.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <tuple>

namespace winxcoop::bootstrap
{
    namespace
    {
        using sparkplug::reconstruction::spMemoryStream;
        template<class Scalar, class Data>
        bool AssignmentFields(spMemoryStream& stream, Assignment& a, Scalar scalar, Data data)
        {
            return scalar(stream, a.sessionId) && scalar(stream, a.scenarioId) &&
                scalar(stream, a.inputToken) && scalar(stream, a.worldToken) &&
                scalar(stream, a.hostInputNonce) && scalar(stream, a.hostWorldNonce) &&
                scalar(stream, a.guestInputNonce) && scalar(stream, a.guestWorldNonce) &&
                scalar(stream, a.levelEpoch) && scalar(stream, a.generation) && scalar(stream, a.level) && scalar(stream, a.entry) &&
                scalar(stream, a.controlPort) && scalar(stream, a.inputPort) && scalar(stream, a.worldPort) &&
                scalar(stream, a.checkpointBytes) && data(stream, a.payloadDigest.data(), 32);
        }
        std::uint32_t Size(const Message& m) noexcept
        {
            switch (m.kind)
            {
            case Kind::Hello: return 40;
            case Kind::Offer: return 148;
            case Kind::Chunk: return ChunkHeaderBytes + static_cast<std::uint32_t>(m.chunk.size());
            case Kind::Commit: case Kind::BytesAck: return 52;
            case Kind::Stop: return 12;
            case Kind::OwnerReady: return 52;
            case Kind::OwnerNotice: return 40;
            default: return 0;
            }
        }
        template<class Scalar, class Data>
        bool Fields(spMemoryStream& stream, Message& m, std::uint32_t& version, Scalar scalar, Data data, bool reading)
        {
            auto kind = static_cast<std::uint32_t>(m.kind);
            if (!scalar(stream, version) || !scalar(stream, kind)) return false;
            m.kind = static_cast<Kind>(kind);
            switch (m.kind)
            {
            case Kind::Hello: return data(stream, m.profile.data(), 32);
            case Kind::Offer: return scalar(stream, m.transferId) && AssignmentFields(stream, m.assignment, scalar, data) && scalar(stream, m.totalBytes);
            case Kind::Chunk:
            {
                auto bytes = static_cast<std::uint32_t>(m.chunk.size());
                if (!scalar(stream, m.transferId) || !scalar(stream, m.offset) || !scalar(stream, m.totalBytes) ||
                    !scalar(stream, bytes) || bytes == 0 || bytes > MaximumChunkBytes) return false;
                if (reading) m.chunk.resize(bytes);
                return data(stream, m.chunk.data(), bytes);
            }
            case Kind::Commit: case Kind::BytesAck:
                return scalar(stream, m.transferId) && scalar(stream, m.totalBytes) && data(stream, m.digest.data(), 32);
            case Kind::Stop:
            {
                auto terminal = static_cast<std::uint32_t>(m.terminal);
                if (!scalar(stream, terminal)) return false;
                m.terminal = static_cast<Terminal>(terminal); return true;
            }
            case Kind::OwnerReady:
            {
                auto role = static_cast<std::uint32_t>(m.ready.role);
                if (!scalar(stream, role) || !scalar(stream, m.ready.fullSnapshotApplied) ||
                    !scalar(stream, m.ready.sessionId) || !scalar(stream, m.ready.levelEpoch) ||
                    !scalar(stream, m.ready.generation) || !scalar(stream, m.ready.freshNeutralInput) ||
                    !scalar(stream, m.ready.revision) || !scalar(stream, m.ready.nativeTick)) return false;
                m.ready.role = static_cast<Role>(role); return true;
            }
            case Kind::OwnerNotice:
            {
                auto role = static_cast<std::uint32_t>(m.notice.role), noticeKind = static_cast<std::uint32_t>(m.notice.kind);
                if (!scalar(stream, role) || !scalar(stream, noticeKind) || !scalar(stream, m.notice.sessionId) ||
                    !scalar(stream, m.notice.oldEpoch) || !scalar(stream, m.notice.oldGeneration) ||
                    !scalar(stream, m.notice.operationId)) return false;
                m.notice.role = static_cast<Role>(role); m.notice.kind = static_cast<NoticeKind>(noticeKind); return true;
            }
            default: return false;
            }
        }
    }

    bool operator==(const Assignment& a, const Assignment& b) noexcept
    {
        return std::tie(a.sessionId, a.scenarioId, a.inputToken, a.worldToken,
            a.hostInputNonce, a.hostWorldNonce, a.guestInputNonce, a.guestWorldNonce,
            a.levelEpoch, a.generation, a.level, a.entry, a.controlPort, a.inputPort, a.worldPort, a.checkpointBytes, a.payloadDigest) ==
            std::tie(b.sessionId, b.scenarioId, b.inputToken, b.worldToken,
            b.hostInputNonce, b.hostWorldNonce, b.guestInputNonce, b.guestWorldNonce,
            b.levelEpoch, b.generation, b.level, b.entry, b.controlPort, b.inputPort, b.worldPort, b.checkpointBytes, b.payloadDigest);
    }
    bool Empty(const Digest& d) noexcept { return std::all_of(d.begin(), d.end(), [](auto b) { return b == 0; }); }
    bool operator==(const ReadyReceipt& a, const ReadyReceipt& b) noexcept
    {
        return std::tie(a.role, a.fullSnapshotApplied, a.freshNeutralInput, a.sessionId, a.levelEpoch, a.generation, a.revision, a.nativeTick) ==
            std::tie(b.role, b.fullSnapshotApplied, b.freshNeutralInput, b.sessionId, b.levelEpoch, b.generation, b.revision, b.nativeTick);
    }
    bool ValidReady(const ReadyReceipt& r) noexcept
    {
        return (r.role == Role::Host || r.role == Role::Guest) && r.fullSnapshotApplied <= 1 &&
            (r.role != Role::Guest || r.fullSnapshotApplied == 1) && r.freshNeutralInput == 1 &&
            r.sessionId && r.levelEpoch && r.generation && r.revision && r.nativeTick;
    }
    bool operator==(const OwnerNotice& a, const OwnerNotice& b) noexcept
    {
        return std::tie(a.role, a.kind, a.sessionId, a.oldEpoch, a.oldGeneration, a.operationId) ==
            std::tie(b.role, b.kind, b.sessionId, b.oldEpoch, b.oldGeneration, b.operationId);
    }
    bool ValidNotice(const OwnerNotice& n) noexcept
    {
        return (((n.kind == NoticeKind::GuestPrepared || n.kind == NoticeKind::GuestLeaveRequested) && n.role == Role::Guest) ||
            ((n.kind == NoticeKind::HostRequestIssued || n.kind == NoticeKind::HostInputFenced || n.kind == NoticeKind::HostSessionEnded) && n.role == Role::Host)) &&
            n.sessionId && n.oldEpoch && n.oldGeneration && n.operationId;
    }
    bool ValidPayloadBytes(std::uint32_t bytes) noexcept
    { return bytes == SeedWitnessBytes || (bytes >= SeedWitnessBytes + 349 && bytes <= MaximumBootstrapBytes); }
    bool ValidAssignment(const Assignment& a) noexcept
    {
        if (!a.sessionId || !a.scenarioId || !a.inputToken || a.inputToken > UINT16_MAX || !a.worldToken ||
            a.inputToken == a.worldToken || !a.levelEpoch || !a.generation || a.level < 1 || a.level > 50 ||
            !a.controlPort || a.controlPort > UINT16_MAX || !a.inputPort || a.inputPort > UINT16_MAX ||
            !a.worldPort || a.worldPort > UINT16_MAX || a.controlPort == a.inputPort ||
            a.controlPort == a.worldPort || a.inputPort == a.worldPort ||
            (a.checkpointBytes && (a.checkpointBytes < 349 || a.checkpointBytes > MaximumCheckpointBytes)) || Empty(a.payloadDigest)) return false;
        const std::array<std::uint64_t, 4> nonces{{a.hostInputNonce, a.hostWorldNonce, a.guestInputNonce, a.guestWorldNonce}};
        for (std::size_t i = 0; i < nonces.size(); ++i)
        {
            if (!nonces[i]) return false;
            for (std::size_t j = 0; j < i; ++j) if (nonces[j] == nonces[i]) return false;
        }
        return true;
    }
    Digest ComputeDigest(const void* bytes, std::size_t count)
    {
        if ((!bytes && count) || count > MaximumBootstrapBytes) throw std::invalid_argument("Bounded bootstrap digest input required");
        std::vector<std::uint8_t> storage; // Must outlive BCryptDestroyHash.
        struct Hash final
        {
            BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
            ~Hash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
        } owner;
        if (BCryptOpenAlgorithmProvider(&owner.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("Bootstrap SHA256 provider unavailable");
        DWORD objectBytes = 0, received = 0;
        if (BCryptGetProperty(owner.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
            sizeof(objectBytes), &received, 0) < 0 || received != sizeof(objectBytes) || !objectBytes || objectBytes > 65536)
            throw std::runtime_error("Bootstrap SHA256 object size invalid");
        storage.resize(objectBytes);
        if (BCryptCreateHash(owner.algorithm, &owner.hash, storage.data(), objectBytes, nullptr, 0, 0) < 0 ||
            (count && BCryptHashData(owner.hash, static_cast<PUCHAR>(const_cast<void*>(bytes)), static_cast<ULONG>(count), 0) < 0))
            throw std::runtime_error("Bootstrap SHA256 hashing failed");
        Digest result{};
        if (BCryptFinishHash(owner.hash, result.data(), static_cast<ULONG>(result.size()), 0) < 0)
            throw std::runtime_error("Bootstrap SHA256 finish failed");
        return result;
    }
    bool ValidMessage(const Message& m) noexcept
    {
        const bool noAssignment = m.assignment == Assignment{};
        const bool noReady = m.ready == ReadyReceipt{};
        if (m.kind != Kind::OwnerReady && !noReady) return false;
        if (m.kind != Kind::OwnerNotice && !(m.notice == OwnerNotice{})) return false;
        switch (m.kind)
        {
        case Kind::Hello:
            return !Empty(m.profile) && !m.transferId && Empty(m.digest) && noAssignment && !m.offset && !m.totalBytes && m.chunk.empty() && m.terminal == Terminal::None;
        case Kind::Offer:
            return Empty(m.profile) && Empty(m.digest) && m.transferId && ValidAssignment(m.assignment) &&
                m.totalBytes == SeedWitnessBytes + m.assignment.checkpointBytes && !m.offset && m.chunk.empty() && m.terminal == Terminal::None;
        case Kind::Chunk:
            return Empty(m.profile) && Empty(m.digest) && noAssignment && m.transferId && ValidPayloadBytes(m.totalBytes) &&
                !m.chunk.empty() && m.chunk.size() <= MaximumChunkBytes && m.offset < m.totalBytes &&
                m.chunk.size() <= m.totalBytes - m.offset && m.terminal == Terminal::None;
        case Kind::Commit: case Kind::BytesAck:
            return Empty(m.profile) && !Empty(m.digest) && noAssignment && m.transferId && ValidPayloadBytes(m.totalBytes) &&
                !m.offset && m.chunk.empty() && m.terminal == Terminal::None;
        case Kind::Stop:
            return Empty(m.profile) && Empty(m.digest) && noAssignment && !m.transferId && !m.offset && !m.totalBytes &&
                m.chunk.empty() && m.terminal >= Terminal::LocalStop && m.terminal <= Terminal::Internal;
        case Kind::OwnerReady:
            return ValidReady(m.ready) && Empty(m.profile) && Empty(m.digest) && noAssignment &&
                !m.transferId && !m.offset && !m.totalBytes && m.chunk.empty() && m.terminal == Terminal::None;
        case Kind::OwnerNotice:
            return ValidNotice(m.notice) && Empty(m.profile) && Empty(m.digest) && noAssignment &&
                !m.transferId && !m.offset && !m.totalBytes && m.chunk.empty() && m.terminal == Terminal::None;
        default: return false;
        }
    }
    std::optional<std::vector<std::uint8_t>> Encode(const Message& value)
    {
        if (!ValidMessage(value)) return std::nullopt;
        const auto bytes = Size(value);
        if (!bytes || bytes > networking::TcpPacketFramer::MaximumPayloadSize) return std::nullopt;
        spMemoryStream stream;
        if (!stream.ResizeAndSetSize(bytes) || !stream.Reset()) return std::nullopt;
        Message copy = value; std::uint32_t version = ProtocolVersion;
        if (!Fields(stream, copy, version, [](auto& s, auto& v) { return s.Write(v); },
            [](auto& s, auto* data, std::uint32_t count) { return s.WriteData(data, count); }, false)) return std::nullopt;
        std::uint32_t actual = 0;
        if (!stream.GetSize(&actual) || actual != bytes) return std::nullopt;
        const auto* data = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return std::vector<std::uint8_t>(data, data + bytes);
    }
    std::optional<Message> Decode(const networking::SessionMessage& value)
    {
        if (value.classId != ApplicationClass || value.payload.size() < 12 ||
            value.payload.size() > networking::TcpPacketFramer::MaximumPayloadSize) return std::nullopt;
        spMemoryStream stream;
        if (!stream.ResizeAndSetSize(static_cast<std::uint32_t>(value.payload.size()))) return std::nullopt;
        std::memcpy(stream.GetBuffer(), value.payload.data(), value.payload.size());
        Message result; std::uint32_t version = 0, consumed = 0;
        if (!Fields(stream, result, version, [](auto& s, auto& v) { return s.Read(v); },
            [](auto& s, auto* data, std::uint32_t count) { return s.ReadData(data, count); }, true) ||
            version != ProtocolVersion || value.key != static_cast<std::uint32_t>(result.kind) ||
            !stream.GetCurrentPosition(consumed) || consumed != value.payload.size() || !ValidMessage(result)) return std::nullopt;
        return result;
    }
}
