cmake_minimum_required(VERSION 3.20)
project(SparkplugReconstruction LANGUAGES CXX)

add_library(SparkBase STATIC
    Code/SparkBase/spApp.cpp
    Code/SparkBase/spApp.h
    Code/SparkBase/spAsyncFileStreamManager.cpp
    Code/SparkBase/spAsyncFileStreamManager.h
    Code/SparkBase/spBaseObject.cpp
    Code/SparkBase/spBaseObject.h
    Code/SparkBase/spErrorManager.cpp
    Code/SparkBase/spErrorManager.h
    Code/SparkBase/spFileStream.cpp
    Code/SparkBase/spFileStream.h
    Code/SparkBase/spMemoryStream.cpp
    Code/SparkBase/spMemoryStream.h
    Code/SparkBase/spPCKManager.cpp
    Code/SparkBase/spPCKManager.h
    Code/SparkBase/spStream.cpp
    Code/SparkBase/spStream.h
    Code/SparkBase/spSubscriptionManager.cpp
    Code/SparkBase/spSubscriptionManager.h
    Code/SparkBasePS2/spPS2Helper.cpp
    Code/SparkBasePS2/spPS2Helper.h
    Code/SparkBasePS2/spPS2IOPModuleManager.cpp
    Code/SparkBasePS2/spPS2IOPModuleManager.h
    Code/SparkplugPS2/spPS2App.cpp
    Code/SparkplugPS2/spPS2App.h
    Code/SparkplugPS2/spPS2ErrorManager.cpp
    Code/SparkplugPS2/spPS2ErrorManager.h
    Analysis/PC/SparkBaseAbi.h
    Analysis/PS2/SparkBaseAbi.h
    Analysis/PS2/spPS2FileStreamState.h
    Analysis/PS2/spPS2AsyncFileStreamManagerState.h
)
if(WIN32)
    target_sources(SparkBase PRIVATE
        Code/SparkplugPC/spPCApp.cpp
        Code/SparkplugPC/spPCApp.h
        Code/SparkplugPC/spPCErrorManager.cpp
        Code/SparkplugPC/spPCErrorManager.h
        Code/SparkBasePC/spPCAsyncFileStreamManager.cpp
        Code/SparkBasePC/spPCAsyncFileStreamManager.h
        Code/SparkBasePC/spPCFileStream.cpp
        Code/SparkBasePC/spPCFileStream.h
    )
endif()
target_include_directories(SparkBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SparkBase PUBLIC cxx_std_17)

# The original source path places this layer in Code/Sparkplug rather than
# SparkBase.  Keep the reconstructed target boundary explicit as well.
add_library(SparkplugEngine STATIC
    Code/Sparkplug/spBoundingVolume.cpp
    Code/Sparkplug/spBoundingVolume.h
    Code/Sparkplug/spOBBBV.cpp
    Code/Sparkplug/spOBBBV.h
    Code/Sparkplug/spCollisionInfo.cpp
    Code/Sparkplug/spCollisionInfo.h
    Code/Sparkplug/spCollisionInfoSerializer.cpp
    Code/Sparkplug/spCollisionInfoSerializer.h
    Code/Sparkplug/spSceneManager.cpp
    Code/Sparkplug/spSceneManager.h
    Code/SparkplugDX/spDXLight.cpp
    Code/SparkplugDX/spDXLight.h
    Code/Sparkplug/spRenderController.cpp
    Code/Sparkplug/spRenderController.h
    Code/Sparkplug/spTextureTrack.cpp
    Code/Sparkplug/spTextureTrack.h
    Code/Sparkplug/spAnimTexController.cpp
    Code/Sparkplug/spAnimTexController.h
    Code/SparkplugDX/spDXTexture.cpp
    Code/SparkplugDX/spDXTexture.h
    Code/SparkplugDX/spDXTextureSerializer.cpp
    Code/SparkplugDX/spDXTextureSerializer.h
    Code/SparkplugDX/spDXMaterial.cpp
    Code/SparkplugDX/spDXMaterial.h
    Code/SparkplugDX/spDXMaterialSerializer.cpp
    Code/SparkplugDX/spDXMaterialSerializer.h
    Code/SparkplugDX/spDXShaderLayer.cpp
    Code/SparkplugDX/spDXShaderLayer.h
    Code/SparkplugDX/spDXShader.cpp
    Code/SparkplugDX/spDXShader.h
    Code/SparkplugDX/spDXVertexShader.cpp
    Code/SparkplugDX/spDXVertexShader.h
    Code/SparkplugDX/spDXShaderManager.cpp
    Code/SparkplugDX/spDXShaderManager.h
    Code/SparkplugPC/spPCShaderManager.cpp
    Code/SparkplugPC/spPCShaderManager.h
    Code/Sparkplug/spParser.cpp
    Code/Sparkplug/spParser.h
    Code/SparkplugPC/spPCEffectTemplate.cpp
    Code/SparkplugPC/spPCEffectTemplate.h
    Code/SparkplugPC/spPCRFXFileLoader.cpp
    Code/SparkplugPC/spPCRFXFileLoader.h
    Code/SparkplugPC/spPCVertexShader.cpp
    Code/SparkplugPC/spPCVertexShader.h
    Code/SparkplugDX/spDXVertexDeclaration.cpp
    Code/SparkplugDX/spDXVertexDeclaration.h
    Code/SparkplugPC/spPCVertexDeclaration.cpp
    Code/SparkplugPC/spPCVertexDeclaration.h
    Code/Sparkplug/spPartitionNode.cpp
    Code/Sparkplug/spPartitionNode.h
    Code/Sparkplug/spOctreeNode.cpp
    Code/Sparkplug/spOctreeNode.h
    Code/Sparkplug/spBSPNode.cpp
    Code/Sparkplug/spBSPNode.h
    Code/Sparkplug/spZonePortal.cpp
    Code/Sparkplug/spZonePortal.h
    Code/Sparkplug/spZonePortalNode.cpp
    Code/Sparkplug/spZonePortalNode.h
    Code/Sparkplug/spTaskTimer.cpp
    Code/Sparkplug/spTaskTimer.h
    Code/Sparkplug/spController.cpp
    Code/Sparkplug/spController.h
    Code/Sparkplug/spActor.cpp
    Code/Sparkplug/spActor.h
    Code/Sparkplug/spAnimationManager.cpp
    Code/Sparkplug/spAnimationManager.h
    Code/Sparkplug/spEvaluator.cpp
    Code/Sparkplug/spFunctionEval.cpp
    Code/Sparkplug/spColorEval.cpp
    Code/Sparkplug/spMaterialColorController.cpp
    Code/Sparkplug/spColorFuncEval.cpp
    Code/Sparkplug/spTransFunctionEval.cpp
    Code/Sparkplug/spUVController.cpp
    Code/Sparkplug/spEvaluator.h
    Code/Sparkplug/spSubController.cpp
    Code/Sparkplug/spSubController.h
    Code/Sparkplug/spTrack.cpp
    Code/Sparkplug/spTrack.h
    Code/Sparkplug/spAnimTrack.cpp
    Code/Sparkplug/spAnimTrack.h
    Code/Sparkplug/spAnimation.cpp
    Code/Sparkplug/spAnimation.h
    Code/Sparkplug/spAnimationSerializer.cpp
    Code/Sparkplug/spAnimationSerializer.h
    Code/Sparkplug/spTransformEval.cpp
    Code/Sparkplug/spTransformEval.h
    Code/Sparkplug/spTransformTrackEval.cpp
    Code/Sparkplug/spTransformTrackEval.h
    Code/Sparkplug/spNodeController.cpp
    Code/Sparkplug/spNodeController.h
    Analysis/PC/spAnimationMath.h
    Analysis/PC/spAnimationKeySampling.h
    Analysis/PC/spNodeTransformMath.h
    Analysis/PC/spParticleSampling.h
    Code/Sparkplug/spGameLevel.cpp
    Code/Sparkplug/spGameLevel.h
    Code/Sparkplug/spGameLevelSerializer.cpp
    Code/Sparkplug/spGameLevelSerializer.h
    Code/Sparkplug/spDXMeshData.cpp
    Code/Sparkplug/spDXMeshData.h
    Code/Sparkplug/spDXMeshDataSerializer.cpp
    Code/Sparkplug/spDXMeshDataSerializer.h
    Code/Sparkplug/spDXTextureDataSerializer.cpp
    Code/Sparkplug/spDXTextureDataSerializer.h
    Code/Sparkplug/spIndexBuffer.cpp
    Code/Sparkplug/spIndexBuffer.h
    Code/Sparkplug/spLight.cpp
    Code/Sparkplug/spLight.h
    Code/Sparkplug/spLightManager.cpp
    Code/Sparkplug/spLightManager.h
    Code/Sparkplug/spVisibilityManager.cpp
    Code/Sparkplug/spVisibilityManager.h
    Analysis/PC/spVisibilityMath.h
    Analysis/PC/spPolygonClip.h
    Code/Sparkplug/spLightData.cpp
    Code/Sparkplug/spLightData.h
    Code/Sparkplug/spLightDataSerializer.cpp
    Code/Sparkplug/spLightDataSerializer.h
    Code/Sparkplug/spLightSerializer.cpp
    Code/Sparkplug/spLightSerializer.h
    Code/Sparkplug/spMaterialSerializer.cpp
    Code/Sparkplug/spMaterialSerializer.h
    Code/Sparkplug/spMaterial.cpp
    Code/Sparkplug/spMaterial.h
    Code/Sparkplug/spMaterialData.cpp
    Code/Sparkplug/spMaterialData.h
    Code/Sparkplug/spPS2Material.cpp
    Code/Sparkplug/spPS2Material.h
    Code/Sparkplug/spMaterialPassLayer.cpp
    Code/Sparkplug/spMaterialPassLayer.h
    Code/Sparkplug/spMaterialTexture.cpp
    Code/Sparkplug/spMaterialTexture.h
    Code/Sparkplug/spMaterialTextureLayer.cpp
    Code/Sparkplug/spMaterialTextureLayer.h
    Code/Sparkplug/spStdLayer.cpp
    Code/Sparkplug/spStdLayer.h
    Code/Sparkplug/spMaterialRenderTargetTexture.cpp
    Code/Sparkplug/spMaterialRenderTargetTexture.h
    Code/Sparkplug/spMaterialCameraViewTexture.cpp
    Code/Sparkplug/spMaterialCameraViewTexture.h
    Code/Sparkplug/spMaterialCubeMapTexture.cpp
    Code/Sparkplug/spMaterialCubeMapTexture.h
    Code/Sparkplug/spMaterialDataSerializer.cpp
    Code/Sparkplug/spMaterialDataSerializer.h
    Code/Sparkplug/spDXMaterialDataSerializer.cpp
    Code/Sparkplug/spDXMaterialDataSerializer.h
    Code/Sparkplug/spPS2MaterialDataSerializer.cpp
    Code/Sparkplug/spPS2MaterialDataSerializer.h
    Code/Sparkplug/spCameraSerializer.cpp
    Code/Sparkplug/spCameraSerializer.h
    Code/Sparkplug/spCamera.cpp
    Code/Sparkplug/spCamera.h
    Code/Sparkplug/spCameraData.cpp
    Code/Sparkplug/spCameraData.h
    Code/Sparkplug/spCameraDataSerializer.cpp
    Code/Sparkplug/spCameraDataSerializer.h
    Code/Sparkplug/spDataBlockSerializer.cpp
    Code/Sparkplug/spDataBlockSerializer.h
    Code/Sparkplug/spFogSerializer.cpp
    Code/Sparkplug/spFogSerializer.h
    Code/Sparkplug/spFog.cpp
    Code/Sparkplug/spFog.h
    Code/Sparkplug/spMatColorControllerSerializer.cpp
    Code/Sparkplug/spMatColorControllerSerializer.h
    Code/Sparkplug/spLightControllerSerializer.cpp
    Code/Sparkplug/spLightControllerSerializer.h
    Code/Sparkplug/spAnimTexControllerSerializer.cpp
    Code/Sparkplug/spAnimTexControllerSerializer.h
    Code/Sparkplug/spUVControllerSerializer.cpp
    Code/Sparkplug/spUVControllerSerializer.h
    Code/Sparkplug/spTransFunctionEvalSerializer.cpp
    Code/Sparkplug/spTransFunctionEvalSerializer.h
    Code/Sparkplug/spFunctionEvalSerializer.cpp
    Code/Sparkplug/spFunctionEvalSerializer.h
    Code/Sparkplug/spColorFuncEvalSerializer.cpp
    Code/Sparkplug/spColorFuncEvalSerializer.h
    Code/Sparkplug/spSphereBVSerializer.cpp
    Code/Sparkplug/spSphereBVSerializer.h
    Code/Sparkplug/spBoxBVSerializer.cpp
    Code/Sparkplug/spBoxBVSerializer.h
    Code/Sparkplug/spOBBBVSerializer.cpp
    Code/Sparkplug/spOBBBVSerializer.h
    Code/Sparkplug/spMesh.cpp
    Code/Sparkplug/spMesh.h
    Code/Sparkplug/spMeshData.cpp
    Code/Sparkplug/spMeshData.h
    Code/Sparkplug/spMeshDataSerializer.cpp
    Code/Sparkplug/spMeshDataSerializer.h
    Code/Sparkplug/spModel.cpp
    Code/Sparkplug/spModel.h
    Code/Sparkplug/spModelSerializer.cpp
    Code/Sparkplug/spModelSerializer.h
    Code/Sparkplug/spParticleSystem.cpp
    Code/Sparkplug/spParticleSystem.h
    Code/Sparkplug/spParticleSystemSerializer.cpp
    Code/Sparkplug/spParticleSystemSerializer.h
    Code/Sparkplug/spSkin.cpp
    Code/Sparkplug/spSkin.h
    Code/Sparkplug/spSkinSerializer.cpp
    Code/Sparkplug/spSkinSerializer.h
    Code/Sparkplug/spNode.cpp
    Code/Sparkplug/spNode.h
    Code/Sparkplug/spNodeSerializer.cpp
    Code/Sparkplug/spNodeSerializer.h
    Code/Sparkplug/spPlatformSpecificMeshData.cpp
    Code/Sparkplug/spPlatformSpecificMeshData.h
    Code/Sparkplug/spPS2Mesh.cpp
    Code/Sparkplug/spPS2Mesh.h
    Code/Sparkplug/spPS2MeshData.cpp
    Code/Sparkplug/spPS2MeshData.h
    Code/Sparkplug/spPS2MeshDataSerializer.cpp
    Code/Sparkplug/spPS2MeshDataSerializer.h
    Code/Sparkplug/spPS2TextureDataSerializer.cpp
    Code/Sparkplug/spPS2TextureDataSerializer.h
    Code/Sparkplug/spRenderable.cpp
    Code/Sparkplug/spRenderable.h
    Code/Sparkplug/spRenderNode.cpp
    Code/Sparkplug/spRenderNode.h
    Code/Sparkplug/spRenderNodeSerializer.cpp
    Code/Sparkplug/spRenderNodeSerializer.h
    Code/Sparkplug/spRenderer.cpp
    Code/Sparkplug/spRenderer.h
    Code/Sparkplug/spRenderTarget.cpp
    Code/Sparkplug/spRenderTarget.h
    Code/Sparkplug/spCubeRenderTarget.cpp
    Code/Sparkplug/spCubeRenderTarget.h
    Code/Sparkplug/spRenderTargetManager.cpp
    Code/Sparkplug/spRenderTargetManager.h
    Code/Sparkplug/spSceneGraphOptimizer.cpp
    Code/Sparkplug/spSceneGraphOptimizer.h
    Code/Sparkplug/spRenderableSerializer.cpp
    Code/Sparkplug/spRenderableSerializer.h
    Code/Sparkplug/spRenderMesh.cpp
    Code/Sparkplug/spRenderMesh.h
    Code/Sparkplug/spResource.cpp
    Code/Sparkplug/spResource.h
    Code/Sparkplug/spResourceManager.cpp
    Code/Sparkplug/spResourceManager.h
    Code/Sparkplug/spResourceFATSerializer.cpp
    Code/Sparkplug/spResourceFATSerializer.h
    Code/Sparkplug/spSerializer.cpp
    Code/Sparkplug/spSerializer.h
    Code/Sparkplug/spSerializerManager.cpp
    Code/Sparkplug/spSerializerManager.h
    Code/Sparkplug/spSerializerHook.cpp
    Code/Sparkplug/spSerializerHook.h
    Code/Sparkplug/spEngineCore.cpp
    Code/Sparkplug/spEngineCore.h
    Code/Sparkplug/spDebugManager.cpp
    Code/Sparkplug/spDebugManager.h
    Code/Sparkplug/spEntityManager.cpp
    Code/Sparkplug/spEntityManager.h
    Code/Sparkplug/spFontManager.cpp
    Code/Sparkplug/spFontManager.h
    Code/Sparkplug/spInputManager.cpp
    Code/Sparkplug/spInputManager.h
    Code/Sparkplug/spTemplateManager.cpp
    Code/Sparkplug/spTemplateManager.h
    Code/Sparkplug/spTemplateInstance.cpp
    Code/Sparkplug/spTemplateInstance.h
    Code/Sparkplug/spTemplateObject.cpp
    Code/Sparkplug/spTemplateObject.h
    Code/Sparkplug/spTemplateSerializer.cpp
    Code/Sparkplug/spTemplateSerializer.h
    Code/Sparkplug/spTexture.cpp
    Code/Sparkplug/spTexture.h
    Code/Sparkplug/spTextureBuffer.cpp
    Code/Sparkplug/spTextureBuffer.h
    Code/Sparkplug/spTextureData.cpp
    Code/Sparkplug/spPalette.cpp
    Code/Sparkplug/spPalette.h
    Code/Sparkplug/spTextureData.h
    Code/Sparkplug/spTextureDataSerializer.cpp
    Code/Sparkplug/spTextureDataSerializer.h
    Code/Sparkplug/spVertexBuffer.cpp
    Code/Sparkplug/spVertexBuffer.h
    Code/SparkplugDX/spDXMesh.cpp
    Code/SparkplugDX/spDXMesh.h
    Code/SparkplugDX/spDXMeshSerializer.h
    Code/SparkplugDX/spDXCombinedVB.cpp
    Code/SparkplugDX/spDXCombinedVB.h
    Code/SparkplugDX/spDXVertexBuffer.cpp
    Code/SparkplugDX/spDXVertexBuffer.h
    Code/SparkplugDX/spDXIndexBuffer.cpp
    Code/SparkplugDX/spDXIndexBuffer.h
    Code/SparkplugDX/spDXSharedMeshData.cpp
    Code/SparkplugDX/spDXSharedMeshData.h
    Code/SparkplugDX/spDXSharedMeshDataSerializer.cpp
    Code/SparkplugDX/spDXSharedMeshDataSerializer.h
    Code/SparkplugDX/spDXRenderer.cpp
    Code/SparkplugDX/spDXRenderer.h
    Code/SparkplugDX/spDXRenderer_Init.cpp
    Code/SparkplugDX/spDXCamera.cpp
    Code/SparkplugDX/spDXCamera.h
    Code/SparkplugDX/spDXRenderTarget.cpp
    Code/SparkplugDX/spDXRenderTarget.h
    Code/SparkplugDX/spDXCubeRenderTarget.cpp
    Code/SparkplugDX/spDXCubeRenderTarget.h
    Code/SparkplugPS2/spPS2FontManager.cpp
    Code/SparkplugPS2/spPS2FontManager.h
    Code/SparkplugPS2/spPS2InputManager.cpp
    Code/SparkplugPS2/spPS2InputManager.h
    Code/SparkplugPS2/spPS2Renderer.cpp
    Code/SparkplugPS2/spPS2Renderer.h
    Code/SparkplugPS2/spPS2Camera.cpp
    Code/SparkplugPS2/spPS2Camera.h
    Code/SparkplugPS2/spPS2RenderTarget.cpp
    Code/SparkplugPS2/spPS2RenderTarget.h
    Code/SparkplugPS2/spPS2RenderTargetManager.cpp
    Code/SparkplugPS2/spPS2RenderTargetManager.h
    Analysis/PC/SparkplugAbi.h
    Analysis/PS2/SparkplugAbi.h
)
if(WIN32)
    target_sources(SparkplugEngine PRIVATE
        Code/SparkplugPC/spPCFontManager.cpp
        Code/SparkplugPC/spPCFontManager.h
        Code/SparkplugPC/spDXInputManager.cpp
        Code/SparkplugPC/spDXInputManager.h
        Code/SparkplugPC/spPCRenderer.cpp
        Code/SparkplugPC/spPCRenderer.h
        Code/SparkplugPC/spPCRenderTarget.cpp
        Code/SparkplugPC/spPCRenderTarget.h
        Code/SparkplugPC/spPCRenderTargetManager.cpp
        Code/SparkplugPC/spPCRenderTargetManager.h
    )
endif()
target_include_directories(SparkplugEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SparkplugEngine PUBLIC cxx_std_17)
target_link_libraries(SparkplugEngine PUBLIC SparkBase)

if(MSVC)
    target_compile_options(SparkBase PUBLIC /EHsc)
    target_compile_options(SparkBase PRIVATE /W4 /permissive-)
    target_compile_options(SparkplugEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(SparkBase PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(SparkplugEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(SparkBaseTests Tests/spBaseObjectTests.cpp)
    target_link_libraries(SparkBaseTests PRIVATE SparkBase)
    add_test(NAME SparkBaseTests COMMAND SparkBaseTests)

    add_executable(SparkplugEngineTests Tests/spEngineCoreTests.cpp)
    target_link_libraries(SparkplugEngineTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugEngineTests COMMAND SparkplugEngineTests)

    add_executable(SparkplugDXSharedMeshTests Tests/spDXSharedMeshTests.cpp)
    target_link_libraries(SparkplugDXSharedMeshTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugDXSharedMeshTests COMMAND SparkplugDXSharedMeshTests)
    set_tests_properties(SparkplugDXSharedMeshTests PROPERTIES TIMEOUT 20)

    add_executable(SparkplugAnimationTests Tests/spAnimationRuntimeTests.cpp)
    target_link_libraries(SparkplugAnimationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugAnimationTests COMMAND SparkplugAnimationTests)

    add_executable(SparkplugNodeWorldTests Tests/spNodeWorldTests.cpp)
    target_link_libraries(SparkplugNodeWorldTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugNodeWorldTests COMMAND SparkplugNodeWorldTests)
    add_executable(SparkplugAnimationKeyTests Tests/spAnimationKeyTests.cpp)
    target_link_libraries(SparkplugAnimationKeyTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugAnimationKeyTests COMMAND SparkplugAnimationKeyTests)
    add_executable(SparkplugAnimationObjectTests Tests/spAnimationObjectTests.cpp)
    target_link_libraries(SparkplugAnimationObjectTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugAnimationObjectTests COMMAND SparkplugAnimationObjectTests)
    add_executable(SparkplugSanReaderTests Tests/spSanReaderTests.cpp)
    target_link_libraries(SparkplugSanReaderTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSanReaderTests COMMAND SparkplugSanReaderTests)
    add_executable(SparkplugActorTests Tests/spActorTests.cpp)
    target_link_libraries(SparkplugActorTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugActorTests COMMAND SparkplugActorTests)
    add_executable(SparkplugAnimationManagerTests Tests/spAnimationManagerTests.cpp)
    target_link_libraries(SparkplugAnimationManagerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugAnimationManagerTests COMMAND SparkplugAnimationManagerTests)
    add_executable(SparkplugTransformInputTests Tests/spTransformInputTests.cpp)
    target_link_libraries(SparkplugTransformInputTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugTransformInputTests COMMAND SparkplugTransformInputTests)
    add_executable(SparkplugActorBindingTests Tests/spActorBindingTests.cpp)
    target_link_libraries(SparkplugActorBindingTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugActorBindingTests COMMAND SparkplugActorBindingTests)
    add_executable(SparkplugTaskTimerTests Tests/spTaskTimerTests.cpp)
    target_link_libraries(SparkplugTaskTimerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugTaskTimerTests COMMAND SparkplugTaskTimerTests)
    add_executable(SparkplugRenderNodeTests Tests/spRenderNodeTests.cpp)
    target_link_libraries(SparkplugRenderNodeTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRenderNodeTests COMMAND SparkplugRenderNodeTests)
    add_executable(SparkplugLightManagerTests Tests/spLightManagerTests.cpp)
    target_link_libraries(SparkplugLightManagerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugLightManagerTests COMMAND SparkplugLightManagerTests)
    add_executable(SparkplugRendererProtocolTests Tests/spRendererProtocolTests.cpp)
    target_link_libraries(SparkplugRendererProtocolTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererProtocolTests COMMAND SparkplugRendererProtocolTests)
    add_executable(SparkplugVisibilityTests Tests/spVisibilityTests.cpp)
    target_link_libraries(SparkplugVisibilityTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugVisibilityTests COMMAND SparkplugVisibilityTests)
    set_tests_properties(SparkplugVisibilityTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugDXRenderStateTests Tests/spDXRenderStateTests.cpp)
    add_executable(SparkplugMaterialApplyTests Tests/spMaterialApplyTests.cpp)
    add_executable(SparkplugRendererDrawTests Tests/spRendererDrawTests.cpp)
    add_executable(SparkplugRendererSubmitTests Tests/spRendererSubmitTests.cpp)
    add_executable(SparkplugRendererLightsTests Tests/spRendererLightsTests.cpp)
    add_executable(SparkplugDXLightTests Tests/spDXLightTests.cpp)
    add_executable(SparkplugRendererCachedDrawTests Tests/spRendererCachedDrawTests.cpp)
    add_executable(SparkplugRendererMatricesTests Tests/spRendererMatricesTests.cpp)
    add_executable(SparkplugRendererMatrixInputsTests Tests/spRendererMatrixInputsTests.cpp)
    add_executable(SparkplugShaderParameterAppendTests Tests/spShaderParameterAppendTests.cpp)
    target_link_libraries(SparkplugShaderParameterAppendTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugShaderParameterAppendTests COMMAND SparkplugShaderParameterAppendTests)
    set_tests_properties(SparkplugShaderParameterAppendTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererMatrixInputsTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererMatrixInputsTests COMMAND SparkplugRendererMatrixInputsTests)
    set_tests_properties(SparkplugRendererMatrixInputsTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererMatricesTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererMatricesTests COMMAND SparkplugRendererMatricesTests)
    set_tests_properties(SparkplugRendererMatricesTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererCachedDrawTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererCachedDrawTests COMMAND SparkplugRendererCachedDrawTests)
    set_tests_properties(SparkplugRendererCachedDrawTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugDXLightTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugDXLightTests COMMAND SparkplugDXLightTests)
    set_tests_properties(SparkplugDXLightTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererLightsTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererLightsTests COMMAND SparkplugRendererLightsTests)
    set_tests_properties(SparkplugRendererLightsTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererSubmitTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererSubmitTests COMMAND SparkplugRendererSubmitTests)
    set_tests_properties(SparkplugRendererSubmitTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugShaderManagerTests Tests/spShaderManagerTests.cpp)
    add_executable(SparkplugParserTests Tests/spParserTests.cpp)
    add_executable(SparkplugEffectTemplateTests Tests/spEffectTemplateTests.cpp)
    target_link_libraries(SparkplugEffectTemplateTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugEffectTemplateTests COMMAND SparkplugEffectTemplateTests)
    set_tests_properties(SparkplugEffectTemplateTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugShaderCompileTests Tests/spShaderCompileTests.cpp)
    target_link_libraries(SparkplugShaderCompileTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugShaderCompileTests COMMAND SparkplugShaderCompileTests)
    set_tests_properties(SparkplugShaderCompileTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugRendererGeneratedDrawTests Tests/spRendererGeneratedDrawTests.cpp)
    add_executable(SparkplugRFXFileTests Tests/spRFXFileTests.cpp)
    target_link_libraries(SparkplugRFXFileTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRFXFileTests COMMAND SparkplugRFXFileTests)
    set_tests_properties(SparkplugRFXFileTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererGeneratedDrawTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererGeneratedDrawTests COMMAND SparkplugRendererGeneratedDrawTests)
    set_tests_properties(SparkplugRendererGeneratedDrawTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugParserTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugParserTests COMMAND SparkplugParserTests)
    set_tests_properties(SparkplugParserTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugShaderConstantsTests Tests/spShaderConstantsTests.cpp)
    add_executable(SparkplugVertexShaderTests Tests/spVertexShaderTests.cpp)
    target_link_libraries(SparkplugVertexShaderTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugVertexShaderTests COMMAND SparkplugVertexShaderTests)
    set_tests_properties(SparkplugVertexShaderTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugShaderConstantsTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugShaderConstantsTests COMMAND SparkplugShaderConstantsTests)
    set_tests_properties(SparkplugShaderConstantsTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugShaderManagerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugShaderManagerTests COMMAND SparkplugShaderManagerTests)
    set_tests_properties(SparkplugShaderManagerTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererDrawTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererDrawTests COMMAND SparkplugRendererDrawTests)
    set_tests_properties(SparkplugRendererDrawTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugMaterialApplyTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugMaterialApplyTests COMMAND SparkplugMaterialApplyTests)
    set_tests_properties(SparkplugMaterialApplyTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugRendererSceneTests Tests/spRendererSceneTests.cpp)
    add_executable(SparkplugShaderLayerTests Tests/spShaderLayerTests.cpp)
    target_link_libraries(SparkplugShaderLayerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugShaderLayerTests COMMAND SparkplugShaderLayerTests)
    set_tests_properties(SparkplugShaderLayerTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugRendererSceneTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererSceneTests COMMAND SparkplugRendererSceneTests)
    set_tests_properties(SparkplugRendererSceneTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugDXRenderStateTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugDXRenderStateTests COMMAND SparkplugDXRenderStateTests)
    set_tests_properties(SparkplugDXRenderStateTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugOctreeTests Tests/spOctreeTests.cpp)
    target_link_libraries(SparkplugOctreeTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugOctreeTests COMMAND SparkplugOctreeTests)
    set_tests_properties(SparkplugOctreeTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugZonePortalTests Tests/spZonePortalTests.cpp)
    target_link_libraries(SparkplugZonePortalTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugZonePortalTests COMMAND SparkplugZonePortalTests)
    set_tests_properties(SparkplugZonePortalTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugPolygonClipTests Tests/spPolygonClipTests.cpp)
    target_link_libraries(SparkplugPolygonClipTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugPolygonClipTests COMMAND SparkplugPolygonClipTests)
    set_tests_properties(SparkplugPolygonClipTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugBspTests Tests/spBspTests.cpp)
    target_link_libraries(SparkplugBspTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugBspTests COMMAND SparkplugBspTests)
    set_tests_properties(SparkplugBspTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugVisibilityPlaneStorageTests Tests/spVisibilityPlaneStorageTests.cpp)
    target_link_libraries(SparkplugVisibilityPlaneStorageTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugVisibilityPlaneStorageTests COMMAND SparkplugVisibilityPlaneStorageTests)
    set_tests_properties(SparkplugVisibilityPlaneStorageTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugDataBlockWriterTests Tests/spDataBlockWriterTests.cpp)
    target_link_libraries(SparkplugDataBlockWriterTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugDataBlockWriterTests COMMAND SparkplugDataBlockWriterTests)
    set_tests_properties(SparkplugDataBlockWriterTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugSaveReferenceTests Tests/spSaveReferenceTests.cpp)
    target_link_libraries(SparkplugSaveReferenceTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSaveReferenceTests COMMAND SparkplugSaveReferenceTests)
    set_tests_properties(SparkplugSaveReferenceTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugReadReferenceTests Tests/spReadReferenceTests.cpp)
    target_link_libraries(SparkplugReadReferenceTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugReadReferenceTests COMMAND SparkplugReadReferenceTests)
    set_tests_properties(SparkplugReadReferenceTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugFullLoaderTests Tests/spFullLoaderTests.cpp)
    target_link_libraries(SparkplugFullLoaderTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugFullLoaderTests COMMAND SparkplugFullLoaderTests)
    set_tests_properties(SparkplugFullLoaderTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugVertexDeclarationTests Tests/spVertexDeclarationTests.cpp)
    target_link_libraries(SparkplugVertexDeclarationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugVertexDeclarationTests COMMAND SparkplugVertexDeclarationTests)
    set_tests_properties(SparkplugVertexDeclarationTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugMeshReaderTests Tests/spMeshReaderTests.cpp)
    target_link_libraries(SparkplugMeshReaderTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugMeshReaderTests COMMAND SparkplugMeshReaderTests)
    set_tests_properties(SparkplugMeshReaderTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugNodeSerializationTests Tests/spNodeSerializationTests.cpp)
    add_executable(SparkplugCollisionCoreTests Tests/spCollisionCoreTests.cpp)
    target_link_libraries(SparkplugCollisionCoreTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugCollisionCoreTests COMMAND SparkplugCollisionCoreTests)
    set_tests_properties(SparkplugCollisionCoreTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugLightSerializationTests Tests/spLightSerializationTests.cpp)
    target_link_libraries(SparkplugLightSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugLightSerializationTests COMMAND SparkplugLightSerializationTests)
    set_tests_properties(SparkplugLightSerializationTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugNodeSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugNodeSerializationTests COMMAND SparkplugNodeSerializationTests)
    set_tests_properties(SparkplugNodeSerializationTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugSceneSerializationTests Tests/spSceneSerializationTests.cpp)
    add_executable(SparkplugParticleSerializationTests Tests/spParticleSerializationTests.cpp)
    target_link_libraries(SparkplugParticleSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugParticleSerializationTests COMMAND SparkplugParticleSerializationTests)
    set_tests_properties(SparkplugParticleSerializationTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugSkinSerializationTests Tests/spSkinSerializationTests.cpp)
    add_executable(SparkplugSkinRenderTests Tests/spSkinRenderTests.cpp)
    add_executable(SparkplugSceneManagerTests Tests/spSceneManagerTests.cpp)
    add_executable(SparkplugRendererFogTests Tests/spRendererFogTests.cpp)
    target_link_libraries(SparkplugRendererFogTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugRendererFogTests COMMAND SparkplugRendererFogTests)
    set_tests_properties(SparkplugRendererFogTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugSceneManagerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSceneManagerTests COMMAND SparkplugSceneManagerTests)
    set_tests_properties(SparkplugSceneManagerTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugSkinRenderTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSkinRenderTests COMMAND SparkplugSkinRenderTests)
    set_tests_properties(SparkplugSkinRenderTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugSkinSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSkinSerializationTests COMMAND SparkplugSkinSerializationTests)
    set_tests_properties(SparkplugSkinSerializationTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugSceneSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugSceneSerializationTests COMMAND SparkplugSceneSerializationTests)
    set_tests_properties(SparkplugSceneSerializationTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugMaterialSerializationTests Tests/spMaterialSerializationTests.cpp)
    target_link_libraries(SparkplugMaterialSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugMaterialSerializationTests COMMAND SparkplugMaterialSerializationTests)
    set_tests_properties(SparkplugMaterialSerializationTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugTextureSerializationTests Tests/spTextureSerializationTests.cpp)
    add_executable(SparkplugUVFunctionTests Tests/spUVFunctionTests.cpp)
    add_executable(SparkplugColorFunctionTests Tests/spColorFunctionTests.cpp)
    add_executable(SparkplugMaterialColorTests Tests/spMaterialColorTests.cpp)
    target_link_libraries(SparkplugMaterialColorTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugMaterialColorTests COMMAND SparkplugMaterialColorTests)
    set_tests_properties(SparkplugMaterialColorTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugColorFunctionTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugColorFunctionTests COMMAND SparkplugColorFunctionTests)
    set_tests_properties(SparkplugColorFunctionTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugUVFunctionTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugUVFunctionTests COMMAND SparkplugUVFunctionTests)
    set_tests_properties(SparkplugUVFunctionTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugFunctionEvalTests Tests/spFunctionEvalTests.cpp)
    target_link_libraries(SparkplugFunctionEvalTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugFunctionEvalTests COMMAND SparkplugFunctionEvalTests)
    set_tests_properties(SparkplugFunctionEvalTests PROPERTIES TIMEOUT 20)
    add_executable(SparkplugMaterialControllerTests Tests/spMaterialControllerTests.cpp)
    target_link_libraries(SparkplugMaterialControllerTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugMaterialControllerTests COMMAND SparkplugMaterialControllerTests)
    set_tests_properties(SparkplugMaterialControllerTests PROPERTIES TIMEOUT 20)
    target_link_libraries(SparkplugTextureSerializationTests PRIVATE SparkplugEngine)
    add_test(NAME SparkplugTextureSerializationTests COMMAND SparkplugTextureSerializationTests)
    set_tests_properties(SparkplugTextureSerializationTests PROPERTIES TIMEOUT 20)
    set_tests_properties(SparkBaseTests SparkplugEngineTests SparkplugAnimationTests
        SparkplugNodeWorldTests SparkplugAnimationKeyTests SparkplugAnimationObjectTests SparkplugSanReaderTests SparkplugActorTests SparkplugAnimationManagerTests SparkplugTransformInputTests SparkplugActorBindingTests SparkplugTaskTimerTests SparkplugRenderNodeTests SparkplugLightManagerTests SparkplugRendererProtocolTests PROPERTIES TIMEOUT 20)
endif()
