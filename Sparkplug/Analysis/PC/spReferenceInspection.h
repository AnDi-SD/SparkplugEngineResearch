#pragma once
// Host metadata observation, not a runtime resource or an original game class.
// Prefix decoding remains the shared spSerializer implementation. This helper
// only bounds/skips the inline body; it never substitutes a referenced object.
#include "Code/Sparkplug/spSerializer.h"
#include "Code/SparkBase/spStream.h"

namespace sparkplug::evidence::pc::serialization
{
    struct InspectedReference
    {std::uint32_t offset=0,size=0,id=0,inlineSize=0;};
    inline bool InspectReference(reconstruction::spStream& source,std::uint32_t available,
        bool exact,InspectedReference& result,std::string* error)
    {
        result={};std::uint32_t start=0;
        reconstruction::spSerializer::ReferencePrefixForAnalysis prefix;
        const auto fail=[&](const char* text){if(error)*error=text;return false;};
        if(!source.GetCurrentPosition(start)||
            !reconstruction::spSerializer::ReadReferencePrefixForAnalysis(source,source,prefix,error,available))return false;
        const auto prefixSize=prefix.id?8u:4u;
        if(available<prefixSize||prefix.inlineSize>available-prefixSize
            ||(exact&&prefix.inlineSize!=available-prefixSize)||prefix.inlineSize>0x7fffffffu)
            return fail("Invalid bounded inspected reference extent");
        if(!source.Seek(reconstruction::spStream::SeekSource::essCurrent,static_cast<std::int32_t>(prefix.inlineSize)))
            return fail("Cannot skip bounded inspected reference body");
        result={start,prefixSize+prefix.inlineSize,prefix.id,prefix.inlineSize};return true;
    }
}
