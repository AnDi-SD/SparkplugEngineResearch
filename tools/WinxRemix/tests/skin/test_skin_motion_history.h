#pragma once
// Own differential motion test, included after the authored fixture helpers.
// Positive and signed B2 translations use one immutable mesh each. A separate
// rigid API instance provides a simultaneous screen-space motion-vector oracle.
static bool MotionAlbedoReady() {
  if(GetForegroundWindow()!=window)Fail("motion readiness lost foreground",0);
  RECT rect{};POINT origin{};
  if(!GetClientRect(window,&rect)||rect.right!=960||rect.bottom!=540||!ClientToScreen(window,&origin))
    Fail("motion readiness rectangle",GetLastError());
  HDC desktop=GetDC(nullptr);if(!desktop)Fail("motion readiness DC",GetLastError());
  const COLORREF colors[]={GetPixel(desktop,origin.x+336,origin.y+277),GetPixel(desktop,origin.x+624,origin.y+277)};
  ReleaseDC(nullptr,desktop);
  if(GetForegroundWindow()!=window)Fail("motion readiness foreground changed",0);
  for(const auto color:colors) {
    if(color==CLR_INVALID)Fail("motion readiness pixel",0);
    if(GetRValue(color)<200||GetGValue(color)<200||GetBValue(color)<200)return false;
  }
  return true;
}
static unsigned MotionHistory(const SkinVertices& baseInput,const std::vector<uint32_t>& triangleIndices,
    remixapi_MaterialHandle materialHandle,const remixapi_CameraInfo& camera) {
  // Our new differential experiment. Same authored mesh at each side; skin
  // palette translation on the right, equivalent rigid transform on the left.
  // Pixel samples compare the same frame, avoiding a temporal screenshot oracle.
  Config("rtx.postfx.enableMotionBlur","False");
  fprintf(journal,"{\"event\":\"motion_contract\",\"vertices\":%zu,\"subdivisions\":%u,\"separationPixels\":288,\"skinCategoryIgnoresMotion\":false,\"deltaPixels\":[0.24,0.18]}\n",baseInput.size(),24u);fflush(journal);
  const auto start=GetTickCount64();unsigned captures=0;
  for(unsigned signedProfile=0;signedProfile<2;++signedProfile){
    const unsigned count=2;std::vector<float> weights;std::vector<uint32_t> bones;
    for(size_t v=0;v<baseInput.size();++v){weights.push_back(signedProfile?-.25f:.25f);weights.push_back(signedProfile?1.25f:.75f);bones.push_back(0);bones.push_back(1);}
    const std::array<remixapi_Transform,4> identityPalette={Bone(0,0,0),Bone(0,0,0),Bone(0,0,0),Bone(0,0,0)};
    winx_remix::skin_packet::SignedSkin encoded;
    if(!winx_remix::skin_packet::EncodeSignedSkin(SourcePacket(baseInput,triangleIndices,count,weights,bones,identityPalette),encoded))Fail("motion encoding",signedProfile);
    const auto skin=Mesh(baseInput,triangleIndices,materialHandle,0x57584d4f544e1000ull+signedProfile,encoded.influences,encoded.weights,encoded.indices);
    const auto reference=Mesh(baseInput,triangleIndices,materialHandle,0x57584d4f544e2000ull+signedProfile,0,weights,bones);
    for(unsigned phase=0;phase<5;++phase){
      Config("rtx.debugView.debugViewIdx",phase==0?"23":"21");
      Focus();const auto begin=GetTickCount64();unsigned phaseFrames=0,readinessSamples=0;
      float dx=0,dy=0;
      do {
        if(GetTickCount64()-start>120000)Fail("motion time bound",0);
        const float sign=(phaseFrames&1)?-1.f:1.f;
        dx=phase==2||phase==3?.00125f*sign:0;
        dy=phase==3?.0009375f*sign:0;
        const std::array<remixapi_Transform,4> palette={Bone(0,dx,dy),Bone(0,dx,dy),Bone(0,dx,dy),Bone(0,dx,dy)};
        winx_remix::skin_packet::SignedSkin posed;
        if(!winx_remix::skin_packet::EncodeSignedSkin(SourcePacket(baseInput,triangleIndices,count,weights,bones,palette),posed))Fail("motion pose encoding",phase);
        if(posed.weights!=encoded.weights||posed.indices!=encoded.indices)Fail("motion immutable mesh changed",phase);
        std::vector<remixapi_Transform> submitted(posed.palette.size());
        for(size_t b=0;b<posed.palette.size();++b)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)submitted[b].matrix[row][col]=posed.palette[b][row][col];
        Begin();Api(api.SetupCamera(&camera),"motion camera");
        remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;blend.writeMask=15;blend.alphaTestCompareOp=7;
        blend.textureColorOperation=1;blend.textureColorArg1Source=2;blend.textureColorArg2Source=1;blend.textureAlphaOperation=1;blend.textureAlphaArg1Source=2;blend.tFactor=0xffffffff;
        remixapi_InstanceInfoBoneTransformsEXT ext{};ext.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;ext.pNext=&blend;ext.boneTransforms_values=submitted.data();ext.boneTransforms_count=uint32_t(submitted.size());
        remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&ext;instance.mesh=skin;instance.doubleSided=1;
        instance.categoryFlags=REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING;
        instance.transform=Bone(0,1.5f,-.2f);Api(api.DrawInstance(&instance),"motion skin");
        instance.pNext=&blend;instance.mesh=reference;instance.transform=Bone(0,-1.5f+dx,-.2f+dy);
        Api(api.DrawInstance(&instance),"motion rigid reference");End();++phaseFrames;
      }while(phaseFrames<90||GetTickCount64()-begin<1500||(phase==0&&(++readinessSamples,!MotionAlbedoReady())));
      if(phase==0){fprintf(journal,"{\"event\":\"motion_albedo_ready\",\"signedProfile\":%u,\"samples\":%u,\"frame\":%u}\n",signedProfile,readinessSamples,frame);fflush(journal);}
      char filename[80]{};sprintf_s(filename,"motion-s%u-p%u.bmp",signedProfile,phase);Capture(filename);++captures;
      fprintf(journal,"{\"event\":\"motion_phase\",\"signedProfile\":%u,\"phase\":%u,\"frames\":%u,\"dx\":%.9g,\"dy\":%.9g,\"file\":\"%s\"}\n",signedProfile,phase,phaseFrames,dx,dy,filename);fflush(journal);
    }
    Api(api.DestroyMesh(reference),"retire motion reference");Api(api.DestroyMesh(skin),"retire motion skin");
  }
  return captures;
}
