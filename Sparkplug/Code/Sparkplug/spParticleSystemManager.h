#pragma once

#include <cstdint>

namespace sparkplug::reconstruction
{
    // Captured PC list dispatch with borrowed nodes and payloads.
    class spParticleSystemManager
    {
      public:
        using Address = std::uint32_t;
        static constexpr std::uint32_t ClassID = 0x00921A65;
        static constexpr Address CapturedPrimary = 0x006E70F8;

        struct ListNodeForAnalysis
        {
            const ListNodeForAnalysis* next = nullptr;
            Address particle = 0;
        };

        struct ListForAnalysis
        {
            Address primary = 0;
            const ListNodeForAnalysis* sentinel = nullptr;
        };

        struct DispatchForAnalysis
        {
            Address (*readExistingEngineCore)(void*) = nullptr;
            std::uint32_t (*readDeltaBits)(void*, Address engineCore) = nullptr;
            void (*updateParticle)(void*, Address particle, std::uint32_t deltaBits) = nullptr;
            void* context = nullptr;
        };

        enum class UpdateResultForAnalysis
        {
            Completed,
            UnqualifiedPrimary,
            IncoherentList,
            MissingDispatch,
            MissingExistingEngineCore,
        };

        // The supplied list is a finite, coherent sentinel-ended ring of live nodes.
        // The current node must stay live across dispatch for its next-link read.
        // The callback runs synchronously. Completed updates survive a later refusal.
        [[nodiscard]] static UpdateResultForAnalysis UpdateExistingForAnalysis(
            const ListForAnalysis&, const DispatchForAnalysis&);
    };
}
