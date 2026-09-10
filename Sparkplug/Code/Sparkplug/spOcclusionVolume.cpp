#include "spOcclusionVolume.h"
#include "spZonePortal.h"
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
    triangleIndices_.clear();borderPoints_.clear();shapeBuffersPrepared_=false;walkStamp_=0;
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
bool spOcclusionVolume::SetShapeBuffersForAnalysis(std::vector<Vector3> points,
    std::vector<std::uint16_t> triangleIndices,std::uint32_t walkStamp)
{
    if(points.size()>4096||triangleIndices.size()>3072||triangleIndices.size()%3)return false;
    for(const auto& point:points)for(float value:point)if(!std::isfinite(value))return false;
    for(auto index:triangleIndices)if(index>=points.size())return false;
    // PC13D0486 reserves triangleCount faces. fresh-init-batch-run1 exposes
    // stale original face pointers when a single triangle grows its reverse
    // side; the driver guard below excludes that case instead of repairing it.
    std::vector<FaceForAnalysis> faces;faces.reserve(triangleIndices.size()/3);
    edges_.clear();faces_=std::move(faces);points_=std::move(points);
    triangleIndices_=std::move(triangleIndices);borderPoints_.clear();
    borderCount_=0;planar_=0;walkStamp_=walkStamp;shapeBuffersPrepared_=true;return true;
}
spOcclusionVolume::FaceForAnalysis* spOcclusionVolume::FindOrCreateFaceForAnalysis(
    const Vector3* first,const Vector3* second,const Vector3* third)
{
    // PC4708F1 invokes471420; use its existing shared finite-input slice.
    const auto plane=spZonePortal::PlaneFromFirstThreeForAnalysis(*first,*second,*third);
    for(auto& face:faces_)if(EqualComponents<4>(face.plane.data(),plane.data()))return &face;
    // PC47099D..470A30: append plane and the original three pointers. A match
    // retains the first face's pointers. No camera-side initialization occurs.
    FaceForAnalysis face;face.plane=plane;face.positions={first,second,third};
    faces_.push_back(face);return &faces_.back();
}
bool spOcclusionVolume::BuildFacesAndEdgesForAnalysis()
{
    if(!shapeBuffersPrepared_||edges_.size()>MaxShapeEdgesForAnalysis-triangleIndices_.size())return false;
    // PC13B4FA8..500B walks UInt16 triples in wire order, using world-VB
    // position offset/stride. Prepared points are those actual XYZ records.
    // Protected856CF0 jointly changes EAX and a stack pointer; the observed
    // resulting cursor is IBdata+4, then +6 per triangle (13B5345).
    for(std::size_t i=0;i<triangleIndices_.size();i+=3)
    {
        const Vector3* vertices[3]={&points_[triangleIndices_[i]],
            &points_[triangleIndices_[i+1]],&points_[triangleIndices_[i+2]]};
        auto* face=FindOrCreateFaceForAnalysis(vertices[0],vertices[1],vertices[2]);
        // PC13B501C..512C allocates all three edges, then appends them in
        // order. Unwritten edge+10/padding are not exposed as initialized data.
        std::array<std::unique_ptr<EdgeForAnalysis>,3> triangle;
        for(std::size_t k=0;k<3;++k)
        {
            triangle[k]=std::make_unique<EdgeForAnalysis>();auto& edge=*triangle[k];
            edge.start=vertices[k];edge.end=vertices[(k+1)%3];edge.own=face;
            edge.walkStamp=walkStamp_;edge.border=1;
        }
        for(auto& edge:triangle)edges_.push_back(std::move(edge));
    }
    borderCount_=static_cast<std::uint32_t>(edges_.size()); //13B5394
    return true; //13B539E; no geometric rejection inside this producer
}
bool spOcclusionVolume::BuildShapeForAnalysis()
{
    // Host unsupported boundary: fresh-init-batch-run1 positive-u32 returned
    // AL1, but its first three edge.own pointers no longer belonged to the
    // current face vector after reverse-side growth. Do not silently fix the
    // original by reserving extra faces. Zero triangles also has an unchecked
    // first-edge access. These refusals are not original game return values.
    if(!shapeBuffersPrepared_||triangleIndices_.size()<6||
        edges_.size()+triangleIndices_.size()>MaxShapeEdgesForAnalysis/2)return false;
    // PC13D047C..04C7 reserves faces/edges; face storage was reserved before
    // binding host pointers. Reserve does not clear logical topology.
    edges_.reserve(edges_.size()+triangleIndices_.size());
    if(!BuildFacesAndEdgesForAnalysis()||!MergeCollinearEdgesForAnalysis()||
        !LinkOppositeEdgesForAnalysis()||!RemoveCoplanarEdgesForAnalysis()||
        !CheckPlanarityForAnalysis())return false;
    if(edges_.empty())return false; // host guard for original13D0573 dereference
    if(!ConnectOutgoingEdgesForAnalysis(*edges_.front()))return false;
    borderPoints_.reserve(edges_.size()); //13D05A5..05DA,46ECE0; remains empty
    if(planar_)
    {
        const auto count=edges_.size();edges_.reserve(count*2);
        // PC13D062C..06DC iterates the original range backwards. Use the own
        // face's retained three pointers in reverse order, not current endpoints
        // and not a negated cached plane (the roundings can differ).
        for(std::size_t i=count;i>0;--i)
        {
            const auto& source=*edges_[i-1];auto edge=std::make_unique<EdgeForAnalysis>();
            edge->start=source.end;edge->end=source.start;
            const auto positions=source.own->positions;
            edge->own=FindOrCreateFaceForAnalysis(positions[2],positions[1],positions[0]);
            edge->walkStamp=walkStamp_;edge->border=1;edges_.push_back(std::move(edge));
        }
        // PC13D06E3..070D deliberately ignores the second call's AL. Existing
        // outgoing lists are retained; borderCount is not doubled afterwards.
        (void)ConnectOutgoingEdgesForAnalysis(*edges_[count]);
    }
    return true;
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
