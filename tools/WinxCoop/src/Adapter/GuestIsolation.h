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
        bool CanFollow(std::uint32_t target) const noexcept;
        bool PrepareFollow(std::uint32_t target, std::uint32_t flow,
            std::uint32_t privateReturn, std::uint32_t expectedEntryEsp) noexcept;
        bool CompleteFollowRequest(std::uint32_t target) noexcept;
        // Only after actual old native unload and a freshly observed assigned
        // Playing. Keeps published hooks; replaces private motion admission.
        bool BeginNextScene(std::uint32_t target) noexcept;
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
