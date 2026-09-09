#include "spFont.h"
#include "spTextureData.h"
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
}
