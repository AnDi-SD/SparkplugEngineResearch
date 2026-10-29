#pragma once
#include "RuntimeContract.h"
#include <cstddef>

namespace winxcoop::adapter
{
    enum class PassiveAnimation : std::uint32_t { Idle, Walk, Jump };
    enum class PassiveActorError : std::uint32_t
    { None, Context, LocalIdentity, Memory, Graph, Ownership, Signature, NativeException, Animation, Appearance, Transform, Capacity };
    using NativeGate = bool (*)(std::uint32_t address, std::size_t bytes) noexcept;
    using PassivePhaseGate = bool (*)() noexcept;
    struct PassiveActorContext final
    {
        DWORD mainThreadId{};
        NativeGate gate{};
        PassivePhaseGate safeGamePhase{};
    };
    struct PassiveActorStatistics final
    {
        std::uint32_t nativeCalls{}, created{}, destroyed{}, nodes{}, skins{}, materials{};
    };

    // Own Alfea28 adapter, ported from the qualified A diagnostic. Only native
    // graph/Actor APIs are called; no player/controller/input/camera is created.
    // The caller admits the pinned EXE/assets and keeps this DLL loaded until
    // its own game exits. All entry points require the root's world/pre-unload
    // phase witness and the original main thread. No native calls in destructors.
    class PassiveActor final
    {
    public:
        bool Configure(const PassiveActorContext& context) noexcept;
        bool Create(const char* mediaDirectory) noexcept;
        // Raw stock character-root PRS. The display wrapper supplies the
        // model's omitted Y half-turn; the local root is never modified.
        bool ApplyRemote(const Transform& transform, PassiveAnimation animation) noexcept;
        // Read-only lifetime witness. A paused/menu flow may retain this same
        // scene while no new gameplay command is admissible.
        bool ValidateLiveOwnership() noexcept;
        bool DestroyBeforeUnload() noexcept;
        bool Created() const noexcept;
        bool Poisoned() const noexcept { return poisoned_; }
        PassiveActorError Error() const noexcept { return error_; }
        const char* LastError() const noexcept { return errorText_; }
        PassiveActorStatistics Statistics() const noexcept { return statistics_; }
    private:
        struct State;
        State* state_{};
        PassiveActorContext context_{};
        PassiveActorStatistics statistics_{};
        PassiveActorError error_{};
        bool poisoned_{};
        const char* errorText_{"none"};
        bool Phase() noexcept;
        bool Fail(PassiveActorError error, const char* text, bool poison = false) noexcept;
    };
}
