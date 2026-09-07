#include "spPCRFXFileLoader.h"
#include "../SparkBase/spStream.h"
#include <regex>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace sparkplug::reconstruction
{
    std::unique_ptr<spPCEffectTemplate> spPCRFXFileLoader::LoadFileForAnalysis(
        std::unique_ptr<spStream> source,const char* filename,const HexScanForAnalysis& scan,
        spPCEffectTemplate::CompilerStateForAnalysis& state,std::string& diagnostic)
    {
        state.active=true;diagnostic.clear();
        auto text=ReadFileTextForAnalysis(std::move(source),filename);
        if(!text)return nullptr; // original Open failure leaves global active
        return LoadDocumentForAnalysis(std::string_view(reinterpret_cast<const char*>(text->bytes.get()),text->size),scan,state,diagnostic);
    }
    std::unique_ptr<spPCEffectTemplate> spPCRFXFileLoader::LoadDocumentForAnalysis(
        std::string_view input,const HexScanForAnalysis& scan,
        spPCEffectTemplate::CompilerStateForAnalysis& state,std::string& diagnostic)
    {
        state.active=true;diagnostic.clear();
        if(!scan)return nullptr;
        input=input.substr(0,input.find('\0'));const std::string document(input);
        // ECMAScript dot excludes line breaks; explicit byte class preserves
        // the observed Boost behavior of both exact original regex patterns.
        static const std::regex idPattern(R"rfx(<RmStringVariable NAME="ID"(?:[\s\S]*?)VALUE="([\s\S]*?)"/>)rfx");
        static const std::regex namePattern(R"rfx(<RmDirectXEffect NAME="([\s\S]*?)" TYPE(?:[\s\S]*?)>)rfx");
        std::smatch match;
        if(!std::regex_search(document,match,idPattern))
        {diagnostic="Effect file contains no class ID";state.active=false;return nullptr;}
        std::uint32_t identity=0;const int scanned=scan(match[1].str(),identity);
        if(!scanned)
        {diagnostic="Effect file contains invalid class ID";state.active=false;return nullptr;}
        if(scanned<0)
        {diagnostic="Uninitialized original class ID after scanf EOF";state.active=false;return nullptr;}
        if(!std::regex_search(document,match,namePattern))
        {diagnostic="Effect file does not contain a valid effect name";state.active=false;return nullptr;}
        auto result=std::make_unique<spPCEffectTemplate>(identity,document,2);
        result->SetNameForAnalysis(match[1].str());target_=result.get();state.active=false;return result;
    }
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spPCRFXFileLoader>();}
        const spRTTIRecord Record{spPCRFXFileLoader::ClassID,spParser::ClassID,"spPCRFXFileLoader",&spParser::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        std::uint32_t FloatBits(float value) noexcept
        {std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;}
        std::optional<std::uint32_t> RegisterFromDecimalPrefix(std::string_view value)
        {
            std::size_t i=0;while(i<value.size()&&std::string_view(" \t\r\n\v\f").find(value[i])!=std::string_view::npos)++i;
            const bool negative=i<value.size()&&value[i]=='-';
            if(i<value.size()&&(value[i]=='-'||value[i]=='+'))++i;
            std::uint64_t result=0;const std::uint64_t limit=negative?0x80000000ull:0x7fffffffull;
            for(;i<value.size()&&value[i]>='0'&&value[i]<='9';++i)
            {result=result*10+static_cast<unsigned>(value[i]-'0');if(result>limit)return std::nullopt;}
            return negative?0u-static_cast<std::uint32_t>(result):static_cast<std::uint32_t>(result);
        }
    }
    const spRTTIRecord& spPCRFXFileLoader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spPCRFXFileLoader::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spPCRFXFileLoader::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCRFXFileLoader>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spPCRFXFileLoader::ConsumeForAnalysis(const spPCEffectTemplate::XmlEventForAnalysis& event)
    {
        if(!target_)return false;
        if(!event.start)
        {
            if(event.qualifiedName=="RmPass")
            {target_->AppendPassForAnalysis(current_);for(auto& shader:current_.shaders)shader.ResetForAnalysis();}
            return true;
        }
        const auto& tag=event.qualifiedName;
        auto find=[&](const char* name)->const std::string*
        {auto i=event.attributes.find(name);return i==event.attributes.end()?nullptr:&i->second;};
        if(tag=="RmHLSLShader"||tag=="RmShader")
        {
            const auto* pixel=find("PIXEL_SHADER");if(!pixel)return true;
            activeShader_=*pixel=="TRUE"?1u:0u;auto& shader=current_.shaders[*activeShader_];
            if(const auto* code=find("CODE"))shader.text[0]=*code;
            if(tag=="RmShader"){shader.flags={1,1,1};return true;}
            shader.flags[2]=0;
            for(const auto& field:std::array<std::pair<const char*,unsigned>,3>{{{"DECLARATION_BLOCK",1},{"ENTRY_POINT",2},{"TARGET",3}}})
                if(const auto* value=find(field.first))shader.text[field.second]=*value;
            if(const auto* value=find("TARGET"))
            {
                if(*value=="vs_1_1"||*value=="ps_1_1"){shader.flags[0]=1;shader.flags[1]=1;}
                else if(*value=="vs_2_0"||*value=="ps_2_0"){shader.flags[0]=0;shader.flags[1]=2;}
                else if(*value=="ps_1_4"){shader.flags[0]=4;shader.flags[1]=1;}
            }
            return true;
        }
        if(tag=="RmShaderConstant")
        {
            if(!activeShader_)return false; // Original logs an error; error-object path remains open.
            const auto* name=find("NAME");if(!name)return true;
            const auto* reg=find("REGISTER");if(!reg)return true;
            // Native unbounded copy targets a32-byte stack field. Keep portable
            // storage safe and report cases beyond the evidenced contract.
            if(name->size()>=32)return false;
            const auto value=RegisterFromDecimalPrefix(*reg);if(!value)return false;
            current_.shaders[*activeShader_].parameters.push_back({*name,*value,1});return true;
        }
        if(tag=="RmStreamChannel")
        {if(const auto* usage=find("USAGE");usage&&*usage=="6")target_->OrField40ForAnalysis(1);return true;}
        const std::uint32_t kind=tag=="RmBooleanVariable"?1:tag=="RmFloatVariable"?3:tag=="RmVectorVariable"?5:tag=="RmColorVariable"?4:tag=="Rm2DTextureVariable"?6:0;
        if(!kind)return true;
        const auto* name=find("NAME");if(!name)return false; // Native diagnostic path remains open.
        spPCEffectTemplate::VariableForAnalysis variable;variable.name=*name;variable.displayName=*name;variable.kind=kind;
        if(const auto* editable=find("ARTIST_EDITABLE"))variable.artistEditable=*editable=="TRUE";
        auto number=[&](const char* attribute)->std::optional<double>
        {
            const auto* text=find(attribute);if(!text)return std::nullopt;
            const double value=std::strtod(text->c_str(),nullptr);
            if(!std::isfinite(value))return std::nullopt;
            return value;
        };
        auto word=[&](const char* attribute)->std::optional<std::uint32_t>
        {
            const auto value=number(attribute);if(!value)return std::nullopt;
            const float rounded=static_cast<float>(*value);
            if(!std::isfinite(rounded))return std::nullopt;
            return FloatBits(rounded);
        };
        if(kind==1)
        {
            const auto* value=find("VALUE");if(!value)return false;
            variable.payloadWords={*value=="TRUE"?1u:0u};
        }
        else if(kind==3)
        {
            const auto value=word("VALUE");if(!value)return false;
            variable.payloadWords.resize(3);variable.payloadWords[0]=value;
            if(const auto* clamp=find("CLAMP"))
            {
                if(*clamp=="TRUE"){variable.payloadWords[1]=word("MIN");variable.payloadWords[2]=word("MAX");}
                else{variable.payloadWords[1]=0;variable.payloadWords[2]=FloatBits(1000.f);}
            }
        }
        else if(kind==5)
        {
            variable.payloadWords.resize(12);
            for(unsigned i=0;i<4;++i)
            {
                const auto value=word(("VALUE_"+std::to_string(i)).c_str());if(!value)return false;
                variable.payloadWords[i]=value;
            }
        }
        else if(kind==4)
        {
            std::uint32_t color=0xff000000u;
            for(unsigned i=0;i<3;++i)
            {
                const auto value=number(("VALUE_"+std::to_string(i)).c_str());if(!value)return false;
                const double scaled=std::trunc(*value*255.);
                if(scaled<-2147483648.||scaled>2147483647.)return false;
                color|=(static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled))&255u)<<(16-8*i);
            }
            variable.payloadWords.resize(3);variable.payloadWords[0]=color;
        }
        else
        {
            const auto* file=find("FILE_NAME");if(!file)return false;
            variable.payloadText={*file,"",""};
        }
        target_->AppendVariableForAnalysis(std::move(variable));return true;
    }
}
