#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::analysis::pc
{
    struct wxAssetManagerAbi
    {
        std::array<std::byte, 0x10> base;
        std::uint32_t singletonVtable;
        std::uint32_t root;
        std::uint32_t languageMode;
        std::uint32_t languageDirectory;
        std::array<std::uint32_t, 19> categories;
        std::array<std::uint32_t, 69> subcategories;
    };
    static_assert(sizeof(wxAssetManagerAbi) == 0x180);
    static_assert(offsetof(wxAssetManagerAbi, categories) == 0x20);
    static_assert(offsetof(wxAssetManagerAbi, subcategories) == 0x6c);
}
