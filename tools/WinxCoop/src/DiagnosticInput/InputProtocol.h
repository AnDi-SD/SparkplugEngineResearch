#pragma once
// Our diagnostic application envelope. SessionCore remains the only original
// Packet/MemoryStream wire carrier; GameplayProtocol V3 is unchanged.
#include "InputIpc.h"
#include "Networking/SessionCore.h"
#include <optional>
#include <vector>

namespace winxcoop::diagnostic_input
{
    constexpr std::uint32_t ApplicationClass = 0x34494357, ProtocolVersion = 4;
    constexpr std::uint16_t WireBytes = 140;
    enum class Kind : std::uint32_t { Hello = 1, Grant = 2, Input = 3, Ack = 4, Pause = 5, Stop = 6, SourceState = 7 };
    constexpr std::uint32_t NeutralRequired = 1;
    struct Message final
    {
        Kind kind = Kind::Hello;
        std::uint64_t sessionId{}, scenarioId{}, connectionToken{};
        std::uint32_t levelEpoch{}, bodyGeneration{}, inputEpoch{}, grant{};
        std::uint64_t leaseDeadlineMilliseconds{};
        std::uint32_t maximumSourceSequence{}, acknowledgedSourceSequence{}, flags{}, terminalReason{};
        std::uint32_t sourceRevision{}; // Hello/SourceState only; flags bit0 = source Ready.
        InputRecord input;
    };
    [[nodiscard]] bool ValidInput(const InputRecord&, bool fromGuest) noexcept;
    [[nodiscard]] bool ValidMessage(const Message&) noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Encode(const Message&);
    [[nodiscard]] std::optional<Message> Decode(const networking::SessionMessage&);
}
