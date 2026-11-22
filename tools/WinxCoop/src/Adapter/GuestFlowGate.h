#pragma once
#include <windows.h>
#include <cstdint>

namespace winxcoop::adapter
{
    class PinnedImage;

    // Guest-only policy for the original main-thread Flow request paths.
    // The adapter owns main-thread/reentrancy admission before Handle. These
    // rare permanent INT3 entries remain resident until the owned game exits.
    class GuestFlowGate final
    {
    public:
        bool Prepare(PinnedImage& image, std::uint32_t assigned) noexcept;
        bool Install() noexcept;
        bool Handles(std::uint32_t address) const noexcept;
        bool Handle(CONTEXT& context) noexcept;
        bool SceneReady() noexcept;
        // Retirement revokes gameplay admission; it does not reopen the
        // original new-game paths while the client is finishing its menus.
        void Retire() noexcept;
        const char* Failure() const noexcept { return failure_; }
        void TraceCounters() const noexcept;

    private:
        struct Record final { std::uint32_t object{}, table{}; };
        PinnedImage* image_{};
        std::uint32_t assigned_{}, flow_{}, playing_{};
        bool prepared_{}, installAttempted_{}, installed_[3]{};
        bool logoInProgress_{}, logoQueued_{}, initialInProgress_{}, initialQueued_{};
        bool initialWrapperSeen_{}, ready_{}, retired_{}, firstBlockLogged_{}, failed_{};
        std::uint32_t calls_{}, wrappers_{}, bootAllowed_{}, uiAllowed_{}, popsAllowed_{}, resumesAllowed_{};
        std::uint32_t playableBlocked_{}, unknownBlocked_{}, failedAddress_{};
        const char* failure_{"none"};
        bool Fail(const char* reason, std::uint32_t address = 0) noexcept;
        bool Lookup(std::uint32_t target, Record& record) const noexcept;
        bool UserInterface(const Record& record) const noexcept;
        bool RetainedWithUiAbove(const Record& record) const noexcept;
        bool SafePop() const noexcept;
        bool ReplayPush(CONTEXT& context, std::uint32_t address) noexcept;
        bool Reject(CONTEXT& context, std::uint32_t argumentBytes, std::uint32_t target,
            std::uint32_t mode, std::uint32_t delay, bool playable) noexcept;
    };
}
