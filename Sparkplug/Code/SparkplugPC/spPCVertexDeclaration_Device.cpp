#include "spPCVertexDeclaration.h"
#include <utility>

namespace sparkplug::reconstruction
{
    spPCVertexDeclaration::~spPCVertexDeclaration(){(void)ReleaseDeviceForAnalysis();}

    spPCVertexDeclarationResultForAnalysis spPCVertexDeclaration::InitializeDeviceForAnalysis(
        std::uint32_t flags,std::uintptr_t device,const spPCVertexDeclarationDeviceForAnalysis& api)
    {
        if(!device||!api.create||!api.release)return {};
        if(deviceHandle_&&(releaseContext_!=api.context||releaseDevice_!=api.release))return {};
        // Original4B23C0 precedes the temporary element builder and COM call.
        if(!spDXVertexDeclaration::InitializeForAnalysis(flags))return {};
        spPCVertexDeclarationOutputForAnalysis output{flags,false};
        {
            std::vector<spPCVertexElementForAnalysis> temporary;std::uint32_t bytes=0;
            if(!BuildElementsForAnalysis(flags,temporary,bytes))return {};
            // Extra CPU shadow is the existing analysis facade, separate from
            // the COM input's temporary storage. Prepare it before acquiring a
            // COM output so host allocation failure cannot hide that output.
            try{elements_=temporary;}catch(...){return {};}
            allocationBytes_=bytes;releaseContext_=api.context;releaseDevice_=api.release;
            (void)api.create(api.context,device,temporary.data(),temporary.size(),output);
        } // native4C9D5D frees temporary elements before publishing field18
        deviceHandle_=output.value;
        deviceHandleInitialized_=output.initialized||!output.value;
        deviceInitializationAttempted_=true;
        return {deviceHandleInitialized_,true};
    }
    spPCVertexDeclarationResultForAnalysis spPCVertexDeclaration::BindDeviceForAnalysis(
        std::uintptr_t device,const spPCVertexDeclarationDeviceForAnalysis& api,std::uintptr_t) const noexcept
    {
        if(!device||!api.bind||!deviceHandleInitialized_)return {};
        (void)api.bind(api.context,device,deviceHandle_);
        return {true,true};
    }
    bool spPCVertexDeclaration::ReleaseDeviceForAnalysis() noexcept
    {
        if(!deviceHandleInitialized_)return false;
        if(deviceHandle_)
        {
            // Unlike the index-buffer wrapper, declaration dtor clears its
            // field regardless of the returned COM reference count.
            (void)releaseDevice_(releaseContext_,deviceHandle_);deviceHandle_=0;
        }
        return true;
    }
}
