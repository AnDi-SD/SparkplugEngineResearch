#pragma once

// Standalone PC5991C0 ->598CA0/598ED0 and5991E0 ->598BE0/598ED0. Original C++
// owners, filename and method names remain unknown. The retry operation is
// shared by original level and individual-instance consumers.
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spGameLevel;
    class spTemplateInstance;
    enum class spGameLevelInitializationStatusForAnalysis
    {
        Completed,
        InvalidInputGraph,
        AttachedObjectLimit,
        EntityLimit,
        PassLimit,
    };
    struct spGameLevelInitializationLimitsForAnalysis final
    {
        std::size_t maximumAttachedObjects = 65536;
        std::size_t maximumEntities = 65536;
        std::uint32_t maximumPasses = 256;
    };
    struct spGameLevelInitializationResultForAnalysis final
    {
        spGameLevelInitializationStatusForAnalysis status = spGameLevelInitializationStatusForAnalysis::Completed;
        std::size_t attachedObjectsVisited = 0;
        std::size_t entitiesCollected = 0;
        std::size_t notificationsSent = 0;
        std::uint32_t passesCompleted = 0;
    };

    // The established GameLevel/TemplateInstance graph and every receiver must
    // outlive this synchronous call. Collection preserves native list order
    // and duplicate raw pointers, with no intrusive/shared ownership changes.
    // Each fresh notification is code1C, zero other known fields, payload equal
    // to the zero-based pass index. Its mutable tail LOW BYTE requests retry.
    // Native tail upper bytes are unassigned and have no modeled meaning.
    // Stable graph and receiver lifetimes are required. Limits are analytical
    // prefix refusals, never native successful termination/nontermination.
    [[nodiscard]] spGameLevelInitializationResultForAnalysis InitializeGameLevelForAnalysis(
        spGameLevel&, const spGameLevelInitializationLimitsForAnalysis& = {});
    // PC5991E0 starts at this instance's attached-object list. It does not
    // visit an enclosing level or neighbouring instances. Borrow/lifetime,
    // notification and analytical limit contracts are identical to the above.
    [[nodiscard]] spGameLevelInitializationResultForAnalysis InitializeTemplateInstanceForAnalysis(
        spTemplateInstance&, const spGameLevelInitializationLimitsForAnalysis& = {});
}
