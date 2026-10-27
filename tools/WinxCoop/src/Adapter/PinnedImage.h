#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace winxcoop::adapter
{
    // Own source/live signature gate. The source file remains open without
    // write/delete sharing while any qualified native function can be called.
    class PinnedImage final
    {
    public:
        bool Open() noexcept;
        bool Verify(std::uint32_t address, std::size_t bytes) const noexcept;
        void Close() noexcept;
    private:
        HANDLE file_{INVALID_HANDLE_VALUE}, mapping_{};
        const BYTE* view_{};
        DWORD size_{};
        const IMAGE_NT_HEADERS32* pe_{};
    };
}
