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
endif()
