#include "spDXVertexDeclaration.h"
#include "spDXMesh.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXVertexDeclaration::ClassID,spCrossPlatform::ClassID,
            "spDXVertexDeclaration",&spCrossPlatform::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXVertexDeclaration::StaticRTTI() noexcept
    { (void)Registered;return Record; }
    const spRTTIRecord& spDXVertexDeclaration::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spDXVertexDeclaration::vfunc_10(spCloneManager&) const
    { return nullptr; } // Native4A1BF0, separate from concrete PC clone.
    bool spDXVertexDeclaration::InitializeForAnalysis(std::uint32_t flags)
    { fvfCode_=spDXMesh::ComponentFlagsToFVFForAnalysis(flags);return true; }
}
