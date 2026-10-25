#pragma once
// Own recording-API contract checks. Included only by the submit fixture.
namespace skin_gpu_test {
using namespace submit_test;
namespace gpu=winx_remix::skin_gpu_submit;
static gpu::Prepared PrepareGpu(const packet::Packet& source,DWORD cull=D3DCULL_NONE) {
  gpu::Prepared output;
  Check(gpu::Prepare(source,std::vector<uint32_t>(source.vertices.size(),0xffabcdef),cull,output),"signed preparation accepts original negative and nonunit weights");
  return output;
}
static void Payload(const gpu::Prepared& input) {
  Check(observedVertices.size()==input.vertices.size()&&observedIndices==input.indices&&observedWeights==input.skin.weights&&
        observedBones==input.skin.indices&&observedInfluences==input.skin.influences,"API receives all immutable signed arrays");
  Check(!memcmp(observedVertices.data(),input.vertices.data(),input.vertices.size()*sizeof(input.vertices[0])),"API preserves original undeformed vertex bytes");
}
static submit::DrawResult Draw(gpu::Prepared& input,remixapi_MeshHandle mesh,remixapi_MaterialHandle material) {
  activeGpu=&input;activeMesh=mesh;activeMaterial=material;drawState.cull=input.cull;
  return gpu::Draw(mesh,material,input,drawState,contract,[]{return true;});
}
static void DrawOkay(gpu::Prepared& input,remixapi_MeshHandle mesh,remixapi_MaterialHandle material) {
  const auto result=Draw(input,mesh,material);
  Check(result.apiCalled&&result.apiSucceeded&&result.stateStable,"one stable signed draw");
  Check(observedTransforms.size()==input.transforms.size()&&!memcmp(observedTransforms.data(),input.transforms.data(),input.transforms.size()*sizeof(input.transforms[0])),"draw receives this pose exactly once");
}
static void Run() {
  Clean();gpuMode=true;
  for(unsigned b=1;b<=4;++b)for(DWORD cull=D3DCULL_NONE;cull<=D3DCULL_CCW;++cull) {
    ++frameId;auto source=Quad(b);auto input=PrepareGpu(source,cull);const auto material=Material(900+b);
    const auto mesh=gpu::Resource(input,material,1);Check(mesh!=nullptr,"signed immutable mesh owned");Payload(input);
    Check(surfaceMeshBytes==4*64+6*4+4*(b+1)*8,"signed byte budget includes weights and bone indices");
    const auto before=creates;DrawOkay(input,mesh,material);
    ++frameId;source.palette[0][0][3]+=10;auto pose=PrepareGpu(source,cull);
    Check(gpu::Resource(pose,material,1)==mesh&&creates==before,"pose and frame changes reuse one immutable mesh");DrawOkay(pose,mesh,material);
    Check(gpu::Resource(pose,material,2)!=mesh,"identical geometry of another actor has a distinct owner");
    auto changed=input;changed.skin.weights[0]+=.125f;
    Check(gpu::Resource(changed,material,1)!=mesh,"authored weight change creates new immutable geometry");
    changed=input;changed.vertices[0].color^=1;Check(gpu::Resource(changed,material,1)!=mesh,"material vertex color participates in signed key");
    changed=input;std::swap(changed.indices[0],changed.indices[1]);Check(gpu::Resource(changed,material,1)!=mesh,"signed topology participates in key");
    const auto otherMaterial=Material(1900+b);Check(gpu::Resource(input,otherMaterial,1)!=mesh,"signed material identity participates in key");
    const auto calls=creates;
    Check(!gpu::Resource(input,material,0)&&!gpu::Resource(input,reinterpret_cast<remixapi_MaterialHandle>(99999),1)&&creates==calls,"unknown owner and material reject before API");
    auto shortPalette=input;shortPalette.transforms.resize(3);std::fill(shortPalette.skin.indices.begin(),shortPalette.skin.indices.end(),0);
    Check(gpu::Layout(shortPalette)&&!Draw(shortPalette,mesh,material).apiCalled,"palette must cover cached mesh indices even when replacement input remaps its own indices");
    auto baked=submit_test::Prepare(source,cull);
    Check(!submit::Draw(mesh,material,baked,drawState,contract).apiCalled,"baked instance cannot draw a skinned mesh without its palette");
    Clean();
  }
  {
    auto source=Quad(4);const auto untransformed=PrepareGpu(source);gpu::Prepared transformed;
    packet::pc::FixedUvRegistersForAnalysis uv{{{0,-1,2,NAN},{1,0,3,NAN},{.5f,-.25f,0,NAN}}};
    const auto colors=std::vector<uint32_t>(source.vertices.size(),0xffabcdef);
    Check(gpu::Prepare(source,colors,D3DCULL_NONE,transformed,&uv),"qualified UV transform prepares original signed geometry");
    for(size_t i=0;i<source.vertices.size();++i) {
      const auto& v=transformed.vertices[i];
      Check(v.texcoord[0]==source.vertices[i].uv[1]+.5f&&v.texcoord[1]==-source.vertices[i].uv[0]-.25f,"shader xy reaches API without homogeneous division");
      Check(!memcmp(v.position,untransformed.vertices[i].position,24)&&v.color==untransformed.vertices[i].color,"UV transform does not deform positions, normals or colors");
    }
    Check(transformed.skin.weights==untransformed.skin.weights&&transformed.skin.indices==untransformed.skin.indices&&transformed.indices==untransformed.indices,"UV transform retains signed weights and topology");
    const auto material=Material(2800);const auto mesh=gpu::Resource(transformed,material,7);
    Check(mesh!=nullptr,"transformed UV mesh is owned");Payload(transformed);DrawOkay(transformed,mesh,material);
    source.palette[0][0][3]+=2;gpu::Prepared pose;
    Check(gpu::Prepare(source,colors,D3DCULL_NONE,pose,&uv)&&gpu::Resource(pose,material,7)==mesh,"bone animation reuses the transformed UV mesh");
    uv[2][0]+=.25f;
    Check(gpu::Prepare(source,colors,D3DCULL_NONE,pose,&uv)&&gpu::Resource(pose,material,7)!=mesh,"changed UV output creates a distinct immutable resource");
    uv[2][0]-=.25f;
    Check(gpu::Prepare(source,colors,D3DCULL_NONE,pose,&uv)&&gpu::Resource(pose,material,7)==mesh,"restoring UV output reuses its existing resource");
    const auto previous=pose.vertices;uv[0][0]=NAN;
    Check(!gpu::Prepare(source,colors,D3DCULL_NONE,pose,&uv)&&!memcmp(pose.vertices.data(),previous.data(),previous.size()*sizeof(previous[0])),"invalid UV preparation preserves prior output");
    Clean();
  }
  auto input=PrepareGpu(Quad(4));auto material=Material(2900);auto mesh=gpu::Resource(input,material,3);DrawOkay(input,mesh,material);
  const auto refused=[&](const gpu::Prepared& bad){const auto before=creates;const auto count=draws;
    Check(!gpu::Resource(bad,material,3)&&!gpu::Draw(mesh,material,bad,drawState,contract,[]{return true;}).apiCalled&&creates==before&&draws==count,"malformed signed input does not reach API");};
  auto bad=input;bad.skin.weights.pop_back();refused(bad);bad=input;bad.skin.weights[0]=-1;refused(bad);
  bad=input;bad.skin.indices[0]=33;refused(bad);bad=input;bad.indices[0]=4;refused(bad);
  bad=input;bad.cull=0;refused(bad);bad=input;bad.transforms[0].matrix[0][0]=INFINITY;refused(bad);
  bad=input;bad.vertices[0].position[0]=NAN;refused(bad);bad=input;bad.vertices[0].normal[0]=NAN;refused(bad);
  bad=input;bad.vertices[0].texcoord[0]=NAN;refused(bad);
  unsigned predicates=0;auto before=draws;
  auto result=gpu::Draw(mesh,material,input,drawState,contract,[&]{++predicates;return false;});
  Check(!result.apiCalled&&predicates==1&&draws==before,"stale live state rejects immediately before draw commit");
  result=gpu::Draw(mesh,material,input,drawState,contract,[]{throw std::bad_alloc();return true;});
  Check(!result.apiCalled&&draws==before,"allocation failure before commit allows original fallback");
  reenterDraw=true;DrawOkay(input,mesh,material);reenterDraw=false;
  failDraw=true;result=Draw(input,mesh,material);failDraw=false;
  Check(result.apiCalled&&!result.apiSucceeded&&result.stateStable,"signed API failure cannot authorize a repeated original draw");
  throwDraw=true;result=Draw(input,mesh,material);throwDraw=false;
  Check(result.apiCalled&&!result.apiSucceeded,"signed exception after API commit remains irreversible");
  retireDraw=true;result=Draw(input,mesh,material);retireDraw=false;
  Check(result.apiCalled&&result.apiSucceeded&&!result.stateStable&&meshes.empty()&&materials.empty(),"reentrant retirement drains signed mesh before material");
  Check(!Draw(input,mesh,material).apiCalled,"retired signed handle rejected");Clean();
  material=Material(2901);failCreate=true;const auto destroyed=destroys;
  Check(!gpu::Resource(input,material,4)&&destroys==destroyed+1&&meshes.empty()&&!surfaceMeshBytes,"failed signed Create with a returned handle is destroyed once");failCreate=false;Clean();
  material=Material(2902);retireCreate=true;
  Check(!gpu::Resource(input,material,5)&&meshes.empty()&&materials.empty()&&!surfaceMeshBytes,"retirement inside signed Create cannot publish a usable handle");retireCreate=false;Clean();
  material=Material(2903);mesh=gpu::Resource(input,material,6);before=draws;
  result=gpu::Draw(mesh,material,input,drawState,contract,[]{RetireSurfaceResources(true);return true;});
  Check(!result.apiCalled&&draws==before&&meshes.empty()&&materials.empty(),"retirement during final predicate prevents API commit");Clean();
  gpuMode=false;activeGpu=nullptr;
}
} // namespace skin_gpu_test
