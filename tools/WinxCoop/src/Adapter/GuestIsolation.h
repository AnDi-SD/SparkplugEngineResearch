#pragma once
#include <windows.h>
#include <cstdint>

namespace winxcoop::adapter
{
    class PinnedImage;
    // Own policy for a freshly assigned passive guest. The ordinary server
    // never prepares or installs this module. Physics, profile update and the
    // stock Actor/animation clock remain original; only effect sources change.
    class GuestIsolation final
    {
    public:
        bool Prepare(PinnedImage& image, DWORD mainThreadId, std::uint32_t assignedLevel) noexcept;
        bool Install() noexcept;
        bool BindMotion(std::uint32_t playerRecord) noexcept;
        bool VerifyMotion(std::uint32_t playerRecord) noexcept;
        bool Handles(std::uint32_t address) const noexcept;
        bool Handle(CONTEXT& context) noexcept;
        bool SceneReady() noexcept;
        // Complete only the terminal startup overlay of an actually withheld
        // cinematic. Called on ready ticks after motion/scene qualification;
        // a later request cannot be lost by running only on first SceneReady.
        bool CompletePresentation() noexcept;
        void Retire() noexcept;
        void CloseBeforeHooks() noexcept;
        const char* Failure() const noexcept { return failure_; }
    private:
        struct State;
        State* state_{};
        const char* failure_{"none"};
        bool Fail(const char* reason) noexcept;
    };
}
