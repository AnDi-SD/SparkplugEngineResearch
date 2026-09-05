#pragma once

// Inferred declaration path. The native class name and complete two-platform
// ABI survive, but neither executable preserves an original source filename.

#include "spMesh.h"

namespace sparkplug::reconstruction
{
    // Native spRenderMesh is an abstract, storage-free identity layer between
    // spMesh and platform render meshes. It does not override the null clone
    // slot inherited from spMesh.
    class spRenderMesh : public spMesh
    {
    public:
        static constexpr spClassID ClassID = 0x67974A9C;

        spRenderMesh() noexcept = default;
        ~spRenderMesh() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}
