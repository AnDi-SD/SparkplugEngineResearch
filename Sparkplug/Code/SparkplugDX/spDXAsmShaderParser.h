#pragma once

// Original PC class identity; declaration/TU path inferred. The factory has
// exactly the16-byte BaseObject prefix and no additional instance storage.
// This is the verified lifecycle interface, not the still-unrecovered assembly
// grammar, source parsing or an invented successful shader compiler.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spDXAsmShaderParser final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x0B3E0AA9;
        static constexpr std::uint32_t NativeObjectSizeForAnalysis=0x10;

        spDXAsmShaderParser() noexcept=default;
        ~spDXAsmShaderParser() override=default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
    };
}
