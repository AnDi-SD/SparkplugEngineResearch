#include "spParticleSystemManager.h"

namespace sparkplug::reconstruction
{
    spParticleSystemManager::UpdateResultForAnalysis spParticleSystemManager::UpdateExistingForAnalysis(
        const ListForAnalysis& list, const DispatchForAnalysis& dispatch)
    {
        using Result = UpdateResultForAnalysis;
        if (list.primary != CapturedPrimary)
            return Result::UnqualifiedPrimary;
        auto* sentinel = list.sentinel;
        if (!sentinel)
            return Result::IncoherentList;

        auto* node = sentinel->next;
        if (node == sentinel)
            return Result::Completed;
        if (!dispatch.readExistingEngineCore || !dispatch.readDeltaBits || !dispatch.updateParticle)
            return Result::MissingDispatch;

        // PC 0045A390 advances after dispatch and reloads the existing core per node.
        do
        {
            if (!node)
                return Result::IncoherentList;

            const auto core = dispatch.readExistingEngineCore(dispatch.context);
            if (!core)
                return Result::MissingExistingEngineCore;
            const auto delta = dispatch.readDeltaBits(dispatch.context, core);
            const auto particle = node->particle;
            if (!particle)
                return Result::IncoherentList;
            dispatch.updateParticle(dispatch.context, particle, delta);
            node = node->next;
            sentinel = list.sentinel;
            if (!sentinel)
                return Result::IncoherentList;
        } while (node != sentinel);
        return Result::Completed;
    }
}
