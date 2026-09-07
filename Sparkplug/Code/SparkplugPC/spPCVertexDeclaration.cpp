#include "spPCVertexDeclaration.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spPCVertexDeclaration>(); }
        const spRTTIRecord Record{spPCVertexDeclaration::ClassID,spDXVertexDeclaration::ClassID,
            "spPCVertexDeclaration",&spDXVertexDeclaration::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spPCVertexDeclaration::StaticRTTI() noexcept { (void)Registered;return Record; }
    const spRTTIRecord& spPCVertexDeclaration::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spPCVertexDeclaration::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spPCVertexDeclaration>();manager.RegisterClone(*this,*clone);
        // Native concrete clone copies inherited shared name only. Base copy
        // does not propagate opaque root fieldC or declaration payload.
        return spNamedObject::vfunc_14(*clone,manager) ? std::move(clone) : nullptr;
    }
    bool spPCVertexDeclaration::InitializeForAnalysis(std::uint32_t flags)
    {
        if(!spDXVertexDeclaration::InitializeForAnalysis(flags))return false;
        return BuildElementsForAnalysis(flags,elements_,allocationBytes_);
    }
    bool spPCVertexDeclaration::BuildElementsForAnalysis(std::uint32_t flags,
        std::vector<spPCVertexElementForAnalysis>& elements,std::uint32_t& nativeAllocationBytes)
    {
        elements.clear();nativeAllocationBytes=0;
        // 4B22B0: highest UV bit determines count, not popcount. The weight
        // count used by this helper is4/2/1 for bits8/4/2; bit10 is NOT tested.
        std::uint32_t uv=0;
        for(std::uint32_t i=0;i<8;++i)if(flags&(0x800u<<i))uv=i+1;
        const std::uint32_t weights=(flags&8)?4:(flags&4)?2:(flags&2)?1:0;
        std::uint32_t capacity=1+uv+weights;
        for(const auto bit:{0x40u,0x80u,0x100u,0x200u,0x400u,0x20u,0x80000u,0x100000u})
            if(flags&bit)++capacity;
        nativeAllocationBytes=(capacity+1)*8;
        try
        {
            elements.reserve(capacity+1);
            std::uint16_t offset=0;
            auto add=[&](std::uint8_t type,std::uint8_t usage,std::uint16_t advance,std::uint8_t index=0)
            { elements.push_back({0,offset,type,0,usage,index});offset=static_cast<std::uint16_t>(offset+advance); };
            add((flags&1)?3:2,0,(flags&1)?16:12);
            if(weights)add(weights==1?0:3,1,weights==1?4:16);
            if(flags&0x20)add(3,2,16);
            if(flags&0x40)add(2,3,12);
            if(flags&0x80)add(0,4,4);
            if(flags&0x100)add(4,10,4);
            if(flags&0x200)add(4,10,4,1);
            if(flags&0x400)add(2,5,16); // Native advances16 despite type2 occupying12.
            for(std::uint32_t i=0;i<uv;++i)add(1,5,8,static_cast<std::uint8_t>(i));
            if(flags&0x80000)add(2,6,12);
            if(flags&0x100000)add(2,7,12);
            elements.push_back({0xff,0,0x11,0,0,0});
            return true;
        }
        catch(...)
        {
            elements.clear();nativeAllocationBytes=0;return false; // explicit safe host allocation failure
        }
    }
}
