#pragma once
// Original PC class; source/header path inferred. This portable source does
// not create a host D3D device or reproduce the original 32-bit object ABI.
#include "../SparkplugDX/spDXPixelShader.h"

namespace sparkplug::reconstruction
{
    class spPCPixelShader final : public spDXPixelShader
    {
    public:
        static constexpr spClassID ClassID=0x77F8325C;
        static constexpr std::uint32_t NativeObjectSizeForAnalysis=0x54;
        spPCPixelShader() noexcept=default;
        ~spPCPixelShader() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // PC4C9EB0 has one bytecode argument; unlike spPCVertexShader it has
        // no unused second word. Calls COM+1A8 with output at native this50.
        // HRESULT is ignored and no overwritten device handle is released.
        [[nodiscard]] bool CreateFromBytecodeForAnalysis(const std::uint32_t*,
            CreateDeviceShaderForAnalysis,ReleaseDeviceShaderForAnalysis,void*) noexcept override;
        [[nodiscard]] std::uintptr_t GetDeviceShaderForAnalysis() const noexcept{return deviceShader_;}
    private:
        std::uintptr_t deviceShader_=0;
        // Own host lifetime boundary, absent from the native object layout.
        ReleaseDeviceShaderForAnalysis release_=nullptr;
        void* deviceContext_=nullptr;
    };
}
