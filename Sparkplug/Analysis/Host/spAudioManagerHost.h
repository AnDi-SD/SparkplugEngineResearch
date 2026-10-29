#pragma once

#include "Code/SparkplugDX/spDXAudioBank.h"
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    // Own captured bank-registry input, not original manager construction,
    // global startup, an audio device or the native344-byte object layout.
    enum class spAudioManagerPcRegistryProfileForAnalysis
    {Unqualified,StableOwnedInactiveNullDevice,StableOwnedCapturedEntriesInactiveNullDevice};

    struct spAudioManagerCapturedPcRegistryForAnalysis final
    {
        spAudioManagerPcRegistryProfileForAnalysis profile=
            spAudioManagerPcRegistryProfileForAnalysis::Unqualified;
        std::vector<std::unique_ptr<spDXAudioBank>> banks;
        // StableOwnedInactiveNullDevice asserts exclusive synchronous use,
        // no allocator/destructor callbacks or reentry and an inactive/null
        // backend. Input banks are unique owners of real concrete empty banks;
        // IDs may repeat. The adapter validates count/null/empty properties.
        // The separate CapturedEntries profile admits1..2 banks with1..8
        // total real accepted Entries, including mixed empty banks. Final
        // DXBank's typed captured constructor establishes their Entry graph;
        // nonnull plain Names retain external owners through the operation.
        // No aliases/reentry, SDK resources or native Add/Load are admitted.
    };
}
