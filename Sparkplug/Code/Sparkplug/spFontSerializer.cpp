#include "spFontSerializer.h"
#include "spFont.h"
#include "spTextureData.h"
#include "spSerializerManager.h"
#include "spResourceManager.h"
#include "Analysis/PC/spSectionCursor.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> CreateFontSerializer() { return std::make_unique<spFontSerializer>(); }
const spRTTIRecord FontSerializerRecord{spFontSerializer::ClassID,spSerializer::ClassID,
    "spFontSerializer",&spSerializer::StaticRTTI(),&CreateFontSerializer,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(FontSerializerRecord);
}
const spRTTIRecord& spFontSerializer::StaticRTTI() noexcept { (void)Registered;return FontSerializerRecord; }
const spRTTIRecord& spFontSerializer::vfunc_18() const noexcept { return FontSerializerRecord; }
bool spFontSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const {
    return ReadFieldsForAnalysis(context,source,size,object,error,nullptr);
}
bool spFontSerializer::InspectPayloadForAnalysis(spStream& source,std::uint32_t size,spFont& partial,
    InspectionForAnalysis& observation,std::string* error) const {
    observation={};spSerializerManager manager;spResourceManager resources;
    spSerializerReadContextForAnalysis context(manager,resources);
    return ReadFieldsForAnalysis(context,source,size,partial,error,&observation);
}
bool spFontSerializer::ReadFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error,InspectionForAnalysis* observation) const {
    if(error)error->clear();
    evidence::pc::serialization::SectionCursor cursor(context,source,size,true,error);
    auto* font=dynamic_cast<spFont*>(&object);
    if(!font)return cursor.Fail("Font reader target mismatch");
    while(const auto* header=cursor.Next()) {
        if(header->IsTerminator())return true;
        if(header->fieldID!=0) { if(!cursor.Skip())return cursor.Fail("Cannot skip Font field");continue; }
        constexpr std::uint32_t tail=8+17*spFont::GlyphCount;
        if(header->payloadSize<4+tail)return cursor.Fail("Truncated Font reference/metrics/glyphs");
        const auto end=header->dataStreamPosition+header->payloadSize;
        if(observation) {
            if(!evidence::pc::serialization::InspectReference(source,header->payloadSize-tail,
                true,observation->image,error))return cursor.Fail("Invalid Font image extent");
            observation->hasImage=true;
        } else {
            auto* image=ReadSequenceReferenceForAnalysis(context,spTextureData::ClassID,source,end-tail,error);
            if(context.failed)return false;
            auto owner=std::dynamic_pointer_cast<spTextureData>(context.ShareObjectForAnalysis(image));
            if(image&&!owner)return cursor.Fail("Font image lacks canonical TextureData owner");
            font->image_=std::move(owner);
        }
        // PC442711/442725 and44274C..442787: raw UInt32 metrics, byte width,
        // two Vector2 values for each byte character20..FF. No UV validation.
        std::uint32_t baseline=0;
        if(!source.Read(font->height_)||!source.Read(baseline))return cursor.Fail("Cannot read Font metrics");
        font->baseline_=baseline;
        for(auto& glyph:font->glyphs_) {
            if(!source.Read(glyph.width)||!source.ReadData(glyph.uv0.data(),8)||!source.ReadData(glyph.uv1.data(),8))
                return cursor.Fail("Cannot read Font glyph");
        }
    }
    return false;
}
}
