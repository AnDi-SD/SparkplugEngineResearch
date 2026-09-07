#include "Code/SparkplugPC/spPCRFXFileLoader.h"
#include "Code/SparkBase/spStream.h"
#include "Code/SparkplugPC/spPCVertexShader.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    std::string Hex(std::string_view bytes)
    {std::string out;for(unsigned char c:bytes){out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}return out;}
    std::string Quote(std::string_view text)
    {
        std::string out="\"";
        for(unsigned char c:text)
        {
            if(c=='"'||c=='\\'){out+='\\';out+=c;}
            else if(c<32){out+="\\u00";out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}
            else out+=c;
        }
        return out+'"';
    }
    std::string Document(std::string id="1234ABCD",std::string name="Tiny")
    {return "<RmDirectXEffect NAME=\""+name+"\" TYPE=\"DirectX\"><RmStringVariable NAME=\"ID\" VALUE=\""+id+"\"/></RmDirectXEffect>";}
    void Replace(std::string& value,const std::string& a,const std::string& b){value.replace(value.find(a),a.size(),b);}
    std::map<std::string,std::string> Cases()
    {
        auto lower=Document();for(char& c:lower)if(c>='A'&&c<='Z')c+=32;
        auto nl=Document();Replace(nl,"VALUE=","\nVALUE=");Replace(nl,"TYPE=","TYPE\n=");
        auto spaced=Document();Replace(spaced,"ABCD\"/>","ABCD\" />");
        auto reordered=Document();Replace(reordered,"NAME=\"Tiny\" TYPE=\"DirectX\"","TYPE=\"DirectX\" NAME=\"Tiny\"");
        return {{"basic",Document()},{"signed",Document("-1")},{"hex-prefix-tail",Document(" \t+0x7fTAIL")},
            {"lowercase",lower},{"newlines",nl},{"raw-entity",Document("1234ABCD","A&amp;B")},
            {"empty-name",Document("1234ABCD","")},{"duplicates",Document("1","First")+Document("2","Second")},
            {"nul-tail",Document()+std::string("\0Ignored trailing data",22)},
            {"missing-id","<RmDirectXEffect NAME=\"Tiny\" TYPE=\"DirectX\"/>"},
            {"missing-name","<RmStringVariable NAME=\"ID\" VALUE=\"1\"/>"},
            {"invalid-id",Document("xyz")},{"spaced-close",spaced},{"reordered-name",reordered}};
    }
    struct Lifetime{unsigned opens=0,reads=0,closes=0;};
    struct File final : spStream
    {
        std::string data;Lifetime& count;bool opened=false;
        File(std::string bytes,Lifetime& c):data(std::move(bytes)),count(c){}
        ~File() override{if(opened)(void)Close();}
        bool Open(const char* name) override{return Open(1,name);}
        bool Open(std::uint32_t mode,const char* name) override
        {if(mode!=1||std::string(name)!="fixture.rfx"||opened)return false;opened=true;++count.opens;return true;}
        bool Close() override{opened=false;++count.closes;return true;}
        bool Seek(SeekSource,std::int32_t) override{return false;}
        bool GetCurrentPosition(std::uint32_t&) const override{return false;}
        bool ReadData(void* out,std::uint32_t n) override
        {if(n!=data.size())return false;std::memcpy(out,data.data(),n);++count.reads;return n!=0;}
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
        bool GetSize(std::uint32_t* n) const override{*n=static_cast<std::uint32_t>(data.size());return true;}
    };
    std::string Capture(const std::string& mode)
    {
        const auto data=Cases().at(mode);Lifetime lifetime;spPCRFXFileLoader loader;
        spPCEffectTemplate::CompilerStateForAnalysis state;std::string diagnostic;std::ostringstream scans;bool first=true;
        auto scan=[&](std::string_view input,std::uint32_t& identity)
        {
            const std::string text(input);unsigned int value=0;const int result=std::sscanf(text.c_str(),"%x",&value);
            if(result>0)identity=value;
            if(!first)scans<<',';first=false;scans<<'['<<Quote(input)<<','<<result<<',';
            if(result>0)scans<<value;else scans<<"null";scans<<']';return result;
        };
        auto effect=loader.LoadFileForAnalysis(std::make_unique<File>(data,lifetime),"fixture.rfx",scan,state,diagnostic);
        Check(!state.active,"original completed file paths clear active flag");
        Check(bool(effect)==diagnostic.empty(),"success versus original diagnostic");
        Check(lifetime.opens==1&&lifetime.reads==1&&lifetime.closes==1,"file consumed and destroyed once");
        std::ostringstream out;out<<'['<<Quote(mode)<<',';
        if(effect)
        {
            Check(effect->KindForAnalysis()==2&&!effect->ReadyForAnalysis()&&effect->DocumentForAnalysis()==data.substr(0,data.find('\0')),"uninitialized exact type2 template");
            out<<'['<<effect->IdentityForAnalysis()<<','<<Quote(effect->NameForAnalysis())<<','<<Quote(Hex(effect->DocumentForAnalysis()))<<",2,0]";
        }
        else out<<"null";
        out<<','<<Quote(diagnostic)<<",0,["<<scans.str()<<"]]";return out.str();
    }
    std::string Decode(const std::string& text)
    {
        if(text=="-")return {};std::string out;
        for(std::size_t i=0;i<text.size();i+=2)out+=static_cast<char>(std::stoul(text.substr(i,2),nullptr,16));return out;
    }
    std::string Pipeline(const std::string& mode,const std::string& data,const spPCEffectTemplate::XmlEventsForAnalysis& events)
    {
        Lifetime lifetime;spPCRFXFileLoader loader;spPCEffectTemplate::CompilerStateForAnalysis state;std::string diagnostic;
        auto scan=[](std::string_view text,std::uint32_t& out){unsigned value=0;const int result=std::sscanf(std::string(text).c_str(),"%x",&value);if(result>0)out=value;return result;};
        auto effect=loader.LoadFileForAnalysis(std::make_unique<File>(data,lifetime),"fixture.rfx",scan,state,diagnostic);
        Check(effect&&effect->IdentityForAnalysis()==1&&effect->NameForAnalysis()=="X"&&!effect->ReadyForAnalysis(),"file metadata produces same template input");
        Check(lifetime.closes==1&&!state.active,"file lifetime completes before XML");
        Check(effect->InitializeFromEventsForAnalysis(&events)&&effect->ReadyForAnalysis(),"same template XML initialized");
        Check(effect->PassesForAnalysis().size()==1,"one XML-produced pass");
        spPCVertexShader shader;std::ostringstream calls;
        spPCEffectTemplate::CompilerForAnalysis compiler=[&](const spPCEffectTemplate::CompilerRequestForAnalysis& request)
        {
            calls<<'['<<Quote(request.assembly?"assembly":"hlsl")<<','<<Quote(request.source)<<",[";
            if(!request.assembly)calls<<Quote(request.entry)<<','<<Quote(request.target);
            calls<<"],"<<request.flags<<",["<<int(state.active)<<"]]";
            spPCEffectTemplate::CompilerOutputForAnalysis result;result.bytecode=std::vector<std::uint8_t>{0x10,0x20,0x30,0x40,0x50};
            if(mode=="hlsl")result.reflection=std::vector<spPCEffectTemplate::ParameterForAnalysis>{{"MatDiffuse",4,1}};return result;
        };
        Check(spPCEffectTemplate::CompileShaderForAnalysis(effect->PassesForAnalysis()[0].shaders[0],"","",shader,compiler,state),"file-produced shader compiles through explicit SDK");
        Check(!state.active&&shader.CompiledCodeForAnalysis()==std::vector<std::uint8_t>({0x10,0x20,0x30,0x40,0x50}),"owned bytecode and successful activity reset");
        std::ostringstream out;out<<'['<<Quote(mode)<<",[1,\"X\","<<Quote(Hex(effect->DocumentForAnalysis()))<<"],1,[[";
        bool firstShader=true;
        for(const auto& part:effect->PassesForAnalysis()[0].shaders)
        {
            if(!firstShader)out<<',';firstShader=false;out<<"[[";
            for(unsigned i=0;i<4;++i){if(i)out<<',';out<<Quote(part.text[i]);}
            out<<"],["<<unsigned(part.flags[0])<<','<<unsigned(part.flags[1])<<','<<unsigned(part.flags[2])<<"],[";
            bool first=true;for(const auto& p:part.parameters){if(!first)out<<',';first=false;out<<'['<<Quote(p.name)<<','<<p.startRegister<<','<<p.registerCount<<']';}out<<"]]";
        }
        out<<"]],["<<calls.str()<<"],[";bool first=true;
        for(const auto& p:shader.GetParametersForAnalysis())
        {if(!first)out<<',';first=false;out<<'['<<Quote(p.name.data())<<','<<p.type<<','<<p.startRegister<<','<<p.registerCount<<']';}
        const auto& code=shader.CompiledCodeForAnalysis();out<<"],"<<shader.GetScalarWordsForAnalysis()[8]<<','<<Quote(Hex(std::string_view(reinterpret_cast<const char*>(code.data()),code.size())))<<",0]";
        return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--pipeline")
        {
            std::string doc,kind,name;unsigned count;std::cin>>doc;spPCEffectTemplate::XmlEventsForAnalysis events;
            while(std::cin>>kind>>name>>count)
            {
                spPCEffectTemplate::XmlEventForAnalysis event;event.start=kind=="S";event.qualifiedName=Decode(name);
                for(unsigned i=0;i<count;++i){std::string key,value;std::cin>>key>>value;event.attributes.emplace(Decode(key),Decode(value));}events.push_back(std::move(event));
            }
            std::cout<<Pipeline(argv[2],Decode(doc),events)<<'\n';
        }
        else if(argc==3&&std::string(argv[1])=="--case")std::cout<<Capture(argv[2])<<'\n';
        else{for(const auto& [mode,data]:Cases())Capture(mode);std::cout<<"PASS "<<checks<<'/'<<checks<<": PC RFX file metadata and ownership\n";}
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
