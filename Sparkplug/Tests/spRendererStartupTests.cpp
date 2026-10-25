#include "Code/SparkplugDX/spDXRenderer_Startup.h"
#include "Code/SparkBase/spStream.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
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
    {out<<'[';bool first=true;for(const auto& event:events){if(!first)out<<',';first=false;out<<'"';for(char c:event){if(c=='\\'||c=='"')out<<'\\';out<<c;}out<<'"';}out<<']';}
    struct File final : spStream
    {
        std::string bytes,name;std::vector<std::string>& events;bool opened=false;
        File(std::string b,std::string n,std::vector<std::string>& e):bytes(std::move(b)),name(std::move(n)),events(e){}
        ~File() override{if(opened)(void)Close();}
        bool Open(const char* n) override{return Open(1,n);}
        bool Open(std::uint32_t mode,const char* n) override{Check(mode==1&&n==name&&!opened,"readonly shader open");opened=true;events.push_back("open:"+name);return true;}
        bool Close() override{opened=false;events.push_back("file-close");return true;}
        bool Seek(SeekSource,std::int32_t) override{return false;}
        bool GetCurrentPosition(std::uint32_t&) const override{return false;}
        bool ReadData(void* out,std::uint32_t n) override{Check(n==bytes.size(),"whole shader document read");std::memcpy(out,bytes.data(),n);events.push_back("read:"+std::to_string(n));return n!=0;}
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
        bool GetSize(std::uint32_t* n) const override{*n=static_cast<std::uint32_t>(bytes.size());events.push_back("size:"+std::to_string(bytes.size()));return true;}
    };
    struct Context
    {
        std::vector<std::string> events;std::uint32_t remaining=0;std::uintptr_t next=100;
        Renderer::PresentParametersForAnalysis lastPP{};bool created=false;
        static std::int32_t Index(void* c,std::uint32_t bytes,std::uint32_t usage,std::uint32_t format,std::uint32_t pool,std::uintptr_t* out) noexcept
        {auto& x=*static_cast<Context*>(c);x.events.push_back("index:"+std::to_string(bytes)+":"+std::to_string(usage)+":"+std::to_string(format)+":"+std::to_string(pool));*out=x.next++;return -1;}
        static std::uint32_t Release(void* c,std::uintptr_t handle) noexcept
        {auto& x=*static_cast<Context*>(c);x.events.push_back("release:"+std::to_string(handle));return x.remaining;}
        static std::int32_t Render(void* c,std::uint32_t index,std::uint32_t value) noexcept
        {auto& x=*static_cast<Context*>(c);x.events.push_back("w:"+std::to_string(index)+":"+std::to_string(value));return -1;}
    };
    Renderer::StartupAPIForAnalysis API(Context& c,const std::string& mode,Renderer::StartupGlobalsForAnalysis& globals,
        spPCEffectTemplate::CompilerStateForAnalysis& compiler)
    {
        Renderer::StartupAPIForAnalysis api;api.device.indexContext=&c;api.device.createIndex=&Context::Index;api.device.releaseIndex=&Context::Release;
        api.device.adapterCount=[&](std::uintptr_t){c.events.push_back("count");return 1u;};
        api.device.adapterDescription=[&](std::uintptr_t,std::uint32_t index)->std::optional<std::array<char,17>>{c.events.push_back("description:"+std::to_string(index));std::array<char,17> out{};out[0]='X';return out;};
        api.device.displayFormat=[&](std::uintptr_t,std::uint32_t index)->std::optional<std::uint32_t>{c.events.push_back("display:"+std::to_string(index));return 0x16;};
        api.device.capabilities=[&](std::uintptr_t,std::uint32_t a,std::uint32_t t)->std::optional<Renderer::DeviceCapabilitiesForAnalysis>{c.events.push_back("caps:"+std::to_string(a)+":"+std::to_string(t));return Renderer::DeviceCapabilitiesForAnalysis{0xfffe0101,0xffff0101};};
        api.device.create=[&](std::uintptr_t,std::uint32_t a,std::uint32_t t,std::uintptr_t w,std::uint32_t flags,Renderer::PresentParametersForAnalysis& pp,std::uintptr_t& device){c.events.push_back("create:"+std::to_string(a)+":"+std::to_string(t)+":"+std::to_string(w)+":"+std::to_string(flags));c.lastPP=pp;c.created=true;device=0xd309;return -1;};
        api.createDirect3D=[&,mode](std::uint32_t sdk){c.events.push_back("d3d:"+std::to_string(sdk));return mode=="null-interface"?std::uintptr_t(0):std::uintptr_t(0xd301);};
        api.releaseDevice=[&](std::uintptr_t handle){Check(handle==0xd309,"release device identity");c.events.push_back("device-release");return 7u;};
        api.releaseDirect3D=[&](std::uintptr_t handle){Check(handle==0xd301,"release Direct3D identity");c.events.push_back("d3d-release");return 9u;};
        api.readback.render=[](std::uintptr_t,std::uint32_t,std::uint32_t&)->std::int32_t{throw std::runtime_error("warm reject must skip readback");};
        api.readback.texture=[](std::uintptr_t,std::uint32_t,std::uint32_t,std::uint32_t&)->std::int32_t{throw std::runtime_error("warm reject must skip readback");};
        api.readback.submit=&Context::Render;api.readback.context=&c;
        api.shader.directory.first=[&](std::string_view pattern,std::string&)->std::intptr_t{c.events.push_back("first:"+std::string(pattern));return -1;};
        api.shader.directory.next=[](std::intptr_t,std::string&)->bool{throw std::runtime_error("no next after invalid first");};
        api.shader.directory.close=[&](std::intptr_t handle){Check(handle==-1,"invalid directory close retained");c.events.push_back("find-close:-1");};
        api.shader.loadStandalone=[](std::string_view,bool)->std::unique_ptr<spDXShader>{throw std::runtime_error("empty directory has no shader load");};
        api.shader.open=[&,mode](std::string_view name)->std::unique_ptr<spStream>
        {return std::make_unique<File>(mode=="null-template"?"<RmDirectXEffect NAME=\"X\" TYPE=\"\"/>":"<RmDirectXEffect NAME=\"X\" TYPE=\"\"><RmStringVariable NAME=\"ID\" VALUE=\"1\"/></RmDirectXEffect>",std::string(name),c.events);};
        api.shader.scanHex=[](std::string_view text,std::uint32_t& out){unsigned v=0;const int n=std::sscanf(std::string(text).c_str(),"%x",&v);if(n>0)out=v;return n;};
        api.shader.decodeXml=[&](std::string_view)->std::optional<spPCEffectTemplate::XmlEventsForAnalysis>{Check(globals.shaders&&globals.shaders->FixedTemplateForAnalysis(),"manager and template published before init");c.events.push_back("initialize:owned");return spPCEffectTemplate::XmlEventsForAnalysis{};};
        api.shaderRoot="root\\";api.shaderState=&compiler;return api;
    }
    void Owned(Renderer::StartupStateForAnalysis& state,Context& c)
    {
        state.device.device=0xd309;state.device.direct3D=0xd301;state.active=1;
        for(unsigned n=0;n<3;++n)
        {
            auto index=std::make_shared<spDXIndexBuffer>();Check(index->CreateDeviceBufferForAnalysis(16,8,0x65,1,&Context::Index,&Context::Release,&c),"owned live index created");
            if(!n)state.device.persistentIndices=std::move(index);else state.device.transientIndices.push_back(std::move(index));
        }
        c.events.clear();
    }
    void Common(std::ostream& out,const Renderer::StartupStateForAnalysis& state,unsigned result)
    {
        out<<'['<<result<<',';Array(out,state.device.copiedConfiguration);const auto& i=state.commonIndices;const auto& v=state.commonVertices;
        out<<",["<<int(i.IsInitializedForAnalysis())<<','<<static_cast<unsigned>(i.GetTypeForAnalysis())<<','<<i.GetPrimitiveCountForAnalysis()<<','<<i.GetIndexCountForAnalysis()<<','<<i.GetFormatFlagsForAnalysis()<<"],[";
        out<<int(v.IsInitializedForAnalysis())<<','<<v.GetComponentFlagsForAnalysis()<<','<<v.GetVertexCountForAnalysis()<<','<<v.GetVertexSizeForAnalysis()<<','<<v.GetFlagsForAnalysis()<<']';
    }
    std::string Run(const std::string& phase,const std::string& mode)
    {
        Context c;c.remaining=mode=="owned-retained"?7:0;Renderer::StartupStateForAnalysis state;Renderer::StartupGlobalsForAnalysis globals;
        spPCEffectTemplate::CompilerStateForAnalysis compiler;auto api=API(c,mode,globals,compiler);Renderer::DeviceConfigurationForAnalysis config{640,480,2,1,1,0,0x12345678,0};
        std::ostringstream out;
        if(phase=="teardown")
        {
            Check(mode=="empty"||mode=="owned-zero"||mode=="owned-retained","known teardown case");if(mode!="empty")Owned(state,c);
            Check(Renderer::ReleaseStartupDeviceForAnalysis(state,api),"teardown completes");
            out<<"[["<<int(bool(state.device.persistentIndices))<<",0,0,0],["<<state.device.device<<','<<state.device.direct3D<<','<<+state.active<<"],";Events(out,c.events);out<<']';
        }
        else if(phase=="common")
        {
            Check(mode=="empty"||mode=="owned-zero"||mode=="owned-retained","known common case");if(mode!="empty")Owned(state,c);
            const auto r=Renderer::InitializeCommonStartupForAnalysis(state,config,192,96,api);Check(r.completed&&r.nativeReturn,"common startup completes");
            Common(out,state,1);out<<',';Events(out,c.events);out<<']';
        }
        else
        {
            Check(phase=="whole"&&(mode=="minimal"||mode=="null-interface"||mode=="null-template"),"known whole startup case");
            std::string diagnostic;const auto r=Renderer::StartDeviceForAnalysis(state,globals,config,192,96,api,diagnostic);
            Check(r.completed&&r.nativeReturn==(mode=="minimal"),"whole startup native return");
            Common(out,state,r.nativeReturn?1:0);out<<",["<<int(bool(state.device.direct3D))<<','<<int(bool(state.device.device))<<','<<+state.active<<"],";
            Array(out,config);out<<',';if(c.created)Array(out,c.lastPP);else out<<"null";
            out<<','<<state.scratchCount<<",[";bool first=true;for(const auto& m:state.scratch){if(!first)out<<',';first=false;Array(out,m);}out<<"],";
            out<<int(bool(globals.shaders&&globals.shaders->FixedTemplateForAnalysis()))<<',';Events(out,c.events);out<<',';
            if(mode=="minimal")Check(state.scratch.size()==128&&state.scratch[127][15]==0x3f800000,"all scratch records initialized");
            // The same explicit owner teardown closes retained device handles.
            const auto before=c.events.size();Check(Renderer::ReleaseStartupDeviceForAnalysis(state,api),"post-startup teardown");
            Events(out,std::vector<std::string>(c.events.begin()+before,c.events.end()));out<<']';
            state.commonIndices.ReleaseForAnalysis();state.commonVertices.ReleaseForAnalysis();globals.shaders.reset();globals.errors.reset();
        }
        return out.str();
    }
    std::string SelectorInput(std::uint32_t index,std::string words)
    {
        Renderer::DeviceStateSelectorForAnalysis state;state.dirty=1;std::replace(words.begin(),words.end(),',',' ');std::istringstream input(words);std::uint32_t word;
        while(input>>word)state.table.push_back(word);
        Check(input.eof()&&state.table.size()<=256,"bounded integer table");const auto r=Renderer::SelectDeviceStateForAnalysis(state,index);Check(r.has_value(),"bounded selector result");
        std::ostringstream out;out<<'['<<+*r<<','<<+state.dirty<<',';Array(out,state.cache);out<<']';return out.str();
    }
    void Guards()
    {
        Context c;Renderer::StartupStateForAnalysis state;Renderer::StartupGlobalsForAnalysis globals;spPCEffectTemplate::CompilerStateForAnalysis compiler;
        auto api=API(c,"minimal",globals,compiler);api.maximumCpuBytes=100;Renderer::DeviceConfigurationForAnalysis config{};
        state.active=1;Check(!Renderer::InitializeCommonStartupForAnalysis(state,config,192,96,api).completed&&state.active==1&&c.events.empty(),"host allocation bound precedes teardown");
        Check(!Renderer::CreateStartupIndexForAnalysis(state,192,api).completed,"missing native device cannot become successful index creation");
        globals.shaders=std::make_shared<spPCShaderManager>();
        Check(globals.shaders->ScanStandaloneForAnalysis("root\\",api.shader.directory,api.shader.loadStandalone),"explicit previously scanned manager");
        c.events.clear();std::string diagnostic;
        Check(!Renderer::StartDeviceForAnalysis(state,globals,config,192,96,api,diagnostic).completed&&state.active==1&&c.events.empty(),"unsupported repeated shader-manager startup refused before renderer mutation");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==4&&std::string(argv[1])=="--selector-table")std::cout<<SelectorInput(static_cast<std::uint32_t>(std::stoul(argv[2])),argv[3])<<'\n';
        else if(argc==3){const std::string option=argv[1];Check(option.rfind("--",0)==0,"explicit startup phase");std::cout<<Run(option.substr(2),argv[2])<<'\n';}
        else
        {
            for(const char* phase:{"teardown","common"})for(const char* mode:{"empty","owned-zero","owned-retained"})Run(phase,mode);
            for(const char* mode:{"minimal","null-interface","null-template"})Run("whole",mode);
            SelectorInput(171,"0,255,999");Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC renderer startup/teardown\n";
        }
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
