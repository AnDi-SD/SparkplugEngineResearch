#pragma once
#include <windows.h>
#include <cstdint>

namespace winxcoop::adapter
{
    class PinnedImage;
    class GuestIsolation;

    // One-purpose original-main-thread Flow request. The call executes only
    // after the World VEH has returned, so existing Flow gate INT3s are not
    // recursive handler calls. No original code/prologue is copied or bypassed.
    class GuestSceneTransition final
    {
    public:
        bool Prepare(PinnedImage&, GuestIsolation&, DWORD mainThread) noexcept;
        bool CanBeginAtWorld(const CONTEXT&, std::uint32_t targetLevel, std::uint32_t targetEntry) const noexcept;
        bool BeginAtWorld(CONTEXT&, std::uint32_t targetLevel, std::uint32_t targetEntry) noexcept;
        bool Handles(std::uint32_t exceptionAddress) const noexcept;
        bool Complete(CONTEXT&) noexcept;
        bool OnLaterWorld(CONTEXT&) noexcept;
        bool Pending() const noexcept { return pending_; }
        bool HasRetiredPage() const noexcept { return retired_; }
        std::uint32_t TargetLevel() const noexcept { return target_; }
        const char* Failure() const noexcept { return failure_; }

    private:
        PinnedImage* image_{};
        GuestIsolation* isolation_{};
        DWORD thread_{};
        BYTE* page_{};
        BYTE code_[23]{};
        CONTEXT saved_{};
        std::uint32_t flow_{}, target_{}, trap_{}, originalReturn_{};
        std::uint64_t began_{}, completedAt_{};
        bool prepared_{}, pending_{}, retired_{}, failed_{};
        const char* failure_{"none"};
        bool Fail(const char*) noexcept;
        bool PageMatches() const noexcept;
    };
}
