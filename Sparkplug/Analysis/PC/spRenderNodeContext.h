#pragma once
// Explicit PC renderer consumer state, not a recovered original class/ABI.
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/Sparkplug/spLightManager.h"
namespace sparkplug::evidence::pc
{
    struct RenderNodeContextForAnalysis final
    {
        reconstruction::spDXRenderer::MatrixStateForAnalysis* matrices=nullptr;
        reconstruction::spDXRenderer::MatrixInputSubmitForAnalysis setMatrix=nullptr;
        void* deviceContext=nullptr;
        bool preserveLightSelection=false;
        const reconstruction::spLightManager::CacheForAnalysis* currentLights=nullptr;
        std::array<float,4> currentSphere{};
    };
}
