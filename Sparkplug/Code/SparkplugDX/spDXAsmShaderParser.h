#pragma once

// Original PC class identity; declaration/TU path inferred. The factory has
// exactly the16-byte BaseObject prefix and no additional instance storage.
// The nonvirtual parser consumes register/Component comments. Instruction
// assembly belongs to the external D3DX assembler, not this class.
#include "../SparkBase/spBaseObject.h"
#include <string_view>

namespace sparkplug::reconstruction
{
    class spDXShader;
    class spDXAsmShaderParser final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x0B3E0AA9;
        static constexpr std::uint32_t NativeObjectSizeForAnalysis=0x10;

        spDXAsmShaderParser() noexcept=default;
        ~spDXAsmShaderParser() override=default;

        // PC4CF950: scan every // marker, append declared parameters and OR
        // the case-sensitive component masks. Existing descriptors remain;
        // the output mask is reset. Native always returns AL=1.
        // False is a host-domain guard: a matching name must fit 32 bytes
        // including NUL and decimal conversions must fit signed int32.
        // Guard failures are refused before any output mutation. Input is a
        // C string (the first embedded NUL ends it), not an ASM grammar.
        [[nodiscard]] bool ParseAnnotationsForAnalysis(std::string_view,
            spDXShader&,std::uint32_t& componentFlags) const;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
    };
}
