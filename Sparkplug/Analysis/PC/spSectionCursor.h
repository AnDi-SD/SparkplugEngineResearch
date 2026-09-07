#pragma once
// Analytical host envelope guard, NOT an original engine class. All wire
// parsing still goes through reconstructed spDataBlockSerializer. Shared by
// sequential inherited sections; stricter than the native malformed reader.
#include "Code/Sparkplug/spSerializer.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/SparkBase/spStream.h"

namespace sparkplug::evidence::pc::serialization
{
    class SectionCursor final
    {
        using Context=reconstruction::spSerializerReadContextForAnalysis;
        using Header=reconstruction::spDataBlockHeaderForAnalysis;
        reconstruction::spStream& source_;
        Context& context_;
        std::string* error_;
        reconstruction::spDataBlockSerializer blocks_;
        std::uint32_t end_=0,count_=0;
        const Header* previous_=nullptr;
        bool exact_,valid_=false;
    public:
        SectionCursor(Context& context,reconstruction::spStream& source,std::uint32_t size,bool exact,std::string* error)
            :source_(source),context_(context),error_(error),exact_(exact)
        {
            std::uint32_t start=0,physical=0;
            if(context.failed||!size||size>16u*1024u*1024u||!source.GetCurrentPosition(start)||!source.GetSize(&physical)
                ||source.GetLogicalOriginForAnalysis()>physical){Fail("Invalid bounded derived section");return;}
            physical-=source.GetLogicalOriginForAnalysis();
            if(start>physical||size>physical-start){Fail("Derived section exceeds logical file extent");return;}
            end_=start+size;valid_=true;
        }
        bool Fail(const char* message)
        {context_.failed=true;valid_=false;if(error_)*error_=message;return false;}
        const Header* Next()
        {
            if(!valid_)return nullptr;
            std::uint32_t position=0;
            if(!source_.GetCurrentPosition(position)||(previous_&&position!=previous_->dataStreamPosition+previous_->payloadSize))
            {Fail("Derived field did not consume its exact extent");return nullptr;}
            if(++count_>65536||position>=end_){Fail("Missing derived section terminator or field-count overflow");return nullptr;}
            previous_=blocks_.ReadHeaderForAnalysis(source_);
            if(!previous_||previous_->dataStreamPosition>end_||previous_->payloadSize>end_-previous_->dataStreamPosition)
            {Fail("Truncated derived field");return nullptr;}
            if(previous_->IsTerminator()&&exact_&&previous_->dataStreamPosition!=end_)
            {Fail("Trailing derived section bytes");return nullptr;}
            return previous_;
        }
        template<class T>bool Read(T& value)
        {return previous_&&previous_->payloadSize==sizeof(value)&&source_.ReadData(&value,sizeof(value));}
        bool Skip()
        {return previous_&&reconstruction::spDataBlockSerializer::SkipDataForAnalysis(source_,*previous_);}
    };
}
