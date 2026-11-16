#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc::challenge_params
{
    inline constexpr std::uint32_t ClassId = 0x347F5242;
    inline constexpr std::uint32_t PrimaryTable = 0x00701290;
    inline constexpr std::uint32_t RegistrationRecord = 0x00754BA8;
    inline constexpr std::uint32_t InteriorDefaultsHelper = 0x0057FC40;
    inline constexpr std::size_t InteriorOffset = 0x124;
    inline constexpr std::size_t ParameterBlockBytes = 76;
    inline constexpr std::size_t CapturedPrefixBytes = 0x170;
    inline constexpr std::array<std::uint32_t, 19> DefaultWords{120000U, 15000U, 16U, 0U, 20U, 10U, 40U, 75U, 10U, 20U, 40U, 60U, 100U, 150U, 200U, 250U, 300U, 350U, 10U};
    // Captured prefix368bytes is the minimum touched parent region. The old
    // factory allocation also measured368bytes; neither defines current sizeof.
}
