#include "spDXShaderManager.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXShaderManager::ClassID,spBaseObject::ClassID,"spDXShaderManager",&spBaseObject::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXShaderManager::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXShaderManager::vfunc_18() const noexcept{return Record;}
    bool spDXShaderManager::ScanStandaloneForAnalysis(std::string_view root,
        const DirectoryForAnalysis& directory,const LoadStandaloneForAnalysis& load)
    {
        root=root.substr(0,root.find('\0'));
        // Native sprintf has a 256-byte stack target. Restrict this portable
        // interface to a fresh manager and paths fitting that original target.
        if(scanAttempted_||!directory.first||!directory.next||!directory.close||!load||root.size()+13>=256)return false;
        scanAttempted_=true;
        for(bool pixel:{false,true})
        {
            const std::string glob=std::string(root)+(pixel?"Shaders\\*.psh":"Shaders\\*.vsh");
            std::string basename;const auto handle=directory.first(glob,basename);
            if(handle!=-1)
            {
                do
                {
                    auto shader=load(basename,pixel);
                    if(shader)(pixel?pixelShaders_:vertexShaders_).push_back(std::move(shader));
                }while(directory.next(handle,basename));
            }
            directory.close(handle);
        }
        // PC4B0890 returns AL=1 even when no file or no shader was retained.
        return true;
    }
}
