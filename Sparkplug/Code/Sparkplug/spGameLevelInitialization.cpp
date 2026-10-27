#include "spGameLevelInitialization.h"
#include "spGameLevel.h"
#include "spEntity.h"
#include "Code/SparkBase/spNotification.h"
#include <vector>

namespace sparkplug::reconstruction
{
    namespace
    {
        using PendingEntities = std::vector<spBaseObject*>;
        bool CollectInstanceEntities(const spTemplateInstance& instance,
            const spGameLevelInitializationLimitsForAnalysis& limits,
            spGameLevelInitializationResultForAnalysis& result, PendingEntities& pending)
        {
            for (const auto& object : instance.GetAttachedObjectsForAnalysis())
            {
                if (result.attachedObjectsVisited == limits.maximumAttachedObjects)
                {
                    result.status = spGameLevelInitializationStatusForAnalysis::AttachedObjectLimit;
                    return false;
                }
                ++result.attachedObjectsVisited;
                if (!object)
                {
                    result.status = spGameLevelInitializationStatusForAnalysis::InvalidInputGraph;
                    return false;
                }
                if (!object->IsKindOf(spEntity::ClassID)) continue;
                if (pending.size() == limits.maximumEntities)
                {
                    result.status = spGameLevelInitializationStatusForAnalysis::EntityLimit;
                    return false;
                }
                pending.push_back(object.get());
                ++result.entitiesCollected;
            }
            return true;
        }
        void NotifyPendingEntities(PendingEntities& pending,
            const spGameLevelInitializationLimitsForAnalysis& limits,
            spGameLevelInitializationResultForAnalysis& result)
        {
            while (!pending.empty())
            {
                if (result.passesCompleted == limits.maximumPasses)
                {
                    result.status = spGameLevelInitializationStatusForAnalysis::PassLimit;
                    return;
                }
                std::size_t retryCount = 0;
                for (auto* object : pending)
                {
                    spNotificationForAnalysis notification;
                    notification.code = 0x1C;
                    notification.payload = result.passesCompleted;
                    object->vfunc_0C(&notification);
                    ++result.notificationsSent;
                    if ((notification.tail & 0xFF) != 0) pending[retryCount++] = object;
                }
                ++result.passesCompleted;
                pending.resize(retryCount);
            }
        }
    }
    spGameLevelInitializationResultForAnalysis InitializeGameLevelForAnalysis(
        spGameLevel& level, const spGameLevelInitializationLimitsForAnalysis& limits)
    {
        spGameLevelInitializationResultForAnalysis result;
        // Technical storage substitution: native collection creates a list,
        // then copies its borrowed pointers into an array and destroys the
        // temporary list before the first notification. This single raw-pointer
        // vector preserves collection, compaction and receiver lifetime roles;
        // it does not reproduce native allocation/failure/container ABI.
        PendingEntities pending;
        for (const auto& instance : level.GetInstancesForAnalysis())
        {
            if (!instance)
            {
                result.status = spGameLevelInitializationStatusForAnalysis::InvalidInputGraph;
                return result;
            }
            if (!CollectInstanceEntities(*instance, limits, result, pending)) return result;
        }
        NotifyPendingEntities(pending, limits, result);
        return result;
    }
    spGameLevelInitializationResultForAnalysis InitializeTemplateInstanceForAnalysis(
        spTemplateInstance& instance, const spGameLevelInitializationLimitsForAnalysis& limits)
    {
        spGameLevelInitializationResultForAnalysis result;
        PendingEntities pending;
        if (CollectInstanceEntities(instance, limits, result, pending))
            NotifyPendingEntities(pending, limits, result);
        return result;
    }
}
