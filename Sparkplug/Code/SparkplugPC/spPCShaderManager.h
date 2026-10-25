#pragma once
// Proven TU: Z:\Sparkplug\Code\SparkplugPC\spPCShaderManager.cpp.
#include "../SparkplugDX/spDXShaderManager.h"
#include "spPCVertexShader.h"
#include "spPCEffectTemplate.h"
#include "spPCShaderSource.h"
#include "spPCRFXFileLoader.h"
#include "spPCPixelShader.h"
#include "spPCErrorManager.h"
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
        using SourceInputsForAnalysis=PCShaderSourceInputsForAnalysis;
        [[nodiscard]] static SourceInputsForAnalysis BuildSourceInputsForAnalysis(const KeyForAnalysis&);
        // Explicit already-loaded template input for original owned field40.
        // Reload/replace remains separate; ownership transfers only on success.
        [[nodiscard]] bool SetFixedTemplateForAnalysis(std::unique_ptr<spPCEffectTemplate>&);
        struct StartupForAnalysis
        {
            DirectoryForAnalysis directory;
            LoadStandaloneForAnalysis loadStandalone;
            // Explicit file-stream and XML-library boundaries. Decoded events
            // are consumed by the real recovered template/RFX callbacks.
            std::function<std::unique_ptr<spStream>(std::string_view)> open;
            spPCRFXFileLoader::HexScanForAnalysis scanHex;
            std::function<std::optional<spPCEffectTemplate::XmlEventsForAnalysis>(std::string_view)> decodeXml;
        };
        struct StartupResultForAnalysis {bool completed=false,nativeReturn=false;};
        // External D3DX buffer interface. Wrapper destruction is not Release:
        // original assembly-error paths retain returned library interfaces.
        // Data must stay valid until explicit Release; error data is NUL text.
        struct StandaloneBufferForAnalysis
        {
            virtual ~StandaloneBufferForAnalysis()=default;
            [[nodiscard]] virtual std::uint32_t SizeForAnalysis()=0;
            [[nodiscard]] virtual const std::uint8_t* DataForAnalysis()=0;
            virtual void ReleaseForAnalysis() noexcept=0;
        };
        struct StandaloneAssemblyForAnalysis
        {
            std::int32_t hresult=0; // Native tests code pointer, not HRESULT.
            std::shared_ptr<StandaloneBufferForAnalysis> code,error;
        };
        struct StandaloneForAnalysis
        {
            std::function<std::unique_ptr<spStream>(std::string_view)> open;
            std::function<StandaloneAssemblyForAnalysis(const spPCEffectTemplate::CompilerRequestForAnalysis&)> assemble;
            // Explicit CRT _splitpath(fullpath,null,null,fname,null) boundary.
            std::function<std::string(std::string_view)> filenameStem;
            std::function<void(std::string_view)> debugText;
            spPCErrorManager::ConsoleForAnalysis errorConsole;
            bool deviceShadersEnabled=false; // F2F5 gates PIXEL creation only.
            spPCVertexShader::CreateDeviceShaderForAnalysis createVertex=nullptr;
            spPCPixelShader::CreateDeviceShaderForAnalysis createPixel=nullptr;
            spPCVertexShader::ReleaseDeviceShaderForAnalysis release=nullptr;
            void* context=nullptr;
        };
        enum class StandaloneStatusForAnalysis
        {Loaded,FileUnavailable,AssemblyRejected,InvalidInput,InvalidLibraryContract,UnsupportedAnnotation};
        struct StandaloneResultForAnalysis
        {
            bool completed=false;
            StandaloneStatusForAnalysis status=StandaloneStatusForAnalysis::InvalidInput;
            std::unique_ptr<spDXShader> shader;
            // Original did not release these on the represented failure path.
            // Retain explicit wrappers instead of silently fixing ownership.
            std::vector<std::shared_ptr<StandaloneBufferForAnalysis>> unreleased;
        };
        // Whole PC4C8F10: shared4D0A30 read -> assemble -> Size -> name ->
        // real4CF950 annotations -> optional device Create -> error/code Release.
        // No bytecode copy to shader48/4C occurs here, unlike generated shaders.
        // OS/CRT/SDK/COM remain external; safe source/file failure RAII follows
        // the already documented shared ReadFileTextForAnalysis contract.
        [[nodiscard]] static StandaloneResultForAnalysis LoadStandaloneFileForAnalysis(
            std::string_view root,std::string_view filename,bool pixel,const StandaloneForAnalysis&,
            spPCEffectTemplate::CompilerStateForAnalysis&,std::shared_ptr<spPCErrorManager>&);
        // Native4C97E0: base VS/PS scan -> load Fixed.rfx -> publish field40
        // -> initialize -> loader cleanup. Initialization AL is ignored.
        // This interface supports a fresh manager only. Original repeated
        // startup overwrites field40 without freeing its old value; refuse
        // that unsafe lifetime before any mutation instead of releasing it.
        [[nodiscard]] StartupResultForAnalysis StartForAnalysis(std::string_view root,
            const StartupForAnalysis&,spPCEffectTemplate::CompilerStateForAnalysis&,std::string& diagnostic);
        [[nodiscard]] const spPCEffectTemplate* FixedTemplateForAnalysis() const noexcept{return fixedTemplate_.get();}
        [[nodiscard]] bool HasStartupStateForAnalysis() const noexcept
        {return startupAttempted_||StandaloneScanAttemptedForAnalysis()||fixedTemplate_||!cache_.empty();}
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
        bool startupAttempted_=false;
    };
}
