#include "wxLogoSharedResourceLoading.h"
#include "wxAssetManager.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace winx::reconstruction
{
    void LoadLogoSharedResourcesForAnalysis(wxLogoResourceLoadingHost& host)
    {
        std::array<char,0x100> name;
        // Safe host storage only, not a native buffer default. Admitted shared
        // BuildPath results provide all used bytes; tail bytes are not state.
        name.fill('\x7F');
        constexpr std::array<std::string_view,2> keys{"loading_empty.stx","loading_full.stx"};
        for(std::size_t index=0;index<keys.size();++index)
        {
            auto* service=host.ResolveLogoAssetManagerForAnalysis();
            if(!service)throw std::logic_error("Logo resource loading requires a name service");
            std::string path;
            if(!service->BuildPathForAnalysis(path,0x100,4,0,keys[index]))
                throw std::domain_error("Logo resource path requires an initialized asset catalog and capacity256");
            std::copy(path.begin(),path.end(),name.begin());name[path.size()]='\0';
            auto* manager=host.ResolveLogoResourceManagerForAnalysis();
            if(!manager)throw std::logic_error("Logo resource loading requires a ResourceManager");
            auto* loaded=host.LoadLogoResourceForAnalysis(*manager,name.data());
            // Native reads the global AFTER LoadResource returns. Loaded is
            // retained across the old object's deleting callback.
            auto*& slot=host.BorrowLogoSharedResourceForAnalysis(index);
            auto* old=slot;
            if(old==loaded)continue;
            if(old)
            {
                auto& refs=host.BorrowLogoNativeReferenceCount8ForAnalysis(*old);
                refs=static_cast<std::uint16_t>(refs-1u);
                if(refs==0)host.DestroyLogoReferencedObjectForAnalysis(*old,1);
            }
            if(loaded)
            {
                auto& refs=host.BorrowLogoNativeReferenceCount8ForAnalysis(*loaded);
                refs=static_cast<std::uint16_t>(refs+1u);
            }
            slot=loaded;
        }
    }
}
