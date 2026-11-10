#pragma once
// WinxCoop-owned progression decisions. Native receipts are supplied by their
// trusted scene owner; this wire codec does not inspect or simulate a game.
#include "Networking/SessionCore.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace winxcoop::progression
{
    constexpr std::uint32_t ApplicationClass = 0x36504357, ProtocolVersion = 1;
    constexpr std::uint16_t WireBytes = 244;
    using Digest = std::array<std::uint8_t, 32>;
    enum class Role : std::uint32_t { Host = 1, Guest = 2 };
    enum class Kind : std::uint32_t { Prepare = 1, Retired = 2, Load = 3, Loaded = 4, Resume = 5, Abort = 6 };
    enum class Reason : std::uint32_t { LevelChange = 1, Reload = 2, Respawn = 3, Checkpoint = 4, Cutscene = 5 };
    enum class Terminal : std::uint32_t
    { None = 0, InvalidRecord = 1, Identity = 2, Authority = 3, Sequence = 4,
      NativeReceipt = 5, Timeout = 6, Clock = 7, Backpressure = 8, CounterExhausted = 9, Disconnected = 10 };

    struct SceneIdentity final { std::uint32_t levelEpoch{}, generation{}; };
    struct LevelDescriptor final { std::uint32_t level{}, entry{}; Digest digest{}; };
    struct Decision final
    {
        std::uint64_t operation{};
        SceneIdentity previous, next;
        Reason reason = Reason::LevelChange;
        LevelDescriptor destination;
    };
    // Tokens and opaque mapping nonces, never game pointers. A new native scene
    // owns fresh endpoints. Input's token may equal its Host mapping nonce.
    struct ChannelGeneration final
    {
        std::uint64_t sessionId{}, inputToken{}, worldToken{};
        std::uint64_t hostInputNonce{}, hostWorldNonce{}, guestInputNonce{}, guestWorldNonce{};
    };
    struct ConnectionIdentity final { std::uint64_t sessionId{}, scenarioId{}, token{}; };
    constexpr std::uint32_t CompleteRetirementMask = 0x1FF;
    struct Message final
    {
        Kind kind = Kind::Prepare;
        ConnectionIdentity connection;
        Decision decision;
        ChannelGeneration channels;
        Digest seed{}, witness{};
        std::uint64_t appliedRevision{}, nativeTick{};
        std::uint32_t retirementMask{};
        Terminal terminal = Terminal::None;
    };
    [[nodiscard]] bool operator==(const SceneIdentity&, const SceneIdentity&) noexcept;
    [[nodiscard]] bool operator==(const LevelDescriptor&, const LevelDescriptor&) noexcept;
    [[nodiscard]] bool operator==(const Decision&, const Decision&) noexcept;
    [[nodiscard]] bool operator==(const ChannelGeneration&, const ChannelGeneration&) noexcept;
    [[nodiscard]] bool operator==(const ConnectionIdentity&, const ConnectionIdentity&) noexcept;
    [[nodiscard]] bool operator==(const Message&, const Message&) noexcept;
    [[nodiscard]] bool ValidScene(SceneIdentity) noexcept;
    [[nodiscard]] bool ValidDescriptor(const LevelDescriptor&) noexcept;
    [[nodiscard]] bool ValidChannels(const ChannelGeneration&) noexcept;
    [[nodiscard]] bool ValidDecision(const Decision&) noexcept;
    [[nodiscard]] bool ValidMessage(const Message&) noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Encode(const Message&);
    [[nodiscard]] std::optional<Message> Decode(const networking::SessionMessage&);
}
