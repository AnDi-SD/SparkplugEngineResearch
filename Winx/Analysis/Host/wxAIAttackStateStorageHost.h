#pragma once
#include "wxAIActionCommandExitHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    enum class wxAIAttackStateProfileForAnalysis { Unqualified, PcCapturedStableDisjointStorage };
    enum class wxAIAttackStateResultForAnalysis
    {
        Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, MissingOwner,
        MissingStorage, MissingActor, MissingState, MissingStateValue, MissingCommand,
        CommandNotAdmitted, UnqualifiedHook,
    };
    // Our raw-data storage contract. This C++ extension does not assert any
    // native class relationship. Every returned record remains live, disjoint
    // and stable throughout one call; the state124 record's owner is unknown.
    class wxAIAttackStateStorageHost : public wxAIActionCommandExitHost
    {
    public:
        virtual std::optional<void*> ReadState124ForAnalysis(void* actor) noexcept = 0;
        virtual std::optional<std::uint32_t> ReadStateWord130ForAnalysis(void* state) noexcept = 0;
    };
    // Our bounded data-access diagnostic, separate from native PCs/stack/ABI.
    // Portable record/pointer identities are projected by the replay driver;
    // encoded native addresses are never reinterpreted as C++ pointers.
    struct wxAIAttackStateAccessForAnalysis final
    {
        enum class Kind { ReadPointer, ReadValue, WriteValue };
        Kind kind;
        const void* record;
        std::uint32_t offset;
        std::uint8_t bytes;
        std::uint32_t value;
        const void* pointer;
    };
}
