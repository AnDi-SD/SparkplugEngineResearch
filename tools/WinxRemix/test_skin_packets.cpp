#include "winx_skin_packet_remix.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
namespace packet=winx_remix::skin_packet;
static unsigned checks;
static void Check(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
static packet::Matrix4 Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
struct Input {
  uint32_t influences,flags,stride=0,base=2,count=5,start=3,triangles=3,type=3;
  unsigned position=0,normal=0,weight=0,bone=0,uv=0,color=0;
  std::vector<packet::Element> elements;
  std::vector<uint8_t> vertices,indices;
  std::vector<packet::Matrix4> palette;
  explicit Input(unsigned influenceCount,unsigned attributes=3):influences(influenceCount),flags(0x20|0x40|(influenceCount==1?2:influenceCount==2?4:8)|
      (attributes&1?0x800:0)|(attributes&2?0x100:0)),palette(4,Identity()) {
    uint32_t allocation=0;Check(sparkplug::reconstruction::BuildPCVertexDeclarationElementsForAnalysis(flags,elements,allocation),"shared original declaration emitter");
    for(const auto& e:elements){if(e.stream==0xff)break;stride=(std::max)(stride,unsigned(e.offset)+(e.type<=3?(e.type+1)*4:4));
      if(e.usage==1)weight=e.offset;if(e.usage==2)bone=e.offset;if(e.usage==3)normal=e.offset;
      if(e.usage==5)uv=e.offset;if(e.usage==10)color=e.offset;}
    vertices.resize((base+count)*stride,0xcc);indices.resize((start+triangles+2)*2,0xff);
    for(unsigned v=0;v<count;++v){
      for(unsigned c=0;c<3;++c){Real(v,position+c*4,float(v*10+c));Real(v,normal+c*4,c==2?1.f:0.f);}
      for(unsigned b=0;b<influences;++b){Real(v,weight+b*4,1.f/influences);Real(v,bone+b*4,float((v+b)%4));}
      if(attributes&1){Real(v,uv,float(v)*.125f);Real(v,uv+4,.75f);}
      if(attributes&2){const uint32_t argb=0x12345670+v;memcpy(vertices.data()+(base+v)*stride+color,&argb,4);}
    }
    const uint16_t order[5]={2,0,1,1,3};for(unsigned i=0;i<5;++i)Index(i,order[i]);
    Real(4,0,(std::numeric_limits<float>::quiet_NaN)());
    for(unsigned b=0;b<4;++b){palette[b][12]=float(b*2);palette[b][13]=float(b*3);palette[b][14]=float(b*5);}
  }
  void Real(unsigned v,unsigned offset,float value){memcpy(vertices.data()+(base+v)*stride+offset,&value,4);}
  void Index(unsigned i,uint16_t value){memcpy(indices.data()+(start+i)*2,&value,2);}
  bool Build(packet::Packet& out,packet::Error* error=nullptr){return packet::Build(vertices,indices,elements.data(),elements.size(),stride,base,count,start,triangles,type,influences,palette.data(),palette.size(),out,error);}
};
static std::vector<uint8_t> Encoded(const packet::Packet& p){std::vector<uint8_t> bytes;Check(packet::Encode(p,bytes),"packet encoding");return bytes;}
static void Reject(Input input,const char* why){Input valid(1);packet::Packet out;Check(valid.Build(out),"sentinel packet");const auto before=Encoded(out);packet::Error error{};
  Check(!input.Build(out,&error)&&error!=packet::Error::None,why);Check(Encoded(out)==before,"failed build preserves previous packet");}
static void Run(){
  for(unsigned b=1;b<=4;++b)for(unsigned attributes=0;attributes<4;++attributes){Input input(b,attributes);packet::Packet p;
    const auto original=input.vertices;Check(input.Build(p),"all B1..4 and optional attributes");
    Check(input.vertices==original&&p.influences==b&&p.attributes==attributes,"input bytes and declared attributes preserved");
    Check(p.vertices.size()==4&&p.indices==std::vector<uint32_t>({0,1,2,2,1,2,2,2,3}),"odd strip winding, repeated and degenerate indices preserved");
    Check(p.vertices[0].sourceIndex==2&&p.vertices[1].sourceIndex==0&&p.vertices[2].sourceIndex==1&&p.vertices[3].sourceIndex==3,"first-reference remapping independent of source order");
    Check(p.vertices[0].skin.position==packet::pc::FixedSkinVector({20,21,22,1})&&p.vertices[0].skin.normal==packet::pc::FixedSkinVector({0,0,1,1}),"FLOAT3 shader default fourth components");
    Check(p.vertices[0].color==(attributes&2?0x12345672u:0xffffffffu)&&p.vertices[0].uv[0]==(attributes&1?.25f:0.f),"authored ARGB/UV and declared defaults");
    Check(p.palette[3][0][3]==6&&p.palette[3][1][3]==9&&p.palette[3][2][3]==15,"native row-major palette becomes shader rows");
    const auto bytes=Encoded(p);packet::Packet loaded;Check(packet::Decode(bytes,loaded)&&Encoded(loaded)==bytes,"lossless bounded wire round trip");
    auto expected=std::vector<packet::pc::FixedSkinDeformationForAnalysis>{};Check(packet::Deform(loaded,expected)&&expected.size()==4,"shared Fixed evaluates decoded packet");
    input.vertices.assign(input.vertices.size(),0);input.palette.assign(4,Identity());std::vector<packet::pc::FixedSkinDeformationForAnalysis> repeated;
    Check(packet::Deform(p,repeated)&&repeated[0].position==expected[0].position,"packet owns vertices and matrices after source mutation");
  }
  {Input input(2);input.type=2;input.triangles=1;input.Index(0,3);input.Index(1,1);input.Index(2,2);packet::Packet p;
    Check(input.Build(p)&&p.indices==std::vector<uint32_t>({0,1,2})&&p.vertices[0].sourceIndex==3,"triangle list uses exact indexBegin and vertexBegin");}
  {Input input(2);input.palette.assign(4,Identity());for(unsigned v=0;v<4;++v){input.Real(v,input.weight,-.25f);input.Real(v,input.weight+4,.75f);}
    packet::Packet p;Check(input.Build(p),"negative nonunit weights remain valid original data");std::vector<packet::pc::FixedSkinDeformationForAnalysis> out;
    Check(packet::Deform(p,out)&&out[0].position==packet::pc::FixedSkinVector({10,10.5f,11,1}),"explicit last weight and negative contribution reach recovered formula");}
  {Input input(1);input.palette.assign(16,Identity());input.Real(2,input.bone,15);packet::Packet p;Check(input.Build(p),"all16 original shader matrices admitted");input.palette.push_back(Identity());Reject(input,"Fixed shader palette bound16");}
  {Input x(2);x.Real(0,x.bone,.5f);Reject(x,"fractional active bone");}
  {Input x(2);x.Real(0,x.weight,INFINITY);Reject(x,"nonfinite weight");}
  {Input x(2);x.Real(0,x.normal,NAN);Reject(x,"nonfinite referenced normal");}
  {Input x(2);x.Index(2,4);Reject(x,"formerly-unused nonfinite vertex becomes referenced");}
  {Input x(2);x.Index(2,5);Reject(x,"index outside declared subrange");}
  {Input x(2);x.base=UINT32_MAX;Reject(x,"overflowing vertex base");}
  {Input x(2);x.start=UINT32_MAX;Reject(x,"overflowing index base");}
  {Input x(2);x.vertices.pop_back();Reject(x,"truncated declared vertex range");}
  {Input x(2);x.indices.pop_back();Reject(x,"truncated index range");}
  {Input x(2);x.palette[0][3]=1;Reject(x,"projective native matrix cannot be silently truncated");}
  {Input x(2);x.elements[0].type=3;Reject(x,"FLOAT4 position is a different contract");}
  {Input x(2);x.elements[1].offset=4;Reject(x,"overlapping declaration fields");}
  {Input x(2);x.elements.back().type=0;Reject(x,"invalid declaration terminator");}
  {Input x(2);for(auto& e:x.elements)if(e.usage==3&&e.stream!=0xff)e.usage=6;Reject(x,"missing normal has no invented replacement");}
  {Input x(2);packet::Packet p;Check(x.Build(p),"wire input");const auto before=Encoded(p);auto bytes=before;
    for(size_t size:{size_t(0),size_t(31),bytes.size()-1}){auto cut=std::vector<uint8_t>(bytes.begin(),bytes.begin()+size);Check(!packet::Decode(cut,p)&&Encoded(p)==before,"truncation does not publish partial packet");}
    bytes.push_back(0);Check(!packet::Decode(bytes,p)&&Encoded(p)==before,"trailing data rejected");
    for(unsigned offset:{0u,4u,8u,12u,16u,20u,24u,28u}){bytes=before;std::fill_n(bytes.begin()+offset,4,uint8_t(0xff));Check(!packet::Decode(bytes,p)&&Encoded(p)==before,"malformed wire header rejects before allocation");}
    bytes=before;std::fill_n(bytes.end()-4,4,uint8_t(0xff));Check(!packet::Decode(bytes,p)&&Encoded(p)==before,"wire index range validated");
    std::vector<packet::pc::FixedSkinDeformationForAnalysis> out(1);out[0].position={1,2,3,4};
    for(auto& v:p.vertices)v.skin.normal={0,0,0,1};Check(!packet::Deform(p,out)&&out.size()==1&&out[0].position[3]==4,"undefined normal direction refuses without changing output");
  }
  {Input input(2,0);packet::Packet p;Check(input.Build(p),"absent attribute packet");const auto original=Encoded(p);
    auto bytes=original;const auto vertexOffset=32+p.palette.size()*48;
    const float nondefault=1;memcpy(bytes.data()+vertexOffset+64,&nondefault,4);
    Check(!packet::Decode(bytes,p)&&Encoded(p)==original,"absent UV cannot carry undeclared values");
    bytes=original;bytes[vertexOffset+72]=0;
    Check(!packet::Decode(bytes,p)&&Encoded(p)==original,"absent color cannot carry undeclared values");}
}
static void RemixPreparation(){
  const std::array<std::array<float,3>,4> expected={{{24,27,32},{25,28.5f,34.5f},{22.5f,24.75f,28.25f},{23,25.5f,29.5f}}};
  for(unsigned b=1;b<=4;++b){Input input(b);
    const std::array<float,4> weights=b==1?std::array<float,4>{1,0,0,0}:b==2?std::array<float,4>{.5f,.5f,0,0}:b==3?std::array<float,4>{.25f,.25f,.5f,0}:std::array<float,4>{.25f,.25f,.25f,.25f};
    for(unsigned v=0;v<input.count;++v)for(unsigned i=0;i<b;++i)input.Real(v,input.weight+i*4,weights[i]);
    packet::Packet p;Check(input.Build(p),"binary normalized B1..4 bake source");const auto before=Encoded(p);
    packet::WeightCompatibility compatibility;packet::BakedMesh baked;
    Check(packet::InspectRemixWeights(p,compatibility)&&compatibility.Exact(),"exact effective Remix weights B1..4");
    Check(packet::Bake(p,baked)&&baked.vertices[0].position==expected[b-1],"baked world positions retain authored weights and palette translation");
    Check(baked.vertices[0].normal==std::array<float,3>{0,0,1},"baked normals are unit directions, not Fixed float4 magnitudes");
    Check(baked.indices==p.indices&&baked.attributes==p.attributes&&baked.vertices[0].uv==p.vertices[0].uv&&baked.vertices[0].color==p.vertices[0].color,"bake preserves winding, UV, color and presence flags");
    Check(Encoded(p)==before,"bake and weight inspection do not alter the source packet");
    p.vertices[0].skin.position[0]=0;p.indices[0]=3;p.palette.clear();
    Check(baked.vertices[0].position==expected[b-1]&&baked.indices[0]==0,"baked storage survives source mutation and palette destruction");
    const auto prior=baked.vertices[0].position;Check(!packet::Bake(p,baked)&&baked.vertices[0].position==prior,"invalid source preserves previous baked output");
  }
  for(const auto weights:std::array<std::array<float,2>,3>{{{.25f,.25f},{1.25f,-.25f},{.5f,.5f+0x1p-23f}}}){
    Input input(2);for(unsigned v=0;v<input.count;++v)for(unsigned i=0;i<2;++i)input.Real(v,input.weight+i*4,weights[i]);
    packet::Packet p;packet::WeightCompatibility compatibility;packet::BakedMesh baked;Check(input.Build(p),"nonrepresentable authored weights");
    Check(packet::InspectRemixWeights(p,compatibility)&&!compatibility.Exact(),"nonunit, negative and near-unit weights need the baked path");
    Check(packet::Bake(p,baked),"original nonrepresentable weights remain bakeable");
    if(weights[0]>1)Check(compatibility.changedVertices==0&&compatibility.negativeVertices==4,"negative weight remains incompatible even with exact implicit last");
    if(weights[0]==.5f)Check(compatibility.maximumLastWeightDelta>0&&compatibility.maximumLastWeightDelta<1e-5,"sum tolerance is not an admission rule");
  }
  {Input input(1);for(unsigned v=0;v<input.count;++v)input.Real(v,input.weight,2);
    packet::Packet p;packet::WeightCompatibility compatibility;packet::BakedMesh baked;Check(input.Build(p),"explicit B1 weight2");
    Check(packet::InspectRemixWeights(p,compatibility)&&compatibility.changedVertices==4&&!compatibility.Exact(),"B1 implicit weight1 differs from authored2");
    Check(packet::Bake(p,baked)&&baked.vertices[0].position==std::array<float,3>{48,54,64},"B1 bake preserves the explicit weight2");}
  {Input input(3);for(unsigned v=0;v<input.count;++v)for(unsigned i=0;i<3;++i)input.Real(v,input.weight+i*4,(std::numeric_limits<float>::max)());
    packet::Packet p;packet::WeightCompatibility compatibility;Check(input.Build(p),"finite source weights may overflow the remainder");
    Check(packet::InspectRemixWeights(p,compatibility)&&compatibility.nonfiniteRemainders==4&&!compatibility.Exact(),"nonfinite effective remainder cannot qualify");}
}
static int Inspect(const char* path,bool dump=false){
  std::ifstream stream(path,std::ios::binary|std::ios::ate);if(!stream)throw std::runtime_error("cannot open packet");
  const auto size=stream.tellg();if(size<0||uint64_t(size)>packet::MaximumWireBytes)throw std::runtime_error("packet size bound");
  std::vector<uint8_t> bytes(size_t(size),0);stream.seekg(0);if(!stream.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("partial read");
  packet::Packet p;std::vector<packet::pc::FixedSkinDeformationForAnalysis> deformed;packet::Error error;
  if(!packet::Decode(bytes,p,&error)||!packet::Deform(p,deformed,&error))throw std::runtime_error("packet decode/deformation refused");
  printf("{\"status\":\"PASS\",\"vertices\":%zu,\"triangles\":%zu,\"influences\":%u,\"bones\":%zu,\"gpu\":false",p.vertices.size(),p.indices.size()/3,p.influences,p.palette.size());
  if(dump){fputs(",\"deformed\":[",stdout);bool first=true;
    for(const auto& vertex:deformed){printf("%s[",first?"":",");first=false;
      for(unsigned i=0;i<4;++i)printf("%s%.9g",i?",":"",double(vertex.position[i]));
      for(float value:vertex.normal)printf(",%.9g",double(value));fputc(']',stdout);}
    fputc(']',stdout);}
  fputs("}\n",stdout);return ferror(stdout)?1:0;
}
int main(int argc,char** argv){try{
  if(argc==3&&std::string(argv[1])=="--inspect")return Inspect(argv[2]);
  if(argc==3&&std::string(argv[1])=="--deform-json")return Inspect(argv[2],true);
  if(argc!=1)throw std::runtime_error("usage: test_skin_packets [--inspect|--deform-json packet.skp]");
  Run();RemixPreparation();printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"gameCode\":false}\n",checks);return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
