#include "spDXRenderer.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"

namespace sparkplug::reconstruction
{
    spDXRenderer::DeviceDeclarationLookupForAnalysis spDXRenderer::GetDeviceVertexDeclarationForAnalysis(
        std::uint32_t flags,std::uintptr_t device,const spPCVertexDeclarationDeviceForAnalysis& api)
    {
        const auto found=vertexDeclarations_.find(flags);
        if(found!=vertexDeclarations_.end())
        {
            // A portable CPU-only cache entry has no original COM state.
            // Captured null and actual known-null device results remain hits.
            return {!found->second||found->second->HasDeviceInitializationForAnalysis(),found->second};
        }
        if(vertexDeclarations_.size()>=4096)return {};
        try
        {
            auto declaration=std::make_shared<spPCVertexDeclaration>();
            const auto initialized=declaration->InitializeDeviceForAnalysis(flags,device,api);
            if(!initialized.nativeReturn)return {};
            vertexDeclarations_.emplace(flags,declaration);return {initialized.completed,std::move(declaration)};
        }
        catch(...){return {};}
    }
    bool spDXRenderer::InsertCapturedVertexDeclarationForAnalysis(std::uint32_t flags,
        std::shared_ptr<spPCVertexDeclaration> value)
    {
        if(vertexDeclarations_.size()>=4096)return false;
        try{return vertexDeclarations_.emplace(flags,std::move(value)).second;}
        catch(...){return false;}
    }
    bool spDXRenderer::ReleaseDeviceVertexDeclarationsForAnalysis() noexcept
    {
        for(auto& pair:vertexDeclarations_)
            if(pair.second&&!pair.second->ReleaseDeviceForAnalysis())return false;
        vertexDeclarations_.clear();return true;
    }
}
