#include "spSkyBox.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spSkyBox>();}
        const spRTTIRecord Record{spSkyBox::ClassID,spRenderNode::ClassID,"spSkyBox",
            &spRenderNode::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spSkyBox::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spSkyBox::vfunc_18() const noexcept{return Record;}
    // Nonempty native clone has not been compared; do not inherit a clone
    // which would silently create an ordinary RenderNode with a different ID.
    std::unique_ptr<spBaseObject> spSkyBox::vfunc_10(spCloneManager&) const{return nullptr;}
    bool spSkyBox::vfunc_14(spBaseObject&,spCloneManager&) const{return false;}
    bool spSkyBox::UpdateWorldForAnalysis(std::uint32_t flags,const Matrix3* camera) noexcept
    {
        if(!spRenderNode::UpdateWorldForAnalysis(flags,camera))return false;
        // Original49E440 writes this AFTER the inherited bounds/light update,
        // even when the inherited transform dirty bit was clear.
        worldOrientation_=GetOrientationForAnalysis();return true;
    }
    bool spSkyBox::RenderSkyForAnalysis(BaseSupportDrawForAnalysis draw,void* context)
    {
        if(!draw)return false; // absent host backend; no native NULL call
        for(std::size_t i=0;i<GetRenderableCountForAnalysis();++i)
            GetRenderableForAnalysis(i)->SetFogForAnalysis(nullptr);
        return draw(context,*this,false);
    }
}
