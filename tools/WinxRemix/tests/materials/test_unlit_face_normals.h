#pragma once
// Original system FFP pixels plus the actual material/geometry API path.
static std::vector<DWORD> UnlitPixels(IDirect3DDevice9* device) {
  IDirect3DSurface9* target=nullptr;IDirect3DSurface9* copy=nullptr;D3DSURFACE_DESC desc{};
  Hr(device->GetRenderTarget(0,&target),"own FFP render target");Hr(target->GetDesc(&desc),"own target description");
  Check(desc.Width==64&&desc.Height==64,"bounded normal-reference raster");
  Hr(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr),"own raster readback");
  Hr(device->GetRenderTargetData(target,copy),"complete original FFP pixel readback");
  D3DLOCKED_RECT lock{};Hr(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY),"read owned pixels");
  std::vector<DWORD> pixels(desc.Width*desc.Height);
  for(UINT y=0;y<desc.Height;++y)memcpy(pixels.data()+y*desc.Width,static_cast<const uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch,desc.Width*4);
  Hr(copy->UnlockRect(),"close pixel readback");copy->Release();target->Release();return pixels;
}
template<class Draw> static void UnlitFaceNormals(IDirect3DDevice9* device,IDirect3DVertexBuffer9* original,
                                                const Vertex (&source)[4],Draw&& draw) {
  DWORD format=0;Hr(device->GetFVF(&format),"save normal declaration");
  material_channels::Input observed{};
  Check(material_channels::Read(device,observed,preserveUnlitColor)&&!observed.lighting,"actual unlit mode is explicit input");
  draw();const auto expected=lastVertices;
  const auto albedo=Dds(lastAlbedo),emission=Dds(lastEmission);
  const auto comparison=keepMaterialChannelsForComparison;keepMaterialChannelsForComparison=true;
  constexpr DWORD clear=0xff123456;
  Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,clear,1,0),"clear normal-reference target");draw();
  const auto normalPixels=UnlitPixels(device);
  unsigned colored=0;for(auto pixel:normalPixels)colored+=(pixel&0xffffffu)!=(clear&0xffffffu);
  Check(colored>0,"original unlit reference actually rasterizes pixels");
  struct Compact {float x,y,z;DWORD color;float u,v;};Compact vertices[4]{};
  for(unsigned i=0;i<4;++i)vertices[i]={source[i].x,source[i].y,source[i].z,source[i].color,source[i].u,source[i].v};
  IDirect3DVertexBuffer9* compact=nullptr;void* bytes=nullptr;
  constexpr DWORD compactFormat=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
  Hr(device->CreateVertexBuffer(sizeof(vertices),0,compactFormat,D3DPOOL_MANAGED,&compact,nullptr),"normal-free vertex buffer");
  Hr(compact->Lock(0,0,&bytes,0),"write normal-free vertices");memcpy(bytes,vertices,sizeof(vertices));Hr(compact->Unlock(),"complete normal-free vertices");
  Hr(device->SetStreamSource(0,compact,0,sizeof(Compact)),"bind normal-free stream");Hr(device->SetFVF(compactFormat),"normal-free declaration");
  Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,clear,1,0),"clear normal-free reference");draw();
  Check(UnlitPixels(device)==normalPixels,"system unlit FFP is pixel-identical without unused normals");
  keepMaterialChannelsForComparison=false;const auto submittedBefore=materialChannelsSubmitted;
  draw();Check(materialChannelsSubmitted==submittedBefore+1,"normal-free unlit draw reaches material API");
  Check(lastVertices.size()==expected.size(),"same strip triangle expansion");
  for(size_t i=0;i<expected.size();++i) {
    const auto& a=lastVertices[i];const auto& b=expected[i];
    Check(!memcmp(a.position,b.position,12)&&!memcmp(a.texcoord,b.texcoord,8)&&a.color==b.color,"normal generation preserves source position RGBA UV");
    Check(a.normal[0]==0&&a.normal[1]==0&&a.normal[2]==-1,"flat RT normal matches oriented authored plane");
  }
  Check(Dds(lastAlbedo)==albedo&&Dds(lastEmission)==emission,"normal-free path preserves all material texels and mips");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW),"opposite winding policy");draw();
  for(const auto& vertex:lastVertices)Check(vertex.normal[0]==0&&vertex.normal[1]==0&&vertex.normal[2]==1,"generated normal follows RT triangle winding");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"restore two-sided state");
  Hr(device->SetRenderState(D3DRS_LIGHTING,TRUE),"lit mode still needs source normals");
  Check(material_channels::Read(device,observed,preserveUnlitColor)&&observed.lighting,"reused input records changed lighting mode");
  const auto apiBefore=apiDraws;
  Check(!SubmitSurfaceOverlay(device,D3DPT_TRIANGLESTRIP,0,0,4,0,2,BoundChannelTextureHash(device),&observed)&&apiDraws==apiBefore,
    "missing source normals cannot silently qualify lit FFP");
  Hr(device->SetRenderState(D3DRS_LIGHTING,FALSE),"restore unlit mode");draw();
  Check(materialChannelsSubmitted==submittedBefore+3,"normal-free path resumes after rejected lit input");
  Hr(device->SetStreamSource(0,original,0,sizeof(Vertex)),"restore original vertex stream");Hr(device->SetFVF(format),"restore original normal declaration");
  Check(compact->Release()==0,"normal-free buffer retains no extra reference");keepMaterialChannelsForComparison=comparison;
}
