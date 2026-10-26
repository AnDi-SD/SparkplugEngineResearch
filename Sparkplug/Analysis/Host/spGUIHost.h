#pragma once

// Our explicit boundary for GUI dependencies. Node name lookup and collision
// groups are reconstructed common Node/CollisionInfo operations. Reverse
// subscriptions, notification targets and manager focus remain external.
#include "spEntityHost.h"
#include "Code/SparkBase/spNotification.h"
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spGUIObject;
    class spNode;

    using spGUINotificationForAnalysis=spNotificationForAnalysis;

    class spGUIHost : public spEntityHost,public spNotificationHost
    {
    public:
        virtual void SubscribeGUINodeForAnalysis(spNode&, spGUIObject&) = 0;
        virtual void UnsubscribeGUINodeForAnalysis(spNode&, spGUIObject&) noexcept = 0;
        // Focus/hover cleanup is a separate original GUIManager dependency.
        // This hook does not release the Entity-owned reference.
        virtual void DestroyGUIForAnalysis(spGUIObject&) noexcept = 0;
        virtual bool IsGUINodeEnabledForAnalysis(const spNode&) = 0;
        virtual void SetGUINodeEnabledForAnalysis(
            spNode&, std::uint8_t enabled, bool recursive) = 0;
        // Entity+24 is an external notification target. Its producing graph,
        // physical type and lifetime are not invented by the GUI facade.
        virtual void* GetGUINotificationTargetForAnalysis(const spGUIObject&) = 0;
        // Button reads only the opaque pointed event's native word+18.
        // This is not a declaration of an original event class or full layout.
        virtual std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t event) = 0;
        // Widget/Button notification protocols beyond the bounded paths
        // are deliberately unresolved. Consumers must observe this boundary;
        // an inherited no-op must not masquerade as their implementation.
        virtual void UnrecoveredGUINotificationForAnalysis(
            spGUIObject&, const void* notification, const char* reason) noexcept = 0;
    };
}
