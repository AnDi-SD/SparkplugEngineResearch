#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    // Evidence-only captured offsets. raw00 is unclassified storage, not an
    // asserted wxAIAction base. This layout is not the portable facade layout.
    struct wxIceWormHolesAIActionLayout final
    {
        std::array<std::uint8_t, 0x3A8> raw00;
        std::uint32_t raw3A8;
        std::array<std::uint8_t, 0x18> raw3AC;
        std::uint8_t raw3C4;
        std::array<std::uint8_t, 3> raw3C5;
    };
    static_assert(sizeof(wxIceWormHolesAIActionLayout) == 968);
    static_assert(offsetof(wxIceWormHolesAIActionLayout, raw3A8) == 0x3A8);
    static_assert(offsetof(wxIceWormHolesAIActionLayout, raw3C4) == 0x3C4);
    inline constexpr std::uint32_t wxIceWormHolesAIActionClassID = 0x812B81AB;
    inline constexpr std::uint32_t wxIceWormHolesAIActionRegistrationBaseClassID = 0x490A6EB5;
    inline constexpr std::uint32_t wxIceWormHolesAIActionPrimary = 0x006F9D78;
    inline constexpr std::uint32_t wxIceWormHolesAIActionRTTIGetter = 0x00522410;
    inline constexpr std::uint32_t wxIceWormHolesAIActionRTTIRecord = 0x00751068;
    inline constexpr std::uint32_t wxIceWormHolesAIActionMessageHandler = 0x00522490;
    inline constexpr std::size_t wxIceWormHolesAIActionWholeHandlerBytes = 204;
    // Protected lifecycle metadata only; never a callable source contract.
    inline constexpr std::uint32_t wxIceWormHolesAIActionProtectedFactory = 0x00403FE0;
    inline constexpr std::uint32_t wxIceWormHolesAIActionProtectedConstructor = 0x00522370;
    inline constexpr std::size_t wxIceWormHolesAIActionMessageCodeOffset = 0;
    inline constexpr std::size_t wxIceWormHolesAIActionMessagePayloadOffset = 0x18;
    inline constexpr std::size_t wxIceWormHolesAIActionSuppliedMessageBytes = 28;
    // No native message-class layout, physical base or return type asserted.
}
