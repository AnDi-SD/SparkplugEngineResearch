#pragma once

#include "spDXRenderer.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXBallisticPFX
    {
    public:
        static constexpr std::uint32_t ClassID = 0x23A15847;
        static constexpr std::uint32_t CapturedPrimary = 0x006EF8E8;

        struct BorrowedRenderStatesForAnalysis
        {
            std::uint32_t* entries = nullptr;
            std::size_t entryCount = 0;
            spDXRenderer::RenderStateSubmitForAnalysis submit = nullptr;
            void* submitContext = nullptr;
        };

        using ReadRendererForAnalysis = BorrowedRenderStatesForAnalysis (*)(void* context) noexcept;

        // PC 004AC760 for an existing borrowed renderer. Resolve it at each
        // original read site, with cache entries valid through their submission.
        // False denotes refusal of a missing or short binding after any
        // earlier writes. It does not project a native return value.
        [[nodiscard]] static bool ApplyPointStatesForAnalysis(std::uint32_t primary,
            ReadRendererForAnalysis readRenderer, void* context) noexcept;
    };
}
