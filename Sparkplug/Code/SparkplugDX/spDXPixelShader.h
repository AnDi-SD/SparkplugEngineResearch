#pragma once
// Original PC class identity. Header/TU path and method names are inferred;
// the portable interface below is analytical, not the original x86 ABI.
#include "spDXShader.h"

namespace sparkplug::reconstruction
{
    class spDXPixelShader : public spDXShader
    {
    public:
        static constexpr spClassID ClassID=0x2B5D52DF;
        ~spDXPixelShader() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Original primary slot1C is pure virtual. Already-assembled caller
        // bytecode enters here; compilation belongs to spPCEffectTemplate.
        // Extra callback/context arguments are our external-device boundary.
        using CreateDeviceShaderForAnalysis=std::int32_t (*)(void*,const std::uint32_t*,std::uintptr_t*) noexcept;
        using ReleaseDeviceShaderForAnalysis=void (*)(void*,std::uintptr_t) noexcept;
        [[nodiscard]] virtual bool CreateFromBytecodeForAnalysis(const std::uint32_t*,
            CreateDeviceShaderForAnalysis,ReleaseDeviceShaderForAnalysis,void*) noexcept=0;
    protected:
        spDXPixelShader() noexcept=default;
    };
}
