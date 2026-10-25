#include "spDXAsmShaderParser.h"
#include "spDXShader.h"
#include <algorithm>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        bool Space(char c) noexcept
        {return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f';}

        // Match the finite ASCII part of the original sscanf formats. A
        // format-space accepts zero or more input whitespace characters.
        bool Literal(std::string_view text,std::size_t& at,std::string_view pattern)
        {
            for(char c:pattern)
            {
                if(c==' '){while(at<text.size()&&Space(text[at]))++at;}
                else if(at==text.size()||text[at++]!=c)return false;
            }
            return true;
        }
        bool Decimal(std::string_view text,std::size_t& at,std::uint32_t& value,bool& outside)
        {
            while(at<text.size()&&Space(text[at]))++at;
            bool negative=false;
            if(at<text.size()&&(text[at]=='+'||text[at]=='-'))negative=text[at++]=='-';
            const auto begin=at;std::uint64_t magnitude=0;
            const auto bound=negative?0x80000000ull:0x7fffffffull;
            while(at<text.size()&&text[at]>='0'&&text[at]<='9')
            {
                const unsigned digit=static_cast<unsigned>(text[at++]-'0');
                if(magnitude>(bound-digit)/10)outside=true;
                else if(!outside)magnitude=magnitude*10+digit;
            }
            if(at==begin)return false;
            value=negative?0u-static_cast<std::uint32_t>(magnitude):static_cast<std::uint32_t>(magnitude);
            return true;
        }
        struct Declaration
        {
            std::string_view name;
            std::uint32_t start=0,count=0;
            bool matched=false,outside=false;
        };
        Declaration RegisterComment(std::string_view text,bool range)
        {
            Declaration result;std::size_t at=0;
            if(!Literal(text,at,"// "))return result;
            const auto begin=at;
            // Native scanset excludes ':' and literal space only, not tabs.
            while(at<text.size()&&text[at]!=':'&&text[at]!=' ')++at;
            if(at==begin)return result;
            result.name=text.substr(begin,at-begin);
            if(!Literal(text,at,range?" : registers c":" : register c")||
                !Decimal(text,at,result.start,result.outside))return result;
            if(range)
            {
                std::uint32_t ignoredEnd=0;
                if(!Literal(text,at,"-c")||!Decimal(text,at,ignoredEnd,result.outside))return result;
            }
            if(!Literal(text,at," (")||!Decimal(text,at,result.count,result.outside))return result;
            // sscanf has already assigned every conversion: failure of its
            // trailing literal " registers total)" does not undo the match.
            result.matched=true;return result;
        }
        std::uint32_t ComponentMask(std::string_view text)
        {
            static constexpr std::pair<std::string_view,std::uint32_t> entries[]{
                {"Position",0},{"RHW",1},{"Weight1",2},{"Weight2",6},{"Weight3",0xe},{"Weight4",0x1e},
                {"BlendIndex",0x20},{"Normal",0x40},{"PointSize",0x80},{"Color",0x100},{"SpecColor",0x200},
                {"UVW1",0x400},{"UV1",0x800},{"UV2",0x1800},{"UV3",0x3800},{"UV4",0x7800},
                {"UV5",0xf800},{"UV6",0x1f800},{"UV7",0x3f800},{"UV8",0x7f800},
                {"Tangent",0x80000},{"Binormal",0x100000}};
            std::size_t at=0;if(!Literal(text,at,"// Component "))return 0;
            const auto begin=at;
            while(at<text.size()&&text[at]!=':'&&text[at]!=' '&&text[at]!='\r'&&text[at]!='\n'&&text[at]!='\t')++at;
            const auto name=text.substr(begin,at-begin);
            for(const auto& entry:entries)if(name==entry.first)return entry.second;
            return 0;
        }
        std::unique_ptr<spBaseObject> Create()
        {
            return std::make_unique<spDXAsmShaderParser>();
        }
        const spRTTIRecord Record{
            spDXAsmShaderParser::ClassID,spBaseObject::ClassID,
            "spDXAsmShaderParser",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=
            spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }

    bool spDXAsmShaderParser::ParseAnnotationsForAnalysis(std::string_view source,
        spDXShader& shader,std::uint32_t& componentFlags) const
    {
        source=source.substr(0,source.find('\0'));
        std::vector<spDXShader::ParameterForAnalysis> parameters;
        std::uint32_t mask=0;
        for(std::size_t at=source.find("//");at!=std::string_view::npos;at=source.find("//",at+2))
        {
            const auto text=source.substr(at);auto declaration=RegisterComment(text,false);
            if(!declaration.matched)declaration=RegisterComment(text,true);
            if(declaration.outside)return false;
            if(declaration.matched)
            {
                if(declaration.name.size()>=32)return false;
                spDXShader::ParameterForAnalysis parameter;
                std::copy(declaration.name.begin(),declaration.name.end(),parameter.name.begin());
                parameter.startRegister=declaration.start;parameter.registerCount=declaration.count;
                parameters.push_back(parameter);
            }
            else mask|=ComponentMask(text);
        }
        componentFlags=mask;
        for(const auto& parameter:parameters)if(!shader.AppendParameterForAnalysis(parameter))return false;
        return true;
    }

    const spRTTIRecord& spDXAsmShaderParser::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }

    const spRTTIRecord& spDXAsmShaderParser::vfunc_18() const noexcept
    {
        return Record;
    }

    std::unique_ptr<spBaseObject> spDXAsmShaderParser::vfunc_10(
        spCloneManager& manager) const
    {
        // PC004CFB20 calls factory004CFAB0, registers the pair, then invokes
        // inherited BaseObject Copy. There are no assembly/text/code fields
        // copied here and no shader compilation is performed by this method.
        auto clone=std::make_unique<spDXAsmShaderParser>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
}
