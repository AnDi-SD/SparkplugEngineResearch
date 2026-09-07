#pragma once
// Exact PC class/TU; inferred header and analytical callback API.
#include "../Sparkplug/spParser.h"
#include "spPCEffectTemplate.h"
namespace sparkplug::reconstruction
{
    class spPCRFXFileLoader final : public spParser
    {
    public:
        static constexpr spClassID ClassID=0x01A95832;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        void BindForAnalysis(spPCEffectTemplate& target) noexcept{target_=&target;}
        using HexScanForAnalysis=std::function<int(std::string_view,std::uint32_t&)>;
        // Original4D56D0 uses regex metadata before XML; library parser syntax
        // is reproduced for the two exact patterns. Scanf is an external CRT
        // contract. EOF(-1) with unwritten ID is rejected by the host guard.
        [[nodiscard]] std::unique_ptr<spPCEffectTemplate> LoadDocumentForAnalysis(
            std::string_view,const HexScanForAnalysis&,spPCEffectTemplate::CompilerStateForAnalysis&,std::string& diagnostic);
        [[nodiscard]] std::unique_ptr<spPCEffectTemplate> LoadFileForAnalysis(
            std::unique_ptr<spStream>,const char*,const HexScanForAnalysis&,spPCEffectTemplate::CompilerStateForAnalysis&,std::string& diagnostic);
        // Native4D4750/4D3A80 and4D6050 consume XML-library callbacks. These
        // methods do not claim a complete XML parser or file-loading wrapper.
        [[nodiscard]] bool ConsumeForAnalysis(const spPCEffectTemplate::XmlEventForAnalysis&);
        [[nodiscard]] const spPCEffectTemplate::PassForAnalysis& CurrentPassForAnalysis() const noexcept{return current_;}
        [[nodiscard]] std::optional<unsigned> ActiveShaderForAnalysis() const noexcept{return activeShader_;}
    private:
        spPCEffectTemplate* target_=nullptr;
        spPCEffectTemplate::PassForAnalysis current_;
        std::optional<unsigned> activeShader_;
    };
}
