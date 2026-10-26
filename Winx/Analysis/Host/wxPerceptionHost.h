#pragma once

#include <array>

namespace winx::reconstruction
{
    class wxPerception;

    // Project adapter for the borrowed owner/entity/Node chain. It neither
    // creates native targets nor supplies a successful perception result.
    class wxPerceptionHost
    {
    public:
        virtual ~wxPerceptionHost() = default;
        virtual std::array<float, 3> ReadNodePositionForAnalysis(
            const void* object) const = 0;
        virtual void DetachOwnerForAnalysis(wxPerception& perception,
            void* owner) noexcept = 0;
    };
}
