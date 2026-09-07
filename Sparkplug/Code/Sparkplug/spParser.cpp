#include "spParser.h"
#include "../SparkBase/spMemoryStream.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spParser>();}
        const spRTTIRecord Record{spParser::ClassID,spBaseObject::ClassID,"spParser",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spParser::StaticRTTI() noexcept{(void)Registered;return Record;}
    std::optional<spParser::FileTextForAnalysis> spParser::ReadFileTextForAnalysis(
        std::unique_ptr<spStream> source,const char* name)
    {
        if(!source||!name||!source->Open(1,name))return std::nullopt;
        spMemoryStream memory;
        if(!memory.Open(""))return std::nullopt;
        (void)source->CopyTo(memory);
        source.reset(); // original destroys PC file stream before appending
        const std::uint32_t zero=0;
        if(!memory.Write(zero))return std::nullopt; // host allocation guard
        FileTextForAnalysis result;
        if(!memory.GetSize(&result.size))return std::nullopt;
        result.capacity=memory.GetCapacity();result.bytes.reset(memory.ReleaseBuffer());
        return result;
    }
    const spRTTIRecord& spParser::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spParser::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spParser>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    const std::array<bool,255>& spParser::DelimitersForAnalysis() noexcept
    {
        static const auto values=[]
        {
            std::array<bool,255> result{};
            for(unsigned c:{9,10,13,32,33,40,41,42,43,44,45,47,58,59,60,61,62,63,91,93,94,123,125,126})result[c]=true;
            return result;
        }();
        return values;
    }
    std::optional<spParser::NormalizedForAnalysis> spParser::NormalizeForAnalysis(std::string_view input)
    {
        NormalizedForAnalysis result;result.nativeAllocationRequest=input.size();result.nativeEndOffset=input.size();
        const auto& delimiters=DelimitersForAnalysis();unsigned previous=0;
        auto at=[&](std::size_t i){return i<input.size()?static_cast<unsigned char>(input[i]):0u;};
        std::size_t i=0;
        while(i<=input.size())
        {
            unsigned c=at(i);
            if(c=='/'&&i<input.size())
            {
                if(at(i+1)=='/')
                {while(i<=input.size()&&at(i)!='\n')++i;continue;}
                if(at(i+1)=='*')
                {
                    i+=2;
                    while(i<=input.size()&&!(at(i)=='*'&&at(i+1)=='/'))++i;
                    i+=2;continue;
                }
            }
            if(c=='"')
            {
                result.bytes.push_back('"');++i;
                while(i<=input.size()&&at(i)!='"')result.bytes.push_back(static_cast<char>(at(i++)));
                if(i>input.size())return std::nullopt;
                c=at(i); // Backslash has no escape meaning in this routine.
            }
            if(c==9||c==32)
            {
                if(delimiters[previous]||(i<input.size()&&at(i+1)<255&&delimiters[at(i+1)])){++i;continue;}
                result.bytes.push_back(' ');previous=' ';
            }
            else if(c>=32&&c<128)
            {result.bytes.push_back(static_cast<char>(c));previous=c;}
            ++i;
        }
        result.bytes.push_back('\0');return result;
    }
}
