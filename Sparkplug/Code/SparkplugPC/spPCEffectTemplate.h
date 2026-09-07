#pragma once
// Exact PC class/TU; inferred header and analytical API.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>
#include <functional>

namespace sparkplug::reconstruction
{
    class spDXShader;
    class spPCEffectTemplate : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x30E058FF;
        struct ParameterForAnalysis {std::string name;std::uint32_t startRegister=0,registerCount=1;};
        struct ShaderForAnalysis
        {
            std::array<std::string,4> text; // code, declarations, entry, target
            std::array<std::uint8_t,3> flags{}; // minor, major, assembly
            std::vector<ParameterForAnalysis> parameters;
            void ResetForAnalysis();
        };
        struct PassForAnalysis {std::array<ShaderForAnalysis,2> shaders;};
        struct VariableForAnalysis
        {
            std::string name,displayName;
            bool artistEditable=false;
            std::uint32_t kind=0;
            // Raw payload bits are exposed only for the recovered fields.
            // nullopt preserves the native constructor's untouched words.
            std::vector<std::optional<std::uint32_t>> payloadWords;
            std::vector<std::string> payloadText;
        };
        struct XmlEventForAnalysis
        {
            bool start=true;
            std::string qualifiedName;
            std::map<std::string,std::string> attributes;
        };
        using XmlEventsForAnalysis=std::vector<XmlEventForAnalysis>;
        struct RawFlagsForAnalysis {std::uint32_t value=0,knownMask=0;};
        struct CompilerRequestForAnalysis
        {
            bool assembly=false;
            std::string source,entry,target;
            std::uint32_t flags=0;
        };
        struct CompilerOutputForAnalysis
        {
            std::int32_t hresult=0; // Original tests returned code pointer, not HRESULT.
            std::optional<std::vector<std::uint8_t>> bytecode=std::vector<std::uint8_t>{};
            std::optional<std::vector<ParameterForAnalysis>> reflection=std::vector<ParameterForAnalysis>{};
        };
        struct CompilerStateForAnalysis {bool active=false;}; // Original global byte73FE6B.
        using CompilerForAnalysis=std::function<CompilerOutputForAnalysis(const CompilerRequestForAnalysis&)>;
        [[nodiscard]] static std::string BuildShaderSourceForAnalysis(const ShaderForAnalysis&,std::string_view insertion,std::string_view header);
        [[nodiscard]] static bool CompileShaderForAnalysis(const ShaderForAnalysis&,std::string_view insertion,std::string_view header,
            spDXShader&,const CompilerForAnalysis&,CompilerStateForAnalysis&);
        spPCEffectTemplate(std::uint32_t identity,std::string document,std::uint32_t kind);
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        // Original4D0650 type1 executes a real success stub; all non2 kinds
        // finish. Type2 needs decoded XML callback input from an external
        // parser. False means this analytical slice could not complete.
        [[nodiscard]] bool InitializeFromEventsForAnalysis(const XmlEventsForAnalysis* events=nullptr);
        [[nodiscard]] std::uint32_t IdentityForAnalysis() const noexcept{return identity_;}
        [[nodiscard]] std::uint32_t KindForAnalysis() const noexcept{return kind_;}
        [[nodiscard]] const std::string& DocumentForAnalysis() const noexcept{return document_;}
        [[nodiscard]] bool ReadyForAnalysis() const noexcept{return ready_;}
        [[nodiscard]] const std::vector<PassForAnalysis>& PassesForAnalysis() const noexcept{return passes_;}
        [[nodiscard]] const std::vector<VariableForAnalysis>& VariablesForAnalysis() const noexcept{return variables_;}
        void AppendPassForAnalysis(const PassForAnalysis& pass){passes_.push_back(pass);}
        void AppendVariableForAnalysis(VariableForAnalysis variable){variables_.push_back(std::move(variable));}
        void SetOwnedField14ForAnalysis(std::vector<std::uint8_t> value){field14_=std::move(value);}
        void SetNameForAnalysis(std::string value){name_=std::move(value);}
        [[nodiscard]] const std::string& NameForAnalysis() const noexcept{return name_;}
        void SetField40ForAnalysis(std::uint32_t value) noexcept{field40_={value,0xffffffffu};}
        void OrField40ForAnalysis(std::uint32_t bits) noexcept{field40_.value|=bits;field40_.knownMask|=bits;}
        [[nodiscard]] RawFlagsForAnalysis Field40ForAnalysis() const noexcept{return field40_;}
    private:
        std::uint32_t identity_,kind_;
        std::string document_,name_;
        std::vector<std::uint8_t> field14_;
        std::vector<PassForAnalysis> passes_;
        std::vector<VariableForAnalysis> variables_;
        bool ready_=false;
        RawFlagsForAnalysis field40_; // Original constructor leaves this word untouched.
    };
}
