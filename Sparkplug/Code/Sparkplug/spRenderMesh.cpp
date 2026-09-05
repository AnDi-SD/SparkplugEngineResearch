#include "spRenderMesh.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord RenderMeshRecord{
            spRenderMesh::ClassID,
            spMesh::ClassID,
            "spRenderMesh",
            &spMesh::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool RenderMeshRegistered =
            spRTTIManager::Instance().Register(RenderMeshRecord);
    }

    spRenderMesh::~spRenderMesh() = default;

    const spRTTIRecord& spRenderMesh::StaticRTTI() noexcept
    {
        (void)RenderMeshRegistered;
        return RenderMeshRecord;
    }

    const spRTTIRecord& spRenderMesh::vfunc_18() const noexcept
    {
        return RenderMeshRecord;
    }
}
