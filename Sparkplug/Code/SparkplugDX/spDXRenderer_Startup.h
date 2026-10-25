#pragma once

// Analytical split of the proven spDXRenderer_Init.cpp startup/teardown bodies.
#include "spDXRenderer.h"
#include "../Sparkplug/spIndexBuffer.h"
#include "../Sparkplug/spVertexBuffer.h"
#include "../SparkplugPC/spPCShaderManager.h"
#include "../SparkplugPC/spPCErrorManager.h"

namespace sparkplug::reconstruction
{
    struct spDXRenderer::StartupStateForAnalysis
    {
        DeviceLifecycleStateForAnalysis device;
        DeviceStateCachesForAnalysis caches;
        DeviceStateSelectorForAnalysis selector; // explicit shared-global input
        spIndexBuffer commonIndices; // secondary+C138, embedded in original
        spVertexBuffer commonVertices; // secondary+C0DC, embedded in original
        std::uint8_t active=0; // secondary+24
        std::vector<MatrixStateForAnalysis::RawMatrix> scratch; // completeC9B8
        std::uint32_t scratchCount=0; // completeC9BC, not inferred vector size
    };

    struct spDXRenderer::StartupGlobalsForAnalysis
    {
        std::shared_ptr<spPCShaderManager> shaders; // original763024
        std::shared_ptr<spPCErrorManager> errors; // original755264
        // Analytical retention of SDK interfaces native failure paths leak.
        // This collection is NOT a recovered native/global owner relationship.
        std::vector<std::shared_ptr<spPCShaderManager::StandaloneBufferForAnalysis>> unreleasedLibraryInterfaces;
    };

    struct spDXRenderer::StartupAPIForAnalysis
    {
        DeviceLifecycleAPIForAnalysis device;
        DeviceStateReadbackForAnalysis readback;
        std::function<std::uintptr_t(std::uint32_t sdkVersion)> createDirect3D;
        std::function<std::uint32_t(std::uintptr_t)> releaseDevice,releaseDirect3D;
        spPCShaderManager::StartupForAnalysis shader;
        spPCShaderManager::StandaloneForAnalysis standalone;
        spPCEffectTemplate::CompilerStateForAnalysis* shaderState=nullptr;
        std::string shaderRoot;
        // Host ceiling for the two common payloads, not an original maximum.
        std::uint32_t maximumCpuBytes=16u*1024u*1024u;
    };
}
