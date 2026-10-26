#include "Code/wxCentreScreenGameFlowState.h"
#include "Analysis/PC/wxCentreScreenGameFlowStateAbi.h"
#include "Analysis/PS2/wxCentreScreenGameFlowStateAbi.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
    void* Token(unsigned id){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));}
    struct Host final : wxCentreScreenGameFlowStateHost
    {
        struct Allocation final{std::unique_ptr<spMaterial> object;std::uint16_t refs=0;};
        wxGameFlowResourcesForAnalysis resources;
        wxCentreScreenGameFlowState* active=nullptr;
        std::vector<Allocation> materials;
        std::vector<std::weak_ptr<spMaterialPassLayer>> passes;
        std::vector<std::string> events;
        std::uint16_t factoryRefs=0;
        std::uint32_t flags=0,a=0,b=0;
        unsigned currentApp=1,currentWindow=1,alternate=0,graphFrees=0;
        std::array<std::uint32_t,2> windows{0xA5A5A5A5,0xA5A5A5A5};
        void* BorrowInputForAnalysis() override{return Token(0x1234);}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override{return {1200,1};}
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override{return resources;}
        spMaterial* MakeMaterial(std::uint16_t refs)
        {
            auto object=std::make_unique<spDXMaterial>();auto* result=object.get();
            materials.push_back({std::move(object),refs});return result;
        }
        std::size_t ID(spMaterial& value)
        {
            for(std::size_t i=0;i<materials.size();++i)if(materials[i].object.get()==&value)return i;
            throw std::runtime_error("Material must belong to the external arena");
        }
        spMaterial* CreateCentreMaterialForAnalysis() override
        {events.push_back("M");return MakeMaterial(factoryRefs);}
        std::uint16_t& BorrowCentreMaterialRefCount8ForAnalysis(spMaterial& material) override
        {return materials[ID(material)].refs;}
        void DestroyCentreMaterialForAnalysis(spMaterial& material,std::uint32_t flag) override
        {
            const auto index=ID(material);Check(flag==1,"Native deleting flag1");
            Check(materials[index].refs==0,"Delete follows native ref16 reaching zero");
            events.push_back("D"+std::to_string(index+1)+":1");++graphFrees;
            for(std::size_t i=0;i<material.GetPassCountForAnalysis();++i)
                if(auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(i)))
                {
                    ++graphFrees;
                    for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
                        if(const auto& layer=pass->GetLayerForAnalysis(j))
                            graphFrees+=1+static_cast<unsigned>(layer->GetMaterialTextureForAnalysis()!=nullptr);
                }
            if(flags&8)
            {
                active->CentreStateForFixture().material3C=reinterpret_cast<spMaterial*>(Token(0x12345678));
                currentWindow=2;
            }
            materials[index].object.reset();
        }
        std::shared_ptr<spMaterialPassLayer> CreateCentrePassForAnalysis() override
        {
            events.push_back("P");
            if(flags&1)active->CentreStateForFixture().material3C=materials[alternate-1].object.get();
            auto value=std::make_shared<spMaterialPassLayer>();passes.push_back(value);return value;
        }
        std::unique_ptr<spStdLayer> CreateCentreStdLayerForAnalysis() override
        {
            events.push_back("L");
            if(flags&2)Check(passes.back().lock()->SetLayerForAnalysis(0,nullptr),"Declare null layer0 before count reload");
            return std::make_unique<spStdLayer>();
        }
        void* ResolveCentreAppOwnerForAnalysis() override{return Token(currentApp);}
        std::uint32_t ReadCentreAppWord64ForAnalysis(void* owner) override
        {
            Check(owner==Token(1)||owner==Token(2),"App receiver");
            events.push_back("A"+std::to_string(owner==Token(1)?1:2)+":64");
            if(flags&4)currentApp=2;return a;
        }
        std::uint32_t ReadCentreAppWord68ForAnalysis(void* owner) override
        {
            Check(owner==Token(1)||owner==Token(2),"App receiver");
            events.push_back("A"+std::to_string(owner==Token(1)?1:2)+":68");
            return owner==Token(1)?b:b^0xFFFFFFFF;
        }
        void* ResolveCentreWindowForAnalysis() override{return Token(currentWindow);}
        std::uint32_t& BorrowCentreWindowWord1294ForAnalysis(void* window) override
        {
            Check(window==Token(1)||window==Token(2),"Window receiver");
            const auto index=window==Token(1)?0:1;events.push_back("W"+std::to_string(index+1));return windows[index];
        }
        std::string Graph(spMaterial& material)
        {
            std::ostringstream out;out<<"{\"ref16\":"<<materials[ID(material)].refs
                <<",\"override\":"<<unsigned(material.GetRenderOverrideByteForAnalysis())
                <<",\"states\":["<<material.GetRenderStateForAnalysis(3)<<','<<material.GetRenderStateForAnalysis(7)
                <<','<<material.GetRenderStateForAnalysis(8)<<"],\"passes\":[";
            for(std::size_t i=0;i<material.GetPassCountForAnalysis();++i)
            {
                if(i)out<<',';auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(i));
                if(!pass){out<<"null";continue;}
                long owners=0;
                for(const auto& weak:passes)
                    if(auto shared=weak.lock();shared.get()==pass){owners=weak.use_count()-1;break;}
                out<<"{\"ref16\":"<<owners<<",\"blend\":"<<pass->GetFinalBlendOperationForAnalysis()<<",\"layers\":[";
                for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
                {
                    if(j)out<<',';const auto& layer=pass->GetLayerForAnalysis(j);
                    if(!layer){out<<"null";continue;}
                    Check(dynamic_cast<const spStdLayer*>(layer.get())!=nullptr,"Actual reconstructed StdLayer");
                    Check(layer->GetMaterialTextureForAnalysis()!=nullptr,"Actual reconstructed MaterialTexture payload");
                    out<<"{\"classID\":\"234C576B\",\"textureSize\":104}"; // qualified PC native extent, not sizeof(host)
                }
                out<<"]}";
            }
            out<<"]}";return out.str();
        }
    };
    std::string Run(const std::string& op,std::uint16_t refs,std::uint32_t flags,std::uint32_t a,std::uint32_t b)
    {
        Host host;host.factoryRefs=refs;host.flags=flags;host.a=a;host.b=b;
        if(flags&1){host.MakeMaterial(5);host.alternate=1;}
        auto* existing=op=="exit"?host.MakeMaterial(refs):nullptr;
        auto state=std::make_unique<wxCentreScreenGameFlowState>(host);host.active=state.get();
        Check(!state->GetCentreStateForAnalysis().material3CKnown&&!state->GetCentreStateForAnalysis().word40
            &&!state->GetCentreStateForAnalysis().word44,"Own ctor slots remain individually unknown");
        if(op=="exit"||op=="exitNull")
        {state->CentreStateForFixture().material3C=existing;state->CentreStateForFixture().material3CKnown=true;}
        std::ostringstream before;before<<'[';
        if(op=="enterExit"||op=="doubleEntryExit"||op=="enterDestroy")
        {
            state->FlowSlot20ForAnalysis();if(op=="doubleEntryExit")state->FlowSlot20ForAnalysis();
            for(std::size_t i=0;i<host.materials.size();++i)
            {if(i)before<<',';before<<host.Graph(*host.materials[i].object);}
        }
        before<<']';
        if(op!="enterDestroy")state->FlowSlot24ForAnalysis();
        const auto& fields=state->GetCentreStateForAnalysis();unsigned slot=0;
        for(std::size_t i=0;i<host.materials.size();++i)if(fields.material3C==host.materials[i].object.get()&&fields.material3C)slot=static_cast<unsigned>(i+1);
        std::ostringstream final;final<<"{\"materialSlot\":"<<slot<<",\"word40\":"<<fields.word40.value_or(0xCCCCCCCC)
            <<",\"word44\":"<<fields.word44.value_or(0xCCCCCCCC)<<",\"windowWords\":["<<host.windows[0]<<','<<host.windows[1]<<"],\"materials\":[";
        unsigned retained=0;
        for(std::size_t i=0;i<host.materials.size();++i)
        {
            if(i)final<<',';const auto& allocation=host.materials[i];
            final<<"{\"alive\":"<<(allocation.object?"true":"false")<<",\"ref16\":"<<allocation.refs<<",\"graph\":";
            if(allocation.object){++retained;final<<host.Graph(*allocation.object);}else final<<"null";
            final<<'}';
        }
        final<<"]}";
        state.reset();
        Check(retained==static_cast<unsigned>(std::count_if(host.materials.begin(),host.materials.end(),[](const auto& m){return m.object!=nullptr;})),"State destructor leaves arena materials intact");
        std::ostringstream out;out<<"{\"events\":[";
        for(std::size_t i=0;i<host.events.size();++i){if(i)out<<',';out<<'"'<<host.events[i]<<'"';}
        out<<"],\"beforeExit\":"<<before.str()<<",\"final\":"<<final.str()
            <<",\"operationGraphAllocationsFreed\":"<<host.graphFrees
            <<",\"retainedMaterialGraphsAfterStateDestructor\":"<<retained<<'}';return out.str();
    }
    void Contracts()
    {
        Host host;wxCentreScreenGameFlowState source(host),destination(host);host.active=&source;
        auto& target=destination.CentreStateForFixture();target.material3CKnown=true;target.word40=42;target.word44=84;
        spCloneManager manager;Check(source.vfunc_14(destination,manager),"Inherited Copy succeeds");
        Check(target.material3CKnown&&target.word40==42&&target.word44==84,"Copy preserves destination own fields");
        auto clone=manager.Clone(source);auto* typed=dynamic_cast<wxCentreScreenGameFlowState*>(clone.get());
        Check(typed&&typed->IsExactly(wxCentreScreenGameFlowState::ClassID)&&typed->IsKindOf(wxGameFlowState::ClassID),"Typed RTTI/clone");
        Check(!typed->GetCentreStateForAnalysis().material3CKnown&&!typed->GetCentreStateForAnalysis().word40
            &&!typed->GetCentreStateForAnalysis().word44,"Clone own slots remain unwritten");
        Check(&typed->TimerForAnalysis()!=&source.TimerForAnalysis(),"Clone owns a distinct parent timer");
        bool refused=false;try{source.FlowSlot24ForAnalysis();}catch(const std::logic_error&){refused=true;}
        Check(refused&&host.events.empty(),"Unknown cold Exit refuses before foreign effects");
        refused=false;try{(void)source.FlowSlot30ForAnalysis();}catch(const std::logic_error&){refused=true;}
        Check(refused,"Unrecovered Tick is not inherited successful base behavior");
        const auto baseline=Run("enterExit",0,0,0x12345678,0x89ABCDEF);
        Check(baseline.find("\"events\":[\"M\",\"P\",\"L\",\"A1:64\",\"A1:68\",\"D1:1\",\"W1\"]")!=std::string::npos,"Exact normal factory/read/delete/window order");
        Check(baseline.find("\"operationGraphAllocationsFreed\":4")!=std::string::npos,"Exit destroys material/pass/Std/texture graph");
        const auto mutations=Run("enterExit",0,15,0x10203040,0x50607080);
        Check(mutations.find("\"events\":[\"M\",\"P\",\"L\",\"A1:64\",\"A2:68\",\"W1\"]")!=std::string::npos,"Factories/read callbacks preserve exact owner reloads");
        Check(mutations.find("\"retainedMaterialGraphsAfterStateDestructor\":2")!=std::string::npos,"Alternate ref5 material retained at4, original remains1");
        Check(Run("enterExit",0,8,0,0).find("\"D1:1\",\"W2\"")!=std::string::npos,"Delete mutation precedes fresh window load and own pointer clear");
        Check(Run("doubleEntryExit",0,0,0,0).find("\"retainedMaterialGraphsAfterStateDestructor\":1")!=std::string::npos,"Repeated Entry does not release old material");
        Check(Run("enterDestroy",0,0,0,0).find("\"operationGraphAllocationsFreed\":0")!=std::string::npos,"Destructor does not replace Exit");
        Check(Run("exit",0,0,0,0).find("\"ref16\":65535")!=std::string::npos,"Native unsigned decrement wraps at zero");
        Check(Run("enterExit",65535,0,0,0).find("\"ref16\":65535")!=std::string::npos,"Increment and decrement wrap independently");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit")
        {
            std::string op;std::uint32_t refs,flags,a,b;
            while(std::cin>>op>>refs>>flags>>a>>b)
            {Check(refs<=65535,"Ref input fits native16-bit word");std::cout<<Run(op,static_cast<std::uint16_t>(refs),flags,a,b)<<'\n';}
        }
        else{Contracts();std::cout<<"PASS CentreScreen typed ownership, unknown slots, raw refs, callback order, Copy/clone and retained graphs\n";}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
