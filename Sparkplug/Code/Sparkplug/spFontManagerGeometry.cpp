#include "spFontManager.h"
#include "spFont.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace sparkplug::reconstruction {
namespace {
double Signed(std::uint32_t value) noexcept {return value<0x80000000u?double(value):double(value)-4294967296.;}
bool Fail(std::string* error,const char* message){if(error)*error=message;return false;}
}
bool spFontManager::BuildPCText3DGeometryForAnalysis(const std::array<float,3>& position,
    const char* text,std::uint32_t wrap,Text3DGeometryForAnalysis& output,std::string* error) const {
    static_assert(sizeof(Text3DVertexForAnalysis)==24);
    if(error)error->clear();
    Text3DGeometryForAnalysis result;
    if(!text||!*text){output=std::move(result);return true;}
    std::size_t length=0;while(length<=65535&&text[length])++length;
    if(length>65535)return Fail(error,"TEXT_GEOMETRY_INPUT_LIMIT: byte string exceeds65535");
    if(!selectedFont_||!selectedFont_->GetBaselineForAnalysis())return Fail(error,"Text geometry requires a selected Font with assigned baseline");
    if(!std::all_of(position.begin(),position.end(),[](float v){return std::isfinite(v);})
        ||double(position[2])<-2147483648.||double(position[2])>=2147483648.)
        return Fail(error,"Text geometry host position exceeds finite Int32 Z input");
    const auto height=selectedFont_->GetHeightForAnalysis();
    const auto z=static_cast<std::int32_t>(std::trunc(position[2]));
    std::uint32_t top=static_cast<std::uint32_t>(z)-*selectedFont_->GetBaselineForAnalysis()+height;
    std::uint32_t bottom=top-height,width=0;
    std::size_t cursor=0,savedCursor=0,savedVertices=0,savedIndices=0,iterations=0;
    bool newWord=true;
    result.vertices.reserve(std::min<std::size_t>(length,4096)*4);
    result.indices.reserve(std::min<std::size_t>(length,4096)*6);
    while(cursor<length){
        if(++iterations>8*(length+1))return Fail(error,"TEXT_GEOMETRY_ITERATION_LIMIT: original word rollback does not complete within host bound");
        if(newWord){savedCursor=cursor;savedVertices=result.vertices.size();savedIndices=result.indices.size();newWord=false;}
        auto character=static_cast<unsigned char>(text[cursor]);
        if(character==10||character==32)newWord=true;
        if(character==10){width=0;top-=height;bottom-=height;++cursor;continue;}
        // Original uses signed comparison, unlike Font's UInt32 measure path.
        if(wrap&&Signed(width)>Signed(wrap)){
            cursor=savedCursor;result.vertices.resize(savedVertices);result.indices.resize(savedIndices);
            top-=height;bottom-=height;width=0;
        }
        character=static_cast<unsigned char>(text[cursor]);
        if(character>=32){
            const auto& glyph=selectedFont_->GetGlyphsForAnalysis()[character-32];
            if(character!=32){
                if(result.vertices.size()>=4096*4)return Fail(error,"TEXT_GEOMETRY_GLYPH_LIMIT: output exceeds4096 glyphs");
                const float widthFloat=static_cast<float>(Signed(width));
                const float left=static_cast<float>(Signed(width)+double(position[0]));
                const float right=static_cast<float>(double(glyph.width)+double(widthFloat)+double(position[0]));
                const float zTop=static_cast<float>(Signed(top)),zBottom=static_cast<float>(Signed(bottom));
                if(!std::isfinite(left)||!std::isfinite(right))return Fail(error,"Text geometry coordinate overflow");
                const auto base=static_cast<std::uint16_t>(result.vertices.size());
                result.vertices.push_back({{left,position[1],zTop},selectedColor_,glyph.uv0});
                result.vertices.push_back({{right,position[1],zTop},selectedColor_,{glyph.uv1[0],glyph.uv0[1]}});
                result.vertices.push_back({{left,position[1],zBottom},selectedColor_,{glyph.uv0[0],glyph.uv1[1]}});
                result.vertices.push_back({{right,position[1],zBottom},selectedColor_,glyph.uv1});
                for(const auto offset:std::array<std::uint16_t,6>{0,1,2,1,3,2})result.indices.push_back(static_cast<std::uint16_t>(base+offset));
            }
            width+=glyph.width;
        }
        ++cursor;
    }
    output=std::move(result);return true;
}
}
