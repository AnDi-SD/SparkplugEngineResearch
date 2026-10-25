#include "spPCShaderManager.h"
#include "../SparkplugDX/spDXAsmShaderParser.h"
#include "../Sparkplug/spParser.h"
#include "../SparkBase/spStream.h"
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        bool ReportStandaloneError(std::shared_ptr<spPCErrorManager>& owner,
            const std::uint32_t line,const std::string& message,
            const spPCErrorManager::ConsoleForAnalysis& console)
        {
            if(!owner)owner=std::make_shared<spPCErrorManager>();
            auto* error=owner->CreateErrorForAnalysis(spErrorSeverity::Warning,static_cast<std::uint32_t>(spErrorCode::Message),
                "Z:\\Sparkplug\\Code\\SparkplugPC\\spPCShaderManager.cpp",line);
            if(!error||!owner->StoreMessageForAnalysis(*error,message.c_str()))return false;
            owner->PrintErrorForAnalysis(*error,true,console);
            return true;
        }
    }

    spPCShaderManager::StandaloneResultForAnalysis spPCShaderManager::LoadStandaloneFileForAnalysis(
        std::string_view root,std::string_view filename,const bool pixel,const StandaloneForAnalysis& input,
        spPCEffectTemplate::CompilerStateForAnalysis& compilerState,std::shared_ptr<spPCErrorManager>& errors)
    {
        using Status=StandaloneStatusForAnalysis;
        root=root.substr(0,root.find('\0'));filename=filename.substr(0,filename.find('\0'));
        if(!input.open||!input.assemble||!input.filenameStem||!input.debugText||
            root.size()+8+filename.size()>=256||
            ((!pixel||input.deviceShadersEnabled)&&(!input.release||(pixel?!input.createPixel:!input.createVertex))))return {};
        const std::string fullpath=std::string(root)+"Shaders\\"+std::string(filename);
        auto text=spParser::ReadFileTextForAnalysis(input.open(fullpath),fullpath.c_str());
        if(!text)
        {
            const bool reported=ReportStandaloneError(errors,245,"Failed to load shader "+fullpath,input.errorConsole);
            return {reported,Status::FileUnavailable,nullptr,{}};
        }
        spPCEffectTemplate::CompilerRequestForAnalysis request;
        request.assembly=true;
        request.source=std::string(reinterpret_cast<const char*>(text->bytes.get()));
        compilerState.active=true;
        auto assembled=input.assemble(request);
        compilerState.active=false; //4C8FC9, INCLUDING null-code result.
        const auto incomplete=[&](Status status)
        {
            StandaloneResultForAnalysis result;result.status=status;
            if(assembled.code)result.unreleased.push_back(assembled.code);
            if(assembled.error)result.unreleased.push_back(assembled.error);
            return result;
        };
        if(!assembled.code)
        {
            // Native dereferences error unconditionally on this branch. Null
            // error is an invalid SDK contract, not an invented empty message.
            if(!assembled.error)return incomplete(Status::InvalidLibraryContract);
            const auto* diagnostic=assembled.error->DataForAnalysis();
            if(!diagnostic)return incomplete(Status::InvalidLibraryContract);
            input.debugText(reinterpret_cast<const char*>(diagnostic));
            const bool reported=ReportStandaloneError(errors,191,
                "An error occured while compiling the assembled shader code.",input.errorConsole);
            auto result=incomplete(Status::AssemblyRejected);result.completed=reported;
            return result; // Native omits error-buffer Release here.
        }
        const auto codeSize=assembled.code->SizeForAnalysis(); // Queried, native value unused.
        const auto name=input.filenameStem(fullpath);
        if(name.size()>=32)return incomplete(Status::InvalidInput); // Native unbounded field14 strcpy.
        std::unique_ptr<spDXShader> shader;
        if(pixel)shader=std::make_unique<spPCPixelShader>();
        else shader=std::make_unique<spPCVertexShader>();
        auto words=shader->GetScalarWordsForAnalysis();
        std::memcpy(words.data(),name.c_str(),name.size()+1);
        shader->SetScalarWordsForAnalysis(words);
        spDXAsmShaderParser parser;std::uint32_t components=0;
        if(!parser.ParseAnnotationsForAnalysis(request.source,*shader,components))
            return incomplete(Status::UnsupportedAnnotation);
        // Actual4CF950 returns1 throughout its safe annotation domain. The
        // unreachable native false arm is not substituted by a host parse
        // guard failure. Pixel and vertex concrete Create bodies ignore HRESULT.
        if(!pixel||input.deviceShadersEnabled)
        {
            if(codeSize<4||codeSize>0x100000||codeSize%4)return incomplete(Status::InvalidLibraryContract);
            const auto* data=assembled.code->DataForAnalysis();
            if(!data||reinterpret_cast<std::uintptr_t>(data)%alignof(std::uint32_t))
                return incomplete(Status::InvalidLibraryContract);
            const auto* code=reinterpret_cast<const std::uint32_t*>(data);
            if(pixel)(void)static_cast<spPCPixelShader&>(*shader).CreateFromBytecodeForAnalysis(
                code,input.createPixel,input.release,input.context);
            else (void)static_cast<spPCVertexShader&>(*shader).CreateFromBytecodeForAnalysis(
                code,components,input.createVertex,input.release,input.context);
        }
        if(assembled.error)assembled.error->ReleaseForAnalysis();
        assembled.code->ReleaseForAnalysis();
        return {true,Status::Loaded,std::move(shader),{}};
    }
}
