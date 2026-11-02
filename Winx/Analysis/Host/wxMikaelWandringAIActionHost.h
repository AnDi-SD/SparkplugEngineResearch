#pragma once
#include <cstdint>

namespace winx::reconstruction
{
    // Our structural data bindings for an otherwise unknown borrowed owner.
    // Returned cells and all records remain live, disjoint and stable during
    // the call. These observations do not reenter or return a method result.
    class wxMikaelWandringAIActionHost
    {
    public:
        virtual ~wxMikaelWandringAIActionHost() = default;
        virtual std::uint32_t* BorrowOwnerWord1C0ForAnalysis(void* owner) noexcept = 0;
        virtual std::uint32_t* BorrowOwnerWord1BCForAnalysis(void* owner) noexcept = 0;
    };
}
