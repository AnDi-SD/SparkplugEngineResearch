// Own CPU validation/ABI fixture. The shared emitter supplies declarations;
// literal storage supplies observations. No game entry point or COM executes.
#define main skin_observer_fixture_main
#include "test_native_skin_source.cpp"
#undef main
#include "winx_native_skin_vertex_source.h"
namespace skin_vertex_test {
namespace audit=native_skin_vertex_source;
namespace source=native_mesh_source;
namespace transport=native_transport_source;
namespace abi=sparkplug::evidence::pc;
using skin_test::Check;using skin_test::Ptr;using skin_test::Put;
static unsigned parserChecks=0,integrationChecks=0;
static float NaN(){return (std::numeric_limits<float>::quiet_NaN)();}
struct Data {
  unsigned influences,flags,stride=0,base=2,count=4,start=2,primitives=1,type=2,palette=4;
  unsigned weight=0,bone=0;
  std::vector<D3DVERTEXELEMENT9> elements;
  std::vector<uint8_t> vertices,indices;
  explicit Data(unsigned b,bool uv=false):influences(b),flags(0x20|(b==1?2:b==2?4:8)|(uv?0x800:0)){
    std::vector<sparkplug::reconstruction::spPCVertexElementForAnalysis> emitted;uint32_t allocation=0;
    Check(sparkplug::reconstruction::BuildPCVertexDeclarationElementsForAnalysis(flags,emitted,allocation),"actual shared PC emitter");
    elements.resize(emitted.size());memcpy(elements.data(),emitted.data(),emitted.size()*sizeof(elements[0]));
    for(const auto& e:elements){if(e.Stream==0xff)break;
      const unsigned bytes=e.Type<=3?(e.Type+1)*4:4;stride=(std::max)(stride,unsigned(e.Offset)+bytes);
      if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT)weight=e.Offset;if(e.Usage==D3DDECLUSAGE_BLENDINDICES)bone=e.Offset;}
    vertices.resize((base+count)*stride,0);indices.resize((start+3)*2,0xff);
    for(unsigned v=0;v<count;++v){Float(v,0,float(v));Float(v,4,float(v+1));Float(v,8,5);
      for(unsigned j=0;j<influences;++j){Float(v,weight+j*4,1.f/influences);Float(v,bone+j*4,float(j));}}
    Index(0,0);Index(1,1);Index(2,2);
    Float(3,0,NaN()); // Unreferenced native vertices are not submitted.
  }
  void Float(unsigned vertex,unsigned offset,float value){memcpy(vertices.data()+(base+vertex)*stride+offset,&value,4);}
  void Index(unsigned ordinal,uint16_t value){memcpy(indices.data()+(start+ordinal)*2,&value,2);}
  bool Inspect(audit::Summary& out){return audit::Inspect(vertices,indices,elements.data(),unsigned(elements.size()),stride,base,count,start,primitives,type,influences,palette,out);}
};
static bool Empty(const audit::Summary& out){return !out.vertices&&!out.influences&&!out.negative&&!out.nonUnit&&!out.maxIndex&&!out.maxSumError;}
static void Reject(Data data,const char* label){audit::Summary result{};result.vertices=77;result.influences=4;result.maxSumError=123;
  Check(!data.Inspect(result)&&Empty(result),label);}
static void Parser(){const auto before=skin_test::checks;
  for(unsigned b:{1u,2u,4u}){Data data(b);audit::Summary out{};const auto vb=data.vertices,ib=data.indices;
    Check(data.Inspect(out)&&out.vertices==3&&out.influences==b&&!out.negative&&!out.nonUnit&&out.maxIndex==b-1,"B1/B2/B4 full explicit weights through shared declaration");
    Check(data.vertices==vb&&data.indices==ib,"inspection preserves all source bytes");
    data.Index(1,0);Check(data.Inspect(out)&&out.vertices==2,"repeated indices count a vertex only once");
    data.Index(1,1);data.type=3;data.primitives=2;data.indices.resize((data.start+4)*2);data.Index(3,1);
    Check(data.Inspect(out)&&out.vertices==3,"triangle strip indexCount=primitiveCount+2 and repeated indices");
    data.Index(3,3);Reject(data,"strip reaches nonfinite formerly-unused vertex");
  }
  {Data data(2);audit::Summary out{};for(unsigned v=0;v<3;++v){data.Float(v,data.weight,.2f);data.Float(v,data.weight+4,.3f);}
    const auto bytes=data.vertices;Check(data.Inspect(out)&&out.nonUnit==3&&out.maxSumError>.49&&out.maxSumError<.51,"explicit final weight is read, never reconstructed as1-sum");
    Check(data.vertices==bytes,"nonunit weights are diagnostic and unchanged");
    for(unsigned v=0;v<3;++v){data.Float(v,data.weight,-.25f);data.Float(v,data.weight+4,1.25f);}
    Check(data.Inspect(out)&&out.negative==3&&!out.nonUnit&&out.minWeight==-.25f&&out.maxWeight==1.25f,"negative authored weights preserved, not clamped or normalized");}
  {Data data(1);audit::Summary out{};for(unsigned v=0;v<3;++v)data.Float(v,data.weight,2.f);
    Check(data.Inspect(out)&&out.minWeight==2&&out.maxWeight==2&&out.nonUnit==3,"all weights above1 retain true extrema");
    for(unsigned v=0;v<3;++v)data.Float(v,data.weight,-.5f);
    Check(data.Inspect(out)&&out.minWeight==-.5f&&out.maxWeight==-.5f&&out.negative==3,"all negative weights retain true extrema");
    data.Float(0,data.bone+4,NaN());Check(data.Inspect(out),"unused B1 bone lanes have no semantic effect");}
  {Data d(4);d.Float(0,d.bone,.5f);Reject(d,"fractional active bone index rejects");}
  {Data d(4);d.Float(0,d.bone,-1);Reject(d,"negative active bone index rejects");}
  {Data d(4);d.Float(0,d.bone,4);Reject(d,"index equal to palette count rejects");}
  {Data d(4);d.palette=256;d.Float(0,d.bone,255);audit::Summary out{};Check(d.Inspect(out)&&out.maxIndex==255,"exact maximum palette index255 supported");d.palette=257;Reject(d,"palette bound256");}
  {Data d(4);d.Float(0,d.bone,NaN());Reject(d,"NaN bone index rejects");}
  {Data d(4);d.Float(0,d.weight,NaN());Reject(d,"NaN weight rejects");}
  {Data d(4);d.Float(0,8,(std::numeric_limits<float>::infinity)());Reject(d,"infinite used position rejects");}
  {Data d(4);d.Index(1,4);Reject(d,"index cannot exceed declared vertex range");}
  {Data d(4);d.Index(1,65535);Reject(d,"uint16 maximum cannot escape source range");}
  {Data d(4);d.indices.pop_back();Reject(d,"truncated16bit indices reject");}
  {Data d(4);d.vertices.pop_back();Reject(d,"full declared vertex range must fit even when last vertex unused");}
  {Data d(4);--d.stride;Reject(d,"declared indices cannot overrun stride");}
  {Data d(4,true);d.elements[d.elements.size()-2].Offset=uint16_t(d.stride);Reject(d,"unused UV declaration must fit complete stride");}
  {Data d(2);d.elements[1].Offset=28;d.elements[2].Offset=12;d.stride=40;Reject(d,"FLOAT4 weights require16 bytes even with only2 active influences");}
  {Data d(4);d.base=0xffffffff;Reject(d,"vertex base arithmetic uses64bits");}
  {Data d(4);d.start=0xffffffff;Reject(d,"index base arithmetic uses64bits");}
  {Data d(4);d.count=65537;Reject(d,"vertex bound");}
  {Data d(4);d.primitives=32769;Reject(d,"primitive bound");}
  {Data d(4);d.count=0;Reject(d,"empty vertex range rejects");}
  {Data d(4);d.primitives=0;Reject(d,"empty primitive range rejects");}
  {Data d(4);d.type=1;Reject(d,"unsupported native primitive type rejects");}
  {Data d(4);d.influences=0;Reject(d,"zero influences reject");}
  {Data d(4);d.influences=5;Reject(d,"too many influences reject");}
  {Data d(4);d.palette=0;Reject(d,"empty palette rejects");}
  {Data d(4);d.elements.back().Stream=0;Reject(d,"missing declaration terminator rejects");}
  {Data d(4);d.elements[0].Stream=1;Reject(d,"foreign declaration stream rejects");}
  {Data d(4);d.elements[0].Method=1;Reject(d,"nondefault declaration method rejects");}
  {Data d(4);d.elements[1].Type=D3DDECLTYPE_FLOAT2;Reject(d,"wrong weight declaration type rejects");}
  {Data d(4);d.elements[2].Type=D3DDECLTYPE_UBYTE4;Reject(d,"unexpanded packed bone declaration rejects");}
  {Data d(4);d.elements.insert(d.elements.end()-1,d.elements[1]);Reject(d,"duplicate weights reject");}
  {Data d(4);audit::Summary out{};out.vertices=77;Check(!audit::Inspect(d.vertices,d.indices,nullptr,4,d.stride,d.base,d.count,d.start,1,2,4,4,out)&&Empty(out),"null layout clears failed output");}
  parserChecks=skin_test::checks-before;
}
struct Integrated {
  skin_test::Fixture f;Data data{4,true};abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  uint32_t deviceSlot=0xdead1100,vertexSlot=0xdead2200,indexSlot=0xdead3300,declSlot=0xdead4400,declaration[7]{};
  IDirect3DDevice9* Device(){return reinterpret_cast<IDirect3DDevice9*>(&deviceSlot);}
  Integrated(){
    frameId=300;f.hookSlot=Ptr(reinterpret_cast<void*>(&native_skin_source::SkinDraw));
    native_skin_source::enabled=true;native_skin_source::ownerThread=GetCurrentThreadId();
    source::enabled=true;source::ownerThread=GetCurrentThreadId();transport::enabled=true;
    f.skin.boneCount=4;Put(f.renderer,0xc9bc,4);Put(f.renderer,0xc9e8,Ptr(Device()));
    f.mesh.base.base.vertexComponentFlags=data.flags;f.mesh.base.base.vertexCount=data.count;f.mesh.base.base.primitiveCount=data.primitives;
    f.mesh.indexType=data.type;f.mesh.vertexStride=data.stride;f.mesh.vertexBegin=data.base;f.mesh.indexBegin=data.start;
    f.mesh.vertexBuffer=Ptr(&vb);f.mesh.indexBuffer=Ptr(&ib);f.mesh.vertexDeclaration=Ptr(declaration);f.mesh.fvfCode=0x1234;
    vb.base.vtableAddress=abi::spDXVertexBufferVTable;vb.direct3DVertexBuffer=Ptr(&vertexSlot);vb.byteSize=unsigned(data.vertices.size());
    ib.base.vtableAddress=abi::spDXIndexBufferVTable;ib.direct3DIndexBuffer=Ptr(&indexSlot);ib.byteSize=unsigned(data.indices.size());
    declaration[0]=0x6f2e58;declaration[5]=f.mesh.fvfCode;declaration[6]=Ptr(&declSlot);
    source::Bytes v{},i{};v.data=data.vertices;i.data=data.indices;v.generation=i.generation=37;v.nativeBuffer=Ptr(&vb);i.nativeBuffer=Ptr(&ib);
    v.source=i.source="owned_fixture_capture";source::buffers.emplace(&vertexSlot,v);source::buffers.emplace(&indexSlot,i);
    source::retainedBytes=v.data.size()+i.data.size();surfaceBuffers[&vertexSlot]={v.data,true};surfaceBuffers[&indexSlot]={i.data,true};
    std::unique_lock<std::recursive_mutex> lock(guard);Remember();
    audit::enabled=true;audit::output=fopen("native-skin-vertices-fixture.jsonl","wb");Check(audit::output!=nullptr,"owned diagnostic output");
  }
  void Remember(){
    Check(transport::Remember(Device(),&vertexSlot,transport::Kind::Vertex,vb.byteSize,D3DFMT_VERTEXDATA),"owned successful VB observation");
    Check(transport::Remember(Device(),&indexSlot,transport::Kind::Index,ib.byteSize,D3DFMT_INDEX16),"owned successful IB observation");
    Check(transport::Remember(Device(),&declSlot,transport::Kind::Declaration,0,D3DFMT_UNKNOWN,data.elements.data(),unsigned(data.elements.size())),"owned successful declaration observation");
  }
  ~Integrated(){if(audit::output)fclose(audit::output);audit::output=nullptr;audit::enabled=false;
    std::lock_guard<std::recursive_mutex> lock(guard);transport::RetireDevice(Device());transport::enabled=false;
    source::buffers.clear();source::retainedBytes=0;source::layouts.clear();source::enabled=false;surfaceBuffers.clear();surfaceWrites.clear();native_skin_source::enabled=false;}
};
static void Integration(){const auto before=skin_test::checks;Integrated x;
  scene_geometry::Scope scene(Ptr(x.f.scene),Ptr(x.f.camera));native_skin_source::Scope skin(Ptr(&x.f.skin),Ptr(x.f.camera),x.f.Support());
  native_skin_source::Observation packet{};
  Check(native_skin_source::Observe(Ptr(&x.f.mesh),Ptr(x.f.renderer),909,0x46a367,packet)&&packet.withinTolerance,"real palette observer qualifies owned Skin graph before vertex audit");
  auto good=[&](const char* text){const auto m=audit::matched,r=audit::rejected;audit::Capture(packet);Check(audit::matched==m+1&&audit::rejected==r,text);};
  auto bad=[&](const char* text){const auto m=audit::matched,r=audit::rejected;audit::Capture(packet);Check(audit::matched==m&&audit::rejected==r+1,text);};
  good("full native headers/current source/upload/layout/transport tuple accepted without COM");
  const auto m=audit::matched;native_skin_source::Capture(Ptr(&x.f.mesh),Ptr(x.f.renderer),910,0x46a367);
  Check(audit::matched==m+1,"production qualified Skin Capture invokes vertex audit exactly once");
  Check(!source::buffers.at(&x.vertexSlot).verified&&!source::buffers.at(&x.indexSlot).verified&&!testRemixApi,"observer does not silently mark uploads verified or call Remix API");
  surfaceBuffers[&x.vertexSlot].bytes[x.data.base*x.data.stride]^=1;bad("mismatched captured source/upload rejects");surfaceBuffers[&x.vertexSlot].bytes=x.data.vertices;
  surfaceBuffers[&x.indexSlot].complete=false;bad("partial transport upload rejects");surfaceBuffers[&x.indexSlot].complete=true;
  surfaceWrites.emplace(&x.vertexSlot,decltype(surfaceWrites)::mapped_type{});bad("open vertex write rejects");surfaceWrites.erase(&x.vertexSlot);
  surfaceWrites.emplace(&x.indexSlot,decltype(surfaceWrites)::mapped_type{});bad("open index write rejects");surfaceWrites.erase(&x.indexSlot);
  ++source::buffers[&x.indexSlot].generation;bad("source pair generation mismatch rejects");--source::buffers[&x.indexSlot].generation;
  auto& vertices=source::buffers[&x.vertexSlot];vertices.partial=true;vertices.ranges={{x.data.base*x.data.stride,(x.data.base+x.data.count)*x.data.stride}};
  good("exact fully-covered current vertex range accepted within partial combiner buffer");--vertices.ranges[0].second;bad("one uncovered byte rejects source range");vertices.partial=false;vertices.ranges.clear();
  auto& record=transport::records[&x.declSlot];++record.elements[1].Offset;bad("transport declaration differs from common emitter");--record.elements[1].Offset;
  ++x.declaration[5];bad("native declaration FVF identity rejects");--x.declaration[5];
  const auto oldStride=x.f.mesh.vertexStride;x.f.mesh.vertexStride=x.data.bone+16;bad("all declared elements including UV must fit native stride");x.f.mesh.vertexStride=oldStride;
  ++packet.modelCall;bad("foreign Skin call sequence rejects");--packet.modelCall;
  transport::Forget(&x.vertexSlot);bad("retired COM observation rejects stale native pointer");x.Remember();good("fresh observed transport tuple admits current source");
  transport::BeginReset(x.Device());bad("Reset retires and blocks transport tuple");transport::EndReset(x.Device());x.Remember();good("fresh Creates after Reset accepted");
  const auto attempts=audit::attempts;FILE* savedOutput=audit::output;audit::output=nullptr;audit::Capture(packet);
  audit::output=savedOutput;Check(audit::attempts==attempts,"missing diagnostic output causes no reads or credit");
  integrationChecks=skin_test::checks-before;
}
}
int main(){try{skin_test::Watchdog watchdog;skin_vertex_test::Parser();skin_vertex_test::Integration();
  printf("{\"status\":\"PASS\",\"checks\":%u,\"parserChecks\":%u,\"integrationChecks\":%u,\"gpu\":false,\"nativeCode\":false,\"realCOM\":false}\n",skin_test::checks,skin_vertex_test::parserChecks,skin_vertex_test::integrationChecks);return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
