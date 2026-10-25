#include "spDXRenderer_Startup.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        struct IncompleteStandalone {};
        bool StartupInputs(const spDXRenderer::StartupStateForAnalysis& state,
            const spDXRenderer::StartupAPIForAnalysis& api)
        {
            if((state.device.device&&!api.releaseDevice)||(state.device.direct3D&&!api.releaseDirect3D))return false;
            if(state.device.persistentIndices&&state.device.persistentIndices->IsInitializedForAnalysis())return false;
            for(const auto& index:state.device.transientIndices)if(!index||index->IsInitializedForAnalysis())return false;
            return true;
        }

        spDXRenderer::LifecycleResultForAnalysis StartupError(spDXRenderer::StartupGlobalsForAnalysis& globals,
            std::uint32_t line,const char* message)
        {
            // Existing recovered error implementation, rather than a generic
            // success callback for native manager/error construction. Native
            // source path and numeric severity/code are actual call inputs.
            try
            {
                if(!globals.errors)globals.errors=std::make_shared<spPCErrorManager>();
                spError error(spErrorSeverity::Error,1,"Z:\\Sparkplug\\Code\\SparkplugDX\\spDXRenderer_Init.cpp",line);
                try
                {
                    (void)globals.errors->StoreMessageForAnalysis(error,message);
                    globals.errors->LinkErrorForAnalysis(error);
                    (void)globals.errors->HandleForAnalysis(spErrorSeverity::Warning);
                }
                catch(...){globals.errors->ClearForAnalysis();throw;}
            }
            catch(...){return {};}
            return {true,false};
        }
    }

    bool spDXRenderer::ReleaseStartupDeviceForAnalysis(StartupStateForAnalysis& state,const StartupAPIForAnalysis& api)
    {
        if(!StartupInputs(state,api))return false;
        if(state.device.persistentIndices)
        {
            if(!state.device.persistentIndices->ReleaseNativeDeviceBufferForAnalysis())return false;
            state.device.persistentIndices.reset();
        }
        for(auto& index:state.device.transientIndices)
        {
            if(!index->ReleaseNativeDeviceBufferForAnalysis())return false;
            index.reset();
        }
        std::vector<std::shared_ptr<spDXIndexBuffer>>().swap(state.device.transientIndices);
        if(state.device.device)
        {
            (void)api.releaseDevice(state.device.device);
            state.device.device=0; // Native ignores remaining COM refcount.
        }
        if(state.device.direct3D)
        {
            (void)api.releaseDirect3D(state.device.direct3D);
            state.device.direct3D=0;
        }
        state.active=0;
        return true;
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::InitializeCommonStartupForAnalysis(
        StartupStateForAnalysis& state,const DeviceConfigurationForAnalysis& configuration,
        std::uint32_t indexCountInput,std::uint32_t rawVertexBytes,const StartupAPIForAnalysis& api)
    {
        const auto bytes=static_cast<std::uint64_t>(indexCountInput/3)*6+rawVertexBytes;
        if(bytes>api.maximumCpuBytes)return {}; // host guard before teardown
        if(!ReleaseStartupDeviceForAnalysis(state,api))return {};
        state.device.copiedConfiguration=configuration;
        // Actual4548E0 ignores both native initialization ALs and returns1.
        // The portable bodies can additionally reject host allocation; that
        // is incomplete, never an invented native false. Inputs are bounded
        // by the caller's allocation budget, not an original fixed maximum.
        if(!state.commonIndices.InitializeForAnalysis(indexCountInput/3,spIndexBuffer::eIndexBufferType::Type2,0))return {};
        if(!state.commonVertices.InitializeRawForAnalysis(rawVertexBytes))return {};
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::CreateStartupIndexForAnalysis(
        StartupStateForAnalysis& state,std::uint32_t indexBytes,const StartupAPIForAnalysis& api)
    {
        if(!state.device.device||!api.device.createIndex||!api.device.releaseIndex)return {};
        std::shared_ptr<spDXIndexBuffer> replacement;
        try{replacement=std::make_shared<spDXIndexBuffer>();}catch(...){return {};}
        // Native4BD2D0 allocates/constructs the new object before dropping its
        // previous intrusive owner. Shared ownership is the established safe
        // host analogue; uint16 wrap and provider reentry are not modeled.
        state.device.persistentIndices.reset();
        state.device.persistentIndices=std::move(replacement);
        if(!state.device.persistentIndices->CreateDeviceBufferForAnalysis(indexBytes,0x208,0x65,0,
            api.device.createIndex,api.device.releaseIndex,api.device.indexContext))return {};
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::StartDeviceForAnalysis(
        StartupStateForAnalysis& state,StartupGlobalsForAnalysis& globals,DeviceConfigurationForAnalysis& configuration,
        std::uint32_t indexCountInput,std::uint32_t rawVertexBytes,const StartupAPIForAnalysis& api,std::string& diagnostic)
    {
        if(!api.createDirect3D)return {};
        if(globals.shaders&&globals.shaders->HasStartupStateForAnalysis())return {};
        const auto common=InitializeCommonStartupForAnalysis(state,configuration,indexCountInput,rawVertexBytes,api);
        if(!common.completed||!common.nativeReturn)return common;
        state.device.direct3D=api.createDirect3D(0x20);
        if(!state.device.direct3D)return StartupError(globals,0x166,"Direct3DCreateX failed");
        // Original always queries adapter0 diagnostics before preparing PP.
        if(!api.device.adapterDescription)return {};
        if(!api.device.adapterDescription(state.device.direct3D,0))return {};
        PresentParametersForAnalysis parameters{};
        const auto prepared=BuildPresentParametersForAnalysis(state.device,configuration,parameters,api.device);
        if(!prepared.completed)return {};
        const auto created=CreateDeviceForAnalysis(state.device,configuration,parameters,api.device);
        if(!created.completed)return {};
        if(!created.nativeReturn)return StartupError(globals,0x175,"Failed to create D3D device");
        state.device.offscreenC1C0=state.device.wordC9EC=state.device.wordC9F0=0;
        const auto indexed=CreateStartupIndexForAnalysis(state,indexCountInput,api);
        if(!indexed.completed)return {};
        if(!indexed.nativeReturn)return StartupError(globals,0x180,"Failed to create dynamic buffers");
        state.active=1;
        const auto restored=RestoreDeviceStateCachesForAnalysis(state.device.device,state.caches,state.selector,api.readback);
        if(!restored.completed)return {false,false,true};
        if(!restored.nativeReturn)return StartupError(globals,0x194,"Failed to initialize render states");
        // Original frees old scratch BEFORE allocation, creates128 no-op
        // records4A7B40 and writes identity with actual434660 on each record.
        std::vector<MatrixStateForAnalysis::RawMatrix>().swap(state.scratch);
        try{state.scratch.resize(128);}catch(...){state.scratchCount=128;return {};}
        state.scratchCount=128;
        for(auto& matrix:state.scratch)
        {
            matrix.fill(0);
            matrix[0]=matrix[5]=matrix[10]=matrix[15]=0x3f800000;
        }
        if(!api.shaderState)return {};
        try{if(!globals.shaders)globals.shaders=std::make_shared<spPCShaderManager>();}catch(...){return {};}
        auto shaderInput=api.shader;
        auto standalone=api.standalone;
        standalone.deviceShadersEnabled=state.device.pixelShaders; // originalF2F5
        shaderInput.loadStandalone=[&](std::string_view filename,bool pixel)->std::unique_ptr<spDXShader>
        {
            auto loaded=spPCShaderManager::LoadStandaloneFileForAnalysis(api.shaderRoot,filename,pixel,
                standalone,*api.shaderState,globals.errors);
            globals.unreleasedLibraryInterfaces.insert(globals.unreleasedLibraryInterfaces.end(),loaded.unreleased.begin(),loaded.unreleased.end());
            if(!loaded.completed)throw IncompleteStandalone{};
            return std::move(loaded.shader);
        };
        spPCShaderManager::StartupResultForAnalysis shaders;
        try{shaders=globals.shaders->StartForAnalysis(api.shaderRoot,shaderInput,*api.shaderState,diagnostic);}
        catch(const IncompleteStandalone&){return {};}
        if(!shaders.completed)return {};
        if(!shaders.nativeReturn)return StartupError(globals,0x1af,"Failed to initialize shader manager");
        return {true,true};
    }
}
