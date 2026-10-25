#pragma once
// Original PC98BA76FE, abstract registration, base415352A1; path inferred.
#include "../SparkBase/spBaseObject.h"
#include "spDXShader.h"
#include <functional>
#include <string_view>
namespace sparkplug::reconstruction
{
    class spDXShaderManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x98BA76FE;
        ~spDXShaderManager() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        struct DirectoryForAnalysis
        {
            // External CRT _findfirst/_findnext/_findclose. First supplies
            // the first basename and -1 means no entries. Next returns false
            // at the native -1/end boundary. Close is called even for -1.
            std::function<std::intptr_t(std::string_view,std::string&)> first;
            std::function<bool(std::intptr_t,std::string&)> next;
            std::function<void(std::intptr_t)> close;
        };
        // Native virtual +24 is the per-file loader, not the OS enumerator.
        // Its implementation/provider must own its compiler/device contract.
        using LoadStandaloneForAnalysis=std::function<std::unique_ptr<spDXShader>(std::string_view,bool)>;
        [[nodiscard]] bool ScanStandaloneForAnalysis(std::string_view root,
            const DirectoryForAnalysis&,const LoadStandaloneForAnalysis&);
        [[nodiscard]] const std::vector<std::unique_ptr<spDXShader>>& VertexShadersForAnalysis() const noexcept{return vertexShaders_;}
        [[nodiscard]] const std::vector<std::unique_ptr<spDXShader>>& PixelShadersForAnalysis() const noexcept{return pixelShaders_;}
    protected:
        spDXShaderManager() noexcept=default;
        [[nodiscard]] bool StandaloneScanAttemptedForAnalysis() const noexcept{return scanAttempted_;}
    private:
        std::vector<std::unique_ptr<spDXShader>> vertexShaders_,pixelShaders_;
        bool scanAttempted_=false; // Host guard; native repeated scans append.
        // The base name map remains unexposed.
    };
}
