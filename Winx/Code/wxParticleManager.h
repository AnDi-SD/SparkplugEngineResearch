#pragma once

#include "Code/Sparkplug/spParticleSystemManager.h"

#include <array>
#include <cstdint>

// Captured PC update dispatch with existing providers and borrowed values.
class wxParticleManager
{
  public:
    using Address = std::uint32_t;
    using PositionBits = std::array<std::uint32_t, 3>;
    using ListNodeForAnalysis = sparkplug::reconstruction::spParticleSystemManager::ListNodeForAnalysis;
    static constexpr std::uint32_t ClassID = 0x59C9594A;

    struct ListForAnalysis
    {
        Address primary = 0;
        const ListNodeForAnalysis* sentinel = nullptr;
        PositionBits managerPosition{};
    };

    struct ParticleForAnalysis
    {
        Address node = 0;
        std::uint32_t clockBits = 0;
        std::uint32_t durationBits = 0;
        std::uint32_t lifetimeBits = 0;
    };

    struct RenderNodeForAnalysis
    {
        std::uint32_t flags = 0;
        PositionBits worldPosition{};
        Address scene = 0;
    };

    struct DispatchForAnalysis
    {
        Address (*readExistingEngineCore)(void*) = nullptr;
        std::uint32_t (*readDeltaBits)(void*, Address core) = nullptr;
        Address (*readExistingGame)(void*) = nullptr;
        std::uint32_t (*readGameState)(void*, Address game) = nullptr;
        const ParticleForAnalysis* (*readParticle)(void*, Address particle) = nullptr;
        const RenderNodeForAnalysis* (*readNode)(void*, Address node) = nullptr;
        Address (*readCurrentScene)(void*, Address core) = nullptr;
        void (*writeClockBits)(void*, Address particle, std::uint32_t clockBits) = nullptr;
        void (*updateParticle)(void*, Address particle, std::uint32_t deltaBits) = nullptr;
        void* context = nullptr;
    };

    enum class ProfileForAnalysis { Unqualified, PcExistingOwnersFiniteNearest64 };
    enum class UpdateResultForAnalysis
    {
        Completed, UnqualifiedProfile, UnqualifiedPrimary, IncoherentList,
        MissingDispatch, MissingExistingEngineCore, MissingExistingGame,
        MissingBorrowedValue, UnqualifiedArithmetic, UnqualifiedNodePredicate,
    };

    // A finite coherent ring and each borrowed value stay live across synchronous calls.
    // The original reaches a virtual predicate for present nodes with bit 0x200 clear.
    [[nodiscard]] static UpdateResultForAnalysis UpdateExistingForAnalysis(
        const ListForAnalysis&, const DispatchForAnalysis&,
        ProfileForAnalysis = ProfileForAnalysis::Unqualified);
};
