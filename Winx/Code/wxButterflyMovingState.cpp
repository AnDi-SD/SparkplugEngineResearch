#include "wxButterflyMovingState.h"
#include "Analysis/Host/wxButterflyMovingStateHost.h"
#include "Analysis/PC/wxButterflyNumeric.h"
#include <cmath>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        using Vector3=std::array<float,3>;
        std::unique_ptr<spBaseObject> CreateState(){return std::make_unique<wxButterflyMovingState>();}
        const spRTTIRecord record{wxButterflyMovingState::ClassID,wxCharacterState::ClassID,
            "wxButterflyMovingState",&wxCharacterState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxButterflyMovingStateHost& Host(wxCharacterStateHost& value)
        {
            auto* host=dynamic_cast<wxButterflyMovingStateHost*>(&value);
            if(!host)throw std::logic_error("Butterfly PC movement requires a node, clock and random host");
            return *host;
        }
        Vector3 Cross(const Vector3& a,const Vector3& b)
        {return {float(double(a[1])*b[2]-double(b[1])*a[2]),
            float(double(a[2])*b[0]-double(a[0])*b[2]),float(double(b[1])*a[0]-double(a[1])*b[0])};}
        Vector3 Position(wxButterflyMovingStateHost& h,void* n)
        {
            const float x=h.ReadButterflyNodeWordForAnalysis(n,0x20);
            const float z=h.ReadButterflyNodeWordForAnalysis(n,0x28);
            const float y=h.ReadButterflyNodeWordForAnalysis(n,0x24);
            return {x,y,z};
        }
    }
    wxButterflyMovingState::wxButterflyMovingState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxButterflyMovingState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxButterflyMovingState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxButterflyMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxButterflyMovingState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;return clone;
    }
    bool wxButterflyMovingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        auto& h=Host(RequireHostForAnalysis());fields_.origin=Position(h,h.ButterflyNodeForAnalysis(GetOwnerForAnalysis()));
        const float scale=evidence::pc::ButterflyRandomScaleForAnalysis(h.ButterflyRandomForAnalysis());
        void* n=h.ButterflyNodeForAnalysis(GetOwnerForAnalysis());
        h.WriteButterflyNodeWordForAnalysis(n,0x30,scale);h.WriteButterflyNodeWordForAnalysis(n,0x34,scale);
        h.MarkButterflyNodeDirtyForAnalysis(n);h.WriteButterflyNodeWordForAnalysis(n,0x38,scale);
        fields_.phase=1;request.packedKey=(request.packedKey&0xF0000050u)|0x50u;
        void* handle=h.ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        QueuePendingFromState(handle,true,true);SetPendingHandleFromState(handle);ClearTransitionFlag1C();
        return wxCharacterState::vfunc_1C(request);
    }
    void wxButterflyMovingState::RandomHeight()
    {
        auto& h=Host(RequireHostForAnalysis());
        fields_.desiredHeight=evidence::pc::ButterflyRandomHeightForAnalysis(h.ButterflyRandomForAnalysis(),fields_.origin[1]);
    }
    void wxButterflyMovingState::vfunc_30(wxAnimationRequestForAnalysis&)
    {
        auto& h=Host(RequireHostForAnalysis());fields_.current=Position(h,h.ButterflyNodeForAnalysis(GetOwnerForAnalysis()));
        if(fields_.deadline&&fields_.deadline<h.ButterflyClockForAnalysis())
        {fields_.phase=std::uint8_t(!fields_.phase);fields_.initialized=0;}
        AdvancePhaseForAnalysis();
    }
    void wxButterflyMovingState::AdvancePhaseForAnalysis()
    {
        auto& h=Host(RequireHostForAnalysis());
        if(!fields_.initialized)
        {
            void* handle=h.ResolveAnimationForAnalysis(GetOwnerForAnalysis(),fields_.phase?0x50u:0x800050u);
            if(handle!=GetPendingHandleForAnalysis())
            {ReleasePendingFromState();QueuePendingFromState(handle,true,true);SetPendingHandleFromState(handle);}
            h.ButterflyRandomPointForAnalysis(fields_.target,200,fields_.origin);
            // PC writes64 before its height spill, after RNG returns.
            const unsigned random=h.ButterflyRandomForAnalysis();fields_.initialized=1;
            fields_.desiredHeight=evidence::pc::ButterflyRandomHeightForAnalysis(random,fields_.origin[1]);
            const unsigned clock=h.ButterflyClockForAnalysis();
            fields_.deadline=clock+1000u+h.ButterflyRandomForAnalysis()%4001u;
        }
        else
        {
            const float x=float(double(fields_.target[0])-fields_.current[0]);
            const double z=double(fields_.target[2])-fields_.current[2];
            if(z*z+double(x)*x<400.0)h.ButterflyRandomPointForAnalysis(fields_.target,200,fields_.origin);
            if(std::abs(double(fields_.current[1])-fields_.desiredHeight)<10.0)RandomHeight();
        }
        MoveForAnalysis();
    }
    void wxButterflyMovingState::MoveForAnalysis()
    {
        auto& h=Host(RequireHostForAnalysis());
        Vector3 direction{float(double(fields_.target[0])-fields_.current[0]),0,
            float(double(fields_.target[2])-fields_.current[2])};
        evidence::pc::ButterflyNormalizeForAnalysis(direction);
        const double step=(fields_.phase?80.0:40.0)*h.ButterflyDeltaForAnalysis();
        for(float& v:direction)v=float(double(v)*step);
        const float z=float(double(direction[2])+fields_.current[2]);
        double y=double(direction[1])+fields_.current[1];
        const double height=double(fields_.desiredHeight)-fields_.current[1];
        if(std::abs(height)>10.0)
        {
            const double vertical=(fields_.phase?100.0:50.0)*h.ButterflyDeltaForAnalysis();
            y=height>0?y+vertical:y-vertical;
        }
        void* n=h.ButterflyNodeForAnalysis(GetOwnerForAnalysis());
        h.WriteButterflyNodeWordForAnalysis(n,0x20,float(double(direction[0])+fields_.current[0]));
        h.WriteButterflyNodeWordForAnalysis(n,0x28,z);h.WriteButterflyNodeWordForAnalysis(n,0x24,float(y));h.MarkButterflyNodeDirtyForAnalysis(n);
        Vector3 desired{float((double(fields_.target[0])-fields_.current[0])*-1.0),0,
            float((double(fields_.target[2])-fields_.current[2])*-1.0)};
        n=h.ButterflyNodeForAnalysis(GetOwnerForAnalysis());
        Vector3 forward{h.ReadButterflyNodeWordForAnalysis(n,0x58),0,h.ReadButterflyNodeWordForAnalysis(n,0x60)};
        evidence::pc::ButterflyNormalizeForAnalysis(forward);evidence::pc::ButterflyNormalizeForAnalysis(desired);
        const double dot=(double(desired[2])*forward[2]+double(desired[0])*forward[0])+double(desired[1])*forward[1];
        if(dot>double(.985f))return;
        const double cross=double(forward[2])*desired[0]-double(desired[2])*forward[0];
        double angle=double(0x1.921fb6p+0f)*h.ButterflyDeltaForAnalysis();if(!(cross>0))angle=-angle;
        const float cosine=float(std::cos(angle));const double sine=std::sin(angle);
        const float newZ=float(double(forward[2])*cosine-sine*forward[0]);
        forward[0]=float(double(forward[2])*sine+double(cosine)*forward[0]);forward[2]=newZ;
        Vector3 right=Cross({0,1,0},forward);evidence::pc::ButterflyNormalizeForAnalysis(right);
        const Vector3 up=Cross(forward,right);right=Cross(up,forward);
        // PC420470 has an uninitialized-stack branch when both arguments
        // coincide. A zero local basis reaches it; no portable success is invented.
        if(std::abs(double(forward[0])-up[0])<=double(.001f)&&std::abs(double(forward[1])-up[1])<=double(.001f)&&std::abs(double(forward[2])-up[2])<=double(.001f))
            throw std::domain_error("Butterfly degenerate PC matrix basis is outside the qualified domain");
        const std::array<float,9> matrix{right[0],right[1],right[2],up[0],up[1],up[2],forward[0],forward[1],forward[2]};
        n=h.ButterflyNodeForAnalysis(GetOwnerForAnalysis());
        for(unsigned i:{0u,2u,1u,4u,3u,6u,5u,8u,7u})h.WriteButterflyNodeWordForAnalysis(n,0x40+4*i,matrix[i]);
        h.MarkButterflyNodeDirtyForAnalysis(n);h.InvokeButterflyNodeForAnalysis(h.ButterflyNodeForAnalysis(GetOwnerForAnalysis()),false);
    }
}
