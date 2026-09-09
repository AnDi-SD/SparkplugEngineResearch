#pragma once

// Analytical name, not a recovered RTTI class. The same embedded support lives
// at RenderNode+B4, StaticRenderObject+14 and PartitionRenderable+10 on PC.
// This slice shares original469ED0 append and469820 bounds; shared_ptr replaces
// intrusive lifetime, and transfer/removal helpers remain explicit host policy.
#include "Code/Sparkplug/spRenderable.h"
#include "spRenderNodeMath.h"
#include <algorithm>
#include <utility>

namespace sparkplug::evidence::pc
{
    struct RenderSupportForAnalysis
    {
        using Renderable = reconstruction::spRenderable;
        using Matrix4 = render_node_math::Matrix4;
        std::vector<std::shared_ptr<Renderable>> renderables;
        Renderable::BoundingSphere localSphere{}, worldSphere{};

        void Rebuild(const Matrix4& matrix) noexcept
        {
            // Original retains the previous center if no sphere contributes.
            localSphere[3] = 0;
            for (const auto& renderable : renderables)
                render_node_math::Merge(localSphere, renderable->GetBoundingSphereForAnalysis());
            worldSphere = render_node_math::FromCachedMatrix(localSphere, matrix);
        }
        bool Append(std::shared_ptr<Renderable> renderable, const Matrix4& matrix)
        {
            if (!renderable) return false;
            renderables.push_back(std::move(renderable));
            Rebuild(matrix);
            return true;
        }
        std::shared_ptr<Renderable> DetachForHost(Renderable& renderable, const Matrix4& matrix) noexcept
        {
            const auto found = std::find_if(renderables.begin(), renderables.end(),
                [&renderable](const auto& candidate) { return candidate.get() == &renderable; });
            if (found == renderables.end()) return nullptr;
            auto result = std::move(*found);
            renderables.erase(found);
            Rebuild(matrix);
            return result;
        }
        void ClearForHost(const Matrix4& matrix) noexcept
        {
            if (renderables.empty()) return;
            renderables.clear();
            Rebuild(matrix);
        }
    };
}
