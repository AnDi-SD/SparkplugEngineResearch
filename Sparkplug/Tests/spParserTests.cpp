#include "Code/Sparkplug/spParser.h"
#include "Code/SparkBase/spStream.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <map>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    std::string Hex(std::string_view bytes)
    {std::string result;for(unsigned char c:bytes){result.push_back("0123456789abcdef"[c>>4]);result.push_back("0123456789abcdef"[c&15]);}return result;}
    const std::map<std::string,std::string> Cases{
        {"empty",""},{"words","  alpha \t beta\r\n gamma  "},
        {"operators","a + b = ( c , d ) ; x / y : z"},
        {"line-comment","a // gone\n b // second\r\nc"},
        {"block-comment","a /* gone */ b/**/c /* x\ny */ d"},
        {"quoted","  \"a  // b /* c */\" \t \"x\ny\"  "},
        {"controls","a\1b\10c\13d\14e\37f"},
        {"newlines","a\nb\rc\r\nd\t e"},
        {"escaped-quote","\"a\\\" /* b */ \"c\""}
    };
    const std::map<std::string,std::string> FileCases{
        {"tiny","<Root>tiny\r\n</Root>"},{"nul",std::string("a\0b\xff" "c",5)},
        {"growth-before",std::string(4996,'x')},{"growth-at",std::string(5000,'x')},
        {"growth-after",std::string(5001,'x')},{"empty",""}
    };
    struct FileInput final : spStream
    {
        const std::string& data;std::ostringstream& events;bool opened=false;
        FileInput(const std::string& bytes,std::ostringstream& log):data(bytes),events(log){}
        ~FileInput() override{if(opened)(void)Close();}
        bool Open(const char* name) override{return Open(1,name);}
        bool Open(std::uint32_t mode,const char* name) override
        {
            if(mode!=1||std::string(name)!="fixture.rfx"||opened)return false;
            events<<"[\"open\",\"fixture.rfx\",2147483648,3,3,1]";opened=true;return true;
        }
        bool Close() override{events<<",[\"close\"]";opened=false;return true;}
        bool Seek(SeekSource,std::int32_t) override{return false;}
        bool GetCurrentPosition(std::uint32_t&) const override{return false;}
        bool ReadData(void* out,std::uint32_t count) override
        {
            if(count!=data.size())return false;
            events<<",[\"read\",0,"<<count<<','<<count<<']';
            if(count)std::memcpy(out,data.data(),count);return count!=0;
        }
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
        bool GetSize(std::uint32_t* size) const override
        {events<<",[\"size\","<<data.size()<<']';*size=static_cast<std::uint32_t>(data.size());return true;}
    };
    std::string FileCapture(const std::string& mode)
    {
        const auto& data=FileCases.at(mode);std::ostringstream events;
        auto result=spParser::ReadFileTextForAnalysis(std::make_unique<FileInput>(data,events),"fixture.rfx");
        Check(result&&result->size==data.size()+4,"file helper returns four trailing zeros");
        std::string_view bytes(reinterpret_cast<const char*>(result->bytes.get()),result->size);
        Check(bytes==data+std::string(4,'\0'),"all original file bytes preserved");
        Check(result->capacity==((data.size()+4+4999)/5000)*5000,"original capacity growth");
        std::ostringstream out;out<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\","<<result->capacity<<",["<<events.str()<<"]]";
        return out.str();
    }
    std::string Capture(const std::string& mode)
    {
        spParser parser;Check(parser.IsKindOf(spBaseObject::ClassID),"confirmed RTTI ancestry");
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        const auto& table=spParser::DelimitersForAnalysis();
        for(unsigned i=0;i<table.size();++i)if(table[i]){if(!first)out<<',';first=false;out<<i;}
        out<<"],[";
        if(mode=="clone")
        {
            parser.SetState20ForAnalysis(99);auto clone=parser.Clone();auto* typed=dynamic_cast<spParser*>(clone.get());
            Check(typed&&typed->State20ForAnalysis()==0,"original blank parser clone");
            out<<parser.State20ForAnalysis()<<','<<typed->State20ForAnalysis();
        }
        else if(mode!="parser-factory")
        {
            const auto& text=Cases.at(mode);const auto normalized=spParser::NormalizeForAnalysis(text);
            Check(normalized.has_value(),"valid balanced native input");const auto& n=*normalized;
            Check(n.nativeEndOffset==text.size()&&n.state20==1&&n.nativeOwnership,"native postconditions");
            out<<'"'<<Hex(text)<<"\",\""<<Hex(n.bytes)<<"\","<<n.bytes.size()<<','<<n.nativeAllocationRequest<<',';
            out<<(n.bytes.size()>n.nativeAllocationRequest?"true":"false");
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--file")std::cout<<FileCapture(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--case")std::cout<<Capture(argv[2])<<'\n';
        else
        {
            for(const auto& [mode,text]:Cases)Capture(mode);
            for(const auto& [mode,text]:FileCases)FileCapture(mode);
            Capture("parser-factory");Capture("clone");
            Check(!spParser::NormalizeForAnalysis("\"unclosed"),"host rejects original unterminated-quote overread");
            const auto exact=spParser::NormalizeForAnalysis("literal");
            Check(exact&&exact->nativeAllocationRequest==7&&exact->bytes==std::string("literal\0",8),"host preserves result with sufficient storage");
            const auto comment=spParser::NormalizeForAnalysis("alpha/**/beta");
            Check(comment&&comment->bytes==std::string("alphabeta\0",10),"block comment inserts no word separator");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": PC parser normalization and lifetime\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
