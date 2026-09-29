#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    class spEntity;

    // The native manager and reference-counted object are external. The
    // increment/decrement and deletion condition stay in spEntity itself.
    class spEntityHost
    {
    public:
        virtual ~spEntityHost() = default;
        virtual void AddToEngineManagerForAnalysis(spEntity& entity) = 0;
        // Native teardown also owns +14/+1C objects and removes membership.
        // Keep that unclosed sequence behind one explicit scene boundary.
        virtual void DestroyEntityForAnalysis(spEntity& entity) noexcept = 0;
        virtual std::uint16_t GetReferenceCountForAnalysis(void* object) = 0;
        virtual void SetReferenceCountForAnalysis(void* object,
            std::uint16_t count) = 0;
        virtual void DeleteReferenceForAnalysis(void* object) = 0;
    };
}
