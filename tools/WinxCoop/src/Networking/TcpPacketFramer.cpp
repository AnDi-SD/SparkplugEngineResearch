#include "TcpPacketFramer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>

namespace winxcoop::networking
{
    void TcpPacketFramer::Fail(const FramingError error) noexcept
    {
        if(state_ == StreamState::Receiving)
        {
            state_ = StreamState::Failed;
            error_ = error;
        }
    }

    std::unique_ptr<sparkplug::reconstruction::spNetworkPacket> TcpPacketFramer::Decode()
    {
        using namespace sparkplug::reconstruction;
        try
        {
            spMemoryStream stream;
            // ResizeAndSetSize retains the native unchecked allocation result;
            // the explicit buffer check is our resource-failure boundary.
            if(!stream.ResizeAndSetSize(static_cast<std::uint32_t>(expected_)) || !stream.GetBuffer())
            {
                Fail(FramingError::ResourceFailure);
                return nullptr;
            }
            std::memcpy(stream.GetBuffer(), bytes_.data(), expected_);
            auto packet = std::make_unique<spNetworkPacket>(nullptr);
            if(!packet->ReadForAnalysis(stream))
            {
                Fail(FramingError::CodecFailure);
                return nullptr;
            }
            std::uint32_t position = 0;
            if(!stream.GetCurrentPosition(position) || position != expected_)
            {
                Fail(FramingError::CodecFailure);
                return nullptr;
            }
            return packet;
        }
        catch(const std::bad_alloc&)
        {
            Fail(FramingError::ResourceFailure);
        }
        catch(const std::exception&)
        {
            Fail(FramingError::CodecFailure);
        }
        return nullptr;
    }

    FeedResult TcpPacketFramer::Feed(const std::uint8_t* data, const std::size_t size)
    {
        if(size && !data)
            throw std::invalid_argument("TCP framing requires data for a nonzero input size");
        FeedResult result;
        while(state_ == StreamState::Receiving && result.consumed < size)
        {
            const auto amount = std::min(expected_ - buffered_, size - result.consumed);
            std::memcpy(bytes_.data() + buffered_, data + result.consumed, amount);
            buffered_ += amount;
            result.consumed += amount;
            if(buffered_ < expected_)
                break;
            if(expected_ == HeaderSize)
            {
                // The wire's final header scalar is uint16 little endian.
                // Reject its size before passing the frame to native-style
                // Packet Read, which does not provide the original bound.
                const auto payloadSize = std::size_t(bytes_[HeaderSize - 2])
                    | (std::size_t(bytes_[HeaderSize - 1]) << 8);
                if(payloadSize > MaximumPayloadSize)
                {
                    Fail(FramingError::OversizePayload);
                    break;
                }
                expected_ = HeaderSize + payloadSize;
                if(buffered_ < expected_)
                    continue;
            }
            result.packet = Decode();
            if(result.packet)
            {
                buffered_ = 0;
                expected_ = HeaderSize;
            }
            break;
        }
        result.state = state_;
        result.error = error_;
        return result;
    }

    StreamStatus TcpPacketFramer::EndOfInput() noexcept
    {
        if(state_ == StreamState::Receiving)
        {
            if(buffered_)
                Fail(FramingError::TruncatedFrame);
            else
                state_ = StreamState::Ended;
        }
        return {state_, error_};
    }

    StreamStatus TcpPacketFramer::FailTransport() noexcept
    {
        Fail(FramingError::TransportFailure);
        return {state_, error_};
    }

    void TcpPacketFramer::Reset() noexcept
    {
        buffered_ = 0;
        expected_ = HeaderSize;
        state_ = StreamState::Receiving;
        error_ = FramingError::None;
    }
}
