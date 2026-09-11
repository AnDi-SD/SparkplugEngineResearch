#include "spTextRenderable.h"
#include "spFont.h"
#include "spFontManager.h"
#include <cmath>
#include <limits>
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> CreateText(){return std::make_unique<spTextRenderable>();}
const spRTTIRecord Record{spTextRenderable::ClassID,spRenderable::ClassID,"spTextRenderable",
    &spRenderable::StaticRTTI(),&CreateText,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
bool Fail(std::string* error,const char* message){if(error)*error=message;return false;}
}
const spRTTIRecord& spTextRenderable::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spTextRenderable::vfunc_18() const noexcept{return Record;}
bool spTextRenderable::SetTextForAnalysis(const char* text,spFontManager* manager,std::string* error){
    if(!text)return Fail(error,"Text setter received NULL; original PC path dereferences it");
    text_=text;return RebuildLayoutForAnalysis(manager,error);
}
bool spTextRenderable::SetFontForAnalysis(std::shared_ptr<spFont> font,spFontManager* manager,std::string* error){
    font_=std::move(font);return RebuildLayoutForAnalysis(manager,error);
}
bool spTextRenderable::SetWrapWidthForAnalysis(std::uint32_t value,spFontManager* manager,std::string* error){
    wrap_=value;return RebuildLayoutForAnalysis(manager,error);
}
bool spTextRenderable::SetAlignmentForAnalysis(std::uint32_t value,spFontManager* manager,std::string* error){
    alignment_=value;return RebuildLayoutForAnalysis(manager,error);
}
bool spTextRenderable::RebuildLayoutForAnalysis(spFontManager* manager,std::string* error){
    if(error)error->clear();
    if(!text_||text_->empty()){
        measuredWidth_=0;sphere_={};minimum_=BoundsPosition{};
        // PC438096..4380B5 writes minimum TWICE, never maximum.
        return true;
    }
    if(!manager)return Fail(error,"Nonempty Text layout requires explicit FontManager state");
    manager->SelectFontAndColorForAnalysis(font_.get(),color_);
    const auto* font=manager->GetSelectedFontForAnalysis();
    if(!font)return Fail(error,"Text has no selected/default Font; original layout dereferences it");
    if(!font->GetBaselineForAnalysis())return Fail(error,"Text Font baseline is unassigned by original reader");
    std::uint32_t height=0;measuredWidth_=manager->MeasureTextForAnalysis(text_->c_str(),wrap_,&height);
    const double width=measuredWidth_,totalHeight=height;
    // PC converts UInt32 to x87 then spills height and baseline to float first.
    const float fontHeight=static_cast<float>(font->GetHeightForAnalysis());
    const float baseline=static_cast<float>(*font->GetBaselineForAnalysis());
    const float halfWidth=static_cast<float>(width*0.5);
    sphere_={0,0,static_cast<float>(double(fontHeight)-totalHeight*0.5-double(baseline)),
        static_cast<float>(std::sqrt(width*width+totalHeight*totalHeight)*0.5)};
    minimum_=BoundsPosition{-halfWidth,0,static_cast<float>(double(fontHeight)-totalHeight-double(baseline))};
    maximum_=BoundsPosition{halfWidth,0,static_cast<float>(double(fontHeight)-double(baseline))};
    if(alignment_==0){sphere_[0]=halfWidth;(*minimum_)[0]=0;(*maximum_)[0]=static_cast<float>(width);}
    else if(alignment_==2){
        sphere_[0]=-halfWidth;
        // Original43801C stores POSITIVE width in minimum; do not normalize.
        (*minimum_)[0]=static_cast<float>(width);(*maximum_)[0]=0;
    }
    return true;
}
void spTextRenderable::GetBoundsForAnalysis(BoundsPosition& minimum,BoundsPosition& maximum) const noexcept{
    // The base interface cannot express unknown. NaNs are an explicit host
    // sentinel, never claimed to be original uninitialized memory contents.
    const float unknown=std::numeric_limits<float>::quiet_NaN();
    minimum=minimum_.value_or(BoundsPosition{unknown,unknown,unknown});
    maximum=maximum_.value_or(BoundsPosition{unknown,unknown,unknown});
}
bool spTextRenderable::BuildPCGeometryForAnalysis(spFontManager& manager,
    spFontManager::Text3DGeometryForAnalysis& output,std::string* error) const {
    manager.SelectFontAndColorForAnalysis(font_.get(),color_);
    std::array<float,3> position{};
    if(alignment_==1)position[0]=static_cast<float>(-double(measuredWidth_)*0.5);
    else if(alignment_==2)position[0]=static_cast<float>(-double(measuredWidth_));
    return manager.BuildPCText3DGeometryForAnalysis(position,text_?text_->c_str():nullptr,wrap_,output,error);
}
}
