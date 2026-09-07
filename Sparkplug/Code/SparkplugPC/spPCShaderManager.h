#pragma once
// Proven TU: Z:\Sparkplug\Code\SparkplugPC\spPCShaderManager.cpp.
#include "../SparkplugDX/spDXShaderManager.h"
#include "spPCVertexShader.h"
#include "spPCEffectTemplate.h"
#include <array>
namespace sparkplug::reconstruction
{
    // Explicit external compiler/device providers; analytical non-RTTI carrier.
    struct spPCShaderGenerationForAnalysis
    {
        const spPCEffectTemplate::CompilerForAnalysis* compiler=nullptr;
        spPCEffectTemplate::CompilerStateForAnalysis* state=nullptr;
        spPCVertexShader::CreateDeviceShaderForAnalysis create=nullptr;
        spPCVertexShader::ReleaseDeviceShaderForAnalysis release=nullptr;
        void* context=nullptr;
    };
    class spPCShaderManager final : public spDXShaderManager
    {
    public:
        static constexpr spClassID ClassID=0xD5AE63DA;
        using KeyForAnalysis=std::array<std::uint32_t,2>;
        struct KeyLessForAnalysis final {bool operator()(const KeyForAnalysis&,const KeyForAnalysis&) const noexcept;};
        using CacheForAnalysis=std::map<KeyForAnalysis,std::unique_ptr<spPCVertexShader>,KeyLessForAnalysis>;
        struct SelectionForAnalysis final {bool completed=false;spPCVertexShader* shader=nullptr;};
        spPCShaderManager() noexcept=default;
        ~spPCShaderManager() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native4C8980: low mask nibble zero returns NULL BEFORE cache lookup.
        // Nonzero cache miss is incomplete, NOT a successful NULL shader.
        [[nodiscard]] SelectionForAnalysis SelectCachedForAnalysis(const KeyForAnalysis&) const noexcept;
        struct SourceInputsForAnalysis {std::string header,insertion;};
        [[nodiscard]] static SourceInputsForAnalysis BuildSourceInputsForAnalysis(const KeyForAnalysis&);
        // Explicit already-loaded template input for original owned field40.
        // Reload/replace remains separate; ownership transfers only on success.
        [[nodiscard]] bool SetFixedTemplateForAnalysis(std::unique_ptr<spPCEffectTemplate>&);
        [[nodiscard]] SelectionForAnalysis SelectOrCreateForAnalysis(const KeyForAnalysis&,
            const spPCEffectTemplate::CompilerForAnalysis&,spPCEffectTemplate::CompilerStateForAnalysis&,
            spPCVertexShader::CreateDeviceShaderForAnalysis,spPCVertexShader::ReleaseDeviceShaderForAnalysis,void*);
        // Cache-boundary input, not a claim that a supplied shader was compiled.
        // Native map insertion itself has no such validation. Duplicate input
        // is host-rejected rather than inventing an ownership transfer.
        [[nodiscard]] bool CacheShaderForAnalysis(const KeyForAnalysis&,std::unique_ptr<spPCVertexShader>&);
        [[nodiscard]] const CacheForAnalysis& GetCacheForAnalysis() const noexcept{return cache_;}
    private:
        CacheForAnalysis cache_;
        std::unique_ptr<spPCEffectTemplate> fixedTemplate_; // Destroy before cached shaders, as4C9370.
    };
}
