#include "spCameraManager.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spCameraManager>();}
        const spRTTIRecord record{spCameraManager::ClassID,spBaseObject::ClassID,
            "spCameraManager",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    spCameraManager* spCameraManager::instance_=nullptr;
    spCameraManager::spCameraManager()
    {
        words_.reserve(InitialLogicalWordSlotsForAnalysis);
        instance_=this;
        // Successful-return protocol only. Native allocator failure/startup,
        // protected factory internals and secondary class are unqualified.
    }
    spCameraManager::~spCameraManager()
    {
        // PC45AF0B releases the backing storage BEFORE global45AF3B=0.
        // Destroy the swap temporary at this semicolon, before the clear.
        std::vector<std::uint32_t>{}.swap(words_);
        // The native store is unconditional, including older-first teardown.
        instance_=nullptr;
    }
    const spRTTIRecord& spCameraManager::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spCameraManager::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spCameraManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spCameraManager>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return spBaseObject::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spCameraManager::ApplyCapturedRegistryForAnalysis(const spCameraManagerCapturedRegistryForAnalysis& input)
    {
        if(input.profile!=spCameraManagerCapturedRegistryForAnalysis::Profile::PCNonOwningRawWords
            ||input.count>input.rawWords.size())return false;
        // Fixed independent input prevents aliasing the source vector while
        // replacing it. Capacity32 already covers the admitted <=8 words.
        words_.clear();
        for(std::size_t i=0;i<input.count;++i)words_.push_back(input.rawWords[i]);
        field14_=input.field14;opaque18_=input.capturedOpaque18;
        return true;
    }
}
