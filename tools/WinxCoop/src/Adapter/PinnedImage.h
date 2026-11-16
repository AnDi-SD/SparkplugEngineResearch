#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace winxcoop::adapter
{
    // Own source/live feature gate, not a whole-game identity check. The source
    // remains read-pinned while a catalogued native function can be called.
    class PinnedImage final
    {
    public:
        bool Open() noexcept;
        bool Verify(std::uint32_t address, std::size_t bytes) const noexcept;
        const char* Failure() const noexcept { return failure_; }
        std::uint32_t FailureAddress() const noexcept { return failureAddress_; }
        void Close() noexcept;
    private:
        HANDLE file_{INVALID_HANDLE_VALUE}, mapping_{};
        const BYTE* view_{};
        DWORD size_{};
        const IMAGE_NT_HEADERS32* pe_{};
        mutable const char* failure_{"none"};
        mutable std::uint32_t failureAddress_{};
        const BYTE* Source(std::uint32_t address, std::size_t bytes, DWORD flags) const noexcept;
        bool Layout(std::uint32_t address, std::size_t bytes, DWORD flags) const noexcept;
        bool Fail(const char* reason, std::uint32_t address = 0) const noexcept;
    };
}
