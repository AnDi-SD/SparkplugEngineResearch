#include "Code/SparkplugPC/spPCProjectionFX.h"
#include "Code/Sparkplug/spProjectionManager.h"
#include "Analysis/PC/spProjectionFxAbi.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <array>
#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* why) { ++checks; if(!value) throw std::runtime_error(why); }
    struct Host final : spProjectionFxHost
    {
        const void* owner=nullptr;
        const void* material=nullptr;
        mutable unsigned reads=0;
        const void* ReadProjectionMaterial20ForAnalysis(const void* value) const noexcept override
        { if(value!=owner) std::terminate(); ++reads; return material; }
    };
    struct Delete final
    {
        bool* deleted;
        void operator()(spPCProjectionFX* value) const noexcept { delete value; *deleted=true; }
    };
    std::string State(const spPCProjectionFX& fx,const void* owner)
    {
        std::ostringstream out;
        out << std::boolalpha << "{\"nameNull\":" << (fx.GetName()==nullptr)
            << ",\"owner\":" << ((owner && fx.BorrowedOwnerForAnalysis()==owner)?1:0)
            << ",\"initialized\":" << unsigned(fx.InitializedByteForAnalysis())
            << ",\"cacheNull\":" << (fx.Cache20ForAnalysis()==nullptr) << '}';
        return out.str();
    }
    void Replay(const std::string& mode)
    {
        // Logical foreign owner data. This is not a source BoxProjection
        // constructor/destructor or a claim to implement its resource slot94.
        unsigned ownerData=0;
        Host host; host.owner=&ownerData;
        bool deleted=false;
        std::unique_ptr<spPCProjectionFX,Delete> fx(new spPCProjectionFX,Delete{&deleted});
        Check(fx->IsExactly(spPCProjectionFX::ClassID) && fx->IsKindOf(spProjectionFX::ClassID)
            && fx->IsKindOf(spCrossPlatform::ClassID),"Actual derived FX source identity");
        const bool owned=mode=="owned"||mode=="reuse-owner-null";
        if(owned)
        {
            fx->BindOwnerForAnalysis(&ownerData);
            Check(fx->InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::Completed,"No-material FX Init");
        }
        const auto initial=State(*fx,&ownerData);
        std::string cloned="null";
        if(mode=="ctor-clone")
        {
            spCloneManager manager;
            auto object=fx->vfunc_10(manager);
            auto* clone=dynamic_cast<spPCProjectionFX*>(object.get());
            Check(clone && clone!=fx.get(),"Actual shared clone constructor and map registration");
            cloned=State(*clone,&ownerData);
        }
        else if(mode=="flagged")
        {
            fx->SetInitializedByteForFixture(0xA5);
            Check(fx->InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::Completed && host.reads==0,
                "Nonzero initialized byte bypasses owner/data reads");
        }
        else
        {
            Check(owned,"Known replay operation");
            fx->BindOwnerForAnalysis(&ownerData);
            Check(fx->InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::Completed && host.reads==1,
                "Existing FX reuses initialized state without data reads");
            if(mode=="reuse-owner-null")
            {
                fx->BindOwnerForAnalysis(nullptr);
                Check(fx->InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::Completed && host.reads==1,
                    "Nonzero state precedes even missing owner");
            }
        }
        const auto after=State(*fx,&ownerData);
        fx.reset(); Check(deleted,"Actual portable FX destructor called through owned storage");
        std::cout << "{\"mode\":\"" << mode << "\",\"initial\":" << initial
            << ",\"after\":" << after << ",\"clone\":" << cloned << ",\"fxDeleted\":true}";
    }
    void SetupReplay(const std::string& mode)
    {
        // Explicit portable field storage and borrowed projection identities.
        // Native BoxProjection factory/destructor observations are separate.
        unsigned a=0,b=0;
        Host host;host.owner=&a;
        std::unique_ptr<spPCProjectionFX> helper;
        const bool warm=mode=="warm-zero"||mode=="warm-raw-byte";
        if(warm)helper=std::make_unique<spPCProjectionFX>();
        if(mode=="warm-raw-byte")
        {helper->SetInitializedByteForFixture(0xA5);host.material=&b;}
        if(mode=="reuse"||mode=="move-owner")
        {
            Check(InitializeProjectionPCWithOwnedFxForAnalysis(&a,helper,host)==spProjectionFxInitializationResultForAnalysis::Completed,
                "Actual owned-field cold initialization");
            if(mode=="reuse")helper->BindOwnerForAnalysis(nullptr);
            else host.owner=&b; // fixture transfer of the same owned field
        }
        if(mode=="material-prefix")host.material=&b;
        Check(warm||mode=="cold"||mode=="reuse"||mode=="move-owner"||mode=="material-prefix","Known setup mode");
        auto state=[&]() -> std::string
        {
            if(!helper)return "null";
            const auto* owner=helper->BorrowedOwnerForAnalysis();
            std::ostringstream out;out<<std::boolalpha<<"{\"classID\":"<<helper->vfunc_18().classID
                <<",\"owner\":"<<(owner==&a?1:owner==&b?2:0)
                <<",\"initialized\":"<<unsigned(helper->InitializedByteForAnalysis())
                <<",\"cacheNull\":"<<(helper->Cache20ForAnalysis()==nullptr)<<'}';return out.str();
        };
        const auto before=state();auto* prior=helper.get();const auto reads=host.reads;
        const auto result=InitializeProjectionPCWithOwnedFxForAnalysis(const_cast<void*>(host.owner),helper,host);
        const bool completed=result==spProjectionFxInitializationResultForAnalysis::Completed;
        Check(helper && (!prior||helper.get()==prior),"Publish new helper or reuse exact existing field");
        if(mode=="warm-raw-byte")Check(host.reads==reads,"Initialized raw byte prevents material reads");
        if(mode=="material-prefix")
            Check(result==spProjectionFxInitializationResultForAnalysis::UnqualifiedMaterialInitialization
                && helper->BorrowedOwnerForAnalysis()==&a && helper->InitializedByteForAnalysis()==0,
                "Refusal retains newly published helper and owner prefix");
        else Check(completed,"Admitted whole FX initialization returns native success");
        Check(a==0&&b==0,"Borrowed projection/material data is unchanged");
        std::cout<<std::boolalpha<<"{\"mode\":\""<<mode<<"\",\"before\":"<<before<<",\"after\":"<<state()
            <<",\"created\":"<<(prior==nullptr)<<",\"completed\":"<<completed<<",\"result\":";
        if(completed)std::cout<<1;else std::cout<<"null";
        std::cout<<",\"materialUnchanged\":true}";
    }
    const char* PrefixStatus(spProjectionMaterialPreparationPrefixResultForAnalysis result)
    {
        using R=spProjectionMaterialPreparationPrefixResultForAnalysis;
        switch(result)
        {
        case R::AlreadyInitialized:return "AlreadyInitialized";
        case R::UnqualifiedMissingOwner:return "UnqualifiedMissingOwner";
        case R::UnqualifiedMaterialGraph:return "UnqualifiedMaterialGraph";
        case R::UnqualifiedMaterialLayerCreation:return "UnqualifiedMaterialLayerCreation";
        }
        return "Unknown";
    }
    void MaterialPrefixReplay(const std::string& mode,unsigned count,unsigned alpha,unsigned flag,unsigned second)
    {
        Check(count<=8&&second<=8&&alpha<256&&flag<256,"Typed material prefix fixture bounds");
        unsigned owner=0;spPCProjectionFX fx;spDXMaterial material;
        fx.BindOwnerForAnalysis(flag?nullptr:&owner);fx.SetInitializedByteForFixture(static_cast<std::uint8_t>(flag));
        material.SetVertexAlphaByteForAnalysis(static_cast<std::uint8_t>(alpha));
        for(unsigned i=0;i<11;++i)Check(material.SetRenderStateForAnalysis(i,0x22000000+i),"Material input state");
        std::vector<std::shared_ptr<spMaterialPassLayer>> passes;unsigned ordinal=0;
        auto makePass=[&](unsigned layers,unsigned which)
        {
            auto pass=std::make_shared<spMaterialPassLayer>();
            for(unsigned i=0;i<layers;++i)
            {
                std::unique_ptr<spStdLayer> layer;
                if(!(mode=="missing-layer"&&which==0))
                {
                    layer=std::make_unique<spStdLayer>();
                    for(unsigned state=0;state<9;++state)layer->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(state,0x11000000+ordinal*256+state);
                    ++ordinal;
                }
                Check(pass->SetLayerForAnalysis(i,std::move(layer)),"Existing layer graph input");
            }
            Check(material.SetPassForAnalysis(which,pass),"Existing material pass input");passes.push_back(std::move(pass));
        };
        if(mode!="missing-pass")makePass(count,0);
        if(second)makePass(second,1);
        const auto beforeFX=State(fx,&owner);const auto render=material.GetRenderStatesForAnalysis();
        const auto ambient=material.GetAmbientColorForAnalysis(),diffuse=material.GetDiffuseColorForAnalysis(),specular=material.GetSpecularColorForAnalysis(),emissive=material.GetEmissiveColorForAnalysis();
        const auto passCount=material.GetPassCountForAnalysis();
        std::vector<const void*> beforePointers;std::vector<long> references;
        for(const auto& pass:passes)
        {
            beforePointers.push_back(pass.get());references.push_back(pass.use_count());
            for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
            {
                const auto& layer=pass->GetLayerForAnalysis(i);beforePointers.push_back(layer.get());
                beforePointers.push_back(layer?layer->GetMaterialTextureForAnalysis().get():nullptr);
            }
        }
        const auto result=fx.PrepareMaterialLayersPrefixForAnalysis(material);
        std::vector<const void*> afterPointers;std::vector<long> afterReferences;
        bool other=beforeFX==State(fx,&owner)&&render==material.GetRenderStatesForAnalysis()
            &&ambient==material.GetAmbientColorForAnalysis()&&diffuse==material.GetDiffuseColorForAnalysis()
            &&specular==material.GetSpecularColorForAnalysis()&&emissive==material.GetEmissiveColorForAnalysis()
            &&material.GetPassCountForAnalysis()==passCount&&material.GetRenderOverrideByteForAnalysis()==0
            &&material.GetOpaqueRuntimeFieldForAnalysis()==0&&material.GetMaterialColorControllerForAnalysis()==nullptr
            &&!material.HasInitializedSpecularPowerForAnalysis();
        for(const auto& pass:passes)
        {
            afterPointers.push_back(pass.get());afterReferences.push_back(pass.use_count());other=other&&pass->GetFinalBlendOperationForAnalysis()==0;
            for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
            {
                const auto& layer=pass->GetLayerForAnalysis(i);afterPointers.push_back(layer.get());
                const auto* texture=layer?layer->GetMaterialTextureForAnalysis().get():nullptr;afterPointers.push_back(texture);
                if(texture)other=other&&texture->GetFallBackTextureForAnalysis()==nullptr&&texture->GetAnimTextureControllerForAnalysis()==nullptr&&texture->GetUVControllerForAnalysis()==nullptr;
            }
        }
        other=other&&beforePointers==afterPointers&&references==afterReferences;
        Check(other,"Material prefix preserves FX, other material fields and graph ownership");
        const bool whole=result==spProjectionMaterialPreparationPrefixResultForAnalysis::AlreadyInitialized;
        std::cout<<std::boolalpha<<"{\"mode\":\""<<mode<<"\",\"status\":\""<<PrefixStatus(result)<<"\",\"wholeOriginalReturn\":"<<whole<<",\"nativeReturn\":";
        if(whole)std::cout<<1;else std::cout<<"null";
        std::cout<<",\"initialized\":"<<unsigned(fx.InitializedByteForAnalysis())<<",\"cacheNull\":"<<(fx.Cache20ForAnalysis()==nullptr)
            <<",\"ownerBound\":"<<(fx.BorrowedOwnerForAnalysis()==&owner)<<",\"vertexAlpha\":"<<unsigned(material.GetVertexAlphaByteForAnalysis())<<",\"renderStates\":[";
        for(unsigned i=0;i<11;++i){if(i)std::cout<<',';std::cout<<material.GetRenderStateForAnalysis(i);}
        std::cout<<"],\"passLayerCounts\":[";
        for(std::size_t i=0;i<passes.size();++i){if(i)std::cout<<',';std::cout<<passes[i]->GetLayerCountForAnalysis();}
        std::cout<<"],\"textureStates\":[";
        for(std::size_t p=0;p<passes.size();++p)
        {
            if(p)std::cout<<',';std::cout<<'[';
            for(std::size_t i=0;i<passes[p]->GetLayerCountForAnalysis();++i)
            {
                if(i)std::cout<<',';const auto& layer=passes[p]->GetLayerForAnalysis(i);
                if(!layer){std::cout<<"null";continue;}std::cout<<'[';
                const auto& states=layer->GetMaterialTextureForAnalysis()->GetTextureStatesForAnalysis();
                for(unsigned state=0;state<9;++state){if(state)std::cout<<',';std::cout<<states[state];}std::cout<<']';
            }
            std::cout<<']';
        }
        // The source operation contains no allocation/release primitive;
        // pointer identities and shared ownership above are independently guarded.
        std::cout<<"],\"representedOtherStateUnchanged\":true,\"noAllocationOrRelease\":true}";
    }
    void MaterialPrefixGuards()
    {
        using R=spProjectionMaterialPreparationPrefixResultForAnalysis;
        spPCProjectionFX fx;spDXMaterial material;material.SetVertexAlphaByteForAnalysis(0xA5);
        Check(fx.PrepareMaterialLayersPrefixForAnalysis(material)==R::UnqualifiedMissingOwner&&material.GetVertexAlphaByteForAnalysis()==0xA5,"Missing owner refuses before the material prefix");
        fx.SetInitializedByteForFixture(0xA5);
        Check(fx.PrepareMaterialLayersPrefixForAnalysis(material)==R::AlreadyInitialized&&material.GetVertexAlphaByteForAnalysis()==0xA5,"Raw initialized byte bypasses owner and material mutations");
        fx.SetInitializedByteForFixture(0);unsigned owner=0;fx.BindOwnerForAnalysis(&owner);
        auto pass=std::make_shared<spMaterialPassLayer>();auto layer=std::make_unique<spStdLayer>();auto* texture=layer->GetMaterialTextureForAnalysis().get();
        Check(pass->SetLayerForAnalysis(0,std::move(layer))&&pass->SetLayerForAnalysis(1,nullptr)&&material.SetPassForAnalysis(0,pass),"Partial graph guard input");
        Check(fx.PrepareMaterialLayersPrefixForAnalysis(material)==R::UnqualifiedMaterialGraph&&material.GetVertexAlphaByteForAnalysis()==1
            &&texture->GetTextureStatesForAnalysis()[3]==2&&texture->GetTextureStatesForAnalysis()[8]==11&&fx.InitializedByteForAnalysis()==0,"A later null layer preserves the completed prefix and refuses remaining work");
    }
    void Guards()
    {
        Host host; unsigned owner=0,material=0; host.owner=&owner; host.material=&material;
        spPCProjectionFX fx;
        Check(fx.InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::UnqualifiedMissingOwner && host.reads==0,
            "Missing owner is an explicit analytical refusal before foreign data read");
        fx.BindOwnerForAnalysis(&owner);
        Check(fx.InitializeForAnalysis(host)==spProjectionFxInitializationResultForAnalysis::UnqualifiedMaterialInitialization
            && fx.InitializedByteForAnalysis()==0 && fx.Cache20ForAnalysis()==nullptr,
            "Material initialization cannot turn into a successful stub");
        fx.SetName("source-name"); fx.SetInitializedByteForFixture(255);
        spCloneManager manager; auto copy=fx.vfunc_10(manager);
        auto* typed=dynamic_cast<spPCProjectionFX*>(copy.get());
        Check(typed && std::string(typed->GetName())=="source-name" && !typed->BorrowedOwnerForAnalysis()
            && typed->InitializedByteForAnalysis()==0 && !typed->Cache20ForAnalysis(),"Clone retains factory own state and inherited name only");
        Check(spProjectionFX::StaticRTTI().factory==nullptr,"Abstract native family has no registry factory");
        auto factory=spPCProjectionFX::StaticRTTI().factory();
        Check(factory && factory->IsExactly(spPCProjectionFX::ClassID),"Concrete source RTTI factory");
        std::unique_ptr<spPCProjectionFX> absent;
        Check(InitializeProjectionPCWithOwnedFxForAnalysis(nullptr,absent,host)==spProjectionFxInitializationResultForAnalysis::UnqualifiedMissingOwner
            && !absent,"Null projection refuses before construction and owner publication");
    }
    void DrawGateReplay(const std::string& label,bool hasScene,std::uint32_t stamp,
        std::uint32_t mark,std::uint32_t flags,unsigned initialized)
    {
        Check(initialized<256,"Draw gate initialized-byte input");spPCProjectionFX fx;
        fx.SetInitializedByteForFixture(static_cast<std::uint8_t>(initialized));const auto before=State(fx,nullptr);
        spProjectionDrawGateInputForAnalysis input{hasScene?std::optional<std::uint32_t>(stamp):std::nullopt,mark,flags};
        const auto result=fx.EvaluateDrawGateForAnalysis(input);
        Check(before==State(fx,nullptr),"Draw gate never changes represented FX state");
        const bool completed=result==spProjectionDrawGateResultForAnalysis::Skipped;
        std::cout<<std::boolalpha<<"{\"label\":\""<<label<<"\",\"sceneStamp40\":";
        if(hasScene)std::cout<<stamp;else std::cout<<"null";
        std::cout<<",\"supportMark118\":"<<mark<<",\"nodeFlagsB0\":"<<flags<<",\"initialized\":"<<initialized
            <<",\"completed\":"<<completed<<",\"nativeReturn\":";
        if(completed)std::cout<<1;else std::cout<<"null";std::cout<<'}';
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3 && std::string(argv[1])=="--replay") { Replay(argv[2]); std::cout << '\n'; return 0; }
        if(argc==3 && std::string(argv[1])=="--setup-replay") { SetupReplay(argv[2]);std::cout<<'\n';return 0; }
        if(argc==2 && std::string(argv[1])=="--material-prefix")
        {
            std::string line;while(std::getline(std::cin,line))
            {
                if(line.empty())continue;std::istringstream input(line);std::string mode;unsigned count,alpha,flag,second;
                Check(bool(input>>mode>>count>>alpha>>flag>>second),"Material prefix replay input");
                MaterialPrefixReplay(mode,count,alpha,flag,second);std::cout<<'\n';
            }
            return 0;
        }
        if(argc==2 && std::string(argv[1])=="--draw-gates")
        {
            std::string line;while(std::getline(std::cin,line))if(!line.empty())
            {
                std::istringstream in(line);std::string label;unsigned hasScene,initialized;std::uint32_t stamp,mark,flags;
                Check(bool(in>>label>>hasScene>>stamp>>mark>>flags>>initialized)&&hasScene<2,"Draw gate replay input");
                DrawGateReplay(label,hasScene!=0,stamp,mark,flags,initialized);std::cout<<'\n';
            }
            return 0;
        }
        Guards();
        {
            spPCProjectionFX fx;using R=spProjectionDrawGateResultForAnalysis;
            Check(fx.EvaluateDrawGateForAnalysis({std::nullopt,7,0x200})==R::Skipped,"Missing Scene gate precedes Enabled and owner data");
            Check(fx.EvaluateDrawGateForAnalysis({0,0,0x200})==R::UnqualifiedActiveDraw,"Matching zero stamps are active, not missing Scene");
        }
        MaterialPrefixGuards();
        {
            std::ostringstream output;auto* old=std::cout.rdbuf(output.rdbuf());
            try{MaterialPrefixReplay("count-8",8,0,0,0);MaterialPrefixReplay("second-pass",2,0,0,3);}catch(...){std::cout.rdbuf(old);throw;}
            std::cout.rdbuf(old);
        }
        for(const char* mode:{"ctor-clone","owned","flagged","reuse-owner-null"})
        {
            std::ostringstream output; auto* old=std::cout.rdbuf(output.rdbuf());
            try { Replay(mode); } catch(...) { std::cout.rdbuf(old); throw; }
            std::cout.rdbuf(old);
        }
        for(const char* mode:{"cold","reuse","warm-zero","warm-raw-byte","move-owner","material-prefix"})
        {
            std::ostringstream output;auto* old=std::cout.rdbuf(output.rdbuf());
            try{SetupReplay(mode);}catch(...){std::cout.rdbuf(old);throw;}
            std::cout.rdbuf(old);
        }
        std::cout << "ProjectionFx checks=" << checks << '\n'; return 0;
    }
    catch(const std::exception& error) { std::cerr << "ProjectionFx failure: " << error.what() << '\n'; return 1; }
}
