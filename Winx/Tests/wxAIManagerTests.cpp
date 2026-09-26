#include "Code/wxAIManager.h"
#include "Analysis/PC/wxAIManagerAbi.h"
#include "Analysis/PS2/wxAIManagerAbi.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Require(bool ok, const char* why)
    { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
    void* Ptr(unsigned id) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)); }
    unsigned Id(void* p) { return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(p)); }

    struct Host final : wxAIManagerHost
    {
        std::string trace;
        unsigned level = 0, mask = ~0u, playerCounter = 3, removeOnExecute = 0;
        std::int32_t progress = 0;
        bool blocked = false, flag79 = false;
        std::array<float, 3> position{107,1075,764};
        wxAIManager* manager = nullptr;
        void Event(const std::string& s) { if (!trace.empty()) trace += ';'; trace += s; }
        void SubscribeForAnalysis(std::uint32_t g, wxAIManager&) noexcept override { Event("subscribe:"+std::to_string(g)); }
        void UnsubscribeForAnalysis(std::uint32_t g, wxAIManager&) noexcept override { Event("unsubscribe:"+std::to_string(g)); }
        void DestroyMemberForAnalysis(void* p) noexcept override { Event("destroy:"+std::to_string(Id(p))); }
        bool QueryMemberForAnalysis(void* p, std::byte*) noexcept override
        { Event("query:"+std::to_string(Id(p))); return (mask & (1u << Id(p))) != 0; }
        void ExecuteMemberForAnalysis(void* p) noexcept override
        {
            Event("execute:"+std::to_string(Id(p)));
            if (removeOnExecute) { manager->RemoveMemberForAnalysis(Ptr(removeOnExecute)); removeOnExecute = 0; }
        }
        void SetMemberFlagForAnalysis(void* p, std::uint32_t v) noexcept override
        { Event("flag:"+std::to_string(Id(p))+":"+std::to_string(v)); }
        bool IsUpdateBlockedForAnalysis() noexcept override { return blocked; }
        std::uint32_t GetLevelForAnalysis() noexcept override { return level; }
        std::int32_t GetProgress514ForAnalysis() noexcept override { return progress; }
        bool GetGameFlagForAnalysis(std::uint32_t key) noexcept override { Require(key==0x79,"key"); return flag79; }
        void SetGameByte2CB5ForAnalysis(bool v) noexcept override { Event("gamebyte:"+std::to_string(v)); }
        void CallGame2798A0ForAnalysis(std::uint32_t a, bool b) noexcept override
        { Event("gamecall:"+std::to_string(a)+":"+std::to_string(b)); }
        std::array<float,3> GetPlayerPositionForAnalysis() noexcept override { return position; }
        void* FindSceneNodeForAnalysis(const char* name, bool a, bool b) noexcept override
        { Event(std::string("find:")+name+":"+std::to_string(a)+std::to_string(b)); return Ptr(100); }
        void* FindChildForAnalysis(void*, const char* name, bool a, bool b) noexcept override
        { Event(std::string("child:")+name+":"+std::to_string(a)+std::to_string(b)); return Ptr(101); }
        void SetNodeEnabledForAnalysis(void*, bool a, bool b) noexcept override
        { Event("enabled:"+std::to_string(a)+":"+std::to_string(b)); }
        void* FirstNodeObjectForAnalysis(void*) noexcept override { return Ptr(102); }
        void SetCollisionByteForAnalysis(void*, std::uint8_t v) noexcept override { Event("collision:"+std::to_string(v)); }
        std::array<float,3> GetNodePositionForAnalysis(void*) noexcept override { return {1,2,3}; }
        std::array<float,9> GetNodeRotationForAnalysis(void*) noexcept override { return {1,0,0,0,1,0,0,0,1}; }
        void MovePlayerForAnalysis(const std::array<float,3>& p) noexcept override
        { Require(p == std::array<float,3>{1,2,3},"respawn position"); position = p; Event("move"); }
        void SetPlayerRotationForAnalysis(const std::array<float,9>& r) noexcept override
        { Require(r == std::array<float,9>{1,0,0,0,1,0,0,0,1},"respawn rotation"); Event("rotate"); }
        void ResetCameraForAnalysis() noexcept override { Event("camera"); }
        void ScheduleFallForAnalysis(std::uint32_t ms, wxAIManager&) noexcept override { Event("fall:"+std::to_string(ms)); }
        void ScheduleRecoveryForAnalysis(std::uint32_t ms, wxAIManager&) noexcept override { Event("recover:"+std::to_string(ms)); }
        std::uint32_t GetPlayerCounterForAnalysis() noexcept override { return playerCounter; }
        void SetPlayerCounterForAnalysis(std::uint32_t v) noexcept override { playerCounter=v; Event("counter:"+std::to_string(v)); }
        void SendMessageForAnalysis(wxAIManager&, std::uint32_t c, std::uint32_t t, const char* n, std::uint32_t v) noexcept override
        { Event("send:"+std::to_string(c)+":"+std::to_string(t)+":"+n+":"+std::to_string(v)); }
        void SendNumericMessageForAnalysis(wxAIManager&, std::uint32_t c, std::uint32_t t, std::uint32_t n, std::uint32_t v) noexcept override
        { Event("send:"+std::to_string(c)+":"+std::to_string(t)+":"+std::to_string(n)+":"+std::to_string(v)); }
    };

    std::string Run(const std::string& line)
    {
        Host h;
        wxAIManager m(h); h.manager=&m; h.trace.clear();
        std::istringstream in(line);
        char op;
        unsigned v, w;
        while (in >> op)
        {
            switch (op)
            {
            case 'a': in>>v; m.AddMemberForAnalysis(Ptr(v)); break;
            case 'r': in>>v; m.RemoveMemberForAnalysis(Ptr(v)); break;
            case 'p': m.ProcessThreeForAnalysis(); break;
            case 'u': h.Event("result:"+std::to_string(m.UpdateForAnalysis())); break;
            case 'n': in>>v>>w; { wxAIManagerMessageForAnalysis msg{v,w}; m.vfunc_0C(&msg); } break;
            case 'd': m.CountDoorPair03And05ForAnalysis(); break;
            case 'e': m.CountDoorPair01And06ForAnalysis(); break;
            case 'f': m.CountLevel21ForAnalysis(); break;
            case 'g': m.CountLevel18ForAnalysis(); break;
            case 'k': in>>v; m.SetBattleCageForAnalysis(v!=0); break;
            case 'c': m.ClearForAnalysis(); break;
            case 'l': in>>h.level; break;
            case 'q': in>>h.mask; break;
            case 'b': in>>h.blocked; break;
            case 's': in>>h.flag79; break;
            case 't': in>>h.progress; break;
            case 'x': in>>h.position[0]>>h.position[1]>>h.position[2]; break;
            case 'm': in>>h.removeOnExecute; break;
            default: throw std::logic_error("unknown operation");
            }
            Require(!in.fail(),"bad protocol");
        }
        const auto& s=m.GetStateForAnalysis();
        std::ostringstream out;
        out<<m.GetMemberCountForAnalysis()<<' '<<m.GetCursor20ForAnalysis()<<' '<<m.GetCursor24ForAnalysis()
            <<' '<<s.counter28<<' '<<s.counter2C<<' '<<s.triggered30<<' '<<s.counter34<<' '<<s.counter38
            <<' '<<Id(s.respawnPoint)<<' '<<s.pending44<<'|'<<h.trace;
        return out.str();
    }
}

int main(int argc, char** argv)
{
    if (argc==2 && std::string(argv[1])=="--protocol")
    { std::string line; while(std::getline(std::cin,line)) std::cout<<Run(line)<<'\n'; return 0; }

    Require(Run("a 1 a 2 a 3 a 4 a 5 p p") ==
        "5 5 1 0 0 0 0 0 0 0|query:1;execute:1;query:2;execute:2;query:3;execute:3;query:4;execute:4;query:5;execute:5;query:1;execute:1", "round robin");
    Require(Run("a 1 a 1 a 2 q 0 p r 1 n 10166 256") ==
        "1 1 1 0 0 0 0 0 0 0|query:1;query:2;flag:2:256", "duplicate/detach/raw PC flag");
    Require(Run("a 1 a 2 a 3 a 4 m 2 p") ==
        "3 3 3 0 0 0 0 0 0 0|query:1;execute:1;query:3;execute:3;query:4;execute:4", "callback removes next cursor");
    Require(Run("l 6 n 10002 6 x 1 -241 3 u u n 10147 0 n 10147 1").find(
        "fall:200;result:1;result:1;move;rotate;camera;recover:1500;counter:2;send:10006:4:4:0") != std::string::npos, "fall lifecycle");
    Require(Run("a 1 a 2 r 1 c")=="0 0 0 0 0 0 0 0 0 0|destroy:2;unsubscribe:11", "ownership");
    Require(Run("d d") == "0 0 0 2 0 0 0 0 0 0|send:10035:6:door_03:0;send:10035:6:door_05:0;send:10038:6:door_03:0;send:10038:6:door_05:0;send:10034:6:door_03:0;send:10034:6:door_05:0", "door message order");
    Require(Run("l 18 g n 10002 7") == "0 0 0 0 0 0 0 1 0 0|", "level reset preserves counter38");
    Require(Run("l 14 x 257 1075 764 u") == "0 0 0 0 0 0 0 0 0 0|result:1", "strict distance threshold");
    Require(Run("l 6 x 0 -240 0 u") == "0 0 0 0 0 0 0 0 0 0|result:1", "strict fall threshold");
    Require(Run("a 1 l 14 b 1 u") == "1 1 1 0 0 0 0 0 0 0|result:0", "blocked update has no side effects");
    Require(Run("l 30 k 1") == "0 0 0 0 0 0 0 0 0 0|find:battle_cage:11;enabled:1:1;child:collision:10;collision:1", "cage external call order");
    Host h;
    {
        wxAIManager closed(h); closed.ClearForAnalysis();
        bool rejected=false;
        try { closed.AddMemberForAnalysis(Ptr(1)); }
        catch(const std::logic_error&) { rejected=true; }
        Require(rejected,"native stale-cursor state is not silently repaired for reuse");
    }
    wxAIManager::SetFactoryHostForAnalysis(&h);
    {
        wxAIManager source(h); source.AddMemberForAnalysis(Ptr(1));
        source.CountDoorPair03And05ForAnalysis();
        spCloneManager cm;
        auto cloned=cm.Clone(source);
        auto& clone=static_cast<wxAIManager&>(*cloned);
        Require(clone.IsExactly(wxAIManager::ClassID) && clone.IsKindOf(spBaseObject::ClassID),"RTTI");
        Require(clone.GetMemberCountForAnalysis()==0 && clone.GetStateForAnalysis().counter28==0,"clone defaults");
        clone.AddMemberForAnalysis(Ptr(2));
        Require(source.vfunc_14(clone,cm) && clone.GetMemberCountForAnalysis()==1,"inherited Copy preserves destination");
        Require(wxAIManager::GetInstanceForAnalysis()==&clone,"clone replaces singleton");
        cloned.reset();
        Require(wxAIManager::GetInstanceForAnalysis()==nullptr,"destructor clears singleton");
        auto factory=spRTTIManager::Instance().Create(wxAIManager::ClassID);
        Require(factory && factory->IsExactly(wxAIManager::ClassID),"factory");
    }
    wxAIManager::SetFactoryHostForAnalysis(nullptr);
    bool rejected=false;
    try { (void)spRTTIManager::Instance().Create(wxAIManager::ClassID); }
    catch(const std::logic_error&) { rejected=true; }
    Require(rejected,"factory requires external context");
    std::cout<<"wxAIManager checks passed\n";
}
