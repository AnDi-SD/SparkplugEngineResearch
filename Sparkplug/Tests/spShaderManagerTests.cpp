#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
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
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Manager(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--key")std::cout<<Key(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"factory","no-shader","cache","clone"})Manager(mode);
            for(const char* mode:{"components","colors","lights","uv","power","fixed"})Key(mode);
            const std::vector<unsigned> oversized(9);Check(!spDXRenderer::BuildShaderKeyForAnalysis(0,0,0,&oversized,{}),"host light-array bound");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": shader manager/key\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
