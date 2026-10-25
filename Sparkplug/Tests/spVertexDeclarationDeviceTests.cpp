#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include <array>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    using API=spPCVertexDeclarationDeviceForAnalysis;
    struct Fixture
    {
        std::string mode;
        unsigned creates=0,nextHandle=0;
        std::map<std::uintptr_t,unsigned> refs;
        std::vector<std::string> events;
        std::vector<std::array<std::uint32_t,4>> snapshots;
        spPCVertexDeclaration* current=nullptr; // direct reinit observation only
        static std::int32_t Create(void* context,std::uintptr_t device,
            const spPCVertexElementForAnalysis* elements,std::size_t count,spPCVertexDeclarationOutputForAnalysis& output) noexcept
        {
            auto& f=*static_cast<Fixture*>(context);++f.creates;
            if(device!=0x700||!elements||!count)std::terminate();
            std::ostringstream hex;hex<<std::hex<<std::setfill('0');
            const auto* bytes=reinterpret_cast<const unsigned char*>(elements);
            for(std::size_t i=0;i<count*8;++i)hex<<std::setw(2)<<unsigned(bytes[i]);
            f.events.push_back("[\"create-declaration\",\""+hex.str()+"\"]");
            // Cache-created objects are private until lookup returns. The leaf
            // observes the direct out slot and element span; it does not infer
            // a C++ this pointer from the handle's address.
            f.snapshots.push_back({f.current?f.current->GetFVFCodeForAnalysis():0xffffffffu,
                std::uint32_t(output.value),std::uint32_t(count*8),f.current?std::uint32_t(f.current->GetDeviceHandleForAnalysis()):0xffffffffu});
            if(f.mode=="unwritten-cold"||(f.mode=="unwritten-warm"&&f.creates==2))return std::int32_t(0x80004005u);
            if(f.mode=="failed-null"||(f.mode=="reinitialize-null"&&f.creates==2))
            {output.value=0;output.initialized=true;return std::int32_t(0x80004005u);}
            output.value=++f.nextHandle;output.initialized=true;f.refs[output.value]=f.mode=="release-nonzero"?4:1;
            return f.mode=="failed-valid"?std::int32_t(0x80004005u):0;
        }
        static std::int32_t Bind(void* context,std::uintptr_t device,std::uintptr_t handle) noexcept
        {
            auto& f=*static_cast<Fixture*>(context);
            if(device!=0x700||(handle&&!f.refs[handle]))std::terminate();
            f.events.push_back("[\"set-declaration\","+std::to_string(handle)+"]");return std::int32_t(0x80004005u);
        }
        static std::uint32_t Release(void* context,std::uintptr_t handle) noexcept
        {
            auto& f=*static_cast<Fixture*>(context);if(!f.refs[handle])std::terminate();
            const auto left=--f.refs[handle];f.events.push_back("[\"release-declaration\","+
                std::to_string(handle)+","+std::to_string(left)+"]");return left;
        }
        API Api(){return {this,&Create,&Bind,&Release};}
    };
    const std::array<const char*,10> Modes{{"normal","direct","failed-null","failed-valid","unwritten-cold",
        "unwritten-warm","reinitialize","reinitialize-null","release-nonzero","stored-null"}};
    void Case(const std::string& mode,bool capture)
    {
        Fixture f;f.mode=mode;auto api=f.Api();spPCRenderer renderer;
        const bool unsafe=mode=="unwritten-cold"||mode=="unwritten-warm";
        std::vector<std::shared_ptr<spPCVertexDeclaration>> owners;
        std::vector<std::array<std::uintptr_t,3>> steps;
        if(mode=="stored-null")
        {
            Check(renderer.InsertCapturedVertexDeclarationForAnalysis(0x840,nullptr),"captured null map entry");
            const auto result=renderer.GetDeviceVertexDeclarationForAnalysis(0x840,0,{});
            Check(result.completed&&!result.declaration&&f.creates==0,"stored-null hit bypasses COM/factory");
            steps.push_back({0,0,renderer.GetVertexDeclarationCountForAnalysis()});
        }
        else
        {
            std::shared_ptr<spPCVertexDeclaration> object;
            if(mode=="direct")
            {
                object=std::make_shared<spPCVertexDeclaration>();f.current=object.get();
                const auto result=object->InitializeDeviceForAnalysis(0x840,0x700,api);
                Check(result.completed&&result.nativeReturn,"direct cold Init");
            }
            else
            {
                auto lookup=renderer.GetDeviceVertexDeclarationForAnalysis(0x840,0x700,api);
                Check(lookup.declaration&&lookup.completed==(mode!="unwritten-cold"),"device miss or explicit unsafe prefix");object=lookup.declaration;
            }
            owners.push_back(object);
            steps.push_back({object->GetFVFCodeForAnalysis(),object->GetDeviceHandleForAnalysis(),renderer.GetVertexDeclarationCountForAnalysis()});
            if(mode!="direct")
            {
                const auto calls=f.creates;const auto hit=renderer.GetDeviceVertexDeclarationForAnalysis(0x840,0,{});
                Check(hit.completed&&hit.declaration==object&&calls==f.creates,"cache hit bypasses endpoints");
            }
            if(mode=="normal")
            {
                auto second=renderer.GetDeviceVertexDeclarationForAnalysis(0x940,0x700,api);
                Check(second.completed&&second.declaration&&second.declaration!=object,"distinct component key");
                owners.push_back(second.declaration);
                steps.push_back({second.declaration->GetFVFCodeForAnalysis(),second.declaration->GetDeviceHandleForAnalysis(),renderer.GetVertexDeclarationCountForAnalysis()});
            }
            else if(mode=="reinitialize"||mode=="reinitialize-null"||mode=="unwritten-warm")
            {
                f.current=object.get();const auto reinit=object->InitializeDeviceForAnalysis(0x940,0x700,api);
                Check(reinit.completed==!unsafe&&reinit.nativeReturn,"reinit HRESULT and unsafe temporary output");
                Check(renderer.GetDeviceVertexDeclarationForAnalysis(0x840,0,{}).declaration==object,"reinit preserves original cache key");
                steps.push_back({object->GetFVFCodeForAnalysis(),object->GetDeviceHandleForAnalysis(),renderer.GetVertexDeclarationCountForAnalysis()});
            }
            const auto bound=object->BindDeviceForAnalysis(0x700,api,0xdeadbeef);
            if(unsafe)Check(!bound.completed&&!object->HasInitializedDeviceHandleForAnalysis(),"unsafe raw output is not submitted");
            else Check(bound.completed&&bound.nativeReturn,"bind ignores argument/HRESULT");
        }
        Check(renderer.ReleaseDeviceVertexDeclarationsForAnalysis()==!unsafe,"cache release or unsafe refusal");
        if(mode=="direct")for(const auto& owner:owners)Check(owner->ReleaseDeviceForAnalysis(),"direct owner release");
        if(!unsafe)for(const auto& owner:owners)Check(owner->GetDeviceHandleForAnalysis()==0,"Release clears field regardless of count");
        if(capture)
        {
            std::cout<<"{\"completePipeline\":"<<(unsafe?"false":"true")<<",\"steps\":[";bool comma=false;
            for(const auto& row:steps){if(comma)std::cout<<',';comma=true;std::cout<<'['<<row[0]<<','<<row[1]<<','<<row[2]<<']';}
            std::cout<<"],\"creates\":"<<f.creates<<",\"createSnapshots\":[";comma=false;
            for(const auto& row:f.snapshots){if(comma)std::cout<<',';comma=true;std::cout<<'['<<row[0]<<','<<row[1]<<','<<row[2]<<','<<row[3]<<']';}
            std::cout<<"],\"events\":[";comma=false;for(const auto& event:f.events){if(comma)std::cout<<',';comma=true;std::cout<<event;}
            std::cout<<"],\"remainingCOM\":[";comma=false;for(const auto& entry:f.refs){if(comma)std::cout<<',';comma=true;std::cout<<'['<<entry.first<<','<<entry.second<<']';}
            std::cout<<"],\"objectHandlesAfterOwnerRelease\":[";comma=false;if(!unsafe)for(const auto& owner:owners){if(comma)std::cout<<',';comma=true;std::cout<<owner->GetDeviceHandleForAnalysis();}
            std::cout<<"]}\n";
        }
        for(auto& entry:f.refs)while(entry.second)Fixture::Release(&f,entry.first); // explicit fixture cleanup
    }
    void Guards()
    {
        Fixture f;auto api=f.Api();spPCVertexDeclaration object;
        auto absent=api;absent.release=nullptr;
        Check(!object.InitializeDeviceForAnalysis(0x840,0x700,absent).completed&&object.GetFVFCodeForAnalysis()==0&&f.creates==0,"missing owner before mutation");
        Check(object.InitializeDeviceForAnalysis(0x840,0x700,api).completed,"owned device init");
        Check(!object.InitializeForAnalysis(0x940)&&object.GetFVFCodeForAnalysis()==0x112,"CPU mode guard preserves live COM layout");
        auto different=api;different.context=nullptr;
        Check(!object.InitializeDeviceForAnalysis(0x940,0x700,different).completed&&object.GetFVFCodeForAnalysis()==0x112&&f.creates==1,"owner coherence before mutation");
        Check(!object.BindDeviceForAnalysis(0x700,{}).completed,"missing bind is incomplete");
        Check(object.ReleaseDeviceForAnalysis()&&object.ReleaseDeviceForAnalysis()&&f.refs[1]==0,"explicit release is idempotent after clear");
        Check(!object.InitializeForAnalysis(0x940)&&object.GetFVFCodeForAnalysis()==0x112,"released device state remains separate from CPU facade");
        spPCRenderer mixed;
        const auto cpu=mixed.GetVertexDeclarationForAnalysis(0x840);
        Check(cpu&&!cpu->HasDeviceInitializationForAnalysis(),"CPU facade has no device provenance");
        const auto calls=f.creates;const auto refused=mixed.GetDeviceVertexDeclarationForAnalysis(0x840,0x700,api);
        Check(!refused.completed&&refused.declaration==cpu&&f.creates==calls,"CPU-only hit cannot claim device initialization");
        f.mode="failed-null";spPCVertexDeclaration knownNull;
        Check(knownNull.InitializeDeviceForAnalysis(0x840,0x700,api).completed&&knownNull.HasDeviceInitializationForAnalysis(),"written null has device provenance");
        Check(!knownNull.InitializeForAnalysis(0x940)&&knownNull.GetFVFCodeForAnalysis()==0x112,"known-null device state refuses CPU mutation");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--declaration-case")
        {bool found=false;for(const auto* mode:Modes)found|=mode==std::string(argv[2]);Check(found,"known case");Case(argv[2],true);return 0;}
        for(const auto* mode:Modes)Case(mode,false);Guards();
        std::cout<<"PASS "<<checks<<'/'<<checks<<": declaration device/cache lifecycle\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
