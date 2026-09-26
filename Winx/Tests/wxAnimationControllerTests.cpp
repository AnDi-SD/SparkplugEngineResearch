#include "Code/wxAnimationController.h"
#include "Analysis/PC/wxAnimationControllerAbi.h"
#include "Analysis/PS2/wxAnimationControllerAbi.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool ok,const char* msg) {if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
    const spAnimation* Anim(std::uintptr_t x) {return reinterpret_cast<const spAnimation*>(x);}
    std::uintptr_t Id(const void* p) {return reinterpret_cast<std::uintptr_t>(p);}
    std::uint32_t Bits(float v) {std::uint32_t b;std::memcpy(&b,&v,4);return b;}
    struct Host final:wxAnimationControllerHost
    {
        wxAnimationController* controller=nullptr;
        wxAnimationActorFlagsForAnalysis flags{0,0xA5};
        std::uint32_t entityFlags=0,startResult=~0u;
        bool characterField=false,copy=true,found=true,reenterStop=false,switchCharacter=false,audio=true;
        float field54=2,field34=.5f,speed=1;
        unsigned creates=0,destroys=0;
        std::string trace;
        void Event(std::string s) {if(!trace.empty())trace+=';';trace+=s;}
        void ConstructEntityForAnalysis(wxAnimationController& c,bool value) override
        {Check(value,"base ctor true");controller=&c;c.GetStateForAnalysis().entity=this;Event("construct");}
        void DestroyEntityForAnalysis(wxAnimationController&) noexcept override {Event("destroy-base");}
        bool CopyEntityForAnalysis(const wxAnimationController&,wxAnimationController&,spCloneManager&) const override {return copy;}
        void* CreateActorForAnalysis() override {++creates;Event("create");return this;}
        void DestroyActorForAnalysis(void*) noexcept override {++destroys;Event("destroy");}
        void StopAllForAnalysis(void*) noexcept override {Event("stop-all");}
        void BindActorForAnalysis(void*,void* root) override {Check(root==this,"root");Event("bind");}
        void AddActorObserverForAnalysis(void*,wxAnimationController&) override {Event("observe");}
        void* GetEntityRootForAnalysis(void*) noexcept override {return this;}
        std::uint32_t GetEntityFlagsForAnalysis(void*) noexcept override {return entityFlags;}
        void* FindCharacterForAnalysis(void*,std::uint32_t id) override {Check(id==0x3CC73,"character class");Event("find-character");return reinterpret_cast<void*>(1);}
        bool CharacterField148NonzeroForAnalysis(void*) noexcept override {return characterField;}
        wxAnimationActorFlagsForAnalysis& ActorFlagsForAnalysis(void*) noexcept override {return flags;}
        std::uint32_t StartActorForAnalysis(void*,Request& r) override
        {Event("start:"+std::to_string(Id(r.animation))+":"+std::to_string(r.mode)+":"+std::to_string(r.fadeMode)+":"+std::to_string(r.reverse)+":"+std::to_string(Bits(r.initialTime)));r.weight=.75f;return startResult;}
        void StopActorForAnalysis(void*,const Animation* a,bool suppress) override
        {Check(!suppress,"unsuppressed stop");Event("stop:"+std::to_string(Id(a)));if(reenterStop){auto& s=controller->GetStateForAnalysis();s.recent=Anim(42);s.count=5;}}
        void FadeStopActorForAnalysis(void*,const Animation* a,float duration,float fallback) override
        {Check(fallback==0,"zero fallback");Event("fade:"+std::to_string(Id(a))+":"+std::to_string(Bits(duration)));}
        bool FindPlaybackTimesForAnalysis(void*,const Animation* a,float& x,float& y) override
        {Event("find:"+std::to_string(Id(a)));x=field54;y=field34;return found;}
        void SetActorTimeMultiplierForAnalysis(void*,float v) override {speed=v;}
        void ForwardForAnalysis(wxAnimationController&,const wxAnimationMessageForAnalysis& m) noexcept override {Event("forward:"+std::to_string(Id(m.animation1C)));}
        void DispatchStateTagForAnalysis(void* c,const wxAnimationMessageForAnalysis&) noexcept override
        {Event("state:"+std::to_string(Id(c)));if(switchCharacter)controller->GetStateForAnalysis().character=reinterpret_cast<void*>(2);}
        void* GetAudioEmitterForAnalysis(void* c) noexcept override {return audio?c:nullptr;}
        std::uint8_t GetAudioVariantForAnalysis(void* c) noexcept override {return Id(c)==2?9:5;}
        void DispatchAudioTagForAnalysis(void* a,const wxAnimationMessageForAnalysis&,std::uint8_t v) noexcept override
        {Event("audio:"+std::to_string(Id(a))+":"+std::to_string(v));}
    };
    std::string Run(const std::string& text)
    {
        Host h;wxAnimationController c(h);auto& s=c.GetStateForAnalysis();s.actor=&h;h.trace.clear();
        std::istringstream in(text);char op;unsigned a,b,d,e;
        while(in>>op)
        {
            switch(op)
            {
            case 's':in>>a>>b>>s.count>>d>>e;s.old=Anim(a);s.recent=Anim(b);s.mark9=Anim(d);s.mark3=Anim(e);break;
            case 'a':in>>a>>b>>d>>e;h.Event("result:"+std::to_string(c.StartForAnalysis(Anim(a),b,d,static_cast<std::uint8_t>(e))));break;
            case 'n':in>>a>>b;{wxAnimationMessageForAnalysis m{a,nullptr,Anim(b)};c.vfunc_0C(&m);}break;
            case 'p':in>>a>>b;h.Event("result:"+std::to_string(c.HasMarkedForAnalysis(Anim(a),static_cast<std::uint8_t>(b))));break;
            case 'c':in>>a;c.ClearMarksForAnalysis(Anim(a));break;
            case 'q':in>>a;c.StopForAnalysis(Anim(a));break;
            case 'f':{float v;in>>a>>v;c.FadeStopForAnalysis(Anim(a),v);break;}
            case 'r':in>>a;c.RestartReverseForAnalysis(Anim(a));break;
            case 'l':in>>h.found>>h.field54>>h.field34;break;
            case 't':{float v;in>>v;c.SetTimeMultiplierForAnalysis(v);break;}
            case 'g':in>>h.entityFlags>>a>>h.characterField>>b>>d>>e;s.character=reinterpret_cast<void*>(std::uintptr_t(a));s.forceEnable=static_cast<std::uint8_t>(b);h.flags.enabled1C=static_cast<std::uint8_t>(d);h.flags.changed24=static_cast<std::uint8_t>(e);break;
            case 'u':h.Event("result:"+std::to_string(c.vfunc_34_UpdateActorFlagsForAnalysis()));break;
            case 'j':in>>h.reenterStop>>h.switchCharacter>>h.audio;break;
            case 'x':c.StopAllForAnalysis();break;
            case 'z':c.ReleaseActorForAnalysis();break;
            case 'i':c.InitializeForAnalysis();break;
            case 'v':in>>h.startResult;break;
            default:Check(false,"protocol operation");
            }
            Check(!in.fail(),"protocol arguments");
        }
        std::ostringstream out;
        out<<bool(s.actor)<<' '<<Id(s.character)<<' '<<Id(s.old)<<' '<<Id(s.recent)<<' '<<s.count<<' '<<Id(s.mark9)<<' '<<Id(s.mark3)<<' '<<unsigned(s.forceEnable)
            <<' '<<Id(s.request.animation)<<' '<<s.request.mode<<' '<<s.request.reverse<<' '<<s.request.fadeMode<<' '<<Bits(s.request.weight)<<' '<<Bits(s.request.initialTime)
            <<' '<<unsigned(h.flags.enabled1C)<<' '<<unsigned(h.flags.changed24)<<' '<<Bits(h.speed)<<'|'<<h.trace;
        return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==2&&std::string(argv[1])=="--protocol"){std::string s;while(std::getline(std::cin,s))std::cout<<Run(s)<<'\n';return 0;}
    Check(Run("s 7 7 1 0 0 n 3 7").find("1 0 0 7 0 0 7 ")==0,"only old cleared when handles equal");
    Check(Run("n 9 0").find("4294967295")!=std::string::npos,"zero handle decrements zero count");
    Check(Run("s 1 2 0 0 0 j 1 0 1 a 3 2 4 1").find("1 0 42 3 6")==0,"history re-read after stop callback");
    Check(Run("g 0 1 0 0 0 165 j 0 1 1 n 11 0").find("|state:1;audio:2:9")!=std::string::npos,"tag state callback precedes re-read and audio");
    Host h;
    {
        wxAnimationController c(h);Check(!c.GetStateForAnalysis().actor,"constructor defers actor");
        c.InitializeForAnalysis();c.InitializeForAnalysis();Check(h.creates==1,"idempotent actor creation");
        Check(h.trace=="construct;create;bind;observe;find-character;find-character","binding and subscription order");
        spCloneManager cm;auto cloned=cm.Clone(c);auto& copy=static_cast<wxAnimationController&>(*cloned);
        Check(!copy.GetStateForAnalysis().actor&&copy.GetStateForAnalysis().forceEnable==1,"clone keeps fresh own defaults");
        copy.InitializeForAnalysis();h.trace.clear();h.copy=false;
        Check(!c.vfunc_14(copy,cm)&&copy.GetStateForAnalysis().actor,"failed base Copy retains actor");
        h.copy=true;Check(c.vfunc_14(copy,cm)&&!copy.GetStateForAnalysis().actor&&h.trace=="destroy","successful Copy destroys without StopAll");
        wxAnimationController::SetFactoryHostForAnalysis(&h);
        auto made=spRTTIManager::Instance().Create(wxAnimationController::ClassID);
        Check(made&&made->IsKindOf(0x796A1869),"RTTI factory and ancestry");
    }
    wxAnimationController::SetFactoryHostForAnalysis(nullptr);
    Check(h.creates==h.destroys,"owned actors balanced");
    Check(h.trace.find("stop-all;destroy;destroy-base")!=std::string::npos,"destructor order");
    std::cout<<"wxAnimationController checks passed\n";
}
