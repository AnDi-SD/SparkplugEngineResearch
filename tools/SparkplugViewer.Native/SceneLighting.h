#pragma once
// HOST preview ownership/scheduling around the reconstructed light manager.
// This does not implement the original spScene frame or partition registration.
#include "ResourceGraph.h"
#include "Code/Sparkplug/spLightManager.h"
#include "Code/Sparkplug/spRenderNode.h"
#include <unordered_set>
#include <stdexcept>

namespace spvhost {
class SceneLighting final {
    using Node = sparkplug::reconstruction::spNode;
    using Light = sparkplug::reconstruction::spLight;
    using RenderNode = sparkplug::reconstruction::spRenderNode;
    std::shared_ptr<ResourceGraph> graph_;
    sparkplug::reconstruction::spLightManager manager_;
    std::vector<Light*> lights_;
    std::vector<RenderNode*> targets_;
    std::vector<Node*> roots_;
    std::vector<std::pair<Node*,bool>> originalHierarchy_;
    bool attached_ = false;
    static void Need(bool value,const char* error) { if(!value)throw std::runtime_error(error); }
    void SaveHierarchy(Node& node,std::unordered_set<Node*>& visited,unsigned depth) {
        Need(depth<=256&&visited.insert(&node).second,"Lighting requires a bounded acyclic Node graph");
        originalHierarchy_.push_back({&node,node.IsHierarchyActiveForAnalysis()});
        for(std::size_t i=0;i<node.GetChildCountForAnalysis();++i)
            SaveHierarchy(*node.GetChildForAnalysis(i),visited,depth+1);
    }
public:
    SceneLighting(std::shared_ptr<ResourceGraph> graph,
        const std::vector<std::shared_ptr<Node>>& nodes,const std::uint32_t* ids,std::uint32_t count)
        :graph_(std::move(graph)) {
        Need(graph_&&(!count||ids)&&count<=4096,"Invalid lighting graph or light selection");
        Need(!graph_->lightingOwner,"This graph already has a lighting context");
        std::unordered_set<Node*> selected;
        for(const auto& node:nodes)selected.insert(node.get());
        for(auto id:graph_->nodeIDs)
            Need(selected.count(graph_->Node(id).get())!=0,"Lighting requires the complete loaded Node graph");
        std::unordered_set<Node*> visited;
        for(const auto& node:nodes)if(!node->GetParentForAnalysis()) {
            roots_.push_back(node.get());SaveHierarchy(*node,visited,0);
        }
        Need(visited.size()==selected.size(),"Lighting selection omits a Node root or descendant");
        lights_.reserve(count);
        for(std::uint32_t i=0;i<count;++i) {
            auto* light=dynamic_cast<Light*>(graph_->Find(ids[i]));
            Need(light&&selected.count(light),"Selected object is not a Light in this scene");
            Need(manager_.RegisterLightForAnalysis(*light),"Repeated or unsupported light registration");
            lights_.push_back(light);
        }
        for(const auto& node:nodes)if(auto* target=dynamic_cast<RenderNode*>(node.get())) {
            Need(manager_.RegisterRenderTargetForAnalysis(target->GetLightCacheForAnalysis(),
                target->GetWorldBoundingSphereForAnalysis(),target->GetSupportControlsForAnalysis()[1]!=0),
                "Too many or repeated light-cache targets");
            targets_.push_back(target);
        }
    }
    SceneLighting(const SceneLighting&)=delete;
    SceneLighting& operator=(const SceneLighting&)=delete;
    ~SceneLighting() {
        if(!attached_)return;
        for(auto* target:targets_) {
            target->SetSceneLightManagerForAnalysis(nullptr);
            (void)manager_.UnregisterRenderTargetForAnalysis(target->GetLightCacheForAnalysis());
            target->GetLightCacheForAnalysis().ResetSelection();
        }
        for(auto* light:lights_) {
            light->SetSceneLightManagerForAnalysis(nullptr);
            (void)manager_.UnregisterLightForAnalysis(*light);
        }
        // Parent-before-child snapshots restore mixed flags despite the
        // original setter propagating its value recursively to descendants.
        for(const auto& saved:originalHierarchy_)
            if(saved.first->IsHierarchyActiveForAnalysis()!=saved.second)
                saved.first->SetHierarchyActiveForAnalysis(saved.second);
        graph_->lightingOwner=nullptr;
    }
    void Attach(bool activeHierarchy) {
        Need(!attached_&&!graph_->lightingOwner,"Lighting context ownership conflict");
        graph_->lightingOwner=this;attached_=true;
        for(auto* target:targets_) {
            target->GetLightCacheForAnalysis().ResetSelection();
            target->SetSceneLightManagerForAnalysis(&manager_);
        }
        for(auto* light:lights_)light->SetSceneLightManagerForAnalysis(&manager_);
        SetHierarchyActive(activeHierarchy);
    }
    void SetHierarchyActive(bool active) noexcept {
        for(auto* root:roots_)root->SetHierarchyActiveForAnalysis(active);
        for(auto* light:lights_)light->MarkLightDataDirtyForAnalysis();
    }
    void RefreshAfterSample() noexcept {
        // Explicit preview scheduling: all Node poses are current before the
        // final selection. Reuse the original selection method, without a
        // second eligibility/capacity/order algorithm in the application.
        for(auto* target:targets_)
            if(target->GetSupportControlsForAnalysis()[3]&&target->IsEnabledForAnalysis())
                manager_.RebuildCacheForAnalysis(target->GetLightCacheForAnalysis(),
                    target->GetWorldBoundingSphereForAnalysis(),target->GetSupportControlsForAnalysis()[1]!=0);
    }
    const std::vector<RenderNode*>& Targets() const noexcept {return targets_;}
};
}
