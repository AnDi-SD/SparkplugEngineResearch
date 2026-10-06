#pragma once

// Explicit platform boundary. None of these callbacks is a successful default
// replacement; the caller supplies real OS services or a declared fixture.
#include <cstdint>
#include <functional>
#include <optional>

namespace sparkplug::reconstruction { class spPCThread; }
namespace sparkplug::host
{
    struct spPCThreadHost final
    {
        using Handle = std::uintptr_t;
        struct CreateRequest final
        {
            std::uint32_t entryToken = 0;
            reconstruction::spPCThread* parameter = nullptr;
            std::uint32_t flags = 4; // original CREATE_SUSPENDED
            std::uint32_t stackBytes = 0;
            // Security attributes are null; returned threadId is discarded.
        };
        struct ExitCodeObservation final
        {
            bool apiSucceeded = false; // original ignores this return value
            // On failure with an untouched native out-cell, the caller must
            // supply an observed residue to qualify that case. No fake zero.
            std::optional<std::uint32_t> code;
        };
        std::function<Handle(const CreateRequest&)> create;
        std::function<std::uint32_t(Handle, std::uint32_t)> wait;
        std::function<ExitCodeObservation(Handle)> exitCode;
        std::function<std::uint32_t(Handle)> resume;
        std::function<std::uint32_t(Handle)> suspend;
        std::function<std::int32_t(Handle, std::uint32_t)> terminate;
        std::function<void(std::uint32_t)> sleep;
    };
}
