#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/SparkBase/spStream.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    std::string Quote(std::string_view value)
    {
        std::string result="\"";
        for(unsigned char c:value)
        {
            if(c=='"'||c=='\\'){result+='\\';result+=c;}
            else if(c<32){result+="\\u00";result+="0123456789abcdef"[c>>4];result+="0123456789abcdef"[c&15];}
            else result+=c;
        }
        return result+'"';
    }
    std::string Array(const std::vector<std::string>& rows)
    {std::string out="[";for(std::size_t i=0;i<rows.size();++i){if(i)out+=',';out+=rows[i];}return out+']';}
    using Manager=spPCShaderManager;
    struct Buffer final : Manager::StandaloneBufferForAnalysis
    {
        std::string name;std::vector<std::string>& events;
        std::vector<std::uint32_t> code{0xfffe0101,0x0000ffff};std::string text="Fixture warning";
        bool released=false;
        Buffer(std::string label,std::vector<std::string>& target):name(std::move(label)),events(target){}
        std::uint32_t SizeForAnalysis() override
        {Check(!released&&name=="code","only live code Size is queried");events.push_back("[\"Size\","+Quote(name)+"]");return 8;}
        const std::uint8_t* DataForAnalysis() override
        {
            Check(!released,"library data stays live until explicit release");
            events.push_back("[\"Pointer\","+Quote(name)+"]");
            return name=="code"?reinterpret_cast<const std::uint8_t*>(code.data()):reinterpret_cast<const std::uint8_t*>(text.c_str());
        }
        void ReleaseForAnalysis() noexcept override
        {released=true;events.push_back("[\"Release\","+Quote(name)+"]");}
    };
    struct File final : spStream
    {
        std::string data,name;std::vector<std::string>& events;bool fail=false,opened=false;
        File(std::string content,std::string path,std::vector<std::string>& trace,bool reject):
            data(std::move(content)),name(std::move(path)),events(trace),fail(reject){}
        ~File() override{if(opened)(void)Close();}
        bool Open(const char* path) override{return Open(1,path);}
        bool Open(std::uint32_t mode,const char* path) override
        {
            Check(mode==1&&path==name&&!opened,"standalone uses readonly complete path");
            events.push_back("[\"open\","+Quote(name)+",2147483648,3,3,1]");
            opened=!fail;return opened;
        }
        bool Close() override{opened=false;events.push_back("[\"close\"]");return true;}
        bool GetSize(std::uint32_t* out) const override
        {*out=static_cast<std::uint32_t>(data.size());events.push_back("[\"size\","+std::to_string(data.size())+"]");return true;}
        bool ReadData(void* target,std::uint32_t size) override
        {
            Check(size==data.size(),"shared text reader copies complete input once");
            if(size)std::memcpy(target,data.data(),size);
            events.push_back("[\"read\",0,"+std::to_string(size)+","+std::to_string(size)+"]");return size!=0;
        }
        bool Seek(SeekSource,std::int32_t) override{return false;}
        bool GetCurrentPosition(std::uint32_t&) const override{return false;}
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
    };
    struct Context
    {
        bool pixel=false,deviceFailure=false,nullHandle=false;
        std::vector<std::string> events;
    };
    std::int32_t Create(void* opaque,const std::uint32_t* code,std::uintptr_t* output) noexcept
    {
        auto& context=*static_cast<Context*>(opaque);
        if(code[0]!=0xfffe0101||code[1]!=0x0000ffff)return -1;
        *output=context.nullHandle?0:42;
        context.events.push_back("[\"create\","+Quote(context.pixel?"pixel":"vertex")+
            ",\"0101feffffff0000\","+std::to_string(context.deviceFailure)+","+std::to_string(bool(*output))+"]");
        return context.deviceFailure?static_cast<std::int32_t>(0x80004005):0;
    }
    void Release(void* opaque,std::uintptr_t value) noexcept
    {static_cast<Context*>(opaque)->events.push_back("[\"release\","+std::to_string(value==42)+"]");}

    void OwnedErrorQueue()
    {
        spPCErrorManager manager;
        auto* first=manager.CreateErrorForAnalysis(spErrorSeverity::Error,1,"first.cpp",12);
        Check(first&&manager.StoreMessageForAnalysis(*first,"first"),"owned error has persistent message");
        auto* second=manager.CreateErrorForAnalysis(spErrorSeverity::Information,1,"second.cpp",34);
        Check(second&&manager.StoreMessageForAnalysis(*second,"second"),"second owned error stored");
        Check(second->GetSeverityForAnalysis()==spErrorSeverity::Error&&second->GetNextForAnalysis()==first,
            "new error inherits greater head severity and chains newest first");
        Check(manager.GetDataUsedForAnalysis()==93,"both native40byte objects plus message bytes counted");
        std::vector<std::string> printed;
        manager.PrintErrorForAnalysis(*first,true,[&](std::uint32_t color,std::string_view text)
        {Check(color==12,"PC console errors use12");printed.emplace_back(text);});
        Check(printed==std::vector<std::string>{"ERROR: first\n- first.cpp (12)"}&&manager.GetHeadForAnalysis()==second,
            "console prints selected error and retains entire queue");
        Check(!manager.CreateErrorForAnalysis(spErrorSeverity::Fatal,1,nullptr,0)&&manager.GetDataUsedForAnalysis()==93,
            "unsupported immediate fatal dispatch refused before mutation");
        manager.ClearForAnalysis();
        Check(!manager.GetHeadForAnalysis()&&manager.GetDataUsedForAnalysis()==0,"clear unlinks then destroys owned errors");
        auto* reused=manager.CreateErrorForAnalysis(spErrorSeverity::Warning,1,nullptr,0);
        Check(reused&&reused->GetSeverityForAnalysis()==spErrorSeverity::Warning,"cleared queue has no inherited severity");
    }

    std::string Run(const std::string& mode,const bool errorStateOnly=false)
    {
        const bool pixel=mode=="pixel"||mode=="offline-pixel"||mode=="device-hr-failed-null";
        const bool offline=mode=="offline-vertex"||mode=="offline-pixel";
        const bool twice=mode=="assembly-rejected-twice";
        const bool assemblyRejected=mode=="assembly-rejected"||twice,missing=mode=="missing-file",noResults=mode=="missing-code-and-error";
        Check(mode=="vertex"||pixel||offline||mode=="assembler-hr-failed-code"||mode=="warning"||
            mode=="device-hr-failed-output"||mode=="nul-source"||assemblyRejected||missing||noResults,"known standalone case");
        std::string source="// MatDiffuse : register c4 (1 registers total)\n// Component Normal\nmov oPos,v0";
        if(mode=="nul-source")source+=std::string("\0// Component Color",19);
        const std::string filename=pixel?"test.multi.psh":"test.multi.vsh";
        std::vector<std::string> io,sdkEvents,calls,diagnostics,console;
        auto code=std::make_shared<Buffer>("code",sdkEvents),warning=std::make_shared<Buffer>("warning",sdkEvents);
        Context context;context.pixel=pixel;context.deviceFailure=mode=="device-hr-failed-output"||mode=="device-hr-failed-null";
        context.nullHandle=mode=="device-hr-failed-null";
        Manager::StandaloneForAnalysis api;
        spPCEffectTemplate::CompilerStateForAnalysis compilerState;
        std::shared_ptr<spPCErrorManager> errors;
        api.open=[&](std::string_view path){return std::make_unique<File>(source,std::string(path),io,missing);};
        api.assemble=[&](const spPCEffectTemplate::CompilerRequestForAnalysis& request)
        {
            Check(request.assembly&&request.entry.empty()&&request.target.empty()&&request.flags==0,"literal assembler request");
            calls.push_back("[\"assembly\","+Quote(request.source)+",[],0,["+std::to_string(compilerState.active)+"]]");
            Manager::StandaloneAssemblyForAnalysis output;
            output.hresult=mode=="assembler-hr-failed-code"?static_cast<std::int32_t>(0x80004005):0;
            if(!assemblyRejected&&!noResults)output.code=code;
            if(mode=="warning"||assemblyRejected)output.error=warning;
            return output;
        };
        api.filenameStem=[&](std::string_view path)
        {Check(path=="root\\Shaders\\"+filename,"CRT splitter receives fullpath");return std::string("test.multi");};
        api.debugText=[&](std::string_view value){diagnostics.emplace_back(value);};
        api.errorConsole=[&](std::uint32_t color,std::string_view text)
        {console.push_back('['+std::to_string(color)+','+Quote("%s\n")+','+Quote(text)+']');};
        api.deviceShadersEnabled=!offline;api.createVertex=&Create;api.createPixel=&Create;api.release=&Release;api.context=&context;
        auto loaded=Manager::LoadStandaloneFileForAnalysis("root\\",filename,pixel,api,compilerState,errors);
        if(twice)loaded=Manager::LoadStandaloneFileForAnalysis("root\\",filename,pixel,api,compilerState,errors);
        Check(!compilerState.active,"standalone clears active even on assembly rejection");
        if(assemblyRejected||missing||noResults)
        {
            Check(!loaded.shader&&loaded.completed==!noResults,"known failure vs unresolved invalid library output");
            Check(!code->released&&!warning->released,"original failure path does not release SDK interfaces");
            Check(loaded.unreleased.size()==std::size_t(assemblyRejected),"only actually returned unreleased objects retained");
            if(assemblyRejected)
            {
                Check(diagnostics==std::vector<std::string>(twice?2:1,"Fixture warning")&&errors,"SDK diagnostic and concrete error manager used");
                Check(errors->GetDataUsedForAnalysis()==std::size_t(twice?200:100),"native error object and message consume fixed stack");
                Check(!errors->FormatChainForAnalysis(true).empty()&&console.size()==std::size_t(twice?2:1),"console print retains queued error");
            }
            if(missing)Check(errors&&calls.empty(),"file failure skips compiler and reports error");
            if(errorStateOnly)
            {
                std::vector<std::string> chain;
                for(auto* error=errors->GetHeadForAnalysis();error;error=error->GetNextForAnalysis())
                    chain.push_back('['+std::to_string(error->GetCodeForAnalysis())+','+
                        std::to_string(static_cast<std::uint32_t>(error->GetSeverityForAnalysis()))+','+
                        Quote(error->GetSourceFileForAnalysis())+','+std::to_string(error->GetSourceLineForAnalysis())+','+
                        Quote(error->GetMessageForAnalysis())+']');
                return '['+std::to_string(errors->GetDataUsedForAnalysis())+','+Array(chain)+','+Array(console)+']';
            }
            std::vector<std::string> quoted;
            for(const auto& text:diagnostics)quoted.push_back(Quote(text));
            return "[0,"+std::to_string(compilerState.active)+','+Array(calls)+','+Array(sdkEvents)+','+
                Array(quoted)+','+std::to_string(bool(errors))+','+Array(io)+']';
        }
        Check(loaded.completed&&loaded.status==Manager::StandaloneStatusForAnalysis::Loaded&&loaded.shader,"standalone completes with shader");
        Check(!errors&&loaded.unreleased.empty(),"normal loader does not create error manager or retain library results");
        Check(code->released&&warning->released==(mode=="warning"),"error buffer precedes code Release");
        const auto& words=loaded.shader->GetScalarWordsForAnalysis();
        std::ostringstream parameters;parameters<<'[';
        bool first=true;for(const auto& parameter:loaded.shader->GetParametersForAnalysis())
        {if(!first)parameters<<',';first=false;parameters<<'['<<Quote(parameter.name.data())<<','<<parameter.type<<','<<parameter.startRegister<<','<<parameter.registerCount<<']';}parameters<<']';
        const auto handle=pixel?static_cast<spPCPixelShader&>(*loaded.shader).GetDeviceShaderForAnalysis():
            static_cast<spPCVertexShader&>(*loaded.shader).GetDeviceShaderForAnalysis();
        std::ostringstream state;state<<'['<<Quote(reinterpret_cast<const char*>(words.data()))<<','<<words[8]<<','<<parameters.str()
            <<','<<loaded.shader->CompiledCodeForAnalysis().size()<<",0,"<<int(bool(handle))<<']';
        Check(loaded.shader->CompiledCodeForAnalysis().empty(),"standalone never copies code into generated-shader storage");
        loaded.shader.reset();
        return '['+state.str()+",0,"+Array(calls)+','+Array(sdkEvents)+','+Array(context.events)+','+Array(io)+']';
    }
}

int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--error-state")
            std::cout<<Run(std::string(argv[2])=="twice"?"assembly-rejected-twice":"assembly-rejected",true)<<'\n';
        else
        {
            OwnedErrorQueue();
            for(const char* mode:{"vertex","pixel","offline-vertex","offline-pixel","assembler-hr-failed-code","warning",
                "device-hr-failed-output","device-hr-failed-null","nul-source","assembly-rejected","assembly-rejected-twice","missing-file","missing-code-and-error"})Run(mode);
            std::cout<<"PASS "<<checks<<" checks: standalone shader loader\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
