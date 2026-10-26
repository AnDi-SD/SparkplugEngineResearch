#pragma once

// WinxCoop transport adaptation. This persistent framing state is our code;
// the original Server Worker resets its receive buffer between iterations.
#include "Code/Sparkplug/spNetworkPacket.h"
#include "Analysis/PC/spNetworkPacketAbi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace winxcoop::networking
{
    enum class StreamState { Receiving, Ended, Failed };
    enum class FramingError
    {
        None,
        OversizePayload,
        TruncatedFrame,
        TransportFailure,
        CodecFailure,
        ResourceFailure,
    };

    struct StreamStatus
    {
        StreamState state;
        FramingError error;
    };

    struct FeedResult
    {
        std::size_t consumed = 0;
        std::unique_ptr<sparkplug::reconstruction::spNetworkPacket> packet;
        StreamState state = StreamState::Receiving;
        FramingError error = FramingError::None;
    };

    class TcpPacketFramer final
    {
    public:
        static constexpr std::size_t HeaderSize =
            sparkplug::reconstruction::pc::spNetworkPacketAbi::WireHeaderSize;
        static constexpr std::size_t MaximumPayloadSize =
            sparkplug::reconstruction::spNetworkPacket::PayloadCapacity;
        static constexpr std::size_t MaximumFrameSize = HeaderSize + MaximumPayloadSize;

        // One instance belongs to one connection. There is no internal packet
        // queue or callback: Feed yields at most one complete packet, and the
        // caller retains/retries the input after result.consumed itself.
        [[nodiscard]] FeedResult Feed(const std::uint8_t* data, std::size_t size);
        [[nodiscard]] StreamStatus EndOfInput() noexcept;
        [[nodiscard]] StreamStatus FailTransport() noexcept;

        // Explicitly discards the old connection's incomplete frame. Returned
        // packets already belong to the caller and survive Reset/destruction.
        void Reset() noexcept;
        [[nodiscard]] StreamState GetState() const noexcept { return state_; }
        [[nodiscard]] FramingError GetError() const noexcept { return error_; }
        [[nodiscard]] std::size_t PendingByteCount() const noexcept { return buffered_; }
        [[nodiscard]] static constexpr std::size_t PendingCapacity() noexcept { return MaximumFrameSize; }

    private:
        void Fail(FramingError) noexcept;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spNetworkPacket> Decode();

        // Only a length prefix is inspected here. Actual header/payload decode
        // is performed by the shared recovered Packet/MemoryStream component.
        std::array<std::uint8_t, MaximumFrameSize> bytes_{};
        std::size_t buffered_ = 0;
        std::size_t expected_ = HeaderSize;
        StreamState state_ = StreamState::Receiving;
        FramingError error_ = FramingError::None;
    };
}
