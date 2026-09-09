#include "spOcclusionVolume.h"
#include "../../Analysis/PC/spNodeTransformMath.h"
#include <cmath>

namespace sparkplug::reconstruction
{
namespace
{
    std::unique_ptr<spBaseObject> Create(){return std::make_unique<spOcclusionVolume>();}
    const spRTTIRecord Record{spOcclusionVolume::ClassID,spNode::ClassID,"spOcclusionVolume",
        &spNode::StaticRTTI(),&Create,nullptr};
    const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    constexpr float Epsilon=.001F; // PC006E8D10, PS2 literal3A83126F
    template<std::size_t N> bool EqualComponents(const float* a,const float* b)
    {
        for(std::size_t i=0;i<N;++i)
            if(!(std::abs(double(a[i])-b[i])<=double(Epsilon)))return false;
        return true;
    }
}
const spRTTIRecord& spOcclusionVolume::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spOcclusionVolume::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> spOcclusionVolume::vfunc_10(spCloneManager& manager) const
{
    // PC470AD0 calls inherited Node copy; own geometry is not cloned.
    auto result=std::make_unique<spOcclusionVolume>();manager.RegisterClone(*this,*result);
    return spNode::vfunc_14(*result,manager)?std::move(result):nullptr;
}
bool spOcclusionVolume::SetPreparedTopologyForAnalysis(std::vector<Vector3> points,
    const std::vector<PlaneForAnalysis>& planes,const std::vector<PreparedEdgeForAnalysis>& edges,
    std::uint32_t borderCount,std::uint8_t priorPlanar)
{
    // Host bounds belong only to this direct-state analysis seam.
    if(points.size()>4096||planes.size()>4096||edges.size()>4096)return false;
    for(const auto& edge:edges)if(edge.start>=points.size()||edge.end>=points.size()||
        edge.own>=planes.size()||(edge.opposite&&*edge.opposite>=planes.size()))return false;
    edges_.clear();points_=std::move(points);faces_.clear();faces_.reserve(planes.size());
    for(const auto& plane:planes){FaceForAnalysis face;face.plane=plane;faces_.push_back(face);}
    for(const auto& input:edges)
    {
        auto edge=std::make_unique<EdgeForAnalysis>();edge->start=&points_[input.start];
        edge->end=&points_[input.end];edge->own=&faces_[input.own];
        edge->opposite=input.opposite?&faces_[*input.opposite]:nullptr;edge->border=1;
        edges_.push_back(std::move(edge));
    }
    borderCount_=borderCount;planar_=priorPlanar;return true;
}
bool spOcclusionVolume::IsEdgeConvexForAnalysis(const EdgeForAnalysis& edge) const noexcept
{
    if(!edge.start||!edge.end||!edge.own||!edge.opposite)return false; // host incomplete input
    Vector3 direction{};for(std::size_t i=0;i<3;++i)direction[i]=(*edge.end)[i]-(*edge.start)[i];
    evidence::pc::node_math::Normalize(direction); // existing shared PC41D2D0 vector operation
    const auto& own=edge.own->plane;const auto& opposite=edge.opposite->plane;
    // PC46D670: (own normal cross normalized edge) dot opposite normal.
    const double x=double(direction[2])*own[1]-double(direction[1])*own[2];
    const double y=double(direction[0])*own[2]-double(direction[2])*own[0];
    const double z=double(direction[1])*own[0]-double(direction[0])*own[1];
    return ((z*opposite[2]+y*opposite[1])+x*opposite[0])<=double(Epsilon);
}
bool spOcclusionVolume::LinkOppositeEdgesForAnalysis()
{
    // PC46E1A0 / PS2 1CB5A0. Repeated matches are not skipped: original
    // border count can wrap and earlier pointer assignments remain on failure.
    borderCount_=static_cast<std::uint32_t>(edges_.size());
    for(std::size_t i=0;i<edges_.size();++i)for(std::size_t j=i+1;j<edges_.size();++j)
    {
        auto& a=*edges_[i];auto& b=*edges_[j];
        if(!EqualComponents<3>(a.start->data(),b.end->data())||!EqualComponents<3>(a.end->data(),b.start->data()))continue;
        a.opposite=b.own;b.opposite=a.own;a.border=b.border=0;borderCount_-=2;
        if(!IsEdgeConvexForAnalysis(a))return false;
    }
    return true;
}
bool spOcclusionVolume::MergeCollinearEdgesForAnalysis()
{
    // PC46E820 / PS2 1CB990; retained edge keeps its own face, no face gate.
    for(std::size_t i=0;i<edges_.size();++i)
    {
        if(!edges_[i])continue;auto& a=*edges_[i];
        for(std::size_t j=0;j<edges_.size();)
        {
            if(!edges_[j]||!EqualComponents<3>(a.end->data(),edges_[j]->start->data())){++j;continue;}
            const auto& b=*edges_[j];Vector3 first{},second{};
            for(std::size_t k=0;k<3;++k){first[k]=(*a.end)[k]-(*a.start)[k];second[k]=(*b.end)[k]-(*b.start)[k];}
            evidence::pc::node_math::Normalize(first);evidence::pc::node_math::Normalize(second);
            const float difference=static_cast<float>(std::abs((double(first[2])*second[2]+double(first[1])*second[1])+double(first[0])*second[0]-1.));
            if(!(difference<=Epsilon)){++j;continue;}
            a.end=b.end;edges_[j].reset();j=0;
        }
    }
    for(std::size_t i=0;i<edges_.size();)if(!edges_[i]){edges_[i]=std::move(edges_.back());edges_.pop_back();}else ++i;
    borderCount_=static_cast<std::uint32_t>(edges_.size());return true;
}
bool spOcclusionVolume::RemoveCoplanarEdgesForAnalysis()
{
    // PC46EB10: only normal XYZ, no plane-distance comparison; swap last.
    for(std::size_t i=0;i<edges_.size();)
    {
        const auto& edge=*edges_[i];
        if(edge.opposite&&EqualComponents<3>(edge.opposite->plane.data(),edge.own->plane.data()))
        {edges_[i]=std::move(edges_.back());edges_.pop_back();}
        else ++i;
    }
    return true;
}
bool spOcclusionVolume::CheckPlanarityForAnalysis()
{
    // PC46E3B0: closed accepts without comparing faces, open compares all4.
    if(!borderCount_){planar_=0;return true;}
    for(std::size_t i=1;i<faces_.size();++i)
        if(!EqualComponents<4>(faces_[i].plane.data(),faces_[0].plane.data()))return false;
    planar_=1;return true;
}
bool spOcclusionVolume::ConnectOutgoingEdgesForAnalysis(EdgeForAnalysis& edge)
{
    // PC4705A0 / PS2 1CAF90. A nonempty list is the original visitation
    // marker, including a partially appended list left by an earlier failure.
    if(!edge.outgoing.empty())return true;
    for(const auto& candidate:edges_)
    {
        if(!EqualComponents<3>(candidate->start->data(),edge.end->data())||
            EqualComponents<3>(candidate->end->data(),candidate->start->data()))continue;
        Vector3 first{},second{};
        for(std::size_t k=0;k<3;++k){first[k]=(*edge.end)[k]-(*edge.start)[k];second[k]=(*candidate->end)[k]-(*candidate->start)[k];}
        evidence::pc::node_math::Normalize(first);evidence::pc::node_math::Normalize(second);
        const double dot=(double(second[2])*first[2]+double(second[1])*first[1])+double(second[0])*first[0];
        if(std::abs(dot-1.)<=double(Epsilon))return false;
        if(!edge.opposite)
        {
            const auto& normal=edge.own->plane;
            // Original rounds cross X/Y to float temporaries; Z remains x87.
            const float x=static_cast<float>(double(first[1])*normal[2]-double(normal[1])*first[2]);
            const float y=static_cast<float>(double(first[2])*normal[0]-double(normal[2])*first[0]);
            const double z=double(first[0])*normal[1]-double(first[1])*normal[0];
            if((z*second[2]+double(y)*second[1])+double(x)*second[0]>double(Epsilon))return false;
        }
        edge.outgoing.push_back(candidate.get());
    }
    for(auto* next:edge.outgoing)if(!ConnectOutgoingEdgesForAnalysis(*next))return false;
    return true; // a dead end is accepted by this leaf, not proof of full Init
}
}
