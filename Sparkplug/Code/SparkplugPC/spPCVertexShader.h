#pragma once
// Original PC class59D92171; path inferred. No shader assembler is invented.
#include "../SparkplugDX/spDXVertexShader.h"
namespace sparkplug::reconstruction
{
    class spPCVertexShader final : public spDXVertexShader
    {
    public:
        static constexpr spClassID ClassID=0x59D92171;
        spPCVertexShader() noexcept=default;
        ~spPCVertexShader() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // PC4CA030: bytecode is already assembled input; arg2 unused. Device
        // HRESULT ignored, callback writes50 directly; no old-handle Release.
        using CreateDeviceShaderForAnalysis=std::int32_t (*)(void*,const std::uint32_t*,std::uintptr_t*) noexcept;
        using ReleaseDeviceShaderForAnalysis=void (*)(void*,std::uintptr_t) noexcept;
        [[nodiscard]] bool CreateFromBytecodeForAnalysis(const std::uint32_t*,std::uint32_t,
            CreateDeviceShaderForAnalysis,ReleaseDeviceShaderForAnalysis,void*) noexcept;
        [[nodiscard]] std::uintptr_t GetDeviceShaderForAnalysis() const noexcept{return deviceShader_;}
    private:
        std::uintptr_t deviceShader_=0;
        ReleaseDeviceShaderForAnalysis release_=nullptr;
        void* deviceContext_=nullptr;
    };
}
