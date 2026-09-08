// Original golden inputs/results: probe_pc_mesh_bv_core.py, pristine PC.
#include "Code/Sparkplug/spMeshBV.h"
#include "Code/Sparkplug/spMeshBVSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/wxFaceData.h"
#include "Analysis/PC/spVertexBounds.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
using winx::reconstruction::wxFaceData;
namespace {
using Bytes=std::vector<std::uint8_t>;
int checks=0;
void Check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
Bytes Hex(const std::string& text) {
    Bytes out;for(std::size_t i=0;i<text.size();i+=2)out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return out;
}
void Open(spMemoryStream& stream,const Bytes& bytes={}) {
    Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"stream size");
    if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
}
Bytes Data(spMemoryStream& stream) {
    std::uint32_t size=0;Check(stream.GetSize(&size),"stream extent");
    const auto* ptr=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(ptr,ptr+size):Bytes{};
}
void Append(Bytes& out,const Bytes& bytes){out.insert(out.end(),bytes.begin(),bytes.end());}
void Case(int faceMode) {
    (void)wxFaceData::StaticRTTI();
    const auto geometry=Hex("020000000100000000000000000001000200000000000300000000000000000000000000000000000000000000400000000000000000000000000000804000000000");
    Bytes input=Hex("a042"),expected=Hex("e042000000");Append(input,geometry);Append(expected,geometry);
    if(faceMode) {
        const auto face=Hex(faceMode==1?"174c3c31010000000101070202cdab0301de00":"174c3c310100000000");
        input.push_back(0xa1);input.push_back(static_cast<std::uint8_t>(face.size()));Append(input,face);
        expected.push_back(0xe1);expected.push_back(static_cast<std::uint8_t>(face.size()));Append(expected,Hex("000000"));Append(expected,face);
    }
    input.push_back(0);expected.push_back(0);
    spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
    spMeshBVSerializer serializer;spMemoryStream stream;spMeshBV mesh;std::string error;
    Check(!mesh.GetDataForAnalysis()&&mesh.GetBoundingRadiusForAnalysis()==0,"PC factory defaults");
    Open(stream,input);Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(input.size()),mesh,&error),error);
    Check(mesh.GetBoundingCenterForAnalysis()==spMeshBV::Vector3{1,2,0},"PC sphere center");
    Check(mesh.GetBoundingRadiusForAnalysis()==2.2360680103302f,"PC exact float radius");
    Check(mesh.GetDataForAnalysis()->GetIndicesForAnalysis()->GetIndexCountForAnalysis()==3,"shared index reader");
    Check(mesh.GetDataForAnalysis()->GetVerticesForAnalysis()->GetVertexCountForAnalysis()==3,"shared vertex reader");
    const auto* faces=mesh.GetDataForAnalysis()->GetFacesForAnalysis();
    Check(bool(faces)==bool(faceMode),"optional face owner remains distinguishable");
    if(faces) {
        Check(faces->GetElementClassForAnalysis()==wxFaceData::ClassID&&faces->GetElementsForAnalysis().size()==1,"original RTTI face array");
        const auto* face=dynamic_cast<const wxFaceData*>(faces->GetElementsForAnalysis()[0].get());
        Check(face&&face->GetSurfaceTypeForAnalysis()==(faceMode==1?7:0)&&face->GetFlagsForAnalysis()==(faceMode==1?43981:0)
            &&face->GetSurfaceIDForAnalysis()==(faceMode==1?222:0),"PC numeric face fields and defaults");
        auto copyBase=face->Clone();const auto* copy=dynamic_cast<const wxFaceData*>(copyBase.get());
        Check(copy&&copy->GetFlagsForAnalysis()==face->GetFlagsForAnalysis()&&copy->GetSurfaceIDForAnalysis()==face->GetSurfaceIDForAnalysis(),"PC face clone copy dispatch");
    }
    spMeshBV::Vector3 p{7,8,9};spMeshBV::Matrix3 r{1,0,0,0,1,0,0,0,1};const auto oldP=p;const auto oldR=r;
    mesh.UpdateCollisionTransformForAnalysis(p,r,{2,3,4});Check(p==oldP&&r==oldR,"PC MeshBV transform slot is no-op");
    Open(stream);Check(serializer.WritePayloadForAnalysis(stream,mesh,&error),error);Check(Data(stream)==expected,"exact original writer bytes");
    mesh.SetName("mesh collision");auto cloneBase=mesh.Clone();const auto* clone=dynamic_cast<const spMeshBV*>(cloneBase.get());
    Check(clone&&!clone->GetDataForAnalysis()&&std::string(clone->GetName())=="mesh collision","PC inherited clone copies name only");
    const auto radius=mesh.GetBoundingRadiusForAnalysis();Check(mesh.SetDataAndBoundsForAnalysis(nullptr),"PC null replacement succeeds");
    Check(!mesh.GetDataForAnalysis()&&mesh.GetBoundingRadiusForAnalysis()==radius,"PC null replacement preserves bounds");
    Open(stream);Check(!serializer.WritePayloadForAnalysis(stream,mesh,&error),"host rejects native missing geometry dereference");
}
void Invalid() {
    const auto input=Hex("a109174c3c31010000000000");spMemoryStream stream;Open(stream,input);
    spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
    spMeshBV mesh;spMeshBVSerializer serializer;std::string error;
    Check(!serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(input.size()),mesh,&error)&&context.failed,
        "host explicitly rejects native face-before-geometry null access");
    wxFaceData face;Open(stream,Hex("0101070202cdab0301de00"));Check(face.ReadForAnalysis(stream,11),"nondefault face reader");
    Open(stream,Hex("00"));Check(face.ReadForAnalysis(stream,1)&&face.GetFlagsForAnalysis()==0&&face.GetSurfaceTypeForAnalysis()==0,"original reader resets previous values");
    Open(stream,Hex("0101"));Check(!face.ReadForAnalysis(stream,2),"truncated face refuses success");
    Open(stream,Hex("808080808000"));Check(!face.ReadForAnalysis(stream,6),"host bounds malformed small-int");
    Open(stream,Hex("ac0202aa550301110101090202cdab0301de00"));
    Check(face.ReadForAnalysis(stream,19)&&face.GetSurfaceTypeForAnalysis()==9&&face.GetFlagsForAnalysis()==43981
        &&face.GetSurfaceIDForAnalysis()==222,"original skips unknown ID300 and applies repeated fields in order");
    Open(stream);Check(face.WriteForAnalysis(stream),"face canonical writer");
    Check(Data(stream)==Hex("0101090202cdab0301de00"),"exact PC writer after unknown/repeated input");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--vertex-sphere") {
            std::string text;std::getline(std::cin,text);spMemoryStream stream;Open(stream,Hex(text));
            spVertexBuffer vertices;Check(vertices.ReadForAnalysis(stream,static_cast<std::uint32_t>(text.size()/2)),"vertex input");
            std::array<float,4> sphere{};Check(sparkplug::evidence::pc::ComputeVertexSphere(vertices,sphere),"sphere producer");
            std::array<std::uint32_t,4> bits{};std::memcpy(bits.data(),sphere.data(),16);
            std::cout<<'['<<bits[0]<<','<<bits[1]<<','<<bits[2]<<','<<bits[3]<<"]\n";return 0;
        }
        Case(0);Case(1);Case(2);Invalid();std::cout<<"PASS "<<checks<<" MeshBV/face core checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
