#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace winx::reconstruction
{
    using MovingStatePrefixForAnalysis = std::array<std::uint8_t, 0x40>;
    using MovingStateAirEventNameForAnalysis = std::array<std::uint8_t, 16>;

    struct MovingStateAirEventBytesForAnalysis
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
    };

    struct MovingStateAirEventCaptureForAnalysis
    {
        MovingStateAirEventBytesForAnalysis argument;
        std::uint32_t tagAddress;
        MovingStateAirEventBytesForAnalysis tag;
        std::uint32_t nameAddress;
        MovingStateAirEventBytesForAnalysis name;
    };

    namespace moving_state_air_event_operations
    {
        [[nodiscard]] inline std::uint32_t ReadAddress(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }

        template<std::size_t StateByteOffset>
        [[nodiscard]] bool TryApply(MovingStatePrefixForAnalysis& state,
            std::uint32_t primary, const MovingStateAirEventCaptureForAnalysis& event) noexcept
        {
            static_assert(StateByteOffset < MovingStatePrefixForAnalysis{}.size());
            if (ReadAddress(state.data()) != primary || event.tagAddress == 0 || event.nameAddress == 0
                || event.argument.size < 0x20 || event.tag.size < 0x14 || event.name.size < 16
                || event.argument.data == nullptr || event.tag.data == nullptr || event.name.data == nullptr
                || ReadAddress(event.argument.data + 0x1C) != event.tagAddress
                || ReadAddress(event.tag.data + 0x10) != event.nameAddress)
                return false;

            constexpr char Begin[] = "event_air_begin";
            constexpr char End[] = "event_air_end";
            static_assert(sizeof(Begin) == 16 && sizeof(End) == 14);
            if (std::memcmp(event.name.data, Begin, sizeof(Begin)) == 0)
                state[StateByteOffset] = 1;
            else if (std::memcmp(event.name.data, End, sizeof(End)) == 0)
                state[StateByteOffset] = 0;
            return true;
        }
    }
}
