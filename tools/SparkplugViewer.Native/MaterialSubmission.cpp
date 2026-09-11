#include "MaterialSubmission.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include <algorithm>
#include <stdexcept>

namespace spvhost {
using namespace sparkplug::reconstruction;
namespace {
constexpr std::array<std::uint32_t,16> RenderIndices{7,8,9,14,15,19,20,22,23,24,25,27,29,137,145,148};
void Check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
}
std::int32_t MaterialSubmission::Render(void* context,std::uint32_t index,std::uint32_t value) noexcept {
    auto& self=*static_cast<MaterialSubmission*>(context);
    const auto found=std::find(RenderIndices.begin(),RenderIndices.end(),index);
    if(found!=RenderIndices.end()) {
        const auto slot=static_cast<unsigned>(found-RenderIndices.begin());
        self.current_.render[slot]=value;self.current_.knownRender|=1u<<slot;
    }
    return 0;
}
std::int32_t MaterialSubmission::RenderCached(void* context,std::uint32_t index,std::uint32_t value) noexcept {
    auto& self=*static_cast<MaterialSubmission*>(context);
    return index<self.state_.deviceStates.size()&&Renderer::ApplyRenderStateCacheEntryForAnalysis(
        self.state_.deviceStates[index],index,value,Render,context)?0:-1;
}
std::int32_t MaterialSubmission::Texture(void* context,std::uint32_t stage,std::uintptr_t id) noexcept {
    if(stage>=8)return -1;
    static_cast<MaterialSubmission*>(context)->current_.textures[stage]=static_cast<std::uint32_t>(id);return 0;
}
std::int32_t MaterialSubmission::Palette(void*,std::uint32_t) noexcept {return 0;} // runtime BGRA upload already resolves palette
std::int32_t MaterialSubmission::TextureState(void* context,bool sampler,std::uint32_t stage,std::uint32_t index,std::uint32_t value) noexcept {
    if(stage>=8)return -1;int slot=-1;
    if(sampler) {
        switch(index) {case 1:slot=2;break;case 2:slot=3;break;case 4:slot=4;break;
        case 5:slot=5;break;case 6:slot=6;break;case 7:slot=7;break;default:return -1;}
    } else {
        switch(index) {case 1:slot=0;break;case 4:slot=1;break;case 11:slot=8;break;case 24:slot=9;break;default:return -1;}
    }
    auto& self=*static_cast<MaterialSubmission*>(context);
    self.current_.stages[stage*10+slot]=value;self.current_.knownStages[stage]|=1u<<slot;return 0;
}
std::int32_t MaterialSubmission::Transform(void* context,std::uint32_t deviceStage,const Renderer::TextureMatrix4ForAnalysis& matrix) noexcept {
    if(deviceStage<16||deviceStage>=24)return -1;
    auto& self=*static_cast<MaterialSubmission*>(context);const auto stage=deviceStage-16;
    std::copy(matrix.begin(),matrix.end(),self.current_.uv+stage*16);self.current_.knownUV|=1u<<stage;return 0;
}
bool MaterialSubmission::UV(void* context,std::uint32_t stage,const Renderer::TextureMatrix3ForAnalysis& matrix) {
    auto& self=*static_cast<MaterialSubmission*>(context);
    return stage<8&&Renderer::ApplyTextureTransform3ForAnalysis(self.state_.uv[stage],stage,matrix,Transform,context);
}
MaterialSubmission::MaterialSubmission(std::shared_ptr<ResourceGraph> graph):graph_(std::move(graph)),fallback_(spRenderer::CreatePCDefaultMaterialForAnalysis()) {
    Check(bool(graph_)&&bool(fallback_),"Missing material submission graph/default state");
    Check(Renderer::InitializePCSubmissionCachesForAnalysis(state_,Render,this),"Material submission cache initialization failed");
    // Explicit modern backend initial sources match the restored raw source
    // caches (11/10). The original complete device startup is not claimed.
    (void)Render(this,145,1);(void)Render(this,148,0);
    // Other device states/UV stay unknown until an actual producer writes them.
}
void MaterialSubmission::Capture(std::uint32_t id,std::uint32_t frame,
    SpvMaterialDrawPass* output,std::uint32_t capacity,std::uint32_t* count) {
    auto* material=dynamic_cast<spDXMaterial*>(graph_->Find(id));
    Check(material&&material->HasInitializedSpecularPowerForAnalysis(),"Material draw requires loaded PC material with known power");
    CaptureObject(material,frame,output,capacity,count,Renderer::UnassignedPowerPolicyForAnalysis::Reject);
}
void MaterialSubmission::CaptureTextDefault(spDXMaterial& material,std::uint32_t frame,
    SpvMaterialDrawPass* output,std::uint32_t capacity,std::uint32_t* count) {
    Check(material.GetRenderStatesForAnalysis()[8]==2,"Text default requires unlit vertex color mode");
    CaptureObject(&material,frame,output,capacity,count,Renderer::UnassignedPowerPolicyForAnalysis::ObserveUnknown);
}
void MaterialSubmission::CaptureObject(spDXMaterial* material,std::uint32_t frame,
    SpvMaterialDrawPass* output,std::uint32_t capacity,std::uint32_t* count,
    Renderer::UnassignedPowerPolicyForAnalysis powerPolicy) {
    Check(count&&capacity<=8&&(output||!capacity),"Invalid material draw output");
    const auto passCount=material->GetPassCountForAnalysis();
    Check(passCount<=capacity,"Material draw output capacity is too small");
    std::array<spMaterialPassLayer*,8> passes{};
    for(std::size_t i=0;i<passCount;++i) {
        passes[i]=dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(i));
        Check(passes[i]&&passes[i]->GetLayerCountForAnalysis()<=8,"Unsupported material draw pass");
        for(std::size_t l=0;l<passes[i]->GetLayerCountForAnalysis();++l)
            Check(passes[i]->GetLayerForAnalysis(l)&&passes[i]->GetLayerForAnalysis(l)->GetMaterialTextureForAnalysis(),"Missing material draw layer");
    }
    state_.frame=frame;
    Check(Renderer::InstallMaterialForAnalysis(state_.lighting,state_.installedMaterial,material,frame,nullptr,powerPolicy),"Material draw color update failed; prior mutations retained");
    Check(Renderer::ApplyMaterialStateSetForAnalysis(state_.raw,material->GetRenderStatesForAnalysis(),nullptr,state_.lighting,RenderCached,this),"Material draw state translation failed; prior mutations retained");
    const auto& lighting=state_.lighting;
    std::copy(lighting.diffuse.begin(),lighting.diffuse.end(),current_.colors);
    std::copy(lighting.ambient.begin(),lighting.ambient.end(),current_.colors+4);
    std::copy(lighting.specular.begin(),lighting.specular.end(),current_.colors+8);
    std::copy(lighting.emissive.begin(),lighting.emissive.end(),current_.colors+12);
    current_.colors[16]=lighting.specularPower;current_.vertexAlpha=material->GetVertexAlphaByteForAnalysis();
    const auto* defaultPass=dynamic_cast<spMaterialPassLayer*>(fallback_->GetPassForAnalysis(0));
    std::array<const spMaterialTexture*,2> defaults{};
    for(unsigned i=0;i<2;++i)defaults[i]=defaultPass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis().get();
    std::array<SpvMaterialDrawPass,8> result{};
    for(std::size_t p=0;p<passCount;++p) {
        auto& pass=*passes[p];
        Check(pass.UpdateForRenderForAnalysis(0xffffffffu,UV,this),"Material draw pass update failed; prior mutations retained");
        std::array<Renderer::ResolvedTextureBindingForAnalysis,8> bindings{};
        for(std::size_t l=0;l<pass.GetLayerCountForAnalysis();++l) {
            const auto* texture=pass.GetLayerForAnalysis(l)->GetMaterialTextureForAnalysis()->GetTextureForAnalysis();
            if(!texture)continue;
            const auto* dx=dynamic_cast<const spDXTexture*>(texture);const auto textureID=graph_->ID(texture);
            Check(dx&&textureID,"Material draw texture is outside the loaded PC graph");
            bindings[l].identity=reinterpret_cast<std::uintptr_t>(texture);bindings[l].deviceTexture=textureID;
            if(const auto* palette=dx->GetPaletteForAnalysis())bindings[l].paletteIndex=palette->GetIndexForAnalysis();
        }
        Check(Renderer::ApplyPassTextureStatesForAnalysis(state_.textures,pass,defaults,bindings,nullptr,false,false,
            Texture,Palette,TextureState,this),"Material draw texture state translation failed; prior mutations retained");
        Check(Renderer::ApplyPassBlendForAnalysis(state_.raw,*material,pass,nullptr,RenderCached,this),"Material draw blend translation failed; prior mutations retained");
        current_.pass=static_cast<std::uint32_t>(p);result[p]=current_;
    }
    // Outputs are atomic; original graph/controller/cache mutations are not.
    std::copy_n(result.begin(),passCount,output);*count=static_cast<std::uint32_t>(passCount);
}
}
