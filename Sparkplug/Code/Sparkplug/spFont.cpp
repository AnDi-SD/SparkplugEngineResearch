#include "spFont.h"
#include "spTexture.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> CreateFont() { return std::make_unique<spFont>(); }
const spRTTIRecord FontRecord{spFont::ClassID,spNamedObject::ClassID,"spFont",
    &spNamedObject::StaticRTTI(),&CreateFont,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(FontRecord);
}
spFont::~spFont() = default;
const spRTTIRecord& spFont::StaticRTTI() noexcept { (void)Registered; return FontRecord; }
const spRTTIRecord& spFont::vfunc_18() const noexcept { return FontRecord; }
std::uint32_t spFont::MeasureTextForAnalysis(const char* text,std::uint32_t wrapWidth,
    std::uint32_t* height) const noexcept {
    if(!text||!*text)return 0;
    std::uint32_t line=0,maximum=0,totalHeight=height_;
    for(const auto* cursor=reinterpret_cast<const unsigned char*>(text);*cursor;++cursor) {
        const auto character=*cursor;
        if(character==10){line=0;totalHeight+=height_;continue;}
        // Native checks the PREVIOUS line width, including before control bytes.
        if(wrapWidth&&line>wrapWidth){line=0;totalHeight+=height_;}
        if(character<FirstCharacter)continue;
        line+=glyphs_[character-FirstCharacter].width;
        if(line>maximum)maximum=line;
    }
    if(height)*height=totalHeight;
    return maximum;
}
}
