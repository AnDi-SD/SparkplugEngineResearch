#include "wxAlphaManager.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAlphaManagerHost* factoryHost = nullptr;
        wxAlphaManager* instance = nullptr;
        std::unique_ptr<spBaseObject> CreateManager()
        {
            if (!factoryHost) throw std::logic_error("wxAlphaManager requires a factory host");
            return std::make_unique<wxAlphaManager>(*factoryHost);
        }
        const spRTTIRecord record{wxAlphaManager::ClassID, spBaseObject::ClassID,
            "wxAlphaManager", &spBaseObject::StaticRTTI(), &CreateManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool Has(const char* s, const char* token) { return s && std::strstr(s,token); }
    }
    wxAlphaManager::wxAlphaManager(wxAlphaManagerHost& host) : host_(host)
    {
        instance = this;
        try
        {
            pool_.reserve(PoolCount);
            for (std::size_t i=0;i<PoolCount;++i) pool_.push_back(host_.CreatePoolObjectForAnalysis());
            InitializeOverlayForAnalysis();
        }
        catch (...) { ClearForAnalysis(); instance=nullptr; throw; }
    }
    wxAlphaManager::~wxAlphaManager() { ClearForAnalysis(); instance=nullptr; }
    void wxAlphaManager::SetFactoryHostForAnalysis(wxAlphaManagerHost* h) noexcept { factoryHost=h; }
    wxAlphaManager* wxAlphaManager::GetInstanceForAnalysis() noexcept { return instance; }
    const spRTTIRecord& wxAlphaManager::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxAlphaManager::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAlphaManager::vfunc_10(spCloneManager& cm) const
    {
        auto clone=std::make_unique<wxAlphaManager>(host_);
        cm.RegisterCloneForAnalysis(*this,*clone);
        if (!vfunc_14(*clone,cm)) return nullptr;
        return clone;
    }
    void wxAlphaManager::RequireLive() const
    {
        if (cleared_) throw std::logic_error("wxAlphaManager effect storage was cleared");
    }
    void wxAlphaManager::ClearForAnalysis() noexcept
    {
        for (auto* p:pool_) if(p) host_.DestroyPoolObjectForAnalysis(p);
        pool_.clear();
        // Native frees the 200 records without restoring changed materials.
        cleared_=true;
        fade_.field35C=fade_.recipient=nullptr;
        if(overlay_) { host_.ReleaseOverlayMaterialForAnalysis(overlay_);overlay_=nullptr; }
    }
    void wxAlphaManager::ResetForAnalysis()
    {
        RequireLive();
        for(auto* p:pool_) { p->active=0;p->field2C=0;p->field08=nullptr; }
        for(auto& e:effects_) { e.active=e.saved=0;e.node=nullptr; }
        fade_.recipient=nullptr;
    }
    wxAlphaPoolObjectForAnalysis* wxAlphaManager::AcquirePoolObjectForAnalysis()
    {
        RequireLive();
        wxAlphaPoolObjectForAnalysis* last=nullptr;
        for(auto* p:pool_)
        {
            last=p;
            if(!p->active) { p->active=1;p->field2C=0;break; }
        }
        return last; // native exhaustion returns the last occupied object
    }
    void wxAlphaManager::InitializeOverlayForAnalysis()
    {
        RequireLive();
        if(!overlay_) overlay_=host_.CreateOverlayMaterialForAnalysis();
    }
    void wxAlphaManager::NotifyFadeForAnalysis(std::uint8_t value) noexcept
    {
        if(fade_.recipient)
        {
            auto* recipient=fade_.recipient;
            fade_.recipient=nullptr; // clear before callback, including reentry
            host_.NotifyFadeForAnalysis(recipient,*this,0x27A3,value);
        }
        fade_.notified=1;
    }
    void wxAlphaManager::BeginFade(float from,float to,std::uint32_t duration,void* recipient)
    {
        NotifyFadeForAnalysis(static_cast<std::uint8_t>(fade_.to==0));
        fade_.enabled=1;fade_.notified=0;fade_.from=from;fade_.to=to;
        fade_.started=host_.GetSystemMillisecondsForAnalysis();
        fade_.duration=duration;fade_.recipient=recipient;
    }
    void wxAlphaManager::FadeToTransparentForAnalysis(std::uint32_t d,void* r) { BeginFade(1,0,d,r); }
    void wxAlphaManager::FadeToOpaqueForAnalysis(std::uint32_t d,void* r) { BeginFade(0,1,d,r); }
    void wxAlphaManager::DrawFadeForAnalysis() noexcept
    {
        if(!fade_.enabled||!overlay_) return;
        const std::uint32_t elapsed=host_.GetSystemMillisecondsForAnalysis()-fade_.started;
        float alpha;
        if(!fade_.duration||elapsed>fade_.duration)
        {
            alpha=fade_.to;
            if(fade_.to==0) fade_.enabled=0;
            NotifyFadeForAnalysis(static_cast<std::uint8_t>(fade_.to==0));
        }
        else
        {
            const auto delay=static_cast<std::uint32_t>(double(fade_.duration)*0.25);
            const double fraction=elapsed>delay?double(elapsed-delay)/(fade_.duration-delay):0;
            alpha=static_cast<float>(fade_.from+(double(fade_.to)-fade_.from)*fraction);
        }
        host_.DrawOverlayForAnalysis(overlay_,alpha);
    }
    bool wxAlphaManager::RestoreEffectForAnalysis(EffectForAnalysis* e) noexcept
    {
        if(!e||!e->active||!e->saved) return true;
        auto& r=host_.RenderableForAnalysis(e->renderable);
        auto* m=r.material;
        if(!m) return false;
        m->field38=e->saved38;m->field34=e->saved34;
        host_.SetMaterialColorForAnalysis(*m,0x10,e->savedColor10);
        host_.SetMaterialColorForAnalysis(*m,0x08,e->savedColor08);
        if(e->node) if(auto* target=host_.NodeColorTargetForAnalysis(e->node))
            host_.SetNodeColorForAnalysis(target,e->savedNodeColor);
        if(m->firstLayerMode) *m->firstLayerMode=e->savedLayerMode;
        r.field1C=e->saved1C;r.field18=e->saved18;
        host_.RefreshRenderableForAnalysis(e->renderable);
        e->active=e->saved=0;
        return true;
    }
    bool wxAlphaManager::UpdateEffectForAnalysis(EffectForAnalysis* e) noexcept
    {
        if(!e||!e->active) return false;
        const auto now=host_.GetGameMillisecondsForAnalysis();
        if(!e->mode||e->interpolationStarted==~0u) return false;
        auto& r=host_.RenderableForAnalysis(e->renderable);
        auto* m=r.material;
        if(!m||host_.IsRenderableKindOfForAnalysis(e->renderable,0x58DA4026)
            ||host_.IsRenderableKindOfForAnalysis(e->renderable,0x750F73D9)) return true;
        const auto mode=e->mode;
        const std::uint32_t elapsed=now-e->interpolationStarted;
        bool interpolated=false;
        std::uint32_t interpolationTarget=0;
        if(mode==1||mode==2||mode==4)
        {
            if(elapsed>=e->duration||!e->duration) e->current=e->to;
            else
            {
                interpolated=true;interpolationTarget=e->to;
                const float fraction=static_cast<float>(double(elapsed)/e->duration);
                const auto to=host_.UnpackColorForAnalysis(e->to);
                const auto from=host_.UnpackColorForAnalysis(e->from);
                std::uint32_t color=0;
                constexpr unsigned shifts[]{16,8,0,24};
                for(unsigned c=0;c<4;++c)
                {
                    const double difference=c?static_cast<float>(double(to[c])-from[c]):double(to[c])-from[c];
                    color|=std::uint32_t(host_.ConvertColorByteForAnalysis((from[c]+difference*fraction)*255.0))<<shifts[c];
                }
                e->current=color;
            }
        }
        else if(mode==3||mode==5) e->current=e->to;
        if(e->lifetime)
        {
            if(mode==2)
            {
                if(std::uint32_t(now-e->lifetimeStarted)>e->lifetime)
                {
                    std::swap(e->from,e->to);
                    e->current=interpolated?interpolationTarget:host_.UnwrittenReverseColorForAnalysis();
                    e->mode=1;e->interpolationStarted=e->lifetimeStarted=now;
                    if(e->lifetime<e->duration) e->lifetime=e->duration;
                }
            }
            else if(mode==1&&std::uint32_t(now-e->lifetimeStarted)>e->lifetime)
            { (void)RestoreEffectForAnalysis(e);return true; }
        }
        else if(mode==4&&std::uint32_t(now-e->lifetimeStarted)>e->duration)
        { (void)RestoreEffectForAnalysis(e);return true; }
        const auto color10=host_.GetMaterialColorForAnalysis(*m,0x0C);
        auto* target=e->node?host_.NodeColorTargetForAnalysis(e->node):nullptr;
        if(!e->saved)
        {
            e->saved38=m->field38;e->saved34=m->field34;e->savedColor10=color10;
            e->savedColor08=host_.GetMaterialColorForAnalysis(*m,0x04);
            if(target) e->savedNodeColor=host_.GetNodeColorForAnalysis(target);
            e->saved1C=r.field1C;e->saved18=r.field18;
        }
        m->field38=4;
        const auto color=host_.UnpackColorForAnalysis(e->current);
        if(e->mode==4) host_.SetMaterialColorForAnalysis(*m,0x08,color);
        else
        {
            host_.SetMaterialColorForAnalysis(*m,0x10,color);
            m->field34=2;r.field1C=1;r.field18=1;
            host_.RefreshRenderableForAnalysis(e->renderable);
        }
        if(m->firstLayerMode)
        {
            if(!e->saved) e->savedLayerMode=*m->firstLayerMode;
            if(e->mode!=4) *m->firstLayerMode=2;
        }
        e->saved=1;
        return false;
    }
    void wxAlphaManager::UpdateEffectsForAnalysis()
    {
        RequireLive();
        for(auto& e:effects_) if(e.active) (void)UpdateEffectForAnalysis(&e);
    }
    void wxAlphaManager::ApplyToNodeForAnalysis(void* node,const std::array<float,3>& values,std::uint32_t duration,std::uint32_t lifetime)
    {
        RequireLive();
        if(!node) return;
        auto& n=host_.NodeForAnalysis(node);
        if(n.exactClassID!=0x603625D0) return;
        for(auto* renderable:n.renderables)
            if(host_.IsRenderableKindOfForAnalysis(renderable,0x58DA4026)
                ||host_.IsRenderableKindOfForAnalysis(renderable,0x750F73D9)) return;
        EffectForAnalysis* selected=nullptr;
        for(auto* renderable:n.renderables)
        {
            // Deliberately retained across iterations, matching native ESI.
            if(!selected)
            {
                for(auto& e:effects_) if(e.active&&e.renderable==renderable) {selected=&e;break;}
                if(!selected) for(auto& e:effects_) if(!e.active) {selected=&e;e.active=1;e.saved=0;break;}
            }
            if(!selected) continue;
            auto& e=*selected;e.node=node;e.renderable=renderable;
            e.lifetimeStarted=e.interpolationStarted=host_.GetGameMillisecondsForAnalysis();
            e.lifetime=lifetime;e.mode=lifetime?2:1;
            std::uint32_t from=~0u,to=~0u;
            std::uint32_t z;std::memcpy(&z,&values[2],4);
            if(z==0x3F800000)
            {
                from=0xFF000000;
                for(unsigned c=0;c<3;++c) from|=std::uint32_t(host_.ConvertColorByteForAnalysis(flashColor_[c]))<<(16-8*c);
                e.mode=4;e.duration=duration;
            }
            else if(duration==~0u&&lifetime==~0u)
            {
                from=0xFF000000;
                for(unsigned c=0;c<3;++c) from|=std::uint32_t(host_.ConvertColorByteForAnalysis(double(values[c])*255))<<(16-8*c);
                to=from;e.mode=5;e.duration=e.lifetime=0;
            }
            else
            {
                from=0xFFFFFF|(std::uint32_t(host_.ConvertColorByteForAnalysis(double(values[0])*255))<<24);
                to=0xFFFFFF|(std::uint32_t(host_.ConvertColorByteForAnalysis(double(values[1])*255))<<24);
                e.duration=duration;
            }
            e.from=e.current=from;e.to=to;
        }
    }
    void wxAlphaManager::ApplyRecursiveForAnalysis(void* node,const std::array<float,3>& v,std::uint32_t d,std::uint32_t l)
    {
        ApplyToNodeForAnalysis(node,v,d,l);
        for(auto* child:host_.NodeForAnalysis(node).children) ApplyRecursiveForAnalysis(child,v,d,l);
    }
    void wxAlphaManager::FlashRecursiveForAnalysis(void* n,std::uint32_t d,const std::array<float,3>& c)
    { flashColor_=c;ApplyRecursiveForAnalysis(n,{0,0,1},d,0); }
    void wxAlphaManager::SetColorRecursiveForAnalysis(void* node,const std::array<float,3>& v)
    {
        auto& n=host_.NodeForAnalysis(node); // native recursion requires non-null node
        if(n.exactClassID==0x603625D0)
        {
            std::uint32_t packed=0xFF000000;
            for(unsigned c=0;c<3;++c) packed|=std::uint32_t(host_.ConvertColorByteForAnalysis(double(v[c])*255))<<(16-8*c);
            for(auto* renderable:n.renderables)
            {
                if(!renderable) continue;
                auto* m=host_.RenderableForAnalysis(renderable).material;
                if(!m) continue;
                host_.SetMaterialColorForAnalysis(*m,0x08,host_.UnpackColorForAnalysis(packed));
                if(auto* target=host_.NodeColorTargetForAnalysis(node))
                    host_.SetNodeColorForAnalysis(target,host_.UnpackColorForAnalysis(packed));
                host_.SetMaterialColorForAnalysis(*m,0x08,host_.UnpackColorForAnalysis(packed));
            }
        }
        for(auto* child:n.children) SetColorRecursiveForAnalysis(child,v);
    }
    void wxAlphaManager::ClassifyRecursiveForAnalysis(void* node)
    {
        RequireLive();
        auto& n=host_.NodeForAnalysis(node);
        auto* data=n.data0C;
        if(n.firstComponentFlags&&n.name)
        {
            if(Has(n.name,"collidable"))
            {
                *n.firstComponentFlags=std::uint8_t(*n.firstComponentFlags)|8u;
                if(!Has(n.name,"collidable_only"))
                {
                    if(Has(n.name,"fadeable"))
                    {
                        data=AcquirePoolObjectForAnalysis();data->flags04=1;data->node10=node;n.data0C=data;
                    }
                    else
                    {
                        if(n.parent) for(auto* sibling:host_.NodeForAnalysis(n.parent).children)
                        {
                            if(data) break;
                            const auto* name=host_.NodeForAnalysis(sibling).name;
                            if(Has(name,"fadeable"))
                            {
                                host_.ReportFadeableNodeForAnalysis(name);
                                data=AcquirePoolObjectForAnalysis();data->flags04=1;data->node10=sibling;n.data0C=data;
                            }
                        }
                        if(Has(n.name,"clip"))
                        {
                            if(data) data->flags04|=2;
                            else {data=AcquirePoolObjectForAnalysis();data->flags04=2;n.data0C=data;}
                        }
                    }
                }
            }
            else if(Has(n.name,"clip")||Has(n.name,"nv_"))
            {data=AcquirePoolObjectForAnalysis();data->flags04=2;n.data0C=data;}
        }
        for(auto* child:n.children) ClassifyRecursiveForAnalysis(child);
    }
}
