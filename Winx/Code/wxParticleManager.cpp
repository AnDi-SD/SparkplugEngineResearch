#include "wxParticleManager.h"
#include "../Analysis/PC/wxParticleManagerAbi.h"
#include "Analysis/PC/spSphereContactMath.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    using Manager = wxParticleManager;
    using Address = Manager::Address;
    using Result = Manager::UpdateResultForAnalysis;
    using Dispatch = Manager::DispatchForAnalysis;
    using Particle = Manager::ParticleForAnalysis;
    namespace Abi = wxParticleManagerAbi;
    namespace Math = sparkplug::evidence::pc::sphere_contact_math;

    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
    static_assert(Manager::ClassID == Abi::ClassId);

    bool Admitted(std::uint32_t bits)
    {
        const auto magnitude = bits & 0x7FFFFFFFu;
        return magnitude == 0 || (magnitude >= 0x00800000u && magnitude < 0x7F800000u);
    }

    Math::Number FromBits(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return Math::From(value);
    }

    std::uint32_t StoreBits(Math::Number value)
    {
        const auto stored = Math::Store(value);
        std::uint32_t bits;
        std::memcpy(&bits, &stored, sizeof(bits));
        return bits;
    }

    Math::Number DistanceSquared(const Manager::PositionBits& from, const Manager::PositionBits& to)
    {
        const auto square = [&](unsigned lane)
        {
            const auto delta = Math::Subtract(FromBits(from[lane]), FromBits(to[lane]));
            return Math::Multiply(delta, delta);
        };
        const auto z = square(2), x = square(0), y = square(1);
        return Math::Add(Math::Add(z, x), y);
    }

    Result AdvanceClock(const Dispatch& dispatch, Address address, const Particle& particle, std::uint32_t deltaBits)
    {
        if (!dispatch.writeClockBits)
            return Result::MissingDispatch;
        if (!Admitted(particle.clockBits))
            return Result::UnqualifiedArithmetic;
        const auto nextClock = StoreBits(Math::Add(FromBits(deltaBits), FromBits(particle.clockBits)));
        if (!Admitted(nextClock))
            return Result::UnqualifiedArithmetic;
        dispatch.writeClockBits(dispatch.context, address, nextClock);
        return Result::Completed;
    }

    Result UpdateMember(const Dispatch& dispatch, Address address, const Manager::PositionBits& managerPosition,
        bool nearOrigin, Math::Number distanceThreshold, std::uint32_t deltaBits)
    {
        if (!dispatch.readParticle)
            return Result::MissingDispatch;
        const auto* particle = dispatch.readParticle(dispatch.context, address);
        if (!particle)
            return Result::MissingBorrowedValue;
        if (!particle->node)
            return AdvanceClock(dispatch, address, *particle, deltaBits);
        if (!dispatch.readNode)
            return Result::MissingDispatch;
        const auto* node = dispatch.readNode(dispatch.context, particle->node);
        if (!node)
            return Result::MissingBorrowedValue;
        if (!(node->flags & Abi::RequiredNodeFlag))
            return Result::UnqualifiedNodePredicate;

        if (!Admitted(particle->durationBits))
            return Result::UnqualifiedArithmetic;
        const auto duration = FromBits(particle->durationBits);
        if (Math::nearest::Compare(duration, {}) >= 0)
        {
            if (!Admitted(particle->lifetimeBits) || !Admitted(particle->clockBits))
                return Result::UnqualifiedArithmetic;
            const auto end = Math::Add(FromBits(particle->lifetimeBits), duration);
            if (Math::nearest::Compare(end, FromBits(particle->clockBits)) <= 0)
                return AdvanceClock(dispatch, address, *particle, deltaBits);
        }

        if (!std::all_of(node->worldPosition.begin(), node->worldPosition.end(), Admitted))
            return Result::UnqualifiedArithmetic;
        if (!nearOrigin && Math::nearest::Compare(DistanceSquared(managerPosition, node->worldPosition),
                distanceThreshold) >= 0)
        {
            const auto core = dispatch.readExistingEngineCore(dispatch.context);
            if (!core)
                return Result::MissingExistingEngineCore;
            if (!dispatch.readCurrentScene)
                return Result::MissingDispatch;
            if (node->scene == dispatch.readCurrentScene(dispatch.context, core))
                return AdvanceClock(dispatch, address, *particle, deltaBits);
        }

        if (!dispatch.updateParticle)
            return Result::MissingDispatch;
        dispatch.updateParticle(dispatch.context, address, deltaBits);
        return Result::Completed;
    }
}

wxParticleManager::UpdateResultForAnalysis wxParticleManager::UpdateExistingForAnalysis(
    const ListForAnalysis& list, const DispatchForAnalysis& dispatch, ProfileForAnalysis profile)
{
    if (profile != ProfileForAnalysis::PcExistingOwnersFiniteNearest64)
        return Result::UnqualifiedProfile;
    if (list.primary != Abi::CapturedPrimary)
        return Result::UnqualifiedPrimary;
    const auto* const sentinel = list.sentinel;
    if (!sentinel)
        return Result::IncoherentList;
    if (!std::all_of(list.managerPosition.begin(), list.managerPosition.end(), Admitted))
        return Result::UnqualifiedArithmetic;
    auto* node = sentinel->next;
    const bool nearOrigin = std::all_of(list.managerPosition.begin(), list.managerPosition.end(), [](std::uint32_t bits)
    {
        return (bits & 0x7FFFFFFFu) <= Abi::OriginToleranceBits;
    });

    if (!dispatch.readExistingEngineCore || !dispatch.readDeltaBits || !dispatch.readExistingGame || !dispatch.readGameState)
        return Result::MissingDispatch;
    const auto core = dispatch.readExistingEngineCore(dispatch.context);
    if (!core)
        return Result::MissingExistingEngineCore;
    const auto deltaBits = dispatch.readDeltaBits(dispatch.context, core);
    const auto game = dispatch.readExistingGame(dispatch.context);
    if (!game)
        return Result::MissingExistingGame;
    const auto state = dispatch.readGameState(dispatch.context, game);
    if (!Admitted(deltaBits))
        return Result::UnqualifiedArithmetic;
    const auto threshold = FromBits(state == 0x21 ? Abi::State33DistanceSquaredBits : Abi::OrdinaryDistanceSquaredBits);

    for (; node != sentinel; node = node->next)
    {
        if (!node)
            return Result::IncoherentList;
        if (const auto particle = node->particle)
        {
            const auto result = UpdateMember(dispatch, particle, list.managerPosition, nearOrigin, threshold, deltaBits);
            if (result != Result::Completed)
                return result;
        }
    }
    return Result::Completed;
}
