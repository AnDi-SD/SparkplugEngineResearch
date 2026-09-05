#pragma once

// Executable-backed state helpers for the normal (ShellFile) branch of
// spPS2FileStream.  This is analysis code, not a claim about an original
// header or about source-level names.  Platform I/O and the separate shared
// async-manager branch deliberately remain outside this model.

#include <cstdint>

namespace sparkplug::evidence::ps2
{
    struct spPS2SeekResult final
    {
        bool accepted;
        std::uint32_t absolutePosition;
    };

    // Replays the validation performed by sub_001DB7D0 before its buffer
    // selection/refill state machine.  Arithmetic is explicitly wrapping,
    // like the native 32-bit MIPS addu/subu instructions.
    [[nodiscard]] constexpr spPS2SeekResult ResolveNormalFileSeek(
        std::uint32_t currentPosition,
        std::uint32_t logicalOrigin,
        std::uint32_t physicalSize,
        std::uint32_t seekSource,
        std::int32_t offset) noexcept
    {
        const auto offsetBits = static_cast<std::uint32_t>(offset);

        switch (seekSource)
        {
        case 1: // essStart
        {
            const auto target = logicalOrigin + offsetBits;
            if (physicalSize != 0 && target > physicalSize)
            {
                return {false, currentPosition};
            }
            return {true, target};
        }

        case 2: // essEnd; native code performs no bounds check here
            return {true, physicalSize - offsetBits};

        case 4: // essCurrent
        {
            const auto target = currentPosition + offsetBits;
            if ((target & 0x80000000U) != 0U)
            {
                return {false, currentPosition};
            }
            if (physicalSize != 0 && target > physicalSize)
            {
                return {false, currentPosition};
            }
            return {true, target};
        }

        default:
            // The switch falls through to the buffer-maintenance code with
            // +0x34 unchanged and ultimately reports success.
            return {true, currentPosition};
        }
    }

    [[nodiscard]] constexpr bool ReadNormalFilePosition(
        bool isOpen,
        std::uint32_t absolutePosition,
        std::uint32_t logicalOrigin,
        std::uint32_t& result) noexcept
    {
        if (!isOpen)
        {
            result = 0;
            return false;
        }

        result = absolutePosition - logicalOrigin;
        return true;
    }

    [[nodiscard]] constexpr bool ReadNormalFileSize(
        bool isOpen,
        std::uint32_t physicalSize,
        std::uint32_t logicalSizeOverride,
        std::uint32_t& result) noexcept
    {
        if (!isOpen)
        {
            result = 0;
            return false;
        }

        result = logicalSizeOverride != 0
            ? logicalSizeOverride
            : physicalSize;
        return true;
    }
}
