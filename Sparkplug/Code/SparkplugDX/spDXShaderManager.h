#pragma once
// Original PC98BA76FE, abstract registration, base415352A1; path inferred.
#include "../SparkBase/spBaseObject.h"
namespace sparkplug::reconstruction
{
    class spDXShaderManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x98BA76FE;
        ~spDXShaderManager() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    protected:
        spDXShaderManager() noexcept=default;
        // First two owned vectors and base name map are still unexposed:
        // startup file scanning/compile ownership is not reconstructed yet.
    };
}
