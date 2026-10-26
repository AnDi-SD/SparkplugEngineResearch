#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Code/Sparkplug/spResourceManager.h"

namespace winx::reconstruction
{
    class wxAssetManager;
    // The shared wxAssetManager builds paths directly. ResourceManager loading
    // remains an explicit foreign operation. The
    // shared-slot and native-ref words are external storage, not shared_ptr
    // counts. Their addresses stay valid while callback contents may change.
    class wxLogoResourceLoadingHost
    {
    public:
        virtual ~wxLogoResourceLoadingHost()=default;
        virtual wxAssetManager* ResolveLogoAssetManagerForAnalysis()=0;
        virtual sparkplug::reconstruction::spResourceManager* ResolveLogoResourceManagerForAnalysis()=0;
        // The name points into the same temporary buffer for both calls and
        // has synchronous lifetime. Native query return status is ignored.
        virtual sparkplug::reconstruction::spBaseObject* LoadLogoResourceForAnalysis(
            sparkplug::reconstruction::spResourceManager& manager,const char* name)=0;
        virtual sparkplug::reconstruction::spBaseObject*& BorrowLogoSharedResourceForAnalysis(
            std::size_t index)=0;
        virtual std::uint16_t& BorrowLogoNativeReferenceCount8ForAnalysis(
            sparkplug::reconstruction::spBaseObject&)=0;
        virtual void DestroyLogoReferencedObjectForAnalysis(
            sparkplug::reconstruction::spBaseObject&,std::uint32_t deletingFlag)=0;
    };
}
