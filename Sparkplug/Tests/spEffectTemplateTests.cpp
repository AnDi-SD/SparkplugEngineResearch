#include "Code/SparkplugPC/spPCEffectTemplate.h"
#include "Code/SparkplugPC/spPCRFXFileLoader.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    std::string Decode(const std::string& value)
    {
        if(value=="-")return {};
        if(value.size()%2)throw std::runtime_error("hex input length");std::string result;
        for(std::size_t i=0;i<value.size();i+=2)result.push_back(static_cast<char>(std::stoul(value.substr(i,2),nullptr,16)));
        return result;
    }
    void Quoted(std::ostream& out,std::string_view value)
    {
        out<<'"';for(unsigned char c:value)
        {
            if(c=='"'||c=='\\')out<<'\\'<<c;
            else if(c<32||c>=127)out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];
            else out<<c;
        }out<<'"';
    }
    void Pass(std::ostream& out,const spPCEffectTemplate::PassForAnalysis& pass,bool parameters)
    {
        out<<'[';
        for(unsigned i=0;i<2;++i)
        {
            if(i)out<<',';out<<"[[";
            for(unsigned j=0;j<4;++j){if(j)out<<',';Quoted(out,pass.shaders[i].text[j]);}
            out<<"],[";for(unsigned j=0;j<3;++j){if(j)out<<',';out<<unsigned(pass.shaders[i].flags[j]);}out<<"]";
            if(parameters)
            {
                out<<",[";bool first=true;
                for(const auto& parameter:pass.shaders[i].parameters)
                {if(!first)out<<',';first=false;out<<'[';Quoted(out,parameter.name);out<<','<<parameter.startRegister<<','<<parameter.registerCount<<']';}
                out<<']';
            }
            out<<']';
        }out<<']';
    }
    void Passes(std::ostream& out,const spPCEffectTemplate& object,bool parameters=false)
    {
        out<<'[';bool firstPass=true;
        for(const auto& pass:object.PassesForAnalysis())
        {
            if(!firstPass)out<<',';firstPass=false;Pass(out,pass,parameters);
        }out<<']';
    }
    std::string Run(const std::string& mode,std::string data={},const spPCEffectTemplate::XmlEventsForAnalysis* events=nullptr)
    {
        const bool xml=mode.rfind("xml-",0)==0;
        if(!xml)data=mode=="empty"?"":mode=="long-text"?"A longer native template source":"Fixture";
        unsigned kind=mode=="kind0"?0:mode=="kind1"?1:mode=="kind3"?3:2;
        spPCEffectTemplate object(0x12345678,data,kind);
        Check(object.IsKindOf(spBaseObject::ClassID)&&!object.ReadyForAnalysis(),"actual template base/default");
        Check(!object.Clone(),"original template clone returns NULL");
        Check(!spPCEffectTemplate::StaticRTTI().factory,"original template has no RTTI factory");
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<','<<object.IdentityForAnalysis()<<','<<object.KindForAnalysis()<<',';
        Quoted(out,object.DocumentForAnalysis());out<<",[";
        if(kind!=2||xml)
        {
            for(unsigned repeat=0;repeat<2;++repeat)
            {
                if(repeat)out<<',';Check(object.InitializeFromEventsForAnalysis(events),"original known dispatch");
                Check(object.ReadyForAnalysis(),"actual ready flag");out<<1;
                if(xml){out<<',';Passes(out,object,mode=="xml-constant");}
            }
        }
        if(mode=="owned-text"){object.SetOwnedField14ForAnalysis({'f','i','e','l','d','1','4',0});object.SetNameForAnalysis("name");}
        out<<"]]";return out.str();
    }
    std::string RunEvents(const std::string& mode,const spPCEffectTemplate::XmlEventsForAnalysis& events)
    {
        spPCEffectTemplate object(1,"",2);object.SetField40ForAnalysis(0xa5a50020);
        spPCRFXFileLoader loader;loader.BindForAnalysis(object);std::ostringstream out;
        out<<'[';Quoted(out,mode);out<<",[";bool first=true;
        for(const auto& event:events)
        {
            Check(loader.ConsumeForAnalysis(event),"bounded original callback sequence");
            if(!first)out<<',';first=false;
            out<<'['<<object.Field40ForAnalysis().value<<',';
            out<<(loader.ActiveShaderForAnalysis()?static_cast<int>(*loader.ActiveShaderForAnalysis()):-1)<<',';
            Pass(out,loader.CurrentPassForAnalysis(),true);out<<',';Passes(out,object,true);out<<']';
        }
        out<<"]]";return out.str();
    }
    void Variables(std::ostream& out,const spPCEffectTemplate& object)
    {
        out<<'[';bool firstVariable=true;
        for(const auto& variable:object.VariablesForAnalysis())
        {
            if(!firstVariable)out<<',';firstVariable=false;out<<'[';Quoted(out,variable.name);out<<',';Quoted(out,variable.displayName);
            out<<','<<unsigned(variable.artistEditable)<<','<<variable.kind<<",[";bool first=true;
            for(const auto& word:variable.payloadWords){if(!first)out<<',';first=false;if(word)out<<*word;else out<<"null";}
            out<<"],[";first=true;
            for(const auto& value:variable.payloadText){if(!first)out<<',';first=false;Quoted(out,value);}
            out<<"]]";
        }out<<']';
    }
    std::string RunVariables(const std::string& mode,const spPCEffectTemplate::XmlEventsForAnalysis& events)
    {
        spPCEffectTemplate object(1,"",2);spPCRFXFileLoader loader;loader.BindForAnalysis(object);
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<",[";bool firstEvent=true;
        for(const auto& event:events)
        {
            Check(loader.ConsumeForAnalysis(event),"bounded variable callback");
            if(!firstEvent)out<<',';firstEvent=false;Variables(out,object);
        }out<<"]]";return out.str();
    }
    std::string RunCorpus(const std::string& mode,const spPCEffectTemplate::XmlEventsForAnalysis& events)
    {
        spPCEffectTemplate object(1,"",2);object.SetField40ForAnalysis(0);
        spPCRFXFileLoader loader;loader.BindForAnalysis(object);
        for(const auto& event:events)Check(loader.ConsumeForAnalysis(event),"original corpus engine callback");
        std::ostringstream out;out<<'[';Quoted(out,mode);out<<','<<object.Field40ForAnalysis().value<<',';
        Variables(out,object);out<<',';Passes(out,object,true);out<<']';return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else if(argc==3&&(std::string(argv[1])=="--xml"||std::string(argv[1])=="--events"||std::string(argv[1])=="--variables"||std::string(argv[1])=="--corpus"))
        {
            std::string document;std::cin>>document;spPCEffectTemplate::XmlEventsForAnalysis events;
            std::string kind,name;unsigned count;
            while(std::cin>>kind>>name>>count)
            {
                spPCEffectTemplate::XmlEventForAnalysis event;event.start=kind=="S";event.qualifiedName=Decode(name);
                for(unsigned i=0;i<count;++i){std::string key,value;std::cin>>key>>value;event.attributes.emplace(Decode(key),Decode(value));}
                events.push_back(std::move(event));
            }
            std::cout<<(std::string(argv[1])=="--corpus"?RunCorpus(argv[2],events):std::string(argv[1])=="--variables"?RunVariables(argv[2],events):std::string(argv[1])=="--events"?RunEvents(argv[2],events):Run(argv[2],Decode(document),&events))<<'\n';
        }
        else
        {
            for(const char* mode:{"empty","text","long-text","kind0","kind1","kind3","owned-text"})Run(mode);
            spPCEffectTemplate::XmlEventsForAnalysis events{
                {true,"Root",{}},{true,"RmPass",{}},{true,"RmHLSLShader",{{"PIXEL_SHADER","FALSE"},{"CODE","a&b\nc"},{"TARGET","vs_1_1"},{"ENTRY_POINT","Main"},{"DECLARATION_BLOCK","decl"}}},
                {false,"RmHLSLShader",{}},{false,"RmPass",{}},{false,"Root",{}}
            };
            spPCEffectTemplate object(1,"<external XML>",2);
            Check(!object.InitializeFromEventsForAnalysis(),"XML provider remains a required boundary");
            Check(object.InitializeFromEventsForAnalysis(&events),"known XML callback sequence");
            Check(object.PassesForAnalysis().size()==1&&object.PassesForAnalysis()[0].shaders[0].text[0]=="a&b\nc","decoded XML code preserved");
            Check(object.PassesForAnalysis()[0].shaders[0].flags==std::array<std::uint8_t,3>{1,1,0},"native shader target flags");
            Check(object.InitializeFromEventsForAnalysis(&events)&&object.PassesForAnalysis().size()==2,"repeat initialization appends duplicate pass");
            spPCRFXFileLoader loader;Check(loader.IsKindOf(spParser::ClassID),"RFX parser ancestry");
            loader.BindForAnalysis(object);
            Check(loader.ConsumeForAnalysis({true,"RmShader",{{"PIXEL_SHADER","FALSE"}}}),"assembly callback");
            Check(loader.ConsumeForAnalysis({true,"RmShaderConstant",{{"NAME","MatDiffuse"},{"REGISTER","-1"}}}),"constant callback");
            Check(loader.ConsumeForAnalysis({false,"RmPass",{}}),"pass completion");
            Check(loader.CurrentPassForAnalysis().shaders[0].parameters[0].startRegister==0xffffffffu,"parameter persists across reset; signed atoi becomes unsigned register");
            Check(object.Field40ForAnalysis().knownMask==0,"constructor does not invent field40 default");
            Check(loader.ConsumeForAnalysis({true,"RmStreamChannel",{{"USAGE","6"}}})&&object.Field40ForAnalysis().knownMask==1&&object.Field40ForAnalysis().value==1,"stream usage establishes only one previously unknown bit");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": effect template and XML callback slice\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
