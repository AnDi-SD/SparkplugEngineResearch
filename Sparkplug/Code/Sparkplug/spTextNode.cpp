#include "spTextNode.h"
#include "spTextRenderable.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> CreateTextNode(){return std::make_unique<spTextNode>();}
const spRTTIRecord Record{spTextNode::ClassID,spRenderNode::ClassID,"spTextNode",
    &spRenderNode::StaticRTTI(),&CreateTextNode,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spTextNode::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spTextNode::vfunc_18() const noexcept{return Record;}
spTextNode::~spTextNode(){auxiliary_.reset();text_.reset();}
bool spTextNode::AttachRenderableForAnalysis(std::shared_ptr<spRenderable> renderable){
    if(auto text=std::dynamic_pointer_cast<spTextRenderable>(renderable))text_=std::move(text);
    return spRenderNode::AttachRenderableForAnalysis(std::move(renderable));
}
std::shared_ptr<spRenderable> spTextNode::DetachRenderableForAnalysis(spRenderable& renderable) noexcept{
    if(text_.get()==&renderable)text_.reset();
    return spRenderNode::DetachRenderableForAnalysis(renderable);
}
void spTextNode::ClearRenderablesForAnalysis() noexcept{
    text_.reset();spRenderNode::ClearRenderablesForAnalysis();
}
}
