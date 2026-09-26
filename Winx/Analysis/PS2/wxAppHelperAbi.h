#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace winx::analysis::ps2
{
    struct wxAppHelperAbi
    {
        std::array<std::byte, 0x10> base;
        std::uint32_t singletonVtable;
        std::array<std::byte, 0x30> context;
        std::array<std::byte, 0x30> configuration;
        std::uint32_t field74, field78, field7C, field80;
        std::uint8_t active;
        std::array<std::byte, 3> padding85;
        std::uint32_t countdown;
        std::uint8_t menuReady;
        std::array<std::byte, 3> padding8D;
        float field90, field94;
    };
    static_assert(sizeof(wxAppHelperAbi) == 0x98);
    static_assert(offsetof(wxAppHelperAbi, configuration) == 0x44);
    static_assert(offsetof(wxAppHelperAbi, countdown) == 0x88);
}
