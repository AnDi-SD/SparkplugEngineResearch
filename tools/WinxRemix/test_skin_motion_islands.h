#pragma once
// Own two-island deformation oracle. Each island has constant B2 weights;
// separate immutable rigid meshes use the analytically weighted affine matrix.
static bool IslandAlbedoReady() {
  if(GetForegroundWindow()!=window)Fail("island readiness foreground",0);
  RECT rect{};POINT origin{};
  if(!GetClientRect(window,&rect)||rect.right!=960||rect.bottom!=540||!ClientToScreen(window,&origin))
    Fail("island readiness rectangle",GetLastError());
  HDC desktop=GetDC(nullptr);if(!desktop)Fail("island readiness DC",GetLastError());
  bool ready=true;
  for(const POINT pixel:{POINT{336,210},POINT{624,210},POINT{336,365},POINT{624,365}}){
    const auto color=GetPixel(desktop,origin.x+pixel.x,origin.y+pixel.y);
    if(color==CLR_INVALID){ReleaseDC(nullptr,desktop);Fail("island readiness pixel",0);}
    ready=ready&&GetRValue(color)>=200&&GetGValue(color)>=200&&GetBValue(color)>=200;
  }
  ReleaseDC(nullptr,desktop);return ready;
}
static remixapi_Transform IslandAffine(const remixapi_Transform& a,const remixapi_Transform& b,float first,float second){
  remixapi_Transform result{};
  for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)
    result.matrix[row][column]=first*a.matrix[row][column]+second*b.matrix[row][column];
  result.matrix[0][3]-=1.5f;result.matrix[1][3]-=.2f;
  return result;
}
static unsigned MotionIslands(const SkinVertices& baseInput,const std::vector<uint32_t>& triangleIndices,
    remixapi_MaterialHandle materialHandle,const remixapi_CameraInfo& camera){
  Config("rtx.postfx.enableMotionBlur","False");
  SkinVertices input;std::vector<uint32_t> topology;std::array<SkinVertices,2> islands;
  for(unsigned island=0;island<2;++island){
    islands[island]=baseInput;
    for(auto& vertex:islands[island]){vertex.position[0]*=.55f;vertex.position[1]=vertex.position[1]*.55f+(island==0?.8f:-.8f);}
    const auto offset=uint32_t(input.size());input.insert(input.end(),islands[island].begin(),islands[island].end());
    for(const auto index:triangleIndices)topology.push_back(offset+index);
  }
  fprintf(journal,"{\"event\":\"island_contract\",\"vertices\":%zu,\"islandVertices\":%zu,\"islands\":2,\"subdivisions\":24,\"separationPixels\":288,\"skinCategoryIgnoresMotion\":false,\"weights\":[[[0.25,0.75],[0.625,0.375]],[[-0.25,1.25],[1.5,-0.5]]],\"scale\":0.55,\"offsetY\":[0.8,-0.8]}\n",input.size(),baseInput.size());fflush(journal);
  const auto experimentStart=GetTickCount64();unsigned captures=0;
  for(unsigned profile=0;profile<2;++profile){
    const float weightPairs[2][2]={{profile?-.25f:.25f,profile?1.25f:.75f},{profile?1.5f:.625f,profile?-.5f:.375f}};
    std::vector<float> weights;std::vector<uint32_t> bones;
    for(unsigned island=0;island<2;++island)for(size_t v=0;v<baseInput.size();++v){
      weights.push_back(weightPairs[island][0]);weights.push_back(weightPairs[island][1]);bones.push_back(0);bones.push_back(1);
    }
    const std::array<remixapi_Transform,4> initial={Bone(0,0,0),Bone(0,0,0),Bone(0,0,0),Bone(0,0,0)};
    winx_remix::skin_packet::SignedSkin encoded;
    if(!winx_remix::skin_packet::EncodeSignedSkin(SourcePacket(input,topology,2,weights,bones,initial),encoded))Fail("island encoding",profile);
    const auto skin=Mesh(input,topology,materialHandle,0x575849534c531000ull+profile,encoded.influences,encoded.weights,encoded.indices);
    const remixapi_MeshHandle reference[]={
      Mesh(islands[0],triangleIndices,materialHandle,0x575849534c522000ull+profile*2,0,weights,bones),
      Mesh(islands[1],triangleIndices,materialHandle,0x575849534c522001ull+profile*2,0,weights,bones)};
    for(unsigned phase=0;phase<5;++phase){
      Config("rtx.debugView.debugViewIdx",phase==0?"23":"21");Focus();
      const auto begin=GetTickCount64();unsigned phaseFrames=0,readinessSamples=0;float angle=0,dx=0,dy=0;
      do{
        if(GetTickCount64()-experimentStart>120000)Fail("island time bound",0);
        const float sign=(phaseFrames&1)?-1.f:1.f;
        angle=phase==3?.002f*sign:0;
        dx=(phase==2?.00125f:phase==3?.000625f:0.f)*sign;
        dy=phase==3?.00046875f*sign:0;
        const std::array<remixapi_Transform,4> palette={Bone(angle,dx,dy),Bone(-angle,-dx,-dy),Bone(0,0,0),Bone(0,0,0)};
        winx_remix::skin_packet::SignedSkin posed;
        if(!winx_remix::skin_packet::EncodeSignedSkin(SourcePacket(input,topology,2,weights,bones,palette),posed))Fail("island pose",phase);
        if(posed.weights!=encoded.weights||posed.indices!=encoded.indices)Fail("island immutable mesh",phase);
        std::vector<remixapi_Transform> submitted(posed.palette.size());
        for(size_t b=0;b<posed.palette.size();++b)for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)
          submitted[b].matrix[row][column]=posed.palette[b][row][column];
        Begin();Api(api.SetupCamera(&camera),"island camera");
        remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;blend.writeMask=15;blend.alphaTestCompareOp=7;
        blend.textureColorOperation=1;blend.textureColorArg1Source=2;blend.textureColorArg2Source=1;blend.textureAlphaOperation=1;blend.textureAlphaArg1Source=2;blend.tFactor=0xffffffff;
        remixapi_InstanceInfoBoneTransformsEXT ext{};ext.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;ext.pNext=&blend;
        ext.boneTransforms_values=submitted.data();ext.boneTransforms_count=uint32_t(submitted.size());
        remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&ext;instance.mesh=skin;instance.doubleSided=1;
        instance.categoryFlags=REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING;
        instance.transform=Bone(0,1.5f,-.2f);Api(api.DrawInstance(&instance),"island skin");
        instance.pNext=&blend;
        for(unsigned island=0;island<2;++island){instance.mesh=reference[island];
          instance.transform=IslandAffine(palette[0],palette[1],weightPairs[island][0],weightPairs[island][1]);
          Api(api.DrawInstance(&instance),"island rigid reference");}
        End();++phaseFrames;
      }while(phaseFrames<90||GetTickCount64()-begin<1500||(phase==0&&(++readinessSamples,!IslandAlbedoReady())));
      if(phase==0){fprintf(journal,"{\"event\":\"island_albedo_ready\",\"signedProfile\":%u,\"samples\":%u}\n",profile,readinessSamples);fflush(journal);}
      char filename[80]{};sprintf_s(filename,"island-s%u-p%u.bmp",profile,phase);Capture(filename);++captures;
      fprintf(journal,"{\"event\":\"island_phase\",\"signedProfile\":%u,\"phase\":%u,\"frames\":%u,\"angle\":%.9g,\"dx\":%.9g,\"dy\":%.9g,\"file\":\"%s\"}\n",profile,phase,phaseFrames,angle,dx,dy,filename);fflush(journal);
    }
    for(const auto mesh:reference)Api(api.DestroyMesh(mesh),"retire island reference");Api(api.DestroyMesh(skin),"retire island skin");
    fprintf(journal,"{\"event\":\"island_meshes_retired\",\"signedProfile\":%u,\"count\":3}\n",profile);fflush(journal);
  }
  return captures;
}
