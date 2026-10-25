#include "Code/SparkplugDX/spDXAsmShaderParser.h"
#include "Code/SparkplugPC/spPCVertexShader.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;

namespace
{
    unsigned checks=0;
    void Check(bool value,const char* message)
    {
        ++checks;
        if(!value)throw std::runtime_error(message);
    }
    std::string Quote(std::string_view text)
    {
        std::string out="\"";for(unsigned char c:text)
        {
            if(c=='"'||c=='\\'){out+='\\';out+=c;}
            else if(c<32){out+="\\u00";out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}
            else out+=c;
        }return out+'"';
    }
    std::string Capture(std::string_view source)
    {
        spDXAsmShaderParser parser;spPCVertexShader shader;std::uint32_t mask=0xffffffffu;
        Check(parser.ParseAnnotationsForAnalysis(source,shader,mask),"bounded annotation source");
        std::ostringstream out;out<<'['<<mask<<",[";bool first=true;
        for(const auto& p:shader.GetParametersForAnalysis())
        {if(!first)out<<',';first=false;out<<'['<<Quote(p.name.data())<<','<<p.type<<','<<p.startRegister<<','<<p.registerCount<<']';}
        out<<"],"<<shader.GetScalarWordsForAnalysis()[8]<<']';return out.str();
    }
    std::string Decode(const std::string& hex)
    {
        if(hex.size()%2)throw std::runtime_error("even hex input required");std::string out;
        for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return out;
    }
}

int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--parse-hex")
        {std::cout<<Capture(Decode(argv[2]))<<'\n';return 0;}
        const auto& record=spDXAsmShaderParser::StaticRTTI();
        Check(record.classID==0x0B3E0AA9,"original class identity");
        Check(record.baseClassID==spBaseObject::ClassID,"registration BaseObject parent");
        Check(record.factory!=nullptr,"original concrete factory");
        auto source=record.factory();
        Check(dynamic_cast<spDXAsmShaderParser*>(source.get())!=nullptr,"factory produces declared type");
        Check(source->IsExactly(spDXAsmShaderParser::ClassID),"original exact identity getter");
        Check(source->IsKindOf(spBaseObject::ClassID),"original base identity");
        Check(!source->IsExactly(spBaseObject::ClassID),"own identity differs from base");
        Check(spDXAsmShaderParser::NativeObjectSizeForAnalysis==16,"actual original factory extent");
        auto clone=source->Clone();
        Check(clone&&clone.get()!=source.get(),"native clone creates a separate object");
        Check(dynamic_cast<spDXAsmShaderParser*>(clone.get())!=nullptr,"clone uses its concrete factory");
        Check(clone->IsExactly(spDXAsmShaderParser::ClassID),"clone preserves class identity");
        clone.reset();source.reset();
        Check(Capture("vs.1.1\nmov oPos, v0")=="[0,[],0]","instructions are not parsed here");
        Check(Capture("// Component UV2\n// Component Tangent\n// Component Color")=="[530688,[],0]","component OR table");
        Check(Capture("// Component normal\n// component Normal\n// Component Unknown")=="[0,[],0]","case-sensitive and unknown components");
        spDXAsmShaderParser parser;spPCVertexShader shader;std::uint32_t mask=123;
        Check(parser.ParseAnnotationsForAnalysis("// MatDiffuse : register c4 (1 registers total)\n// Unknown : registers c9-c14 (2 registers total)",shader,mask),"both declaration formats");
        Check(mask==0&&shader.GetParametersForAnalysis().size()==2,"reset mask and append declarations");
        Check(shader.GetParametersForAnalysis()[1].startRegister==9&&shader.GetParametersForAnalysis()[1].registerCount==2,"range end ignored in favor of explicit count");
        Check(shader.GetScalarWordsForAnalysis()[8]==3,"count sum rather than max register end");
        Check(parser.ParseAnnotationsForAnalysis("// Component Normal // Component Color",shader,mask)&&mask==0x140,"each marker is examined, not one comment per line");
        Check(shader.GetParametersForAnalysis().size()==2,"new call retains prior descriptors");
        Check(parser.ParseAnnotationsForAnalysis("// MatDiffuse : register c-1 (+2",shader,mask),"sscanf assignments survive missing trailing literal");
        Check(shader.GetParametersForAnalysis().back().startRegister==0xffffffffu&&shader.GetScalarWordsForAnalysis()[8]==5,"signed decimal raw words and accumulated sum");
        const std::string nul="// Component Normal"+std::string(1,'\0')+"// Component Color";
        Check(parser.ParseAnnotationsForAnalysis(nul,shader,mask)&&mask==0x40,"C-string NUL boundary");
        const auto before=shader.GetParametersForAnalysis().size();mask=321;
        Check(!parser.ParseAnnotationsForAnalysis("// "+std::string(32,'N')+" : register c1 (2",shader,mask),"host guards unbounded original name copy");
        Check(mask==321&&shader.GetParametersForAnalysis().size()==before,"guard refusal precedes mutation");
        Check(!parser.ParseAnnotationsForAnalysis("// X : register c2147483648 (1",shader,mask),"host guards decimal overflow");
        std::cout<<"PASS "<<checks<<" ASM parser lifecycle/annotation checks; external assembler not executed\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"FAIL "<<error.what()<<'\n';
        return 1;
    }
}
