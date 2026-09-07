#pragma once
// Original PC class/base names; source/header path inferred. Factory/storage
// foundation only: descriptor interpretation and compiler are separate work.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <string_view>
#include <optional>
namespace sparkplug::evidence::pc {struct RendererMatrixStateForAnalysis;}
namespace sparkplug::reconstruction
{
    class spMaterial;
    class spDXShader : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID=0x468A0AC1;
        ~spDXShader() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // Actual PC factory clears14..34, vector3C/40/44 and code48/4C.
        // Portable raw inputs preserve unknown field semantics, NOT native ABI.
        using ScalarWordsForAnalysis=std::array<std::uint32_t,9>;
        [[nodiscard]] const ScalarWordsForAnalysis& GetScalarWordsForAnalysis() const noexcept{return scalarWords_;}
        void SetScalarWordsForAnalysis(const ScalarWordsForAnalysis& words) noexcept{scalarWords_=words;}
        struct ParameterForAnalysis final
        {
            std::array<char,32> name{};
            std::uint32_t type=0,startRegister=0,registerCount=0;
        };
        using RawMatrixForAnalysis=std::array<std::uint32_t,16>;
        struct LightForAnalysis final
        {
            std::uint32_t type=0;
            std::array<float,4> color{},attenuation{}; // C4..D0;144/148/14C/E0
            std::array<float,3> worldDirection{},localDirection{},worldPosition{}; // A4/58/74
            float innerAngle=0,outerAngle=0; // E4/E8, radians; native cos(angle/2)
        };
        struct ConstantInputsForAnalysis final
        {
            // Explicit cached inputs when no renderer carrier is supplied.
            // With a carrier, types1..4 call the shared reconstructed lazy
            // renderer getter; this mutates its cache exactly on dirty input.
            std::array<RawMatrixForAnalysis,3> cachedMatrices{};
            sparkplug::evidence::pc::RendererMatrixStateForAnalysis* rendererMatrices=nullptr;
            std::vector<RawMatrixForAnalysis> blendMatrices;
            std::vector<RawMatrixForAnalysis> uvMatrices;
            RawMatrixForAnalysis viewMatrix{}; // complete CA80, not cached CB00
            const spMaterial* material=nullptr;
            std::uint32_t constantColor=0;
            bool lightListPresent=false;
            std::vector<LightForAnalysis> lights;
            std::optional<LightForAnalysis> ambientLight,directionalLight;
            std::array<float,4> rendererAmbient{}; // C178..C184
        };
        [[nodiscard]] static std::uint32_t LookupParameterTypeForAnalysis(std::string_view) noexcept;
        // Original45A6C0 takes44 bytes by value, resolves case-sensitive name,
        // appends it, then adds count to34 (sum with uint32 wrap, not max end).
        // Refuse absent in-field NUL as a host bound, not original validation.
        [[nodiscard]] bool AppendParameterForAnalysis(ParameterForAnalysis);
        void SetParametersForAnalysis(std::vector<ParameterForAnalysis> parameters){parameters_=std::move(parameters);}
        [[nodiscard]] const std::vector<ParameterForAnalysis>& GetParametersForAnalysis() const noexcept{return parameters_;}
        void SetCompiledCodeForAnalysis(std::vector<std::uint8_t> code){compiledCode_=std::move(code);}
        [[nodiscard]] const std::vector<std::uint8_t>& CompiledCodeForAnalysis() const noexcept{return compiledCode_;}
        // PC4AE930 is void. Portable bool means this slice completed without
        // unknown semantics/OOB, NOT an invented native return value.
        [[nodiscard]] bool BuildConstantsForAnalysis(std::vector<std::uint32_t>& output,const ConstantInputsForAnalysis&) const;
        // Host guard for4BE2B0's uninitialized4096-byte scratch: refuse a
        // submitted word not written by a known descriptor, never invent zero.
        [[nodiscard]] bool BuildFullyWrittenConstantsForAnalysis(std::vector<std::uint32_t>& output,
            const ConstantInputsForAnalysis&,std::uint32_t submittedRows) const;
    protected:
        spDXShader() noexcept=default;
    private:
        ScalarWordsForAnalysis scalarWords_{};
        std::vector<ParameterForAnalysis> parameters_;
        std::vector<std::uint8_t> compiledCode_; // Original48=size,4C=owned bytes.
    };
}
