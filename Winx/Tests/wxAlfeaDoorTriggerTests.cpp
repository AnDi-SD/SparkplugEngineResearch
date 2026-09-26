#include "Code/wxAlfeaDoorTrigger.h"
#include "Analysis/PC/wxAlfeaDoorTriggerAbi.h"
#include "Analysis/PS2/wxAlfeaDoorTriggerAbi.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool v,const char* msg) { if(!v){std::cerr<<msg<<'\n';std::exit(1);} }
    std::uint32_t Bits(float v) { std::uint32_t b;std::memcpy(&b,&v,4);return b; }
    float Float(std::uint32_t b) { float v;std::memcpy(&v,&b,4);return v; }
    struct Host final : wxAlfeaDoorTriggerHost
    {
        std::string trace;
        std::uint32_t now=0;
        std::int32_t progress=0;
        bool gameFlag=false,can=true,copy=true,linked=true;
        std::array<float,3> forward{0,0,1},player{0,0,0},position{0,0,1};
        void Event(std::string s) { if(!trace.empty())trace+=';';trace+=s; }
        std::uint32_t InitialDoorIdStorageForAnalysis() override { return 0xCCCCCCCC; }
        void ConstructPivotingDoorForAnalysis(wxAlfeaDoorTrigger&,wxPivotingDoorStateForAnalysis& s) override
        { s={};s.node=this;s.enabled=1;s.duration=1000;s.targetAngle=Float(0xBFC90FDB);s.notifyPlayer=1;s.autoDisable=1;Event("construct"); }
        void DestroyPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept override { Event("destroy-base"); }
        bool CopyPivotingDoorForAnalysis(const wxAlfeaDoorTrigger& a,wxAlfeaDoorTrigger& b,spCloneManager&) const override
        { if(copy)b.BaseStateForAnalysis()=a.BaseStateForAnalysis();return copy; }
        bool CleanupPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept override { Event("cleanup-base");return false; }
        bool UpdatePivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept override { Event("update-base");return false; }
        bool CanInteractPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept override { Event("predicate-base");return can; }
        void EnterPivotingDoorForAnalysis(wxAlfeaDoorTrigger&) noexcept override { Event("enter-base"); }
        void NotifyPivotingDoorForAnalysis(wxAlfeaDoorTrigger&,const wxAlfeaDoorMessageForAnalysis& m) noexcept override { Event("notify-base:"+std::to_string(m.code)); }
        void SubscribeForAnalysis(std::uint32_t g,wxAlfeaDoorTrigger&) noexcept override { Event("subscribe:"+std::to_string(g)); }
        void UnsubscribeForAnalysis(std::uint32_t g,wxAlfeaDoorTrigger&) noexcept override { Event("unsubscribe:"+std::to_string(g)); }
        std::uint32_t GetMillisecondsForAnalysis() noexcept override { return now; }
        std::int32_t GetProgress514ForAnalysis() noexcept override { return progress; }
        bool GetGameFlagForAnalysis(std::uint32_t k) noexcept override { Check(k==25,"flag key");return gameFlag; }
        void* GetPlayerTransformForAnalysis() noexcept override { return this; }
        std::array<float,3> GetForwardForAnalysis(void*) noexcept override { return forward; }
        std::array<float,3> GetPositionForAnalysis(void*) noexcept override { return player; }
        std::array<float,3> GetNodePositionForAnalysis(void*) noexcept override { return position; }
        void NormalizeForAnalysis(std::array<float,3>& v) noexcept override
        {
            const double n=std::sqrt(double(v[0])*v[0]+double(v[1])*v[1]+double(v[2])*v[2]);
            if(n>double(0.001f)) { const double f=1.0/n;for(auto& x:v)x=static_cast<float>(x*f); }
            else v={0,0,0};
        }
        double AngleBetweenForAnalysis(const std::array<float,3>& a,const std::array<float,3>& b) noexcept override
        {
            if(std::abs(double(a[0])-b[0])<=double(0.001f)&&std::abs(double(a[1])-b[1])<=double(0.001f)&&std::abs(double(a[2])-b[2])<=double(0.001f))return 0;
            return std::acos(double(a[2])*b[2]+double(a[1])*b[1]+double(a[0])*b[0]);
        }
        void NotifyPlayerForAnalysis(wxAlfeaDoorTrigger&,std::uint32_t c,std::uint32_t a,std::uint32_t b) noexcept override
        { Event("player:"+std::to_string(c)+":"+std::to_string(a)+":"+std::to_string(b)); }
        void* FindSceneNodeForAnalysis(const char* n,bool a,bool b) noexcept override
        { Event("find:"+std::string(n)+":"+std::to_string(a)+std::to_string(b));return linked?this:nullptr; }
        void EnableNodeComponentForAnalysis(void*,std::size_t i) noexcept override { Event("component:"+std::to_string(i)); }
        void SendGroupMessageForAnalysis(wxAlfeaDoorTrigger&,std::uint32_t c,std::uint32_t g,std::uint32_t i,std::uint8_t v) noexcept override
        { Event("send:"+std::to_string(c)+":"+std::to_string(g)+":"+std::to_string(i)+":"+std::to_string(v)); }
        void SetNodeYawForAnalysis(void*,float v) noexcept override { Event("yaw:"+std::to_string(Bits(v))); }
        void MarkNodeDirtyForAnalysis(void*,std::uint32_t b) noexcept override { Event("dirty:"+std::to_string(b)); }
        void RegisterEnumPropertyForAnalysis(const char* label,bool id,const char* const* choices,std::size_t count) override
        { Check(count==(id?11u:5u),"enum count");Check(std::string(choices[0])=="FaragondaDoor","enum order");Event(std::string(label)+std::to_string(count)); }
        bool RegisterPivotingDoorPropertiesForAnalysis() override { Event("properties-base");return false; }
    };

    std::string Run(std::string line)
    {
        Host h;wxAlfeaDoorTrigger d(h,0xCCCCCCCC);h.trace.clear();auto& s=d.BaseStateForAnalysis();
        std::istringstream in(line);char op;unsigned a,b,c,e;
        while(in>>op)
        {
            switch(op)
            {
            case 'i':in>>a;d.SetDoorIdForAnalysis(a);break;
            case 'g':in>>a;d.SetDoorGroupForAnalysis(a);break;
            case 't':in>>h.now;break;
            case 'v':in>>h.progress;break;
            case 'f':in>>h.gameFlag;break;
            case 'b':in>>h.can;break;
            case 'l':in>>h.linked;break;
            case 'a':d.vfunc_58_AnimateForAnalysis();break;
            case 'u':h.Event("result:"+std::to_string(d.vfunc_48_UpdateForAnalysis()));break;
            case 'c':h.Event("result:"+std::to_string(d.vfunc_40_CleanupForAnalysis()));break;
            case 'e':d.vfunc_5C_EnterForAnalysis();break;
            case 'p':h.Event("result:"+std::to_string(d.vfunc_54_CanInteractForAnalysis()));break;
            case 'k':h.Event("result:"+std::to_string(d.SpecialGateForAnalysis()));break;
            case 'q':in>>a>>b;d.QueueOpenForAnalysis(a,b);break;
            case 'n':in>>a>>b>>c>>e;{wxAlfeaDoorMessageForAnalysis m{a,b?&d:nullptr,c,e};d.vfunc_0C(&m);}break;
            case 's':
                in>>s.phase>>a>>b>>s.duration>>s.started>>c;
                s.angle=Float(a);s.targetAngle=Float(b);s.initialAngle=Float(c);
                in>>a>>b>>c>>e;s.enabled=static_cast<std::uint8_t>(a);s.moving=static_cast<std::uint8_t>(b);s.autoDisable=static_cast<std::uint8_t>(c);s.notifyPlayer=static_cast<std::uint8_t>(e);break;
            case 'x':in>>h.forward[0]>>h.forward[1]>>h.forward[2]>>h.position[0]>>h.position[1]>>h.position[2];break;
            default:Check(false,"protocol command");
            }
            Check(!in.fail(),"protocol arguments");
        }
        const auto& f=d.GetFlagsForAnalysis();std::ostringstream out;
        out<<d.GetDoorGroupForAnalysis()<<' '<<d.GetDoorIdForAnalysis()<<' '<<unsigned(f.locked)<<' '<<unsigned(f.queued)<<' '<<unsigned(f.instant)
            <<' '<<unsigned(s.autoDisable)<<' '<<unsigned(s.enabled)<<' '<<unsigned(s.moving)<<' '<<Bits(s.angle)<<' '<<s.started<<' '<<s.phase<<' '<<unsigned(s.notifyPlayer)<<' '<<unsigned(s.interactionBlocked)<<'|'<<h.trace;
        return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==2&&std::string(argv[1])=="--protocol") {std::string line;while(std::getline(std::cin,line))std::cout<<Run(line)<<'\n';return 0;}
    Check(Run("")=="2 3435973836 0 0 0 0 1 0 0 0 0 1 0|","constructor leaves explicit ID storage");
    Check(Run("i 2 v 2 k e f 1 k e")=="2 2 0 0 0 0 1 0 0 0 0 1 0|result:1;result:0;enter-base","special gate");
    Check(Run("i 7 q 8 255 u")=="2 7 0 0 0 0 1 0 0 0 0 1 0|update-base;result:1","ID mismatch");
    Check(Run("i 7 n 10156 0 2 511")=="2 7 0 0 0 0 1 0 0 0 0 1 255|","group byte truncation");
    Check(Run("i 7 n 10158 1 7 1")=="2 7 0 0 0 0 1 0 0 0 0 1 0|","self notification");
    Check(Run("b 0 p").find("|predicate-base;result:0")!=std::string::npos,"parent interaction gate");
    Check(Run("x 0 7 -1 0 9 1 p").find("result:0")!=std::string::npos,"facing away");
    {
        Host lifecycle;
        wxAlfeaDoorTrigger door(lifecycle,7);
        auto& state=door.BaseStateForAnalysis();
        lifecycle.trace.clear();
        door.QueueOpenForAnalysis(7,0);
        Check(door.vfunc_48_UpdateForAnalysis(),"queued update succeeds despite parent false");
        Check(door.GetFlagsForAnalysis().locked==1&&door.GetFlagsForAnalysis().queued==0
            &&state.moving==1&&state.phase==1&&state.angle==0&&state.notifyPlayer==1,"queued opening begins");
        Check(lifecycle.trace=="find::10;component:0;component:1;send:10158:23:7:0;yaw:0;dirty:1;update-base",
            "queued opening callback order and suppressed player notification");
        lifecycle.now=500;door.vfunc_58_AnimateForAnalysis();
        Check(state.phase==1&&state.moving==1&&state.angle==state.targetAngle/2,"opening midpoint");
        lifecycle.now=1000;door.vfunc_58_AnimateForAnalysis();
        Check(state.phase==2&&state.moving==1&&state.angle==state.targetAngle,"opening endpoint defers completion");
        door.vfunc_58_AnimateForAnalysis();
        Check(state.phase==0&&state.moving==0&&door.GetFlagsForAnalysis().locked==1,"opening completion");
        wxAlfeaDoorMessageForAnalysis close{0x27AF,nullptr,7,0};
        door.vfunc_0C(&close);
        Check(state.phase==3&&state.moving==1&&state.started==1000,"close notification starts return");
        lifecycle.now=1500;door.vfunc_58_AnimateForAnalysis();
        Check(state.phase==3&&state.angle==state.targetAngle/2,"closing midpoint");
        lifecycle.now=2000;door.vfunc_58_AnimateForAnalysis();
        Check(state.angle==0&&state.phase==3&&state.moving==1,"closing equality still moves");
        lifecycle.now=2001;door.vfunc_58_AnimateForAnalysis();
        Check(state.angle==0&&state.phase==0&&state.moving==0&&state.enabled==1&&state.autoDisable==0,
            "closing completes only after duration and preserves enabled");
    }
    Host h;
    {
        wxAlfeaDoorTrigger d(h,7);d.QueueOpenForAnalysis(7,255);
        spCloneManager cm;auto cloned=cm.Clone(d);auto& x=static_cast<wxAlfeaDoorTrigger&>(*cloned);
        Check(x.GetFlagsForAnalysis().queued==1&&x.GetFlagsForAnalysis().instant==0,"clone excludes instant byte");
        x.QueueOpenForAnalysis(7,9);Check(d.vfunc_14(x,cm)&&x.GetFlagsForAnalysis().instant==9,"Copy preserves destination instant");
        h.copy=false;d.SetDoorGroupForAnalysis(4);Check(!d.vfunc_14(x,cm)&&x.GetDoorGroupForAnalysis()==2,"parent Copy failure");
        Check(!cm.Clone(d),"failed clone releases destination");
        Check(d.IsKindOf(0x4AC3694A)&&d.IsKindOf(spNamedObject::ClassID),"RTTI registration ancestry");
        h.trace.clear();Check(wxAlfeaDoorTrigger::RegisterPropertiesForAnalysis(h),"properties return true");
        Check(h.trace=="Door Group : 5;Door ID : 11;properties-base","property order");
        wxAlfeaDoorTrigger::SetFactoryHostForAnalysis(&h);
        auto made=spRTTIManager::Instance().Create(wxAlfeaDoorTrigger::ClassID);
        Check(made&&made->IsExactly(wxAlfeaDoorTrigger::ClassID),"RTTI factory");
    }
    wxAlfeaDoorTrigger::SetFactoryHostForAnalysis(nullptr);
    Check(h.trace.find("unsubscribe:23;cleanup-base;destroy-base")!=std::string::npos,"destruction order");
    std::cout<<"wxAlfeaDoorTrigger checks passed\n";
}
