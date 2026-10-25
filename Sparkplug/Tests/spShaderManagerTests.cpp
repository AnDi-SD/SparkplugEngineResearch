#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkBase/spStream.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    std::string Quote(std::string_view text)
    {
        std::string out="\"";for(unsigned char c:text)
        {if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c<32){out+="\\u00";out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}else out+=c;}return out+'"';
    }
    struct StartupFile final : spStream
    {
        std::string data,name;std::vector<std::string>& trace;bool opened=false;
        StartupFile(std::string bytes,std::string filename,std::vector<std::string>& events):data(std::move(bytes)),name(std::move(filename)),trace(events){}
        ~StartupFile() override{if(opened)(void)Close();}
        bool Open(const char* filename) override{return Open(1,filename);}
        bool Open(std::uint32_t mode,const char* filename) override
        {Check(mode==1&&filename==name&&!opened,"startup readonly filename");opened=true;trace.push_back("open:"+name);return true;}
        bool Close() override{opened=false;trace.push_back("file-close");return true;}
        bool Seek(SeekSource,std::int32_t) override{return false;}
        bool GetCurrentPosition(std::uint32_t&) const override{return false;}
        bool ReadData(void* out,std::uint32_t n) override
        {Check(n==data.size(),"startup complete file read");std::memcpy(out,data.data(),n);trace.push_back("read:"+std::to_string(n));return n!=0;}
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
        bool GetSize(std::uint32_t* n) const override{*n=static_cast<std::uint32_t>(data.size());trace.push_back("size:"+std::to_string(data.size()));return true;}
    };
    std::string Startup(const std::string& mode)
    {
        Check(mode=="normal"||mode=="minimal"||mode=="null"||mode=="xml-unresolved","known startup case");
        const std::string document=mode=="null"?"<RmDirectXEffect NAME=\"X\" TYPE=\"\"/>":mode=="minimal"?
            "<RmDirectXEffect NAME=\"X\" TYPE=\"\"><RmStringVariable NAME=\"ID\" VALUE=\"1\"/></RmDirectXEffect>":
            "<RmDirectXEffect NAME=\"X\" TYPE=\"\"><RmStringVariable NAME=\"ID\" VALUE=\"1\"/><RmPass><RmShader PIXEL_SHADER=\"FALSE\" CODE=\"x\"/></RmPass></RmDirectXEffect>";
        spPCShaderManager manager;std::vector<std::string> trace;
        spPCShaderManager::StartupForAnalysis input;
        input.directory.first=[&](std::string_view pattern,std::string&)->std::intptr_t{trace.push_back("first:"+std::string(pattern));return -1;};
        input.directory.next=[](std::intptr_t,std::string&)->bool{throw std::runtime_error("no next after failed first");};
        input.directory.close=[&](std::intptr_t handle){Check(handle==-1,"close called with invalid handle");trace.push_back("find-close:-1");};
        input.loadStandalone=[](std::string_view,bool)->std::unique_ptr<spDXShader>{throw std::runtime_error("no load in empty directory");};
        input.open=[&](std::string_view filename)->std::unique_ptr<spStream>{return std::make_unique<StartupFile>(document,std::string(filename),trace);};
        input.scanHex=[](std::string_view text,std::uint32_t& result){unsigned value=0;const int n=std::sscanf(std::string(text).c_str(),"%x",&value);if(n>0)result=value;return n;};
        input.decodeXml=[&](std::string_view text)->std::optional<spPCEffectTemplate::XmlEventsForAnalysis>
        {
            const auto* effect=manager.FixedTemplateForAnalysis();
            Check(effect&&!effect->ReadyForAnalysis()&&effect->DocumentForAnalysis()==text,"owner published before initialization");
            if(mode=="xml-unresolved")return std::nullopt;
            trace.push_back("initialize:owned");
            if(mode=="minimal")return spPCEffectTemplate::XmlEventsForAnalysis{};
            return spPCEffectTemplate::XmlEventsForAnalysis{{true,"RmPass",{}},{true,"RmShader",{{"PIXEL_SHADER","FALSE"},{"CODE","x"}}},{false,"RmShader",{}},{false,"RmPass",{}}};
        };
        spPCEffectTemplate::CompilerStateForAnalysis state;std::string diagnostic;
        const auto result=manager.StartForAnalysis("root\\",input,state,diagnostic);const auto* effect=manager.FixedTemplateForAnalysis();
        Check(result.nativeReturn==(mode!="null")&&result.completed==(mode!="xml-unresolved"),"distinct native AL and analytical completion");
        Check(!state.active,"normal and metadata failure clear active flag");
        const auto count=trace.size();
        Check(!manager.StartForAnalysis("root\\",input,state,diagnostic).completed&&trace.size()==count,"repeat refused before mutation");
        std::ostringstream out;out<<'['<<int(result.nativeReturn)<<',';
        if(effect)out<<'['<<effect->IdentityForAnalysis()<<','<<Quote(effect->NameForAnalysis())<<','<<effect->KindForAnalysis()<<','<<int(effect->ReadyForAnalysis())<<','<<effect->PassesForAnalysis().size()<<']';else out<<"null";
        out<<','<<int(state.active)<<",[";bool first=true;for(const auto& event:trace){if(!first)out<<',';first=false;out<<Quote(event);}out<<"]]";return out.str();
    }
    void DirectoryScan()
    {
        spPCShaderManager manager;std::vector<std::string> trace;spDXShaderManager::DirectoryForAnalysis directory;
        directory.first=[&](std::string_view glob,std::string& name)->std::intptr_t{trace.push_back(std::string(glob));name="first";return glob.find("vsh")!=std::string_view::npos?11:22;};
        directory.next=[&](std::intptr_t handle,std::string& name){if(handle==11&&name=="first"){name="rejected";return true;}return false;};
        directory.close=[&](std::intptr_t handle){trace.push_back("close"+std::to_string(handle));};
        auto load=[&](std::string_view name,bool pixel)->std::unique_ptr<spDXShader>
        {trace.push_back(std::string(pixel?"PS:":"VS:")+std::string(name));if(name=="rejected")return nullptr;return std::make_unique<spPCVertexShader>();};
        Check(manager.ScanStandaloneForAnalysis("root\\",directory,load),"native base scan completes");
        Check(trace==std::vector<std::string>({"root\\Shaders\\*.vsh","VS:first","VS:rejected","close11","root\\Shaders\\*.psh","PS:first","close22"}),"VS then PS; retain only nonnull; close each cohort before next");
        Check(manager.VertexShadersForAnalysis().size()==1&&manager.PixelShadersForAnalysis().size()==1,"two separately owned stage vectors");
        const auto count=trace.size();Check(!manager.ScanStandaloneForAnalysis("root\\",directory,load)&&trace.size()==count,"repeat scan safe-domain guard");
    }
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(auto v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::string Manager(const std::string& mode)
    {
        spPCShaderManager manager;Check(manager.IsKindOf(spDXShaderManager::ClassID),"original manager inheritance");
        std::map<const spPCVertexShader*,unsigned> tokens{{nullptr,0}};
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        auto separate=[&]{if(!first)out<<',';first=false;};
        auto snapshot=[&](const spPCShaderManager& target)
        {
            separate();out<<'[';bool leading=true;
            for(const auto& [key,shader]:target.GetCacheForAnalysis())
            {if(!leading)out<<',';leading=false;out<<'['<<key[0]<<','<<key[1]<<','<<tokens.at(shader.get())<<']';}out<<']';
        };
        if(mode=="cache"||mode=="clone")
        {
            const std::array<spPCShaderManager::KeyForAnalysis,6> keys{{{257,0},{1,256},{257,1},{1,1},{256,0},{2,0}}};
            unsigned ordinal=0;
            for(auto key:keys)
            {
                auto shader=std::make_unique<spPCVertexShader>();tokens[shader.get()]=++ordinal;
                Check(manager.CacheShaderForAnalysis(key,shader)&&!shader,"actual unique cache ownership");
            }
            snapshot(manager);
            for(auto key:keys)
            {const auto selected=manager.SelectCachedForAnalysis(key);Check(selected.completed,"known cache/no-weight branch");separate();out<<'['<<key[0]<<','<<key[1]<<','<<tokens.at(selected.shader)<<']';}
            if(mode=="clone")
            {
                auto owner=manager.Clone();auto* clone=dynamic_cast<spPCShaderManager*>(owner.get());
                Check(clone&&clone->GetCacheForAnalysis().empty(),"original manager blank clone");snapshot(*clone);
            }
            for(auto key:std::array<spPCShaderManager::KeyForAnalysis,3>{{{1,2},{65537,0},{3,0}}})
            {
                Check(!manager.SelectCachedForAnalysis(key).completed,"cache miss is not a fake generated NULL");
                separate();out<<'['<<key[0]<<','<<key[1]<<",false]";
            }
        }
        else
        {
            for(auto key:std::array<spPCShaderManager::KeyForAnalysis,3>{{{0,0},{0x12345670,0xabcdef12},{0xfffffff0,0xffffffff}}})
            {auto selected=manager.SelectCachedForAnalysis(key);Check(selected.completed&&!selected.shader,"zero blend weights");separate();out<<'['<<key[0]<<','<<key[1]<<",0]";}
            snapshot(manager);
        }
        out<<"]]";return out.str();
    }
    struct Input
    {
        unsigned flags=0,color=4,power=0;std::optional<std::vector<unsigned>> types;
        std::array<unsigned,8> uv{};
    };
    std::string Key(const std::string& mode)
    {
        std::vector<Input> inputs;
        if(mode=="components")
        {
            inputs.push_back({});for(unsigned bit=0;bit<20;++bit){Input x;x.flags=1u<<bit;inputs.push_back(x);}
            for(unsigned flags:{0xffffffffu,0x7fffeu,0x20019u}){Input x;x.flags=flags;inputs.push_back(x);}
        }
        else if(mode=="colors")for(unsigned color:{0u,1u,2u,3u,4u,5u,6u,7u,15u,16u,255u,0xffffffffu})
        {Input x;x.flags=0x180e;x.color=color;inputs.push_back(x);}
        else if(mode=="lights")
        {
            for(unsigned n=0;n<9;++n){Input x;x.flags=0x2012;x.types=std::vector<unsigned>();for(unsigned i=0;i<n;++i)x.types->push_back(i%3);inputs.push_back(x);}
            Input x;x.flags=0x2012;x.types=std::vector<unsigned>{0xffffffff,5,7};inputs.push_back(x);x.types.reset();inputs.push_back(x);
        }
        else if(mode=="uv")
        {
            for(unsigned mask=0;mask<256;++mask){Input x;x.flags=0x2012;for(unsigned i=0;i<8;++i)x.uv[i]=mask&(1u<<i)?2:0;inputs.push_back(x);}
            for(unsigned raw:{1u,3u,4u,8u,0xffffffffu}){Input x;x.flags=0x2012;x.uv.fill(raw);inputs.push_back(x);}
        }
        else if(mode=="power")for(unsigned power:{0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,0x7fc00000u})
        {Input x;x.flags=0x2012;x.power=power;inputs.push_back(x);}
        else if(mode=="fixed")for(unsigned flags:{0u,1u,0x800u,0x40801u})
        {Input x;x.flags=flags;x.power=0x3f800000;x.types=std::vector<unsigned>{0,1,2};x.uv.fill(2);inputs.push_back(x);}
        else throw std::runtime_error("unknown key mode");
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        for(const auto& input:inputs)
        {
            float power;std::memcpy(&power,&input.power,4);
            const auto key=spDXRenderer::BuildShaderKeyForAnalysis(input.flags,input.color,power,input.types?&*input.types:nullptr,input.uv);
            Check(key.has_value(),"bounded original key inputs");if(!first)out<<',';first=false;
            out<<"[["<<input.flags<<','<<input.color<<','<<input.power<<',';
            if(input.types)Array(out,*input.types);else out<<"null";out<<',';Array(out,input.uv);out<<"],";Array(out,*key);out<<']';
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--startup")std::cout<<Startup(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--case")std::cout<<Manager(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--key")std::cout<<Key(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"factory","no-shader","cache","clone"})Manager(mode);
            for(const char* mode:{"components","colors","lights","uv","power","fixed"})Key(mode);
            for(const char* mode:{"normal","minimal","null","xml-unresolved"})Startup(mode);DirectoryScan();
            const std::vector<unsigned> oversized(9);Check(!spDXRenderer::BuildShaderKeyForAnalysis(0,0,0,&oversized,{}),"host light-array bound");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": shader manager/key\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
