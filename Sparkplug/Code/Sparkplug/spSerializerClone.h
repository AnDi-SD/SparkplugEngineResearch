#pragma once

#include "spSerializer.h"
#include <utility>

namespace sparkplug::reconstruction
{
    // Inferred source organisation for the repeated concrete Clone sequence;
    // not an additional native class. The caller uses its own native factory.
    // PC/PS2 evidence: native-spatial-serializer-clone-fix-2026-09-10.md.
    inline std::unique_ptr<spBaseObject> CloneConcreteSerializerForAnalysis(
        const spSerializer& source, std::unique_ptr<spBaseObject> clone,
        spCloneManager& manager)
    {
        if (!clone) return nullptr;
        manager.RegisterCloneForAnalysis(source, *clone);
        return source.vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
}
