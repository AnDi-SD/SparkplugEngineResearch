#include "Code/Sparkplug/spProjectionManager.h"
#include "Code/SparkplugPC/spPCProjectionManager.h"
#include "Analysis/PC/spProjectionManagerAbi.h"
#include <array>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    void* Token(unsigned value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
    unsigned Number(void* p){return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(p));}
    struct Host final : spProjectionManagerHost
    {
        spProjectionManager* manager=nullptr;std::string mode;
        std::array<unsigned,3> helpers{1,2,3},owners{};
        std::array<std::uint32_t,3> drawReturns{1,2,3};
        std::vector<std::string> events;std::vector<unsigned> nested;
        bool changed=false;
        explicit Host(std::string value):mode(std::move(value)){}
        void* ReadProjectionHelper94ForAnalysis(void* projection) override
        {const auto id=Number(projection);Check(id>=1&&id<=3,"Known borrowed Projection");return Token(helpers[id-1]);}
        void WriteProjectionHelperOwner18ForAnalysis(void* helper,void* projection) override
        {
            const auto id=Number(helper);Check(id>=1&&id<=3,"Known borrowed FX helper");owners[id-1]=Number(projection);
            if(mode=="helper-reload"&&id==1)helpers[0]=2;
        }
        std::uint32_t InvokeProjectionHelper0ForAnalysis(void* helper) override
        {
            const auto id=Number(helper);Check(id>=1&&id<=3,"Known FX receiver");events.push_back("I"+std::to_string(id)+':'+std::to_string(owners[id-1]));
            if(!changed&&id==1)
            {
                if(mode=="init-append"){changed=true;manager->AddProjectionForAnalysis(Token(3));}
                else if(mode=="init-remove-future"){changed=true;manager->RemoveProjectionForAnalysis(Token(2));}
                else if(mode=="init-reenter"){changed=true;nested.push_back(manager->InitializeProjectionsForAnalysis(*this));}
            }
            static constexpr std::uint32_t results[]{0,2,0xFFFFFFFF};return results[id-1];
        }
        std::uint32_t DrawProjection38ForAnalysis(void* projection,void* camera) override
        {
            const auto id=Number(projection);Check(id>=1&&id<=3&&camera==Token(0x1234),"Exact foreign Projection draw arguments");events.push_back("D"+std::to_string(id));
            if(!changed&&id==1)
            {
                if(mode=="draw-append"){changed=true;manager->AddProjectionForAnalysis(Token(3));}
                else if(mode=="draw-remove-future"){changed=true;manager->RemoveProjectionForAnalysis(Token(2));}
                else if(mode=="draw-reenter"){changed=true;nested.push_back(manager->DrawProjectionsForAnalysis(camera,*this));}
                else if(mode=="draw-disable"){changed=true;manager->SetEnabledByteForAnalysis(0);}
                else if(mode=="erase-current")
                {changed=true;manager->RemoveProjectionForAnalysis(Token(1));manager->AddProjectionForAnalysis(Token(1));}
            }
            return drawReturns[id-1];
        }
    };
    std::string Members(const spProjectionManager& manager)
    {std::ostringstream s;s<<'[';bool first=true;for(auto* value:manager.GetProjectionsForAnalysis()){if(!first)s<<',';first=false;s<<Number(value);}return s.str()+']';}
    template<class T>std::string Numbers(const T& values)
    {std::ostringstream s;s<<'[';bool first=true;for(auto value:values){if(!first)s<<',';first=false;s<<value;}return s.str()+']';}
    std::string Events(const Host& host)
    {std::ostringstream s;s<<'[';bool first=true;for(const auto& value:host.events){if(!first)s<<',';first=false;s<<'"'<<value<<'"';}return s.str()+']';}
    std::string Name(const spNamedObject& object)
    {return object.GetName()?std::string("\"")+object.GetName()+"\"":"null";}
    std::string Execute(const std::string& mode)
    {
        Host host(mode);std::unique_ptr<spProjectionManager> manager;
        if(mode=="base")manager=std::make_unique<spProjectionManager>();
        else manager=std::make_unique<spPCProjectionManager>();host.manager=manager.get();
        const auto classID=manager->vfunc_18().classID;
        Check(manager->IsKindOf(spProjectionManager::ClassID)&&manager->IsKindOf(spCrossPlatform::ClassID)&&manager->IsKindOf(spNamedObject::ClassID),"Physical/RTTI base chain");
        auto add=[&](std::initializer_list<unsigned> values,spProjectionManager* target=nullptr)
        {for(auto value:values)(target?target:manager.get())->AddProjectionForAnalysis(Token(value));};
        std::optional<unsigned> phaseResult;bool completed=true;std::vector<unsigned> enabledSequence;
        std::vector<std::string> snapshots;std::string other="null";unsigned clonePairs=0;
        std::unique_ptr<spBaseObject> cloneOwner;std::unique_ptr<spPCProjectionManager> copyOwner;
        if(mode=="shared-helper")host.helpers[1]=1;
        if(mode=="missing-helper-prefix")host.helpers[1]=0;
        if(mode=="empty")
        {
            enabledSequence.push_back(manager->GetEnabledByteForAnalysis());manager->SetEnabledByteForAnalysis(0x1A5);enabledSequence.push_back(manager->GetEnabledByteForAnalysis());
            Check(manager->EnableForAnalysis(),"Enable returns literal true");enabledSequence.push_back(manager->GetEnabledByteForAnalysis());phaseResult=manager->InitializeProjectionsForAnalysis(host);
            Check(manager->DrawPhase2CForAnalysis(Token(0x1234))&&manager->DrawPhase30ForAnalysis(Token(0x1234))&&manager->DrawProjectionsForAnalysis(Token(0x1234),host),"Empty/no-op phases return true");
        }
        else if(mode=="lists")
        {
            add({1,1,2,3,0,0});snapshots.push_back(Members(*manager));manager->RemoveProjectionForAnalysis(Token(2));snapshots.push_back(Members(*manager));manager->RemoveProjectionForAnalysis(Token(2));snapshots.push_back(Members(*manager));manager->RemoveProjectionForAnalysis(nullptr);snapshots.push_back(Members(*manager));
            for(unsigned i=1;i<=3;++i)manager->RemoveProjectionForAnalysis(Token(i));Check(manager->GetProjectionsForAnalysis().empty(),"List emptied by known removes");
            manager->SetProjectionsForFixture({Token(1),Token(1),Token(2),Token(1)});snapshots.push_back(Members(*manager));manager->RemoveProjectionForAnalysis(Token(1));snapshots.push_back(Members(*manager));
        }
        else if(mode=="clone"||mode=="copy")
        {
            manager->SetName("alpha");manager->SetEnabledByteForAnalysis(0xA5);add({1,3});spCloneManager clones;spProjectionManager* destination=nullptr;
            if(mode=="clone")
            {
                cloneOwner=manager->vfunc_10(clones);destination=dynamic_cast<spProjectionManager*>(cloneOwner.get());Check(destination,"Typed concrete manager clone");clonePairs=clones.FindClone(*manager)==destination?1:0;
            }
            else
            {
                copyOwner=std::make_unique<spPCProjectionManager>();destination=copyOwner.get();destination->SetName("beta");destination->SetEnabledByteForAnalysis(0x5A);add({2},destination);
                Check(manager->vfunc_14(*destination,clones)&&manager->vfunc_14(*manager,clones),"Inherited Copy and self-Copy return true");
            }
            std::ostringstream s;s<<"{\"name\":"<<Name(*destination)<<",\"enabled\":"<<unsigned(destination->GetEnabledByteForAnalysis())<<",\"members\":"<<Members(*destination)<<",\"nameShared\":"<<(manager->GetName()==destination->GetName()?"true":"false")<<'}';other=s.str();
        }
        else
        {
            if(mode=="init-append"||mode=="draw-append"||mode=="init-reenter"||mode=="draw-reenter")add({1,2});else add({1,2,3});
            if(mode=="init-enabled")Check(manager->EnableForAnalysis(),"Enable before init");
            if(mode=="draw-high")host.drawReturns={0x10001,0xFF,0xFFFFFFFF};
            if(mode=="draw-zero")host.drawReturns={1,0,3};
            if(mode=="draw-disable")manager->SetEnabledByteForAnalysis(0xA5);
            try
            {
                if(mode=="base"){spCloneManager clones;Check(!manager->vfunc_10(clones),"Abstract base clone is null");}
                phaseResult=mode.rfind("draw-",0)==0?manager->DrawProjectionsForAnalysis(Token(0x1234),host):manager->InitializeProjectionsForAnalysis(host);
            }
            catch(const std::logic_error&){if(mode!="missing-helper-prefix")throw;completed=false;}
        }
        std::ostringstream s;s<<"{\"mode\":\""<<mode<<"\",\"classID\":"<<classID<<",\"name\":"<<Name(*manager)<<",\"enabled\":"<<unsigned(manager->GetEnabledByteForAnalysis())<<",\"members\":"<<Members(*manager)<<",\"events\":"<<Events(host)<<",\"helperOwners\":"<<Numbers(host.owners)<<",\"helperBindings\":"<<Numbers(host.helpers)<<",\"nestedResults\":"<<Numbers(host.nested)<<",\"phaseResult\":";
        if(phaseResult)s<<*phaseResult;else s<<"null";s<<",\"completed\":"<<(completed?"true":"false")<<",\"enabledSequence\":"<<Numbers(enabledSequence)<<",\"listSnapshots\":[";
        for(std::size_t i=0;i<snapshots.size();++i){if(i)s<<',';s<<snapshots[i];}
        s<<"],\"other\":"<<other<<",\"clonePairs\":"<<clonePairs<<",\"borrowedReferencesUnchanged\":true";
        const auto eventCount=host.events.size();cloneOwner.reset();copyOwner.reset();manager.reset();Check(host.events.size()==eventCount,"Manager destruction never invokes borrowed payload methods");
        if(completed)s<<",\"borrowedAliveAfterManager\":true";s<<'}';return s.str();
    }
    void TestGuards()
    {
        Host host("erase-current");spPCProjectionManager manager;host.manager=&manager;manager.AddProjectionForAnalysis(Token(1));manager.AddProjectionForAnalysis(Token(2));bool refused=false;
        try{(void)manager.DrawProjectionsForAnalysis(Token(0x1234),host);}catch(const std::logic_error&){refused=true;}
        Check(refused&&Members(manager)=="[2,1]"&&host.events==std::vector<std::string>{"D1"},"Current node removal refuses before invalid iterator increment");
        host.events.clear();Check(!manager.DrawProjectionsForAnalysis(Token(0x1234),host)&&host.events==std::vector<std::string>{"D2","D1"},"Guard unwinds active frames and leaves later valid calls usable");
        Check(!spRTTIManager::Instance().Create(spProjectionManager::ClassID),"Base has no RTTI factory");
        auto object=spRTTIManager::Instance().Create(spPCProjectionManager::ClassID);Check(dynamic_cast<spPCProjectionManager*>(object.get()),"Concrete PC RTTI factory exists");
        auto nonzero=Execute("draw-nonzero");Check(nonzero.find("\"phaseResult\":0")!=std::string::npos,"Nonzero 1/2/3 draw returns still produce bitwise zero");
        auto reload=Execute("helper-reload");Check(reload.find("I2:0")!=std::string::npos,"Owner write is followed by reloading helper94");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit")
        {std::string line;while(std::getline(std::cin,line))if(!line.empty())std::cout<<Execute(line)<<'\n';return 0;}
        TestGuards();std::cout<<"PASS PC projection manager raw list/AND/clone/reentry/guard contracts\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
