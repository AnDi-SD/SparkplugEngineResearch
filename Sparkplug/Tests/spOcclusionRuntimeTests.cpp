#include "Code/Sparkplug/spOcclusionVolumeSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/Host/LegacySortPolicy.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace sparkplug::reconstruction;
using Volume=spOcclusionVolume;
using Policy=sparkplug::host::LegacySortPolicy;
namespace {
using Words=std::vector<std::uint32_t>;
using Bytes=std::vector<unsigned char>;
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t Bits(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
struct Reader {
    std::ifstream file;
    explicit Reader(const std::filesystem::path& path):file(path,std::ios::binary){if(!file)throw std::runtime_error("Missing Occlusion reference capture");}
    Bytes Read(std::size_t size){if(size>65536)throw std::runtime_error("Oversized capture");Bytes result(size);if(size&&!file.read(reinterpret_cast<char*>(result.data()),size))throw std::runtime_error("Truncated capture");return result;}
    std::uint32_t U32(){auto bytes=Read(4);std::uint32_t value;std::memcpy(&value,bytes.data(),4);return value;}
    Words Values(){auto count=U32();if(count>16384)throw std::runtime_error("Oversized capture words");Words result;for(unsigned i=0;i<count;++i)result.push_back(U32());return result;}
};
void Stream(spMemoryStream& stream,const Bytes& bytes) {
    Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"CPU input stream allocation");
    if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
}
Words State(const Volume& volume,bool worldDone) {
    const auto* ib=volume.GetIndexBufferForAnalysis();const auto* vb=volume.GetLocalVertexBufferForAnalysis();
    Check(ib&&vb,"Occlusion owns its initialized CPU geometry");
    Words result{volume.IsInitializedForAnalysis(),volume.IsCameraDirtyForAnalysis(),volume.GetBorderCountForAnalysis(),volume.GetPlanarByteForAnalysis(),ib->GetIndexCountForAnalysis()};
    for(unsigned i=0;i<ib->GetIndexCountForAnalysis();++i)result.push_back(*ib->GetIndexForAnalysis(i));
    result.push_back(vb->GetVertexCountForAnalysis());
    for(std::size_t i=0;i<vb->GetDataForAnalysis().size();i+=4){std::uint32_t word;std::memcpy(&word,vb->GetDataForAnalysis().data()+i,4);result.push_back(word);}
    for(const auto& point:volume.GetPointsForAnalysis())for(float component:point)result.push_back(Bits(component));
    for(float component:volume.GetLocalBoundsForAnalysis())result.push_back(Bits(component));
    for(float component:volume.GetLocalSphereForAnalysis())result.push_back(Bits(component));
    result.push_back(worldDone);if(worldDone)for(float component:volume.GetWorldSphereForAnalysis())result.push_back(Bits(component));
    const auto& points=volume.GetPointsForAnalysis();const auto& faces=volume.GetFacesForAnalysis();const auto& edges=volume.GetEdgesForAnalysis();
    const auto pointIndex=[&](const Volume::Vector3* point){for(std::uint32_t i=0;i<points.size();++i)if(point==&points[i])return i;throw std::runtime_error("Dangling point reference");};
    const auto faceIndex=[&](const Volume::FaceForAnalysis* face){if(!face)return 0xffffffffu;for(std::uint32_t i=0;i<faces.size();++i)if(face==&faces[i])return i;throw std::runtime_error("Dangling face reference");};
    result.push_back(static_cast<unsigned>(faces.size()));
    for(const auto& face:faces){for(float component:face.plane)result.push_back(Bits(component));for(auto* point:face.positions)result.push_back(pointIndex(point));Check(!face.cameraSideKnown,"Unwritten original camera-side word remains unknown");}
    result.push_back(static_cast<unsigned>(edges.size()));
    for(const auto& edge:edges) {
        result.insert(result.end(),{pointIndex(edge->start),pointIndex(edge->end),faceIndex(edge->own),faceIndex(edge->opposite),edge->border,edge->walkStamp,static_cast<unsigned>(edge->outgoing.size())});
        for(auto* target:edge->outgoing) {
            auto found=std::find_if(edges.begin(),edges.end(),[&](const auto& value){return value.get()==target;});
            Check(found!=edges.end(),"Outgoing edge belongs to the current graph");result.push_back(static_cast<unsigned>(found-edges.begin()));
        }
    }
    return result;
}
void Equal(const Words& actual,const Words& expected,const std::string& name) {
    if(actual!=expected) {
        auto mismatch=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());
        const auto index=mismatch.first-actual.begin();
        std::cerr<<name<<" word "<<index<<" differs; actual/expected count "<<actual.size()<<'/'<<expected.size();
        if(mismatch.first!=actual.end())std::cerr<<" actual "<<std::hex<<*mismatch.first;
        if(mismatch.second!=expected.end())std::cerr<<" expected "<<std::hex<<*mismatch.second;
        std::cerr<<std::dec<<'\n';
    }
    Check(actual==expected,"All original initialized state/topology/float words match");
}
void Captures(const std::filesystem::path& path) {
    Reader capture(path);Check(capture.Read(4)==Bytes({'O','C','V','1'}),"Occlusion capture magic");const auto cases=capture.U32();Check(cases==5,"Five fresh original cases");
    for(unsigned ordinal=0;ordinal<cases;++ordinal) {
        const auto label=capture.Read(capture.U32());const std::string name(label.begin(),label.end());
        const auto reader=capture.U32(),ibSize=capture.U32(),vbSize=capture.U32();
        const auto ibBytes=capture.Read(ibSize),vbBytes=capture.Read(vbSize);const auto initial=capture.Values(),world=capture.Values();
        Volume volume;spIndexBuffer indices;spVertexBuffer vertices;std::string error;
        spMemoryStream first;Stream(first,ibBytes);
        if(reader) {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spOcclusionVolumeSerializer serializer(Policy::GeometryDispatch());
            Check(serializer.ReadPayloadForAnalysis(context,first,ibSize,volume,&error)&&!context.failed,"Actual Node plus Occlusion sections call shared Init");
            std::uint32_t position=0;Check(first.GetCurrentPosition(position)&&position==ibSize,"Reader consumed original bounded object extent");
        } else {
            spMemoryStream second;Stream(second,vbBytes);
            Check(indices.ReadForAnalysis(first,ibSize)&&vertices.ReadForAnalysis(second,vbSize),"Original CPU buffer readers");
            const auto saved=vertices.GetDataForAnalysis();
            Check(volume.InitializeForAnalysis(indices,vertices,Policy::GeometryDispatch(),&error),error.c_str());
            Check(vertices.GetDataForAnalysis()==saved,"Init preserves caller VB bytes");
            if(name=="positive-wide")Check(indices.GetIndexForAnalysis(0)==0x10000&&volume.GetIndexBufferForAnalysis()->GetIndexForAnalysis(0)==0,"Original high16 truncation does not alter the input IB");
        }
        Equal(State(volume,false),initial,name+" Init");
        volume.SetPositionForAnalysis({1,2,3});volume.SetScaleForAnalysis({-2,3,4});volume.SetOrientationForAnalysis({0,1,0,-1,0,0,0,0,1});volume.MarkLocalTransformDirtyForAnalysis();
        Check(volume.UpdateWorldForAnalysis(),"Original Occlusion world override");Equal(State(volume,true),world,name+" world");
        Check(volume.UpdateWorldForAnalysis(),"World call with cleared dirty flags");Equal(State(volume,true),world,name+" clean world");
        Check(!volume.SetShapeBuffersForAnalysis({{0,0,0}},{}),"Analysis preparation cannot detach a live runtime's world storage");
        Check(!volume.SetPreparedTopologyForAnalysis({}, {}, {}, 0, 0),"Direct topology preparation cannot replace a live runtime");
        if(!reader) {
            Check(!volume.InitializeForAnalysis(indices,vertices,Policy::GeometryDispatch(),&error),"Host refuses unsupported re-init before mutation");
            Equal(State(volume,true),world,name+" retained after re-init refusal");
        }
        spCloneManager clones;auto clone=volume.vfunc_10(clones);auto* typed=dynamic_cast<Volume*>(clone.get());
        Check(typed&&!typed->IsInitializedForAnalysis()&&!typed->GetIndexBufferForAnalysis()&&typed->GetFacesForAnalysis().empty(),"Original Node-only clone omits Occlusion geometry");
        std::cout<<"  "<<name<<": Init and world match "<<initial.size()+world.size()<<" reference words\n";
    }
    Check(capture.file.peek()==std::char_traits<char>::eof(),"All Occlusion captures consumed");
}
}
int main(int argc,char** argv) {
    try{Captures(argc==2?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"Fixtures/occlusion-core.dat");
        std::cout<<"PASS "<<checks<<"/"<<checks<<": original fresh Occlusion Init/reader/world with named CRT\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
