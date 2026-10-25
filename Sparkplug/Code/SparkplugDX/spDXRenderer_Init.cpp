#include "spDXRenderer.h"
#include "spDXVertexBuffer.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord DXRendererRecord{
            spDXRenderer::ClassID,
            spRenderer::ClassID,
            "spDXRenderer",
            &spRenderer::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool DXRendererRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(DXRendererRecord);
    }

    const spRTTIRecord& spDXRenderer::StaticRTTI() noexcept
    {
        (void)DXRendererRegistered;
        return DXRendererRecord;
    }

    std::optional<std::uint8_t> spDXRenderer::SelectDeviceStateForAnalysis(
        DeviceStateSelectorForAnalysis& state,std::uint32_t index) noexcept
    {
        if(index>=state.cache.size()||(state.dirty&&state.table.size()>256))return std::nullopt;
        if(!state.dirty)return state.cache[index];
        std::uint8_t found=0;
        for(std::uint32_t candidate=0;candidate<256;++candidate)
        {
            found=0; // Original431782; back-edge4317A4 targets THIS reset.
            for(const auto value:state.table)
                if(value==candidate){found=1;break;}
        }
        // Preserve the unusual original: neither fill all256 cache bytes nor
        // replace the candidate with index. Final found is membership of255.
        state.cache[index]=found;state.dirty=0;
        return found;
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::RestoreDeviceStateCachesForAnalysis(
        std::uintptr_t device,DeviceStateCachesForAnalysis& state,DeviceStateSelectorForAnalysis& selector,
        const DeviceStateReadbackForAnalysis& api)
    {
        if(!device||!api.render||!api.texture||!api.submit||(selector.dirty&&selector.table.size()>256))return {};
        constexpr std::uint32_t invalid=0xffffffffu;
        // Shared454940 has no false branch for these valid captured arrays.
        InvalidateCacheWordsForAnalysis(state.rawRender.data(),state.rawRender.size());
        InvalidateCacheWordsForAnalysis(state.rawTexture.data(),state.rawTexture.size());
        state.materialE47C=state.fogCA20=invalid;
        for(std::uint32_t index=0;index<state.render.size();++index)
        {
            const auto selected=SelectDeviceStateForAnalysis(selector,index);
            if(!selected)return {};
            if(!*selected||api.render(device,index,state.render[index])!=0)state.render[index]=invalid;
        }
        for(std::uint32_t stage=0;stage<state.texture.size();++stage)
            for(std::uint32_t index=0;index<state.texture[stage].size();++index)
            {
                const auto selected=SelectDeviceStateForAnalysis(selector,index);
                if(!selected)return {};
                auto& entry=state.texture[stage][index];
                if(!*selected||api.texture(device,stage,index,entry)!=0)entry=invalid;
            }
        state.lightListC190=0;
        constexpr std::array<std::array<std::uint32_t,2>,5> desired{{{143,1},{27,1},{15,1},{24,192},{25,7}}};
        for(const auto& entry:desired)
            (void)ApplyRenderStateCacheEntryForAnalysis(state.render[entry[0]],entry[0],entry[1],api.submit,api.context);
        state.textureIdentitiesE480.fill(invalid);
        state.diffuseSourceE4E8=11;state.ambientSourceE4EC=10;
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::QueryDeviceCapabilitiesForAnalysis(
        DeviceLifecycleStateForAnalysis& state,std::uint32_t adapter,std::uint32_t type,const DeviceLifecycleAPIForAnalysis& api)
    {
        if(!state.direct3D||!api.capabilities)return {};
        const auto caps=api.capabilities(state.direct3D,adapter,type);if(!caps)return {};
        // PC4BCC80 ignores GetDeviceCaps HRESULT, then unsigned-compares the
        // output version tokens. Optional absence is a host initialized-data
        // bound, not an invented HRESULT failure branch.
        state.pixelShaders=caps->pixelVersion>=0xffff0101u;
        state.vertexShaders=caps->vertexVersion>=0xfffe0101u;
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::CreateDeviceForAnalysis(DeviceLifecycleStateForAnalysis& state,
        const DeviceConfigurationForAnalysis& config,PresentParametersForAnalysis& parameters,const DeviceLifecycleAPIForAnalysis& api)
    {
        if(!state.direct3D||!api.adapterCount||!api.capabilities||!api.adapterDescription||!api.create)return {};
        std::uint32_t adapter=0;
        if(!(config[5]&4))
        {const auto count=api.adapterCount(state.direct3D);if(count>256)return {};adapter=count-1;}
        const auto caps=QueryDeviceCapabilitiesForAnalysis(state,adapter,1,api);if(!caps.completed)return {};
        std::uint32_t flags=state.vertexShaders?0x40u:0x20u,type=1;
        const std::uint32_t extra=(config[5]&2)?4u:0u;
        if(config[5]&8){type=2;flags=0x20;}
        static constexpr std::array<char,17> perfHUD{'N','V','I','D','I','A',' ','N','V','P','e','r','f','H','U','D','\0'};
        // The original re-queries adapter count after each nonmatching item.
        // Do not sort, cache or silently skip an uninitialized identifier.
        const auto initialCount=api.adapterCount(state.direct3D);if(initialCount>256)return {};
        if(initialCount>0)
        {
            std::uint32_t i=0;
            do
            {
                const auto description=api.adapterDescription(state.direct3D,i);if(!description)return {};
                if(*description==perfHUD){adapter=i;type=2;flags=0x20;break;}
                ++i;
                const auto count=api.adapterCount(state.direct3D);if(count>256)return {};
                if(i>=count)break;
            }while(true);
        }
        // Actual4BCE3C writes the field directly and does not Release its old
        // value or test HRESULT. The provider may leave that slot unchanged.
        (void)api.create(state.direct3D,adapter,type,config[6],flags|extra,parameters,state.device);
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::BuildPresentParametersForAnalysis(DeviceLifecycleStateForAnalysis& state,
        DeviceConfigurationForAnalysis& config,PresentParametersForAnalysis& out,const DeviceLifecycleAPIForAnalysis& api)
    {
        const bool windowed=(config[4]&255)!=0;
        // Native diagnostic string lookup is unsafe outside0..3. No blanket
        // zeroing: only Reset/startup callers initialize the fourteen words.
        if(!state.direct3D||(!windowed&&config[2]>3)||(!(config[5]&4)&&!api.adapterCount)||
            (windowed&&!api.displayFormat)||(!windowed&&(config[2]==0||config[2]==1||config[2]==2)&&!api.checkDeviceType))return {};
        out[0]=config[0];out[1]=config[1];out[6]=windowed?1u:2u;
        out[9]=1;out[10]=config[3]==1?0x50u:0x4bu;
        std::uint32_t adapter=0;
        if(!(config[5]&4))
        {const auto count=api.adapterCount(state.direct3D);if(count>256)return {};adapter=count-1;}
        if(windowed)
        {
            const auto format=api.displayFormat(state.direct3D,adapter);if(!format)return {};
            if(*format==0x29){state.errorCode75AB9C=0x10000001;return {true,false};}
            out[8]=1;out[2]=*format;out[3]=1;
            config[2]=*format>=20&&*format<=22?2u:((*format>=23&&*format<=26)||*format==30)?1u:3u;
        }
        else
        {
            out[13]=1;if(!config[2])config[2]=2;
            if(config[2]==1||config[2]==2)
            {
                const std::uint32_t first=config[2]==1?0x17u:0x14u;
                const std::uint32_t secondAdapter=config[2]==1?0x18u:0x16u;
                const std::uint32_t secondBuffer=config[2]==1?0x18u:0x15u;
                if(api.checkDeviceType(state.direct3D,0,1,first,first,false)==0)out[2]=first;
                else if(api.checkDeviceType(state.direct3D,0,1,secondAdapter,secondBuffer,false)==0)out[2]=secondBuffer;
                else {state.errorCode75AB9C=0x10000002;return {true,false};}
            }
            out[3]=1+static_cast<std::uint32_t>((config[5]&1)!=0);
        }
        return {true,true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::ResetDeviceForAnalysis(DeviceLifecycleStateForAnalysis& state,
        DeviceConfigurationForAnalysis& config,std::uint32_t indexBytes,const DeviceLifecycleAPIForAnalysis& api,const ResetResourcesForAnalysis& resources)
    {
        if(!state.direct3D)return {true,false};
        // Validate required portable virtual/device inputs before mutation.
        // A missing implementation never becomes a successful reset.
        if(!state.device||!state.persistentIndices||state.persistentIndices->IsInitializedForAnalysis()||
            !api.reset||!api.createIndex||!api.releaseIndex||(state.offscreenC1C0&&!resources.unbindOrdinaryTarget))return {};
        for(const auto& index:state.transientIndices)if(!index||index->IsInitializedForAnalysis())return {};
        for(const auto* cohort:{&resources.ordinary,&resources.cube})for(const auto& target:*cohort)if(!target.release||!target.reinit)return {};
        for(const auto& layer:resources.layers)if(!layer.release||!layer.reinit)return {};
        for(const auto& texture:resources.textures)if(!texture)return {};
        state.copiedConfiguration=config; // Actual4549A0 is a copy, AL=1.
        PresentParametersForAnalysis parameters{};
        const auto prepared=BuildPresentParametersForAnalysis(state,config,parameters,api);
        if(!prepared.completed)return {}; // Host-domain failure, not its AL.
        // Original4BD810 ignores BuildPresentParameters AL, even on failure.
        if(!state.persistentIndices->ReleaseNativeDeviceBufferForAnalysis())return {};
        for(auto& index:state.transientIndices)
        {
            if(!index->ReleaseNativeDeviceBufferForAnalysis())return {};
            index.reset(); // record destructor drops its owned relationship
        }
        std::vector<std::shared_ptr<spDXIndexBuffer>>().swap(state.transientIndices);
        if(state.offscreenC1C0)resources.unbindOrdinaryTarget(); // secondary+4, NOT EndScene
        for(const auto& texture:resources.textures)texture(false);
        // Actual45CD10: ordinary/cube release is void; only layer AL gates.
        for(const auto* cohort:{&resources.ordinary,&resources.cube})for(const auto& target:*cohort)target.release();
        for(const auto& layer:resources.layers)if(!layer.release())return {true,false};
        (void)api.reset(state.device,parameters); // Actual4BD972 ignores HRESULT.
        // Actual45CE70 stops at the first failure; it does not aggregate.
        for(const auto* cohort:{&resources.ordinary,&resources.cube})for(const auto& target:*cohort)if(!target.reinit())return {true,false};
        for(const auto& layer:resources.layers)if(!layer.reinit())return {true,false};
        for(const auto& texture:resources.textures)texture(true);
        state.offscreenC1C0=state.wordC9EC=state.wordC9F0=0;
        if(!state.persistentIndices->CreateDeviceBufferForAnalysis(indexBytes,0x208,0x65,0,api.createIndex,api.releaseIndex,api.indexContext))return {};
        state.declarationC9FC=state.wordCA00=0;
        state.boundIndices.reset();state.boundVertices.reset();
        state.paletteCA0C=state.boundVertexE44C=state.boundPixelE450=0xffffffffu;
        state.lightIdentitiesC198.fill(0);state.lightDirtyC1B8.fill(1);
        if(!resources.restore)return {false,false,true};
        const auto restored=resources.restore();if(!restored)return {false,false,true};
        return {true,*restored,false};
    }
}
