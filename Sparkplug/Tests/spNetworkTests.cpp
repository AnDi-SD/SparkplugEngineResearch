#include "Code/Sparkplug/spNetwork.h"
#include "Analysis/PC/spNetworkAbi.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    // Test-only concrete shell. Original pure network methods have no base
    // implementation. Any accidental call fails instead of simulating a socket.
    class NetworkProbe final : public spNetwork
    {
    public:
        static int alive;
        NetworkProbe(){++alive;}
        ~NetworkProbe() override{--alive;}
        void SetFields(std::uint8_t connected,std::uint32_t type,std::uint32_t address,std::uint16_t port)
        {connected_=connected;type_=type;address_=address;port_=port;}
        bool OpenForAnalysis(std::uint32_t) override{throw std::logic_error("Pure network method");}
        bool ConnectForAnalysis(std::uint32_t,std::uint16_t) override{throw std::logic_error("Pure network method");}
        bool BindForAnalysis(std::uint32_t,std::uint16_t) override{throw std::logic_error("Pure network method");}
        bool ListenForAnalysis() override{throw std::logic_error("Pure network method");}
        spNetworkAcceptForAnalysis AcceptForAnalysis() override{throw std::logic_error("Pure network method");}
        bool CloseForAnalysis() override{throw std::logic_error("Pure network method");}
        bool SetBlockingForAnalysis(std::uint8_t) override{throw std::logic_error("Pure network method");}
        bool AddToSetForAnalysis(std::uint32_t) override{throw std::logic_error("Pure network method");}
        bool RemoveFromSetForAnalysis(std::uint32_t) override{throw std::logic_error("Pure network method");}
        bool ClearSetForAnalysis(std::uint32_t) override{throw std::logic_error("Pure network method");}
        bool SelectForAnalysis(std::uint32_t) override{throw std::logic_error("Pure network method");}
        int SendForAnalysis(const void*,int) override{throw std::logic_error("Pure network method");}
        int SendToForAnalysis(std::uint32_t,std::uint16_t,const void*,int) override{throw std::logic_error("Pure network method");}
        int ReceiveForAnalysis(void*,int) override{throw std::logic_error("Pure network method");}
        int ReceiveFromForAnalysis(std::uint32_t,std::uint32_t,void*,int) override{throw std::logic_error("Pure network method");}
        bool GetLocalAddressForAnalysis(std::uint32_t&) override{throw std::logic_error("Pure network method");}
    };
    int NetworkProbe::alive=0;
    static_assert(std::is_abstract_v<spNetwork>);
    static_assert(!std::is_abstract_v<NetworkProbe>); // +40 inherits its actual null implementation.
    static_assert(std::is_base_of_v<spCrossPlatform,spNetwork>);

    void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
    std::string Snapshot(const spNetwork& n)
    {
        std::ostringstream s;s<<'['<<unsigned(n.IsConnectedForAnalysis())<<','<<n.GetTypeForAnalysis()<<','<<n.GetAddressForAnalysis()<<','<<n.GetPortForAnalysis()<<',';
        if(n.GetName())s<<'"'<<n.GetName()<<'"';else s<<"null";s<<']';return s.str();
    }
    const char* Modes[]={"defaults","nulls","rtti","notify","copy-name","copy-empty","copy-replace","copy-clear","copy-self","copy-self-empty","copy-shared","lifetime-secondary","lifetime-primary","lifetime-source-first"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"Unknown network base case");
        auto source=std::make_unique<NetworkProbe>();auto target=std::make_unique<NetworkProbe>();
        std::vector<std::string> states;std::vector<std::uint32_t> results;spCloneManager manager;
        auto State=[&](const spNetwork& n){states.push_back(Snapshot(n));};
        auto Result=[&](auto value){results.push_back(static_cast<std::uint32_t>(value));};
        if(mode=="defaults"){State(*source);Check(Snapshot(*source)=="[0,1,4294967295,65535,null]","native constructor defaults");}
        else if(mode=="nulls")
        {
            source->SetName("network");source->SetFields(255,0x12345678,0xabcdef01,0x5678);
            Result(source->GetLocalAddressTextForAnalysis()==nullptr);Result(source->Clone()==nullptr);
            Check(manager.FindClone(*source)==nullptr,"base null clone must not map a source");State(*source);
        }
        else if(mode=="rtti")
        {
            for(const auto id:{spNetwork::ClassID,spCrossPlatform::ClassID,spNamedObject::ClassID,spBaseObject::ClassID,0x41fb6c73u,0u,0xffffffffu})
            {Result(id);Result(source->IsExactly(id));Result(source->IsKindOf(id));}
            const auto& record=source->vfunc_18();Result(record.classID);Result(record.baseClassID);Result(record.factory==nullptr);Result(record.propertyRegistrar==nullptr);
            Check(spRTTIManager::Instance().Create(spNetwork::ClassID)==nullptr,"native network registration has no factory");State(*source);
        }
        else if(mode=="notify")
        {
            source->SetName("network");source->SetFields(255,0x12345678,0xabcdef01,0x5678);
            source->vfunc_0C(nullptr);source->vfunc_0C(source.get());State(*source);
        }
        else
        {
            const bool hasName=mode!="copy-empty"&&mode!="copy-clear"&&mode!="copy-self-empty";
            if(hasName)source->SetName("network");
            if(mode=="copy-replace"||mode=="copy-clear")target->SetName("old-network");
            source->SetFields(255,0x12345678,0xabcdef01,0x5678);
            target->SetFields(127,0xfedcba98,0x76543210,0x4321);
            if(mode=="copy-self"||mode=="copy-self-empty")
            {
                Result(source->vfunc_14(*source,manager));State(*source);
                Check(source->GetName()==nullptr,"native self-copy drops the name");
                Check(source->IsConnectedForAnalysis()==255&&source->GetTypeForAnalysis()==0x12345678&&source->GetAddressForAnalysis()==0xabcdef01&&source->GetPortForAnalysis()==0x5678,"native self-copy preserves network fields");
            }
            else
            {
                Result(source->vfunc_14(*target,manager));State(*source);State(*target);
                Check(target->GetName()==source->GetName(),"copy retains exactly the same shared name");
                Check(target->IsConnectedForAnalysis()==127&&target->GetTypeForAnalysis()==0xfedcba98&&target->GetAddressForAnalysis()==0x76543210&&target->GetPortForAnalysis()==0x4321,"copy preserves destination network fields");
                if(mode=="copy-shared"){Result(source->vfunc_14(*target,manager));State(*target);}
            }
            if(mode=="lifetime-source-first"){source.reset();State(*target);Check(target->GetName()!=nullptr,"name survives source lifetime");}
            else if(mode=="lifetime-secondary")
            {std::unique_ptr<spBaseObject> owner(std::move(source));owner.reset();State(*target);}
            else if(mode=="lifetime-primary")
            {std::unique_ptr<spNetworkInterfaceForAnalysis> owner(std::move(source));owner.reset();State(*target);}
        }
        source.reset();target.reset();Check(NetworkProbe::alive==0,"complete network shell deletion");
        std::ostringstream out;out<<"{\"results\":[";
        for(unsigned i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"],\"states\":[";
        for(unsigned i=0;i<states.size();++i){if(i)out<<',';out<<states[i];}out<<"]}";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const char* mode:Modes)(void)Run(mode);
        std::cout<<"PASS "<<std::size(Modes)<<" independent network base cases\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
