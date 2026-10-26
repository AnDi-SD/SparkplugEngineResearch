#include "Code/wxLogoAnimGameFlowState.h"
#include "Code/wxAssetManager.h"
#include "Code/Sparkplug/spResource.h"
#include "Analysis/PC/wxLogoAnimGameFlowStateAbi.h"
#include "Analysis/PS2/wxLogoAnimGameFlowStateAbi.h"
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    void* Token(unsigned value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
    std::string Quote(std::string_view value)
    {
        std::string out="\"";
        for(char c:value){if(c=='\\'||c=='"')out+='\\';out+=c;}
        return out+'"';
    }
    struct AssetPlatform final : wxAssetManagerHost
    {
        std::string root;
        explicit AssetPlatform(std::string value):root(std::move(value)){}
        bool IsPs2ForAnalysis() const noexcept override{return false;}
        std::uint32_t AppLanguageForAnalysis() const noexcept override{return 0;}
        void SetAppLanguageForAnalysis(std::uint32_t) noexcept override{}
        std::optional<std::string> ReadPcRegistryValueForAnalysis(std::string_view,std::string_view name) override
        {return name=="MediaPath"?std::optional<std::string>(root):std::nullopt;}
        std::unique_ptr<wxAssetFileStreamForAnalysis> CreateFileStreamForAnalysis() override{return nullptr;}
        void ReportOpenFailureForAnalysis(std::string_view) noexcept override{}
    };
    struct Host final : wxLogoAnimGameFlowStateHost
    {
        struct Object final{std::unique_ptr<spResource> owner;spBaseObject* address=nullptr;std::uint16_t refs=0;};
        AssetPlatform assetPlatform1{"A/"},assetPlatform2{"B/"};
        wxAssetManager asset1{assetPlatform1},asset2{assetPlatform2};
        spResourceManager manager1,manager2;
        std::array<Object,4> objects;
        std::array<spBaseObject*,2> shared{};
        wxGameFlowResourcesForAnalysis resources;
        wxLogoAnimGameFlowState* active=nullptr;
        HookProfileForAnalysis profile=HookProfileForAnalysis::PC;
        std::set<wxLogoAnimGameFlowState*> listeners;
        std::vector<std::string> events;
        std::size_t assetResolves=0;
        std::string afterFirstAssignment="null";
        std::vector<spTimer::StateForAnalysis> clockEntries;
        std::vector<void*> borrowedInputs;
        std::size_t borrowedIndex=0,loadCount=0,clockCount=0,deleteCount=0;
        const char* temporaryBuffer=nullptr;
        unsigned assetID=1,managerID=1,inputOwner=1,firstOwnerInput=1,appID=1;
        std::array<unsigned,2> loadedIDs{1,2};
        std::uint32_t flags=0,appWord=0;
        std::uint8_t appByte=0;
        std::array<std::uint32_t,2> samples{100,200};
        std::array<std::uint32_t,4> globalWords{11,22,33,44};
        Host()
        {
            asset1.InitializeForAnalysis();asset2.InitializeForAnalysis();
            for(auto& object:objects){object.owner=std::make_unique<spResource>();object.address=object.owner.get();}
        }
        spBaseObject* Pointer(unsigned id){Check(id<=4,"Object ID range");return id?objects[id-1].address:nullptr;}
        unsigned ID(spBaseObject* value) const
        {
            if(!value)return 0;
            for(unsigned i=0;i<objects.size();++i)if(objects[i].address==value)return i+1;
            throw std::runtime_error("Unknown resource identity");
        }
        void* BorrowInputForAnalysis() override
        {const auto i=borrowedIndex++;return i<borrowedInputs.size()?borrowedInputs[i]:Token(0x12345678);}
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override{return {1200,1};}
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override{return resources;}
        HookProfileForAnalysis LogoHookProfileForAnalysis() const noexcept override{return profile;}
        void RegisterLogoReceiverForAnalysis(wxLogoAnimGameFlowState& value,std::uint32_t tag) override
        {Check(tag==0x13&&listeners.insert(&value).second,"Register tag13 once");active=&value;events.push_back("+13");}
        void UnregisterLogoReceiverForAnalysis(wxLogoAnimGameFlowState& value,std::uint32_t tag) override
        {Check(tag==0x13&&listeners.erase(&value)==1,"Unregister tag13 once");active=&value;events.push_back("-13");}
        std::string ReferenceSnapshot() const
        {
            std::ostringstream out;out<<"{\"shared\":["<<ID(shared[0])<<','<<ID(shared[1])<<"],\"refs\":[";
            for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<objects[i].refs;}out<<"]}";return out.str();
        }
        wxAssetManager* ResolveLogoAssetManagerForAnalysis() override
        {
            if(assetResolves++==1)afterFirstAssignment=ReferenceSnapshot();
            events.push_back("ASSET"+std::to_string(assetID));return assetID==1?&asset1:&asset2;
        }
        spResourceManager* ResolveLogoResourceManagerForAnalysis() override{return managerID==1?&manager1:&manager2;}
        spBaseObject* LoadLogoResourceForAnalysis(spResourceManager& manager,const char* path) override
        {
            const auto managerIndex=&manager==&manager1?1u:2u;Check(&manager==&manager1||&manager==&manager2,"Typed ResourceManager receiver");
            if(temporaryBuffer)Check(temporaryBuffer==path,"Both loads use the same local filename buffer");else temporaryBuffer=path;
            const std::string_view name(path);const auto index=name.find("loading_empty.stx")!=std::string_view::npos?0u:1u;
            Check(name=="A/Menus\\loading_empty.stx"||name=="A/Menus\\loading_full.stx"
                ||name=="B/Menus\\loading_empty.stx"||name=="B/Menus\\loading_full.stx","Real shared BuildPath output");
            const auto id=loadedIDs[index];events.push_back("L"+std::to_string(managerIndex)+":"+std::string(name)+":"+std::to_string(id));
            if(loadCount++==0)
            {
                if(flags&1)assetID=2;
                if(flags&2)shared[0]=Pointer(4);
                if(flags&32)managerID=2;
                if(flags&2048)active->LogoStateForFixture().referenced50=Pointer(4);
            }
            return Pointer(id);
        }
        spBaseObject*& BorrowLogoSharedResourceForAnalysis(std::size_t index) override
        {Check(index<2,"Shared slot range");return shared[index];}
        std::uint16_t& BorrowLogoNativeReferenceCount8ForAnalysis(spBaseObject& value) override
        {auto& object=objects[ID(&value)-1];Check(object.owner!=nullptr,"Reference word belongs to a live arena object");return object.refs;}
        void DestroyLogoReferencedObjectForAnalysis(spBaseObject& value,std::uint32_t flag) override
        {
            const auto id=ID(&value);Check(flag==1&&objects[id-1].refs==0,"Delete only zero native reference count with flag1");
            events.push_back("D"+std::to_string(id)+":1:T"+std::to_string(active&&active->HasLiveLogoTimerForAnalysis()?1:0));
            if(deleteCount++==0)
            {
                if(flags&4)managerID=2;
                if(flags&8)shared[1]=Pointer(4);
                if(flags&16)shared[0]=Pointer(2);
            }
            if(flags&4096)active->LogoStateForFixture().referenced50=Pointer(4);
            objects[id-1].owner.reset();
        }
        void* ResolveLogoServiceForAnalysis() override{events.push_back("S");return Token(0x5000);}
        void InvokePS2LogoServiceEntryForAnalysis(void* value) override{Check(value==Token(0x5000),"PS2 service receiver");events.push_back("SERVICE-E");}
        void InvokePS2LogoServiceExitForAnalysis(void* value,std::string_view tag) override
        {Check(value==Token(0x5000)&&tag=="GLOBAL","PS2 GLOBAL tag");events.push_back("SERVICE-X");}
        void* ResolveLogoAudioInterfaceForAnalysis() override{return Token(0x6000);}
        void* GetLogoAudioControllerForAnalysis(void* value) override
        {Check(value==Token(0x6000),"Audio interface receiver");events.push_back("AUDIO");return Token(0x7000);}
        void SetLogoAudioEnabledForAnalysis(void* value,std::uint32_t enabled) override
        {Check(value==Token(0x7000)&&enabled==0,"Audio disabled with literal0");events.push_back("AUDIO:0");if(flags&1024)inputOwner=2;}
        void* ResolveHookInputOwnerForAnalysis() override{return Token(inputOwner);}
        void* ReadHookInputForAnalysis(void* value) override
        {Check(value==Token(1)||value==Token(2),"Captured input owner");return value==Token(1)?Token(firstOwnerInput):Token(3);}
        void ResetHookInputForAnalysis(void* value) override
        {
            const auto id=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(value));events.push_back("R"+std::to_string(id));
            if(flags&512){firstOwnerInput=2;inputOwner=2;}
        }
        void SetHookInputModeForAnalysis(void* value,std::uint32_t mode) override
        {events.push_back("M"+std::to_string(reinterpret_cast<std::uintptr_t>(value))+":"+std::to_string(mode));Check(mode==0,"Input mode0");}
        std::uint32_t ReadLogoTimerClockForAnalysis() override
        {
            Check(active&&clockCount<2,"Two explicit clock samples");const auto sample=samples[clockCount++];events.push_back("T"+std::to_string(clockCount)+":"+std::to_string(sample));
            auto timer=active->LogoTimerForAnalysis().GetStateForAnalysis();
            clockEntries.push_back(timer);
            if((flags&128)&&clockCount==1)timer.accumulated=17;
            if((flags&256)&&clockCount==2){timer.accumulated=29;timer.startedAt=999;timer.active=0xEE;timer.limited=0xA5;timer.limit=0xD00;}
            active->LogoTimerForAnalysis().SetStateForAnalysis(timer);return sample;
        }
        void* ResolveLogoAppOwnerForAnalysis() override{return Token(appID);}
        std::uint8_t ReadLogoAppByte4BForAnalysis(void* value) override
        {const auto id=value==Token(1)?1u:2u;events.push_back("B"+std::to_string(id)+":"+std::to_string(appByte));if(flags&64)appID=2;return appByte;}
        std::uint32_t ReadLogoAppWord60ForAnalysis(void* value) override
        {const auto id=value==Token(1)?1u:2u;const auto word=id==1?appWord:appWord^0xFFFFFFFF;events.push_back("W"+std::to_string(id)+":"+std::to_string(word));return word;}
        std::array<std::uint32_t,4>& BorrowLogoGlobalWordsForAnalysis() override{return globalWords;}
    };
    std::string Snapshot(const wxLogoAnimGameFlowState& state,Host& host)
    {
        const auto& data=state.GetLogoStateForAnalysis();const auto& timer=state.LogoTimerForAnalysis().GetStateForAnalysis();std::ostringstream out;
        out<<"{\"field25\":"<<unsigned(state.GetStateForAnalysis().field25)<<",\"field40\":"<<unsigned(data.field40)
            <<",\"field41\":"<<unsigned(data.field41)<<",\"referenced50\":"<<host.ID(data.referenced50)
            <<",\"words\":["<<data.word44<<','<<data.word48<<','<<data.word4C<<','<<data.word54<<','<<data.word7C<<','<<data.word80
            <<"],\"timer\":["<<unsigned(timer.active)<<','<<timer.accumulated<<','<<timer.startedAt<<','<<unsigned(timer.limited)<<','<<timer.limit<<"],\"globals\":[";
        for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<host.globalWords[i];}out<<"]}";return out.str();
    }
    std::string Run(std::string_view op,const std::array<std::uint32_t,14>& v,Host::HookProfileForAnalysis profile=Host::HookProfileForAnalysis::PC)
    {
        Host host;host.profile=profile;host.flags=v[9];host.appByte=static_cast<std::uint8_t>(v[10]);host.appWord=v[11];host.samples={v[12],v[13]};
        for(std::size_t i=0;i<4;++i){Check(v[5+i]<=65535,"Native ref16 input");host.objects[i].refs=static_cast<std::uint16_t>(v[5+i]);}
        host.shared={host.Pointer(v[1]),host.Pointer(v[2])};host.loadedIDs={v[3],v[4]};
        auto state=std::make_unique<wxLogoAnimGameFlowState>(host);state->LogoStateForFixture().referenced50=host.Pointer(v[0]);
        Check(!state->GetLogoStateForAnalysis().word84,"Native own84 remains unknown");
        if(op=="helper")LoadLogoSharedResourcesForAnalysis(host);
        else if(op=="enter"||op=="enterExit"){state->FlowSlot20ForAnalysis();if(op=="enterExit")state->FlowSlot24ForAnalysis();}
        else if(op=="exit")state->FlowSlot24ForAnalysis();
        else if(op=="leave")state->FlowSlot2CForAnalysis();
        else Check(op=="destroy","Explicit case operation");
        const auto before=Snapshot(*state,host);state.reset();host.active=nullptr;
        std::ostringstream out;out<<"{\"events\":[";for(std::size_t i=0;i<host.events.size();++i){if(i)out<<',';out<<Quote(host.events[i]);}
        out<<"],\"clockEntries\":[";
        for(std::size_t i=0;i<host.clockEntries.size();++i)
        {
            if(i)out<<',';const auto& timer=host.clockEntries[i];out<<'['<<unsigned(timer.active)<<','<<timer.accumulated<<','<<timer.startedAt<<','<<unsigned(timer.limited)<<','<<timer.limit<<']';
        }
        out<<"],\"beforeDestructor\":"<<before<<",\"afterFirstAssignment\":"<<host.afterFirstAssignment
            <<",\"final\":{\"shared\":["<<host.ID(host.shared[0])<<','<<host.ID(host.shared[1])<<"],\"refs\":[";
        for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<host.objects[i].refs;}out<<"],\"alive\":[";
        for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<(host.objects[i].owner?"true":"false");}out<<"]}}";return out.str();
    }
    void Contracts()
    {
        Host host;host.borrowedInputs={Token(1),Token(2)};auto state=std::make_unique<wxLogoAnimGameFlowState>(host);
        Check(state->GetBorrowedInputForAnalysis()==Token(1)&&state->GetLogoStateForAnalysis().cachedInput3C==Token(2),"Constructor performs second distinct member18 borrow");
        Check(state->GetLogoStateForAnalysis().field40==1&&state->GetLogoStateForAnalysis().field41==0&&!state->GetLogoStateForAnalysis().word84,"Raw constructor flags and unwritten84");
        auto destination=std::make_unique<wxLogoAnimGameFlowState>(host);destination->LogoStateForFixture().word48=77;
        state->LogoStateForFixture().referenced50=host.Pointer(1);host.objects[0].refs=2;
        destination->LogoTimerForAnalysis().SetStateForAnalysis({0xA5,123,456,0xEE,999});
        spCloneManager manager;Check(state->vfunc_14(*destination,manager),"Inherited Copy succeeds");
        Check(destination->GetLogoStateForAnalysis().word48==77&&destination->LogoTimerForAnalysis().GetStateForAnalysis().accumulated==123,"Copy does not transfer own words or inline timer");
        auto clone=manager.Clone(*state);auto* typed=dynamic_cast<wxLogoAnimGameFlowState*>(clone.get());
        Check(typed&&typed->IsExactly(wxLogoAnimGameFlowState::ClassID)&&typed->IsKindOf(wxGameFlowState::ClassID),"Typed Logo clone RTTI");
        Check(typed->GetLogoStateForAnalysis().referenced50==nullptr&&!typed->GetLogoStateForAnalysis().word84&&typed->LogoTimerForAnalysis().GetStateForAnalysis().active==0,"Fresh clone defaults, no own resource/timer Copy");
        Check(host.objects[0].refs==2,"Copy and clone do not retain source reference50");
        Check(&typed->TimerForAnalysis()!=&state->TimerForAnalysis() && &typed->LogoTimerForAnalysis()!=&state->LogoTimerForAnalysis(),"Clone parent and inline timers are distinct");
        bool refused=false;try{(void)state->FlowSlot30ForAnalysis();}catch(const std::logic_error&){refused=true;}Check(refused,"Unknown Slot30 body refuses");
        clone.reset();destination.reset();state.reset();Check(host.listeners.empty(),"Every real source listener unregistered");
        Check(host.objects[0].refs==1&&host.objects[0].owner,"Only source destructor releases its existing50 edge");
        std::array<std::uint32_t,14> values{0,1,2,3,4,1,2,0,65535,0,0,0,100,200};
        auto replaced=Run("helper",values);Check(replaced.find("\"shared\":[3,4],\"refs\":[0,1,1,0]")!=std::string::npos,"Replace/delete/retain/wrap exact slots and counts");
        values={0,1,2,1,2,1,1,0,0,0,0,0,100,200};
        auto alias=Run("helper",values);Check(alias.find("\"refs\":[1,1,0,0]")!=std::string::npos&&alias.find("D1:")==std::string::npos,"Alias performs no refcount or delete effects");
        values={0,0,0,1,2,0,0,0,0,960,0,0,100,200};
        const auto pc=Run("enter",values),ps2=Run("enter",values,Host::HookProfileForAnalysis::PS2);
        Check(pc.find("\"timer\":[1,29,200,165,3328]")!=std::string::npos,"Shared timer Reset/Start preserves callback mutations and second sample");
        Check(pc.find("\"W1:0\"")!=std::string::npos&&ps2.find("\"W2:4294967295\"")!=std::string::npos,"PC captured versus PS2 reloaded app owner");
        Check(pc.find("\"R1\",\"M2:0\"")!=std::string::npos,"Reset reloads captured owner's member1C");
        values={1,0,0,1,2,1,0,0,1,4096,0,0,100,200};
        Check(Run("exit",values).find("\"referenced50\":0")!=std::string::npos,"Exit clears50 after deleting callback replacement");
        values[9]=0;Check(Run("destroy",values).find("D1:1:T0")!=std::string::npos,"Destructor destroys inline timer before releasing50");
        Host invalid;wxAssetManager uninitialized(invalid.assetPlatform1);invalid.assetID=1;
        std::string output="sentinel";Check(!uninitialized.BuildPathForAnalysis(output,256,4,0,"loading_empty.stx")&&output=="sentinel","Shared asset safety domain remains explicit");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&(std::string_view(argv[1])=="--emit"||std::string_view(argv[1])=="--emit-ps2"))
        {
            const auto profile=std::string_view(argv[1])=="--emit"?Host::HookProfileForAnalysis::PC:Host::HookProfileForAnalysis::PS2;
            std::string op;while(std::cin>>op){std::array<std::uint32_t,14> values{};for(auto& v:values)Check(bool(std::cin>>v),"Complete source input");std::cout<<Run(op,values,profile)<<'\n';}
        }
        else{Contracts();std::cout<<"PASS Logo typed lifetime, real shared AssetManager/Timer reuse, helper ref assignments, callback reloads and unknown Slot30 guard\n";}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
