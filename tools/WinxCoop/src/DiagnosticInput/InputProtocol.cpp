#include "InputProtocol.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace winxcoop::diagnostic_input
{
    namespace
    {
        using sparkplug::reconstruction::spMemoryStream;
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        // The diagnostic is intentionally Windows x86/x64 little-endian. Write
        // each scalar through the common stream; never copy padded host structs
        // as a wire packet and never implement spNetworkPacket in another codec.
        template<class Stream, class Operation> bool Fields(Stream& stream, Message& m, std::uint32_t& version, Operation op)
        {
            std::uint32_t kind = static_cast<std::uint32_t>(m.kind);
            auto& r = m.input;
            const bool ok = op(stream, version) && op(stream, kind) &&
                op(stream, m.sessionId) && op(stream, m.scenarioId) && op(stream, m.connectionToken) &&
                op(stream, m.levelEpoch) && op(stream, m.bodyGeneration) && op(stream, m.inputEpoch) && op(stream, m.grant) &&
                op(stream, m.leaseDeadlineMilliseconds) && op(stream, m.maximumSourceSequence) &&
                op(stream, m.acknowledgedSourceSequence) && op(stream, m.flags) && op(stream, m.terminalReason) && op(stream, m.sourceRevision) &&
                op(stream, r.inputEpoch) && op(stream, r.sourceSequence) && op(stream, r.captureOrdinal) &&
                op(stream, r.captureMilliseconds) && op(stream, r.hostIngressMilliseconds) &&
                op(stream, r.magnitude) && op(stream, r.relativeDirection) && op(stream, r.referenceHeading) &&
                op(stream, r.buttons) && op(stream, r.grant) && op(stream, r.reserved) && op(stream, r.leaseDeadlineMilliseconds);
            m.kind = static_cast<Kind>(kind); return ok;
        }
        bool Empty(const InputRecord& r) noexcept
        {
            return r.inputEpoch == 0 && r.sourceSequence == 0 && r.captureOrdinal == 0 && r.captureMilliseconds == 0 &&
                r.hostIngressMilliseconds == 0 && r.magnitude == 0 && r.relativeDirection == 0 && r.referenceHeading == 0 &&
                r.buttons == 0 && r.grant == 0 && r.reserved == 0 && r.leaseDeadlineMilliseconds == 0;
        }
        bool Angle(float value) noexcept
        { return std::isfinite(value) && value >= -3.1415927410125732421875f && value <= 3.1415927410125732421875f; }
    }
    bool ValidInput(const InputRecord& r, const bool fromGuest) noexcept
    {
        return r.inputEpoch != 0 && r.sourceSequence != 0 && r.captureOrdinal != 0 && r.grant != 0 &&
            r.leaseDeadlineMilliseconds != 0 && r.reserved == 0 && (r.buttons & ~3u) == 0 &&
            std::isfinite(r.magnitude) && r.magnitude >= 0 && r.magnitude <= 1 &&
            Angle(r.relativeDirection) && Angle(r.referenceHeading) &&
            (!fromGuest || r.hostIngressMilliseconds == 0);
    }
    bool ValidMessage(const Message& m) noexcept
    {
        if(!m.sessionId || !m.scenarioId || !m.levelEpoch || !m.bodyGeneration || m.flags > NeutralRequired ||
            m.terminalReason > static_cast<std::uint32_t>(Terminal::NativeSource)) return false;
        if(m.kind == Kind::Hello)
            return !m.connectionToken && !m.inputEpoch && !m.grant && !m.leaseDeadlineMilliseconds &&
                !m.maximumSourceSequence && !m.acknowledgedSourceSequence && m.sourceRevision && !m.terminalReason && Empty(m.input);
        if(!m.connectionToken) return false;
        if(m.kind != Kind::SourceState && m.sourceRevision) return false;
        switch(m.kind)
        {
        case Kind::Grant:
            return m.inputEpoch && m.grant && m.leaseDeadlineMilliseconds &&
                m.maximumSourceSequence > m.acknowledgedSourceSequence &&
                m.maximumSourceSequence - m.acknowledgedSourceSequence <= MaximumCredits && !m.terminalReason && Empty(m.input);
        case Kind::Input:
            return ValidInput(m.input, true) && m.input.inputEpoch == m.inputEpoch && m.input.grant == m.grant &&
                m.input.leaseDeadlineMilliseconds == m.leaseDeadlineMilliseconds &&
                !m.maximumSourceSequence && !m.acknowledgedSourceSequence && !m.flags && !m.terminalReason;
        case Kind::Ack:
            return m.inputEpoch && m.acknowledgedSourceSequence && !m.grant && !m.leaseDeadlineMilliseconds &&
                !m.maximumSourceSequence && !m.flags && !m.terminalReason && Empty(m.input);
        case Kind::Pause:
            return !m.grant && !m.leaseDeadlineMilliseconds && !m.maximumSourceSequence &&
                !m.acknowledgedSourceSequence && m.flags == NeutralRequired && !m.terminalReason && Empty(m.input);
        case Kind::Stop:
            return m.terminalReason && !m.grant && !m.leaseDeadlineMilliseconds && !m.maximumSourceSequence &&
                !m.acknowledgedSourceSequence && !m.flags && Empty(m.input);
        case Kind::SourceState:
            return m.sourceRevision && !m.grant && !m.leaseDeadlineMilliseconds && !m.maximumSourceSequence &&
                !m.acknowledgedSourceSequence && !m.terminalReason && Empty(m.input);
        default: return false;
        }
    }
    std::optional<std::vector<std::uint8_t>> Encode(const Message& value)
    {
        if(!ValidMessage(value)) return std::nullopt;
        spMemoryStream stream;
        if(!stream.ResizeAndSetSize(WireBytes) || !stream.Reset()) return std::nullopt;
        Message copy = value; std::uint32_t version = ProtocolVersion;
        if(!Fields(stream, copy, version, [](auto& s, auto& field) { return s.Write(field); })) return std::nullopt;
        std::uint32_t bytes = 0;
        if(!stream.GetSize(&bytes) || bytes != WireBytes) return std::nullopt;
        const auto* data = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return std::vector<std::uint8_t>(data, data + bytes);
    }
    std::optional<Message> Decode(const networking::SessionMessage& value)
    {
        if(value.classId != ApplicationClass || value.payload.size() != WireBytes) return std::nullopt;
        spMemoryStream stream;
        if(!stream.ResizeAndSetSize(WireBytes)) return std::nullopt;
        std::memcpy(stream.GetBuffer(), value.payload.data(), WireBytes);
        Message result; std::uint32_t version = 0;
        if(!Fields(stream, result, version, [](auto& s, auto& field) { return s.Read(field); }) ||
            version != ProtocolVersion || value.key != static_cast<std::uint32_t>(result.kind) || !ValidMessage(result)) return std::nullopt;
        return result;
    }
}
