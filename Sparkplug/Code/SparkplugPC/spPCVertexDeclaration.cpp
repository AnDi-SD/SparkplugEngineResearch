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
        return BuildPCVertexDeclarationElementsForAnalysis(flags,elements,nativeAllocationBytes);
    }
}
