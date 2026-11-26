#pragma once

#include "Analysis/PC/wxAnimationControllerAbi.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::reconstruction::pc_animation_controller_marks
{
    using CapturedPrefix = std::array<std::uint8_t, sizeof(evidence::pc::wxAnimationControllerLayout)>;

    inline std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    // The caller supplies a typed PC capture. The handle remains a raw word.
    inline std::uint8_t Consume(CapturedPrefix& controller, std::uint32_t handle) noexcept
    {
        if (handle == 0)
            return 1;

        auto* const mark3 = controller.data() + offsetof(evidence::pc::wxAnimationControllerLayout, mark3);
        auto* const mark9 = controller.data() + offsetof(evidence::pc::wxAnimationControllerLayout, mark9);
        const bool matchesMark3 = ReadWord(mark3) == handle;
        const bool matchesMark9 = ReadWord(mark9) == handle;
        if (matchesMark3)
            std::fill_n(mark3, sizeof(std::uint32_t), std::uint8_t{0});
        if (matchesMark9)
            std::fill_n(mark9, sizeof(std::uint32_t), std::uint8_t{0});
        return static_cast<std::uint8_t>(matchesMark3 || matchesMark9);
    }
}
