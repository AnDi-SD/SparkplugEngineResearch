#pragma once
// Portable state/input carrier for the bounded Skin renderer slices.
// No original RTTI class or original header/type name is claimed.
#include "Code/SparkplugDX/spDXRenderer.h"
namespace sparkplug::evidence::pc
{
    struct SkinRenderContextForAnalysis final
    {
        reconstruction::spDXRenderer::SubmissionStateForAnalysis submission;
        reconstruction::spDXRenderer::MatrixStateForAnalysis matrices;
        reconstruction::spDXRenderer::FogStateForAnalysis fog;
        reconstruction::spDXRenderer::LightSubmissionForAnalysis* lights=nullptr;
        reconstruction::spDXShader::ConstantInputsForAnalysis constants;
        reconstruction::spDXRenderer::SubmissionDeviceForAnalysis device;
        reconstruction::spDXRenderer::MatrixInputSubmitForAnalysis setMatrix=nullptr;
        reconstruction::spPCShaderManager* shaders=nullptr;
        const reconstruction::spPCShaderGenerationForAnalysis* shaderGeneration=nullptr;
        reconstruction::spDXMaterial* fallback=nullptr;
        reconstruction::spDXMaterial* selectedMaterial=nullptr;
        reconstruction::spRenderer::AlphaQueueForAnalysis* alphaQueue=nullptr;
        const reconstruction::spRenderer::AlphaCameraInputForAnalysis* alphaCamera=nullptr;
        reconstruction::spRenderNode* alphaSupport=nullptr;
        // PC C188 preserves C18C/C194. Material6C saves C1C4 through the
        // single shared7400FC byte, not a stack. Callers sharing native state
        // must supply the same save slot; its semantic name remains unknown.
        bool preserveMaterialSelection=false;
        std::uint8_t renderStateByte=0;
        std::uint8_t* sharedSavedRenderStateByte=nullptr;
        // Shared DXMesh initialization has no declaration resolver yet. This
        // is the explicit resolved declaration/handle input for that case.
        const reconstruction::spPCVertexDeclaration* sharedDeclaration=nullptr;
        std::array<std::uintptr_t,3> geometryHandles{};
        std::uint32_t activeBoneCount=0,packedColor=0;
    };
}
