#include "Code/wxAlphaManager.h"
#include "Analysis/PC/wxAlphaManagerAbi.h"
#include "Analysis/PS2/wxAlphaManagerAbi.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool ok,const char* text) {if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
    std::uint32_t Bits(float x) {std::uint32_t b;std::memcpy(&b,&x,4);return b;}
    struct Host final:wxAlphaManagerHost
    {
        std::uint32_t now=0,layer=7,component=0x12340040,kind=0;
        unsigned created=0,destroyed=0,released=0;
        bool materialPresent=true,nodeColorPresent=true;
        wxAlphaMaterialForAnalysis material{8,9,&layer};
        wxAlphaRenderableForAnalysis renderable{&material,6,5};
        wxAlphaNodeForAnalysis node;
        wxAlphaColorForAnalysis color08{.25f,.5f,.75f,1},color10{1,.75f,.5f,.25f},nodeColor{.5f,.25f,1,.75f};
        std::string trace;
        Host() {node.exactClassID=0x603625D0;node.renderables={&renderable};node.firstComponentFlags=&component;}
        void Event(std::string s) {if(!trace.empty())trace+=';';trace+=s;}
        wxAlphaPoolObjectForAnalysis* CreatePoolObjectForAnalysis() override {++created;return new wxAlphaPoolObjectForAnalysis;}
        void DestroyPoolObjectForAnalysis(wxAlphaPoolObjectForAnalysis* p) noexcept override {++destroyed;delete p;}
        void* CreateOverlayMaterialForAnalysis() override {return this;}
        void ReleaseOverlayMaterialForAnalysis(void*) noexcept override {++released;}
        std::uint32_t GetGameMillisecondsForAnalysis() noexcept override {return now;}
        std::uint32_t GetSystemMillisecondsForAnalysis() noexcept override {return now;}
        void NotifyFadeForAnalysis(void*,wxAlphaManager& m,std::uint32_t code,std::uint8_t v) noexcept override
        {Check(code==0x27A3&&!m.GetFadeForAnalysis().recipient,"clear recipient before notify");Event("notify:"+std::to_string(v));}
        void DrawOverlayForAnalysis(void*,float a) noexcept override {Event("draw:"+std::to_string(Bits(a)));}
        wxAlphaNodeForAnalysis& NodeForAnalysis(void*) noexcept override {return node;}
        wxAlphaRenderableForAnalysis& RenderableForAnalysis(void*) noexcept override
        {renderable.material=materialPresent?&material:nullptr;return renderable;}
        bool IsRenderableKindOfForAnalysis(void*,std::uint32_t id) noexcept override {return id==kind;}
        wxAlphaColorForAnalysis GetMaterialColorForAnalysis(wxAlphaMaterialForAnalysis&,unsigned slot) noexcept override
        {Event("get:"+std::to_string(slot));return slot==4?color08:color10;}
        void SetMaterialColorForAnalysis(wxAlphaMaterialForAnalysis&,unsigned slot,const wxAlphaColorForAnalysis& c) noexcept override
        {Event("set:"+std::to_string(slot));(slot==8?color08:color10)=c;}
        void RefreshRenderableForAnalysis(void*) noexcept override {Event("refresh");}
        void* NodeColorTargetForAnalysis(void*) noexcept override {return nodeColorPresent?this:nullptr;}
        wxAlphaColorForAnalysis GetNodeColorForAnalysis(void*) noexcept override {Event("node-get");return nodeColor;}
        void SetNodeColorForAnalysis(void*,const wxAlphaColorForAnalysis& c) noexcept override {Event("node-set");nodeColor=c;}
        wxAlphaColorForAnalysis UnpackColorForAnalysis(std::uint32_t c) noexcept override
        {wxAlphaColorForAnalysis out;const unsigned shifts[]{16,8,0,24};for(unsigned i=0;i<4;++i)out[i]=static_cast<float>(double((c>>shifts[i])&255)*(1.0f/255.0f));return out;}
        std::uint8_t ConvertColorByteForAnalysis(double v) noexcept override {return static_cast<std::uint8_t>(static_cast<std::int64_t>(v));}
        std::uint32_t UnwrittenReverseColorForAnalysis() noexcept override {return 0xA5A5A5A5;}
        void ReportFadeableNodeForAnalysis(const char* n) noexcept override {Event(std::string("fadeable:")+n);}
    };
    std::string Run(const std::string& line)
    {
        Host h;wxAlphaManager manager(h);auto& e=manager.GetEffectForAnalysis(0);
        e.node=&h.node;e.renderable=&h.renderable;
        std::istringstream in(line);char op;unsigned a,b;std::string name;
        while(in>>op)
        {
            switch(op)
            {
            case 't':in>>h.now;break;
            case 'f':in>>a>>b;if(a)manager.FadeToOpaqueForAnalysis(b,&h);else manager.FadeToTransparentForAnalysis(b,&h);break;
            case 'd':manager.DrawFadeForAnalysis();break;
            case 'n':in>>a;manager.NotifyFadeForAnalysis(static_cast<std::uint8_t>(a));break;
            case 's':in>>e.mode>>a>>e.interpolationStarted>>e.lifetimeStarted>>e.from>>e.to>>e.duration>>e.lifetime;e.active=static_cast<std::uint8_t>(a);e.current=e.from;break;
            case 'u':h.Event("result:"+std::to_string(manager.UpdateEffectForAnalysis(&e)));break;
            case 'r':h.Event("result:"+std::to_string(manager.RestoreEffectForAnalysis(&e)));break;
            case 'm':in>>h.materialPresent;break;
            case 'k':in>>h.kind;break;
            case 'z':manager.ResetForAnalysis();break;
            case 'a':{std::array<float,3> v;in>>v[0]>>v[1]>>v[2]>>a>>b;manager.ApplyToNodeForAnalysis(&h.node,v,a,b);break;}
            case 'c':in>>name;h.node.name=name.c_str();manager.ClassifyRecursiveForAnalysis(&h.node);break;
            case 'p':in>>a;for(unsigned i=0;i<a;++i){auto* p=manager.AcquirePoolObjectForAnalysis();p->flags04=static_cast<std::uint16_t>(i);}break;
            default:Check(false,"protocol command");
            }
            Check(!in.fail(),"protocol arguments");
        }
        const auto& f=manager.GetFadeForAnalysis();std::ostringstream out;
        out<<unsigned(f.enabled)<<' '<<unsigned(f.notified)<<' '<<Bits(f.from)<<' '<<Bits(f.to)<<' '<<f.started<<' '<<f.duration<<' '<<bool(f.recipient);
        out<<' '<<e.mode<<' '<<e.active+0<<' '<<e.saved+0<<' '<<e.interpolationStarted<<' '<<e.lifetimeStarted<<' '<<e.from<<' '<<e.to<<' '<<e.current<<' '<<e.duration<<' '<<e.lifetime;
        out<<' '<<h.material.field38<<' '<<h.material.field34<<' '<<h.layer<<' '<<h.renderable.field1C<<' '<<h.renderable.field18+0<<' '<<h.component;
        for(const auto* color:{&h.color08,&h.color10,&h.nodeColor})for(auto v:*color)out<<' '<<Bits(v);
        out<<' '<<(h.node.data0C?h.node.data0C->flags04:0)<<'|'<<h.trace;
        return out.str();
    }
}
int main(int argc,char** argv)
{
    if(argc==2&&std::string(argv[1])=="--protocol") {std::string s;while(std::getline(std::cin,s))std::cout<<Run(s)<<'\n';return 0;}
    Host h;
    {
        wxAlphaManager m(h);Check(h.created==150,"pool allocation count");
        auto* first=m.AcquirePoolObjectForAnalysis();wxAlphaPoolObjectForAnalysis* last=first;
        for(unsigned i=1;i<150;++i)last=m.AcquirePoolObjectForAnalysis();
        Check(m.AcquirePoolObjectForAnalysis()==last,"full pool returns last occupied record");
        first->flags04=3;first->node10=&h;first->field08=&h;first->field2C=1;
        m.ResetForAnalysis();Check(m.AcquirePoolObjectForAnalysis()==first&&first->flags04==3&&first->node10==&h&&!first->field08&&!first->field2C,"reset preserves unrelated fields");
        m.FadeToOpaqueForAnalysis(1000,&h);h.now=250;m.DrawFadeForAnalysis();
        Check(h.trace=="draw:0","fade first quarter holds");
        h.now=1000;m.DrawFadeForAnalysis();Check(!m.GetFadeForAnalysis().notified,"fade equality delays completion");
        h.now=1001;m.DrawFadeForAnalysis();Check(m.GetFadeForAnalysis().enabled&&m.GetFadeForAnalysis().notified,"opaque fade stays enabled");
        Check(h.trace.find("notify:0;draw:1065353216")!=std::string::npos,"opaque completion value is zero");
        m.FadeToTransparentForAnalysis(0,&h);m.DrawFadeForAnalysis();Check(!m.GetFadeForAnalysis().enabled,"zero duration transparent completes");
        Check(h.trace.find("notify:1;draw:0")!=std::string::npos,"transparent completion value is one");
        h.node.renderables={&h.renderable,&h};
        m.ApplyToNodeForAnalysis(&h.node,{1,0,0},1000,0);
        Check(m.GetEffectForAnalysis(0).renderable==&h&&!m.GetEffectForAnalysis(1).active,"native record reused across renderables");
        spCloneManager cm;auto clone=cm.Clone(m);auto& c=static_cast<wxAlphaManager&>(*clone);
        Check(!c.GetEffectForAnalysis(0).active&&!c.GetFadeForAnalysis().enabled,"inherited Copy leaves clone defaults");
        Check(wxAlphaManager::GetInstanceForAnalysis()==&c,"clone overwrites singleton");
        wxAlphaManager::SetFactoryHostForAnalysis(&h);
        auto made=spRTTIManager::Instance().Create(wxAlphaManager::ClassID);
        Check(made&&made->IsKindOf(spBaseObject::ClassID),"RTTI factory");
    }
    wxAlphaManager::SetFactoryHostForAnalysis(nullptr);
    Check(h.created==h.destroyed&&h.released==3&&!wxAlphaManager::GetInstanceForAnalysis(),"owned resources released");
    {
        Host scene;wxAlphaManager m(scene);
        const auto before08=scene.color08,before10=scene.color10,beforeNode=scene.nodeColor;
        m.FlashRecursiveForAnalysis(&scene.node,1000,{220,20,60});
        scene.now=500;m.UpdateEffectsForAnalysis();
        Check(m.GetEffectForAnalysis(0).saved&&scene.material.field38==4&&scene.color08!=before08,"flash applies and saves original material");
        Check(scene.material.field34==9&&scene.layer==7&&scene.renderable.field1C==6,"flash preserves non-color modes");
        scene.now=1000;m.UpdateEffectsForAnalysis();
        Check(m.GetEffectForAnalysis(0).active,"flash equality still active");
        scene.now=1001;m.UpdateEffectsForAnalysis();
        Check(!m.GetEffectForAnalysis(0).active&&!m.GetEffectForAnalysis(0).saved,"flash expires and releases record");
        Check(scene.color08==before08&&scene.color10==before10&&scene.nodeColor==beforeNode
            &&scene.material.field38==8&&scene.material.field34==9&&scene.layer==7
            &&scene.renderable.field1C==6&&scene.renderable.field18==5,"flash restores full external state");
    }
    Check(Run("s 1 1 0 0 4294967295 16777215 1000 0 t 500 u r").find("set:16;set:8;node-set;refresh;result:1")!=std::string::npos,"effect restores material and node colors");
    std::cout<<"wxAlphaManager checks passed\n";
}
