#pragma once
// Original PC class identity; source/header path and member names inferred.
// CP29: factory/lifetime/copy/storage slice, NOT working shader codec/backend.
#include "../Sparkplug/spMaterialTextureLayer.h"
#include <array>
#include <optional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spDXShaderLayer final : public spMaterialTextureLayer
    {
    public:
        static constexpr spClassID ClassID=0x71643E66;
        using RawFloat4ForAnalysis=std::array<std::uint32_t,4>;
        struct ParameterPairForAnalysis
        {
            std::vector<RawFloat4ForAnalysis> first,second;
            bool operator==(const ParameterPairForAnalysis& other) const
            {return first==other.first&&second==other.second;}
        };
        spDXShaderLayer() noexcept=default;
        ~spDXShaderLayer() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;

        // Native+14 is an uninitialized borrowed word in the actual factory.
        // Optional unset preserves "not established", not a fabricated NULL
        // shader default. Token copying never loads/retains/releases a shader.
        void SetShaderWordForAnalysis(std::uint32_t value) noexcept{shaderWord_=value;}
        [[nodiscard]] std::optional<std::uint32_t> GetShaderWordForAnalysis() const noexcept{return shaderWord_;}
        void AppendParameterPairForAnalysis(ParameterPairForAnalysis value);
        void ClearParameterPairsForAnalysis() noexcept;
        [[nodiscard]] const std::vector<ParameterPairForAnalysis>& GetParameterPairsForAnalysis() const noexcept{return parameters_;}
        // Original PC vtable+20/+24 and reader helper5A7DB0 are success stubs.
        [[nodiscard]] bool Slot8ForAnalysis(std::uint32_t,std::uint32_t) const noexcept{return true;}
        [[nodiscard]] bool Slot9ForAnalysis() const noexcept{return true;}
        [[nodiscard]] bool ReaderScalarStubForAnalysis(std::uint32_t) const noexcept{return true;}
    private:
        std::optional<std::uint32_t> shaderWord_;
        std::vector<ParameterPairForAnalysis> parameters_;
    };
}
