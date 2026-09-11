#pragma once
// Owned host observation of the common PC generator and default Font material.
// No editable Mesh resource or synthetic FAT identity is introduced.
#include "MaterialSubmission.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spTexture.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <cmath>
#include <stdexcept>
namespace spvhost {
class TextView final {
public:
    SpvTextViewInfo info{};
    sparkplug::reconstruction::spFontManager::Text3DGeometryForAnalysis geometry;
    std::array<SpvMaterialDrawPass,8> draws{};
    TextView(std::shared_ptr<ResourceGraph> graph,std::uint32_t id):graph_(std::move(graph)) {
        using namespace sparkplug::reconstruction;
        auto* text=dynamic_cast<spTextRenderable*>(graph_->Find(id));
        Check(text,"TEXT_GEOMETRY_TYPE: expected loaded TextRenderable");
        const auto& font=text->GetFontForAnalysis();
        // PC41F3D3/41F3DC returns before Font/material/atlas dependencies.
        // An empty Text is a valid no-draw observation, including omitted Font.
        if(!text->GetTextForAnalysis()||text->GetTextForAnalysis()->empty()) {
            const auto* material=dynamic_cast<const spDXMaterial*>(text->GetMaterialForAnalysis().get());
            info.font=graph_->ID(font.get());info.atlas=font?graph_->ID(font->GetImageForAnalysis()):0;
            info.material=graph_->ID(material);info.powerAssigned=material&&material->HasInitializedSpecularPowerForAnalysis()?1u:0u;
            return;
        }
        Check(bool(font),"TEXT_DEFAULT_FONT: platform default Font is not available");
        auto* atlas=font->GetImageForAnalysis();
        Check(atlas&&graph_->ID(atlas),"TEXT_ATLAS: expected a canonical graph texture");
        text_=text;
        Check(manager_.InitializePCMaterialForAnalysis(),"Text default material initialization failed");
        std::string error;
        if(!text->SelectPCDrawResourcesForAnalysis(manager_,&error)||!manager_.BindPCTextAtlasForAnalysis(&error))throw std::runtime_error(error);
        if(!text->BuildPCGeometryForAnalysis(manager_,geometry,&error))throw std::runtime_error(error);
        // Explicit modern upload bound: original geometry retains raw UV bits.
        for(const auto& vertex:geometry.vertices) {
            for(auto value:vertex.position)Check(std::isfinite(value),"TEXT_NONFINITE_POSITION");
            for(auto value:vertex.uv)Check(std::isfinite(value),"TEXT_NONFINITE_UV");
        }
        auto* material=dynamic_cast<spDXMaterial*>(manager_.GetCurrentMaterialForAnalysis().get());
        Check(material,"Text default is not a PC material");
        submission_=std::make_unique<MaterialSubmission>(graph_);
        const auto materialID=graph_->ID(material);
        info={graph_->ID(font.get()),graph_->ID(atlas),static_cast<std::uint32_t>(geometry.vertices.size()),
            static_cast<std::uint32_t>(geometry.indices.size()),material->HasInitializedSpecularPowerForAnalysis()?1u:0u,
            static_cast<std::uint32_t>(material->GetPassCountForAnalysis()),materialID};
        Capture(1,draws.data(),info.passes);
    }
    void Capture(std::uint32_t frame,SpvMaterialDrawPass* output,std::uint32_t count) {
        Check(count==info.passes&&(output||!count),"Text draw extent mismatch");
        if(geometry.vertices.empty()&&count==0)return;
        std::string error;
        if(!text_->SelectPCDrawResourcesForAnalysis(manager_,&error)||!manager_.BindPCTextAtlasForAnalysis(&error))throw std::runtime_error(error);
        auto* material=dynamic_cast<sparkplug::reconstruction::spDXMaterial*>(manager_.GetCurrentMaterialForAnalysis().get());
        Check(material,"Text selected material is not PC");
        std::uint32_t produced=0;
        if(info.material)submission_->Capture(info.material,frame,output,count,&produced);
        else submission_->CaptureTextDefault(*material,frame,output,count,&produced);
    }
private:
    std::shared_ptr<ResourceGraph> graph_;
    sparkplug::reconstruction::spFontManager manager_;
    sparkplug::reconstruction::spTextRenderable* text_=nullptr;
    std::unique_ptr<MaterialSubmission> submission_;
    static void Check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
};
}
