// Own integration fixture using actual D3D state and the production translator.
// Native addresses below point only into this test's declared ABI objects.
#pragma once
template<class Draw> static void NativeMaterial(IDirect3DDevice9* d,std::vector<uint8_t>& renderer,
    sparkplug::evidence::pc::spDXMaterialObservedLayout& material,
    sparkplug::evidence::pc::spRendererDrawContextObservedLayout& state,
    const native_mesh_source::Geometry& geometry,native_mesh_source::Scope& scope,const Draw& draw) {
  namespace nm=native_material_source;namespace ns=native_mesh_source;namespace abi=sparkplug::evidence::pc;
  const auto beforeChecks=checks;
  const auto savedRenderer=renderer;const auto savedMaterial=material;const auto savedState=state;
  const bool wasEnabled=nm::enabled,wasSubmit=nm::submitEnabled;
  const auto savedAttempts=nm::attempts,savedMatched=nm::matched,savedUsed=nm::used,savedMismatches=nm::mismatches;
  std::array<unsigned,nm::Count> savedRejected{};memcpy(savedRejected.data(),nm::rejected,sizeof(nm::rejected));
  IDirect3DStateBlock9* savedDevice=nullptr;Hr(d->CreateStateBlock(D3DSBT_ALL,&savedDevice),"capture native material device state");
  IDirect3DBaseTexture9* bound=nullptr;Hr(d->GetTexture(0,&bound),"native material actual bound COM");Check(bound!=nullptr,"native material texture exists");
  auto address=[](const void* value){return uint32_t(reinterpret_cast<uintptr_t>(value));};
  abi::spMaterialPassLayerObservedLayout pass{};abi::spStdLayerObservedLayout layer{};
  abi::spMaterialTextureObservedLayout holder{};abi::spDXTextureObservedLayout texture{};
  pass.base.vtableAddress=abi::spMaterialPassLayerVTable;pass.layerCount=1;pass.layers[0]=address(&layer);
  layer.base.base.vtableAddress=abi::spStdLayerVTable;layer.base.materialTexture=address(&holder);
  holder.base.vtableAddress=abi::spMaterialTextureVTable;holder.fallbackTexture=address(&texture);
  holder.textureStates[1]=holder.textureStates[2]=3; // Native MODULATE.
  texture.base.base.base.base.vtableAddress=abi::spDXTexturePrimaryVTable;texture.device=address(d);texture.texture=address(bound);
  material.base.renderStates[8]=2;material.base.passCount=1;material.base.passes[0]=address(&pass);
  state.renderStates[8]=2;state.materialOverride=0;state.materialState=1; // C1C4 alone is not a texture override.
  std::array<uint32_t,9> selectors{},shader{};
  abi::spDXRendererMaterialCacheObservedLayout installed{};installed.installedMaterial=address(&material);
  auto publish=[&](){
    memcpy(state.textureStates,holder.textureStates,sizeof(holder.textureStates));
    memcpy(renderer.data()+abi::spRendererDrawContextOffset,&state,sizeof(state));
    memcpy(renderer.data()+abi::spDXRendererMaterialCacheOffset,&installed,sizeof(installed));
    memcpy(renderer.data()+0xc748,selectors.data(),sizeof(selectors));
    memcpy(renderer.data()+0xe454,shader.data(),sizeof(shader));
  };
  publish();nm::enabled=nm::submitEnabled=true;
  Hr(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_CURRENT),"native material RGB CURRENT");
  Hr(d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_CURRENT),"native material alpha CURRENT");
  Hr(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0),"native material UV0");
  Hr(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE),"native material no UV transform");
  Hr(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP),"native material wrap U");
  Hr(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP),"native material wrap V");
  Hr(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT),"native material filter0 mag");
  Hr(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT),"native material filter0 min");
  Hr(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE),"native material filter0 mip");
  surface_material::Contract observed{};surface_material::ObservedStage stage{};material_channels::Input channels{};
  Check(surface_material::ReadTexture(d,observed,&stage)&&material_channels::Read(d,channels,preserveUnlitColor),"read actual native material reference state");
  nm::Packet packet{};
  auto resolve=[&](const ns::Geometry& input){packet={};return nm::Resolve(d,input,observed,stage,&channels,preserveUnlitColor,packet);};
  auto refused=[&](nm::Reason reason,const char* label){const auto previous=nm::rejected[reason];Check(!resolve(geometry)&&nm::rejected[reason]==previous+1,label);};
  Check(resolve(geometry),"ordinary native material resolves with C1C4 set and zero texture selectors");
  Check(packet.material==address(&material)&&packet.pass==address(&pass)&&packet.layer==address(&layer)&&
    packet.textureOwner==address(&holder)&&packet.texture==address(&texture)&&packet.contract.rgb.operation==3&&
    packet.contract.alpha.operation==3&&packet.sampler.u==1&&packet.sampler.v==1&&packet.sampler.mag==1&&packet.sampler.min==1&&packet.sampler.mip==0,
    "native material packet preserves ownership and maps the declared raw inputs");
  auto nativeDraw=[&](bool native,const char* label){const auto beforeUsed=nm::used,beforeApi=apiDraws,beforeSubmitted=materialChannelsSubmitted;draw();
    Check(nm::used==beforeUsed+unsigned(native)&&apiDraws==beforeApi+1&&materialChannelsSubmitted==beforeSubmitted+1,label);};
  nativeDraw(true,"production draw uses native material and calls API exactly once");
  const auto nativeVertices=lastVertices;const auto nativeBlend=lastBlend;const auto nativeTransform=lastTransform;
  const auto nativeAlbedo=Dds(lastAlbedo),nativeEmission=Dds(lastEmission);const auto nativeSampler=lastSampler;
  nm::submitEnabled=false;nativeDraw(false,"observer keeps D3D material producer without counting native use");
  Check(lastVertices.size()==nativeVertices.size()&&!memcmp(lastVertices.data(),nativeVertices.data(),nativeVertices.size()*sizeof(nativeVertices[0]))&&
    !memcmp(&lastBlend,&nativeBlend,sizeof(lastBlend))&&!memcmp(&lastTransform,&nativeTransform,sizeof(lastTransform))&&
    Dds(lastAlbedo)==nativeAlbedo&&Dds(lastEmission)==nativeEmission&&lastSampler.u==nativeSampler.u&&lastSampler.v==nativeSampler.v&&lastSampler.filter==nativeSampler.filter,
    "native/observer A-B preserves vertices blend transform sampler and complete DDS mip bytes");
  nm::submitEnabled=true;
  auto foreign=geometry;++foreign.mesh;Check(!resolve(foreign),"stale geometry mesh cannot borrow current material scope");
  foreign=geometry;++foreign.submission;Check(!resolve(foreign),"stale geometry sequence cannot borrow current material scope");
  foreign=geometry;++foreign.renderer;Check(!resolve(foreign),"foreign renderer cannot borrow current material scope");
  const auto savedScope=ns::active;ns::active=nullptr;refused(nm::Scope,"material without active scope rejected");ns::active=savedScope;
  scope.valid=false;refused(nm::Scope,"invalid active scope rejected");scope.valid=true;
  const auto savedThread=ns::ownerThread;ns::ownerThread=~savedThread;refused(nm::Scope,"foreign owner thread rejected");ns::ownerThread=savedThread;
  installed.installedMaterial=0;publish();refused(nm::Material,"installed versus selected material mismatch rejected");
  nativeDraw(false,"installed identity mismatch leaves working D3D material fallback");installed.installedMaterial=address(&material);publish();
  ++material.base.base.vtableAddress;refused(nm::Material,"unknown material class rejected");--material.base.base.vtableAddress;
  material.base.passCount=2;refused(nm::Pass,"multiple native material passes rejected");material.base.passCount=1;
  ++pass.base.vtableAddress;refused(nm::Pass,"custom material pass rejected");--pass.base.vtableAddress;
  pass.layerCount=2;refused(nm::Pass,"single pass still requires exactly one layer");pass.layerCount=1;
  ++layer.base.base.vtableAddress;refused(nm::Layer,"custom layer rejected");--layer.base.base.vtableAddress;
  ++holder.base.vtableAddress;refused(nm::Layer,"custom texture holder rejected");--holder.base.vtableAddress;
  layer.base.materialTexture=0;refused(nm::Layer,"null borrowed holder rejected");layer.base.materialTexture=address(&holder);
  state.materialOverride=1;publish();refused(nm::Override,"renderer material override rejected");state.materialOverride=0;
  selectors[1]=1;publish();refused(nm::Override,"RGB source selector rejected despite equal effective operation");
  state.materialState=0;publish();refused(nm::Override,"texture selector still matters with C1C4 clear");state.materialState=1;selectors[1]=0;
  selectors[8]=1;publish();refused(nm::Override,"UV transform source selector rejected");selectors[8]=0;
  publish();uint32_t changed=holder.textureStates[6]+1;memcpy(renderer.data()+0xc898+6*4,&changed,4);
  refused(nm::Override,"effective native texture cache must equal current holder raw state");publish();
  shader[8]=3;shader[3]=1;publish();refused(nm::Override,"selected shader coordinate override rejected");shader[3]=0;
  shader[8]=8;publish();refused(nm::Override,"out-of-bounds shader selector rejected before indexed access");shader[8]=0;publish();
  holder.textureStates[7]=1;publish();refused(nm::Coordinates,"native UV1 remains outside first cohort");holder.textureStates[7]=0;
  holder.textureStates[8]=2;publish();refused(nm::Coordinates,"native transformed UV remains outside first cohort");holder.textureStates[8]=0;
  holder.textureStates[1]=16;publish();refused(nm::Mapping,"out-of-bounds operation table input rejected");holder.textureStates[1]=3;publish();
  ++texture.base.base.base.base.vtableAddress;refused(nm::Texture,"non-DX native texture rejected");--texture.base.base.base.base.vtableAddress;
  texture.device=0;refused(nm::Texture,"native texture belongs to the actual device");texture.device=address(d);
  ++texture.texture;refused(nm::Texture,"native COM texture must equal actual bound texture");--texture.texture;
  state.device=0;publish();refused(nm::Material,"native renderer belongs to the actual device");state.device=address(d);publish();
  material.base.renderStates[8]=3;refused(nm::Mode,"source lighting mode must be unlit2");material.base.renderStates[8]=2;
  state.renderStates[8]=3;publish();refused(nm::Mode,"effective lighting mode must match unlit2");state.renderStates[8]=2;publish();
  Hr(d->SetRenderState(D3DRS_LIGHTING,TRUE),"inject actual lighting mismatch");refused(nm::Device,"actual lighting mismatch rejected");Hr(d->SetRenderState(D3DRS_LIGHTING,FALSE),"restore unlit device");
  Hr(d->SetRenderState(D3DRS_SPECULARENABLE,TRUE),"inject actual specular mismatch");refused(nm::Device,"actual specular mismatch rejected");Hr(d->SetRenderState(D3DRS_SPECULARENABLE,FALSE),"restore specular device");
  Hr(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR),"inject actual sampler mismatch");refused(nm::Device,"actual MIN sampler mismatch rejected");
  nativeDraw(false,"actual sampler mismatch retains complete D3D-source API fallback");Hr(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT),"restore sampler");
  const auto ordinaryStage=stage;stage.r2=D3DTA_DIFFUSE;refused(nm::Arguments,"native packet requires explicit CURRENT argument");
  stage=ordinaryStage;stage.a1=D3DTA_TEXTURE|D3DTA_COMPLEMENT;refused(nm::Arguments,"argument modifiers rejected");
  stage=ordinaryStage;stage.result=D3DTA_TEMP;refused(nm::Arguments,"RESULTARG guard retained");
  stage=ordinaryStage;stage.next=D3DTOP_MODULATE;refused(nm::Arguments,"next-stage guard retained");stage=ordinaryStage;
  Hr(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE),"actual alternate source argument");nativeDraw(false,"actual argument change keeps D3D-source translation");
  Hr(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_CURRENT),"restore actual CURRENT argument");
  const auto beforeResultUsed=nm::used,beforeResultApi=apiDraws,beforeResultSubmitted=materialChannelsSubmitted;
  Hr(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP),"actual unsupported result destination");draw();
  Check(nm::used==beforeResultUsed&&apiDraws==beforeResultApi&&materialChannelsSubmitted==beforeResultSubmitted,"unsupported RESULTARG reaches original D3D draw without native/API credit");
  Hr(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT),"restore actual result destination");
  // Change both the source descriptor and actual bound D3D states. Assertions
  // below inspect the resulting real production API material, not the mapper.
  holder.textureStates[3]=2;holder.textureStates[4]=1;holder.textureStates[6]=2;publish();
  Hr(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP),"changed native clamp U");
  Hr(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_MIRROR),"changed native mirror V");
  Hr(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR),"changed native linear mag");
  Hr(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR),"changed native linear min");
  Hr(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR),"changed native linear mip");
  nativeDraw(true,"changed native sampler reaches production API");
  Check(lastSampler.u==2&&lastSampler.v==1&&lastSampler.filter==1,"recorded API receives clamp mirror and linear sampler values");
  holder.textureStates[2]=4;publish();Hr(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE2X),"changed native alpha operation");
  nativeDraw(true,"changed native alpha operation reaches production API");
  Check(lastBlend.textureAlphaOperation==4&&lastBlend.textureAlphaArg1Source==1&&lastBlend.textureAlphaArg2Source==2,
    "recorded blend receives native alpha2x with texture/current arguments");
  const auto failedUsed=nm::used,failedApi=apiDraws,failedSubmitted=materialChannelsSubmitted,failedRejected=materialChannelsRejected;
  rejectDraw=true;draw();rejectDraw=false;
  Check(nm::used==failedUsed&&apiDraws==failedApi+1&&materialChannelsSubmitted==failedSubmitted&&materialChannelsRejected==failedRejected+1,
    "recording API draw refusal never credits native use and original D3D fallback completes");
  nativeDraw(true,"native material submission recovers after API draw refusal");
  // Expire the borrowed submission without changing addresses: sequence is the
  // lifetime gate, while all texture/material payloads remain local copies.
  ++scope.sequence;refused(nm::Scope,"same-address geometry cannot outlive its submission sequence");--scope.sequence;
  memcpy(renderer.data(),savedRenderer.data(),savedRenderer.size());material=savedMaterial;state=savedState;
  Hr(savedDevice->Apply(),"restore all device state after native material fixture");savedDevice->Release();bound->Release();
  nm::enabled=wasEnabled;nm::submitEnabled=wasSubmit;nm::attempts=savedAttempts;nm::matched=savedMatched;nm::used=savedUsed;nm::mismatches=savedMismatches;
  memcpy(nm::rejected,savedRejected.data(),sizeof(nm::rejected));
  ClearSurfaceBases();nativeMaterialChecks=checks-beforeChecks;
}
