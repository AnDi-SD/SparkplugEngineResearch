#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxTextControllerLayout final
    {
        std::array<std::uint32_t,4> base;
        std::array<std::uint32_t,204> ownWords10;
    };
    static_assert(sizeof(wxTextControllerLayout)==0x340);
    static_assert(offsetof(wxTextControllerLayout,ownWords10)==0x10);
    inline constexpr std::uint32_t wxTextControllerFactory=0x5A8790;
    inline constexpr std::uint32_t wxTextControllerVtable=0x707B20;
    inline constexpr std::uint32_t wxTextControllerRegistration=0x766EF0;
    inline constexpr std::uint32_t wxTextControllerReset=0x5A8490;
    inline constexpr std::uint32_t wxTextControllerDestructor=0x5A8630;
    inline constexpr std::uint32_t wxTextControllerDeletingDestructor=0x5A8850;
}
