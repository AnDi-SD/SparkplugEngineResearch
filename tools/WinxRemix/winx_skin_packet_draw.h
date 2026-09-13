#pragma once
// Own observation after the actual D3D9 indexed call returns. This ties exported
// candidate data to bound inputs/bytecode/constants without suppressing a draw.
namespace skin_packet_capture {
static void DrawResult(IDirect3DDevice9* device,const native_mesh_source::DrawRange& draw,HRESULT result) noexcept {
#if defined(_M_IX86)
  if(!Enabled()||!journal||_ftelli64(journal)>=1024*1024)return;
  const auto skin=native_skin_source::active;const auto mesh=native_mesh_source::active;
  if(!skin||!mesh||!mesh->valid||skin->frame!=frameId||skin->parent||mesh->parent)return;
  const Saved* sample=nullptr;
  for(const auto& row:saved)if(row.frame==frameId&&row.call==skin->sequence&&row.submission==mesh->sequence&&row.skin==skin->skin&&row.mesh==mesh->mesh){sample=&row;break;}
  if(!sample)return;
  // Copy metadata before COM queries; no vector reference crosses them.
  const Saved identity=*sample;
  bool binding=false,constantsWritten=false;unsigned shader=unknownShader,constantCount=0;uint32_t reason=0;
  try {
    namespace source=native_mesh_source;namespace abi=sparkplug::evidence::pc;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* declaration=nullptr;IDirect3DVertexShader9* vs=nullptr;
    struct Release {IDirect3DVertexBuffer9*& vb;IDirect3DIndexBuffer9*& ib;IDirect3DVertexDeclaration9*& declaration;IDirect3DVertexShader9*& vs;
      ~Release(){if(vs)vs->Release();if(declaration)declaration->Release();if(ib)ib->Release();if(vb)vb->Release();}} release{vb,ib,declaration,vs};
    UINT offset=0,stride=0;source::ResourcePair pair{};const auto original=mesh->value;
    if(FAILED(device->GetStreamSource(0,&vb,&offset,&stride))||!vb||FAILED(device->GetIndices(&ib))||!ib||
       FAILED(device->GetVertexDeclaration(&declaration))||!declaration||FAILED(device->GetVertexShader(&vs))||!vs)throw 1u;
    const auto type=original.indexType==2?D3DPT_TRIANGLELIST:D3DPT_TRIANGLESTRIP;
    if((original.indexType!=2&&original.indexType!=3)||draw.type!=type||draw.base<0||uint32_t(draw.base)!=original.vertexBegin||
       draw.minimum||draw.vertices!=original.base.base.vertexCount||draw.start!=original.indexBegin||draw.count!=original.base.base.primitiveCount||
       offset||stride!=original.vertexStride||!source::ReadResourceHeaders(original,pair)||
       pair.vb.direct3DVertexBuffer!=reinterpret_cast<uintptr_t>(vb)||pair.ib.direct3DIndexBuffer!=reinterpret_cast<uintptr_t>(ib))throw 2u;
    uint32_t nativeDeclaration[7]{};
    if(!source::Read(original.vertexDeclaration,nativeDeclaration)||nativeDeclaration[0]!=0x6f2e58||
       nativeDeclaration[5]!=original.fvfCode||nativeDeclaration[6]!=reinterpret_cast<uintptr_t>(declaration)||
       !source::ResolveResourceRanges(original,stride,vb,ib,pair)||pair.vertices->generation!=identity.generation||
       surfaceWrites.count(vb)||surfaceWrites.count(ib))throw 3u;
    std::unique_lock<std::recursive_mutex> borrow(guard);source::TransportWitness witness{};
    if(!native_transport_source::Witness(borrow,device,vb,ib,declaration,witness)||witness.indexFormat!=D3DFMT_INDEX16)throw 4u;
    const auto layout=source::Layout(original.base.base.vertexComponentFlags);
    if(!layout||layout->size()!=witness.elementCount||memcmp(layout->data(),witness.elements,layout->size()*sizeof(packet::Element)))throw 5u;
    shader=RememberShader(vs,true);if(shader==unknownShader)throw 6u;
    D3DCAPS9 caps{};std::array<float,1024> constants{};
    if(FAILED(device->GetDeviceCaps(&caps))||!caps.MaxVertexShaderConst)throw 7u;
    constantCount=(std::min)(unsigned(caps.MaxVertexShaderConst),256u);
    if(FAILED(device->GetVertexShaderConstantF(0,constants.data(),constantCount)))throw 7u;
    abi::spDXMeshObservedLayout final{};uint32_t finalDeclaration[7]{};
    if(!source::Read(mesh->mesh,final)||memcmp(&final,&original,sizeof(final))||
       !source::Read(original.vertexDeclaration,finalDeclaration)||memcmp(nativeDeclaration,finalDeclaration,sizeof(nativeDeclaration))||
       !native_transport_source::Current(borrow,witness)||native_skin_source::active!=skin||native_mesh_source::active!=mesh||
       skin->frame!=frameId||skin->mutation!=scene_geometry::MutationSerial())throw 8u;
    binding=true;wchar_t path[MAX_PATH]{};
    if(swprintf_s(path,L"%s/packet-%04u-draw-%06u.constants",directory,identity.file,drawId)<0)throw 9u;
    const auto file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw 9u;
    DWORD written=0;const bool complete=WriteFile(file,constants.data(),constantCount*16,&written,nullptr)&&written==constantCount*16;
    const bool closed=CloseHandle(file)!=FALSE;constantsWritten=complete&&closed;if(!constantsWritten)throw 9u;
  }catch(unsigned why){reason=why;}catch(...){reason=10;}
  fprintf(journal,"{\"event\":\"draw_result\",\"file\":\"packet-%04u.skp\",\"frame\":%u,\"draw\":%u,\"modelCall\":%llu,\"submission\":%llu,\"originalHr\":%ld,\"type\":%u,\"base\":%d,\"minimum\":%u,\"vertices\":%u,\"start\":%u,\"primitives\":%u,\"boundInputMatches\":%s,\"shader\":%u,\"constantCount\":%u,\"constantsWritten\":%s,\"reason\":%u}\n",
    identity.file,frameId,drawId,identity.call,identity.submission,result,unsigned(draw.type),draw.base,draw.minimum,draw.vertices,draw.start,draw.count,binding?"true":"false",shader,constantCount,constantsWritten?"true":"false",reason);fflush(journal);
#else
  (void)device;(void)draw;(void)result;
#endif
}
}
