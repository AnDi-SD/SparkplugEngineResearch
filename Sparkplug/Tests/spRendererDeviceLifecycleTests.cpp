#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
using Renderer=spDXRenderer;
namespace
{
    unsigned checks=0;
    void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
    template<class T>void Array(std::ostream& out,const T& values)
    {out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<+value;}out<<']';}
    void Events(std::ostream& out,const std::vector<std::string>& events)
    {out<<'[';bool first=true;for(const auto& event:events){if(!first)out<<',';first=false;out<<'"'<<event<<'"';}out<<']';}
    struct IndexCalls
    {
        std::vector<std::string>& events;std::uintptr_t next=100;
        std::uint32_t remaining=0;bool leaveOutput=false;
        static std::int32_t Create(void* context,std::uint32_t bytes,std::uint32_t usage,std::uint32_t format,std::uint32_t pool,std::uintptr_t* out) noexcept
        {
            auto& self=*static_cast<IndexCalls*>(context);
            self.events.push_back("index:"+std::to_string(bytes)+":"+std::to_string(usage)+":"+std::to_string(format)+":"+std::to_string(pool));
            if(!self.leaveOutput)*out=self.next++;
            return static_cast<std::int32_t>(0x80004005u);
        }
        static std::uint32_t Release(void* context,std::uintptr_t handle) noexcept
        {auto& self=*static_cast<IndexCalls*>(context);self.events.push_back("release:"+std::to_string(handle));return self.remaining;}
    };
    Renderer::DeviceLifecycleAPIForAnalysis API(std::vector<std::string>& events)
    {
        Renderer::DeviceLifecycleAPIForAnalysis api;
        api.adapterCount=[&](std::uintptr_t){events.push_back("count");return 3u;};
        api.capabilities=[&](std::uintptr_t,std::uint32_t adapter,std::uint32_t type)->std::optional<Renderer::DeviceCapabilitiesForAnalysis>
        {events.push_back("caps:"+std::to_string(adapter)+":"+std::to_string(type));return Renderer::DeviceCapabilitiesForAnalysis{0xfffe0101,0xffff0101};};
        api.adapterDescription=[&](std::uintptr_t,std::uint32_t adapter)->std::optional<std::array<char,17>>
        {events.push_back("description:"+std::to_string(adapter));std::array<char,17> result{};result[0]='X';return result;};
        api.displayFormat=[&](std::uintptr_t,std::uint32_t adapter)->std::optional<std::uint32_t>
        {events.push_back("display:"+std::to_string(adapter));return 0x16;};
        api.checkDeviceType=[&](std::uintptr_t,std::uint32_t adapter,std::uint32_t type,std::uint32_t display,std::uint32_t buffer,bool windowed)
        {events.push_back("check:"+std::to_string(adapter)+":"+std::to_string(type)+":"+std::to_string(display)+":"+std::to_string(buffer)+":"+std::to_string(windowed));return 0;};
        api.create=[&](std::uintptr_t,std::uint32_t adapter,std::uint32_t type,std::uintptr_t window,std::uint32_t flags,Renderer::PresentParametersForAnalysis& pp,std::uintptr_t& device)
        {events.push_back("create:"+std::to_string(adapter)+":"+std::to_string(type)+":"+std::to_string(window)+":"+std::to_string(flags));device=0xabcd;pp[3]=77;return static_cast<std::int32_t>(0x80004005u);};
        api.reset=[&](std::uintptr_t,Renderer::PresentParametersForAnalysis& pp)
        {events.push_back("reset");pp[3]=77;return static_cast<std::int32_t>(0x80004005u);};
        return api;
    }
    std::string Create(const std::string& mode)
    {
        std::vector<std::string> events;auto api=API(events);Renderer::DeviceLifecycleStateForAnalysis state;
        state.direct3D=0xd301;state.device=0x1234;Renderer::DeviceConfigurationForAnalysis config{};config[6]=0x12345678;
        if(mode=="adapter0")config[5]=4;
        else if(mode=="multithreaded")config[5]=2;
        else if(mode=="ref")config[5]=8;
        else if(mode=="software")api.capabilities=[&](std::uintptr_t,std::uint32_t a,std::uint32_t t)->std::optional<Renderer::DeviceCapabilitiesForAnalysis>
        {events.push_back("caps:"+std::to_string(a)+":"+std::to_string(t));return Renderer::DeviceCapabilitiesForAnalysis{0xfffe0100,0xffff0100};};
        else if(mode=="perfhud")
        {
            config[5]=2;api.adapterDescription=[&](std::uintptr_t,std::uint32_t adapter)->std::optional<std::array<char,17>>
            {events.push_back("description:"+std::to_string(adapter));std::array<char,17> result{};const std::string name=adapter==1?"NVIDIA NVPerfHUD":"X";std::copy(name.begin(),name.end(),result.begin());return result;};
        }
        else if(mode=="create-failure")api.create=[&](std::uintptr_t,std::uint32_t a,std::uint32_t t,std::uintptr_t w,std::uint32_t flags,Renderer::PresentParametersForAnalysis&,std::uintptr_t&)
        {events.push_back("create:"+std::to_string(a)+":"+std::to_string(t)+":"+std::to_string(w)+":"+std::to_string(flags));return static_cast<std::int32_t>(0x80004005u);};
        else Check(mode=="hardware","known CreateDevice case");
        Renderer::PresentParametersForAnalysis pp;for(unsigned i=0;i<pp.size();++i)pp[i]=0x100+i;
        const auto result=Renderer::CreateDeviceForAnalysis(state,config,pp,api);
        Check(result.completed&&result.nativeReturn,"CreateDevice HRESULT ignored");
        Check(state.device==(mode=="create-failure"?0x1234u:0xabcdu),"direct output can stay unchanged on failure");
        std::ostringstream out;out<<"[1,["<<int(state.pixelShaders)<<','<<int(state.vertexShaders)<<','<<state.device<<"],";Array(out,pp);out<<',';Events(out,events);out<<']';return out.str();
    }
    std::string Parameters(const std::string& mode)
    {
        std::vector<std::string> events;auto api=API(events);Renderer::DeviceLifecycleStateForAnalysis state;state.direct3D=0xd301;state.errorCode75AB9C=0xaabbccdd;
        Renderer::DeviceConfigurationForAnalysis config{640,480,2,1,1,0,0x12345678,0};
        if(mode=="windowed-p8")api.displayFormat=[&](std::uintptr_t,std::uint32_t adapter)->std::optional<std::uint32_t>{events.push_back("display:"+std::to_string(adapter));return 0x29;};
        else if(mode!="windowed")
        {
            config[4]=0;config[5]=1;
            if(mode=="fullscreen-auto")config[2]=0;
            else if(mode=="fullscreen16")config[2]=1;
            else if(mode=="fallback"||mode=="formats-fail")
            {
                api.checkDeviceType=[&](std::uintptr_t,std::uint32_t a,std::uint32_t t,std::uint32_t d,std::uint32_t b,bool w)
                {events.push_back("check:"+std::to_string(a)+":"+std::to_string(t)+":"+std::to_string(d)+":"+std::to_string(b)+":"+std::to_string(w));return mode=="fallback"&&d==0x16?0:-1;};
            }
            else Check(mode=="fullscreen","known present parameters case");
        }
        Renderer::PresentParametersForAnalysis pp;for(unsigned i=0;i<pp.size();++i)pp[i]=0x200+i;
        const auto result=Renderer::BuildPresentParametersForAnalysis(state,config,pp,api);
        Check(result.completed,"known present parameter output domain");
        Check(result.nativeReturn==(mode!="windowed-p8"&&mode!="formats-fail"),"native PP error gates");
        Check(pp[7]==0x207&&pp[11]==0x20b&&pp[12]==0x20c,"helper does not zero untouched PP words");
        std::ostringstream out;out<<'['<<int(result.nativeReturn)<<',';Array(out,config);out<<',';Array(out,pp);out<<','<<state.errorCode75AB9C<<',';Events(out,events);out<<']';return out.str();
    }
    std::string Reset(const std::string& mode)
    {
        std::vector<std::string> events;auto api=API(events);IndexCalls index{events};
        api.createIndex=&IndexCalls::Create;api.releaseIndex=&IndexCalls::Release;api.indexContext=&index;
        Renderer::DeviceLifecycleStateForAnalysis state;state.direct3D=0xd301;state.device=0xd309;
        state.persistentIndices=std::make_shared<spDXIndexBuffer>();
        Check(state.persistentIndices->CreateDeviceBufferForAnalysis(16,8,0x65,1,&IndexCalls::Create,&IndexCalls::Release,&index),"initial live index handle");
        state.offscreenC1C0=mode=="restore-boundary"||mode=="target-only-fail"?0:1;state.wordC9EC=7;state.wordC9F0=8;state.declarationC9FC=9;state.wordCA00=10;state.paletteCA0C=11;state.boundVertexE44C=12;state.boundPixelE450=13;
        state.lightIdentitiesC198.fill(99);state.lightDirtyC1B8.fill(0);
        Renderer::ResetResourcesForAnalysis resources;
        if(mode=="target-only-fail")resources.ordinary.push_back({[&]{events.push_back("ordinary-release");},[&]{events.push_back("ordinary-init");return false;}});
        else if(mode!="restore-boundary")
        {
            resources.unbindOrdinaryTarget=[&]{events.push_back("unbind-target");};
            resources.textures.push_back([&](bool reinit){events.push_back(reinit?"texture-init":"texture-release");});
            resources.ordinary.push_back({[&]{events.push_back("ordinary-release");},[&]{events.push_back("ordinary-init");return mode!="target-fail";}});
            resources.cube.push_back({[&]{events.push_back("cube-release");},[&]{events.push_back("cube-init");return true;}});
            resources.layers.push_back({[&]{events.push_back("layer-release");return mode!="layer-fail";},[&]{events.push_back("layer-init");return true;}});
            resources.restore=[&]()->std::optional<bool>{events.push_back("restore");return mode!="restore-fail";};
        }
        Renderer::DeviceConfigurationForAnalysis config{640,480,2,1,1,0,0x12345678,0};
        if(mode=="parameters-fail")api.displayFormat=[&](std::uintptr_t,std::uint32_t adapter)->std::optional<std::uint32_t>{events.push_back("display:"+std::to_string(adapter));return 0x29;};
        else Check(mode=="restore-boundary"||mode=="target-only-fail"||mode=="complete"||mode=="layer-fail"||mode=="target-fail"||mode=="restore-fail","known reset case");
        events.clear();const auto result=Renderer::ResetDeviceForAnalysis(state,config,128,api,resources);
        const bool early=mode=="layer-fail"||mode=="target-fail"||mode=="target-only-fail";
        Check(result.restoreRequired==(mode=="restore-boundary"),"missing restore explicitly incomplete");
        Check(result.completed==(mode!="restore-boundary")&&result.nativeReturn==(!early&&mode!="restore-boundary"&&mode!="restore-fail"),"resource failure gates and ignored API errors");
        if(!early)
        {
            Check(state.persistentIndices->GetByteSizeForAnalysis()==128&&state.lightDirtyC1B8[7]==1,"index recreation and all light dirties");
            Check(state.boundVertexE44C==0xffffffffu&&state.boundPixelE450==0xffffffffu,"shader cache invalidation");
        }
        if(mode=="target-fail")Check(std::find(events.begin(),events.end(),"cube-init")==events.end(),"reinit is fail-fast");
        if(mode=="layer-fail")Check(std::find(events.begin(),events.end(),"reset")==events.end(),"release failure precedes DeviceReset");
        Check(state.copiedConfiguration[2]==2,"copy precedes mutable PP configuration");
        std::ostringstream out;out<<'[';if(result.restoreRequired)out<<"null";else out<<int(result.nativeReturn);
        out<<",["<<state.offscreenC1C0<<','<<state.wordC9EC<<','<<state.wordC9F0<<','<<state.declarationC9FC<<','<<state.wordCA00<<','<<state.paletteCA0C<<','<<state.boundVertexE44C<<','<<state.boundPixelE450<<','<<state.persistentIndices->DeviceBufferForAnalysis()<<','<<state.persistentIndices->GetByteSizeForAnalysis()<<"],";
        Array(out,state.lightIdentitiesC198);out<<',';Array(out,state.lightDirtyC1B8);out<<',';Events(out,events);out<<']';return out.str();
    }
    void BufferContracts()
    {
        std::vector<std::string> events;IndexCalls context{events};
        {
            spDXIndexBuffer buffer;
            Check(buffer.CreateDeviceBufferForAnalysis(32,0x208,0x65,0,&IndexCalls::Create,&IndexCalls::Release,&context),"native index AL ignores failed HRESULT");
            context.remaining=7;Check(buffer.ReleaseNativeDeviceBufferForAnalysis()&&buffer.DeviceBufferForAnalysis()==100,"nonzero Release count preserves pointer");
            context.leaveOutput=true;Check(buffer.CreateDeviceBufferForAnalysis(64,8,0x66,1,&IndexCalls::Create,&IndexCalls::Release,&context)&&buffer.DeviceBufferForAnalysis()==100,"failed create preserves direct output slot");
            Check(!buffer.InitializeForAnalysis(16,8,0x65,1),"live/CPU modes do not mix");
        }
        Check(events.back()=="release:100","destructor releases handle retained by helper");
        {
            context.leaveOutput=false;spDXVertexBuffer buffer;
            Check(buffer.CreateDeviceBufferForAnalysis(48,8,0x112,1,&IndexCalls::Create,&IndexCalls::Release,&context)&&buffer.GetFVFCodeForAnalysis()==0x112,"vertex wrapper stores FVF after API failure");
            Check(!buffer.IsInitializedForAnalysis(),"live path is not a CPU byte shadow");
        }
        spDXIndexBuffer host;Check(host.InitializeForAnalysis(16,8,0x65,1)&&!host.CreateDeviceBufferForAnalysis(16,8,0x65,1,&IndexCalls::Create,&IndexCalls::Release,&context),"host mode remains separate");
    }
    void GuardContracts()
    {
        std::vector<std::string> events;auto api=API(events);Renderer::DeviceLifecycleStateForAnalysis state;state.direct3D=0xd301;
        Renderer::DeviceConfigurationForAnalysis config{};Renderer::PresentParametersForAnalysis pp{};
        api.adapterCount=[](std::uintptr_t){return 257u;};
        Check(!Renderer::CreateDeviceForAnalysis(state,config,pp,api).completed&&events.empty(),"finite host adapter work bound before capabilities/create");
        api=API(events);api.capabilities=[](std::uintptr_t,std::uint32_t,std::uint32_t)->std::optional<Renderer::DeviceCapabilitiesForAnalysis>{return std::nullopt;};
        Check(!Renderer::CreateDeviceForAnalysis(state,config,pp,api).completed&&!state.device,"missing initialized API bytes stays incomplete");
        config[2]=4;pp.fill(77);
        Check(!Renderer::BuildPresentParametersForAnalysis(state,config,pp,api).completed&&pp[0]==77,"unsafe diagnostic table index rejected before writes");
        state.direct3D=0;const auto result=Renderer::ResetDeviceForAnalysis(state,config,0,{},{});
        Check(result.completed&&!result.nativeReturn,"native missing Direct3D interface false before resource inputs");
    }
    std::string Selector(const std::string& mode)
    {
        Renderer::DeviceStateSelectorForAnalysis state;std::uint32_t index=0;
        for(unsigned i=0;i<state.cache.size();++i)state.cache[i]=static_cast<std::uint8_t>((i*29+17)&255);
        state.dirty=1;
        if(mode=="cold-empty"){}
        else if(mode=="cold-zero"){index=7;state.table={0};}
        else if(mode=="cold-255"){index=171;state.table={255};}
        else if(mode=="cold-mixed"){index=255;state.table={0,255,999};state.dirty=128;}
        else if(mode=="cold-sixteen"){index=63;for(unsigned i=0;i<16;++i)state.table.push_back(i);}
        else
        {
            state.dirty=0;std::uint8_t value=0;
            if(mode=="warm-zero"){}
            else if(mode=="warm-one"){index=1;value=1;}
            else if(mode=="warm-two"){index=7;value=2;}
            else if(mode=="warm-high"){index=171;value=128;}
            else {Check(mode=="warm-ff","known selector case");index=255;value=255;}
            state.cache[index]=value;
        }
        const auto before=state.cache;const auto dirty=state.dirty;
        const auto result=Renderer::SelectDeviceStateForAnalysis(state,index);
        Check(result.has_value(),"bounded selector completed");
        const auto expected=dirty?static_cast<std::uint8_t>(mode=="cold-255"||mode=="cold-mixed"):before[index];
        Check(*result==expected&&!state.dirty,"literal final candidate and raw warm AL");
        for(unsigned i=0;i<before.size();++i)Check(state.cache[i]==(dirty&&i==index?expected:before[i]),"only requested cache byte overwritten");
        std::ostringstream out;out<<'['<<+*result<<','<<+state.dirty<<',';Array(out,state.cache);out<<']';return out.str();
    }
    struct RestoreCalls
    {
        Renderer::DeviceStateCachesForAnalysis& state;std::vector<std::string>& events;
        static std::int32_t Submit(void* context,std::uint32_t index,std::uint32_t value) noexcept
        {
            auto& self=*static_cast<RestoreCalls*>(context);
            self.events.push_back("w:"+std::to_string(index)+":"+std::to_string(value)+":"+
                std::to_string(self.state.render[index])+":"+std::to_string(self.state.lightListC190));
            return static_cast<std::int32_t>(0x80004005u);
        }
    };
    std::string Restore(const std::string& mode)
    {
        constexpr std::uint32_t poison=0xa5a5a5a5u;
        Renderer::DeviceStateCachesForAnalysis state;
        state.rawRender.fill(poison);state.rawTexture.fill(poison);state.render.fill(poison);
        for(auto& stage:state.texture)stage.fill(poison);
        state.materialE47C=state.fogCA20=state.lightListC190=poison;
        state.textureIdentitiesE480.fill(poison);state.diffuseSourceE4E8=state.ambientSourceE4EC=poison;
        Renderer::DeviceStateSelectorForAnalysis selector;
        if(mode=="none")selector.cache.fill(0);
        else if(mode=="mixed")for(unsigned i=0;i<selector.cache.size();++i)selector.cache[i]=i%3==0?128:i%3==1?0:255;
        else {Check(mode=="all"||mode=="readback-failure"||mode=="forced-equal"||mode=="cold-zero"||mode=="cold-255","known restore case");selector.cache.fill(1);}
        if(mode=="cold-zero"||mode=="cold-255"){selector.dirty=1;selector.table={mode=="cold-255"?255u:0u};}
        std::vector<std::string> events;RestoreCalls context{state,events};Renderer::DeviceStateReadbackForAnalysis api;
        api.render=[&](std::uintptr_t device,std::uint32_t index,std::uint32_t& out)
        {
            Check(device==0xd309&&&out==&state.render[index],"direct render readback output slot");
            Check(state.materialE47C==0xffffffffu&&state.fogCA20==0xffffffffu&&state.rawTexture[71]==0xffffffffu,"shared invalidation before readback");
            events.push_back("r:"+std::to_string(index));out=0x50000000u|index;
            if(mode=="forced-equal")for(const auto& entry:std::array<std::array<unsigned,2>,5>{{{143,1},{27,1},{15,1},{24,192},{25,7}}})if(index==entry[0])out=entry[1];
            return mode=="readback-failure"&&index==171?-1:0;
        };
        api.texture=[&](std::uintptr_t device,std::uint32_t stage,std::uint32_t index,std::uint32_t& out)
        {
            Check(device==0xd309&&&out==&state.texture[stage][index],"direct texture readback output slot");
            events.push_back("t:"+std::to_string(stage)+":"+std::to_string(index));out=0x60000000u|(stage<<8)|index;
            return mode=="readback-failure"&&index==2?-1:0;
        };
        api.submit=&RestoreCalls::Submit;api.context=&context;
        const auto result=Renderer::RestoreDeviceStateCachesForAnalysis(0xd309,state,selector,api);
        Check(result.completed&&result.nativeReturn,"full restore completes");
        Check(state.render[143]==1&&state.render[24]==192&&state.render[25]==7,"forced desired states cached despite HRESULT");
        Check(state.lightListC190==0&&state.diffuseSourceE4E8==11&&state.ambientSourceE4EC==10,"restore tail fields");
        if(mode=="readback-failure")Check(state.render[171]==0xffffffffu&&state.texture[7][2]==0xffffffffu,"nonzero HRESULT overrides output bytes");
        if(mode=="forced-equal")Check(events.size()==768,"equal desired values suppress all5 writes");
        std::ostringstream out;out<<"[1,";Array(out,state.rawRender);out<<',';Array(out,state.rawTexture);out<<",["<<state.materialE47C<<','<<state.fogCA20<<"],";
        Array(out,state.render);out<<",[";for(unsigned stage=0;stage<8;++stage){if(stage)out<<',';Array(out,state.texture[stage]);}out<<"],";
        out<<state.lightListC190<<',';Array(out,state.textureIdentitiesE480);out<<",["<<state.diffuseSourceE4E8<<','<<state.ambientSourceE4EC<<"],["<<+selector.dirty<<',';Array(out,selector.cache);out<<"],";Events(out,events);out<<']';return out.str();
    }
    void RestoreGuards()
    {
        Renderer::DeviceStateSelectorForAnalysis selector;selector.dirty=1;selector.cache.fill(77);selector.table.resize(257);
        Check(!Renderer::SelectDeviceStateForAnalysis(selector,0)&&selector.dirty==1&&selector.cache[0]==77,"oversize dirty table rejected before mutation");
        selector.dirty=0;Check(Renderer::SelectDeviceStateForAnalysis(selector,255)==77,"warm path ignores unused oversize table");
        Check(!Renderer::SelectDeviceStateForAnalysis(selector,256),"native unsafe cache index explicitly refused");
        Renderer::DeviceStateCachesForAnalysis state;state.materialE47C=71;
        Check(!Renderer::RestoreDeviceStateCachesForAnalysis(0,state,selector,{}).completed&&state.materialE47C==71,"missing readback API stays incomplete before mutation");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--create")std::cout<<Create(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--parameters")std::cout<<Parameters(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--reset")std::cout<<Reset(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--selector")std::cout<<Selector(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--restore")std::cout<<Restore(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"hardware","software","adapter0","multithreaded","ref","perfhud","create-failure"})Create(mode);
            for(const char* mode:{"windowed","windowed-p8","fullscreen","fullscreen16","fullscreen-auto","fallback","formats-fail"})Parameters(mode);
            for(const char* mode:{"restore-boundary","target-only-fail","complete","layer-fail","target-fail","restore-fail","parameters-fail"})Reset(mode);
            BufferContracts();GuardContracts();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC device lifecycle\n";
            for(const char* mode:{"cold-empty","cold-zero","cold-255","cold-mixed","cold-sixteen","warm-zero","warm-one","warm-two","warm-high","warm-ff"})Selector(mode);
            for(const char* mode:{"none","mixed","all","readback-failure","forced-equal","cold-zero","cold-255"})Restore(mode);
            RestoreGuards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC selector and full cache restore\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
