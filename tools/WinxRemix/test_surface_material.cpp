// Native system-D3D9 FFP is the oracle. Compare its RGBA render with a pixel
// shader interpreting the exported Remix blend descriptor and translated UVs.
// This checks the platform translation, not lighting or Remix's GPU result.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define REMIX_ALLOW_X86
#include <remix/remix_c.h>
#include "winx_surface_material.h"
static unsigned checks,pairs,maxDelta;
static void Check(bool ok,const char* what) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
static void Hr(HRESULT hr,const char* what) {if(FAILED(hr)){fprintf(stderr,"HRESULT %08lX: %s\n",hr,what);std::exit(1);}}
struct Buffer: IUnknown {virtual void* STDMETHODCALLTYPE GetBufferPointer()=0;virtual DWORD STDMETHODCALLTYPE GetBufferSize()=0;};
using Assemble=HRESULT(WINAPI*)(const char*,UINT,const void*,void*,DWORD,Buffer**,Buffer**);
struct Vertex {float x,y,z;DWORD color;float uv[2],other[2];};
static constexpr DWORD fvf=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2;
static IDirect3DDevice9* device;
static IDirect3DSurface9 *target,*readback;
static std::vector<uint32_t> Render(const Vertex* vertices,IDirect3DPixelShader9* shader) {
  Hr(device->SetPixelShader(shader),"bind shader");
  Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0),"clear");
  Hr(device->BeginScene(),"begin");
  Hr(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)),"draw");
  Hr(device->EndScene(),"end");
  Hr(device->GetRenderTargetData(target,readback),"readback");
  D3DLOCKED_RECT lock{};Hr(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY),"lock");
  std::vector<uint32_t> pixels(32*32);
  for(unsigned y=0;y<32;++y) memcpy(pixels.data()+y*32,static_cast<const char*>(lock.pBits)+y*lock.Pitch,32*4);
  Hr(readback->UnlockRect(),"unlock");return pixels;
}
static std::string Equation(uint32_t op,uint32_t first,uint32_t second,bool alpha) {
  const char* regs[]={"c1","r0","v0","c0"};
  Check(first<4 && second<4 && op>=1 && (op<=3 || (alpha && op<=6) || (!alpha && op==7)),"exported enum supported by independent shader interpreter");
  std::string mask=alpha?".w":".xyz";
  std::string sourceMask=alpha?".w":"";
  if(op==3) return "mul r1"+mask+", "+regs[first]+sourceMask+", "+regs[second]+sourceMask+"\n";
  if(op==4 || op==5 || op==7) return "mul r1"+mask+", "+regs[first]+sourceMask+", "+regs[second]+sourceMask+"\nmul_sat r1"+mask+", r1"+sourceMask+", c2."+(op==5?"y":"x")+"\n";
  if(op==6) return "add_sat r1"+mask+", "+regs[first]+sourceMask+", "+regs[second]+sourceMask+"\n";
  return "mov r1"+mask+", "+regs[op==1?first:second]+sourceMask+"\n";
}
int main() {
  auto runtime=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  auto factory=runtime?reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(runtime,"Direct3DCreate9")):nullptr;
  Check(factory!=nullptr,"system D3D9 available");auto d3d=factory(D3D_SDK_VERSION);Check(d3d!=nullptr,"D3D9 factory");
  auto hwnd=CreateWindowExW(0,L"STATIC",L"Winx material contract test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  Check(hwnd!=nullptr,"hidden test window");
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;
  pp.BackBufferWidth=32;pp.BackBufferHeight=32;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
  Hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device),"native device");
  Hr(device->CreateRenderTarget(32,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr),"target");
  Hr(device->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr),"readback surface");
  Hr(device->SetRenderTarget(0,target),"set target");
  D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
  for(auto state:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION}) Hr(device->SetTransform(state,&identity),"identity");
  Hr(device->SetFVF(fvf),"FVF");
  for(auto state:{D3DRS_LIGHTING,D3DRS_SPECULARENABLE,D3DRS_ZENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SRGBWRITEENABLE}) Hr(device->SetRenderState(state,0),"disable state");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull");
  IDirect3DTexture9* texture=nullptr;
  Hr(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"texture");
  D3DLOCKED_RECT lock{};Hr(texture->LockRect(0,&lock,nullptr,0),"texture lock");
  for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
    uint32_t pixel=((32+x*47+y*11)<<24)|((21+x*49)<<16)|((17+y*53)<<8)|(27+x*13+y*19);
    memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch+x*4,&pixel,4);
  }
  Hr(texture->UnlockRect(0),"texture unlock");Hr(device->SetTexture(0,texture),"texture bind");
  Hr(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT),"min filter");
  Hr(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT),"mag filter");
  Hr(device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP),"U clamp");
  Hr(device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP),"V clamp");
  Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"single stage");
  Hr(device->SetRenderState(D3DRS_TEXTUREFACTOR,0x4060a0e0),"factor");
  auto sdk=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  auto assemble=sdk?reinterpret_cast<Assemble>(GetProcAddress(sdk,"D3DXAssembleShader")):nullptr;
  Check(assemble!=nullptr,"D3DX assembler");
  Vertex vertices[4]={{-1,1,.5f,0x80336699,{.061f,.083f},{.213f,.137f}},
    {1,1,.5f,0x80336699,{.913f,.083f},{.781f,.137f}},
    {-1,-1,.5f,0x80336699,{.061f,.937f},{.213f,.813f}},
    {1,-1,.5f,0x80336699,{.913f,.937f},{.781f,.813f}}};
  D3DMATRIX transform=identity;transform._11=.63f;transform._12=.07f;transform._21=-.13f;transform._22=.72f;
  transform._31=.17f;transform._32=.11f;transform._41=13;transform._42=17; // distinguish row 3 from row 4
  const DWORD rgbOperations[]={D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DTOP_MODULATE2X};
  const DWORD alphaOperations[]={D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DTOP_MODULATE2X,D3DTOP_MODULATE4X,D3DTOP_ADD};
  const DWORD args[][2]={{D3DTA_TEXTURE,D3DTA_DIFFUSE},{D3DTA_CURRENT,D3DTA_TEXTURE},{D3DTA_TEXTURE,D3DTA_TFACTOR},{D3DTA_TFACTOR,D3DTA_DIFFUSE}};
  for(DWORD rgb:rgbOperations) for(DWORD alpha:alphaOperations) for(const auto& arg:args) for(unsigned variant=0;variant<4;++variant) {
    Hr(device->SetPixelShader(nullptr),"reset shader");
    Hr(device->SetTextureStageState(0,D3DTSS_COLOROP,rgb),"RGB op");
    Hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,arg[0]),"RGB arg1");
    Hr(device->SetTextureStageState(0,D3DTSS_COLORARG2,arg[1]),"RGB arg2");
    Hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,alpha),"alpha op");
    Hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,arg[1]),"alpha arg1");
    Hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG2,arg[0]),"alpha arg2");
    Hr(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,variant/2),"UV set");
    Hr(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,variant%2?D3DTTFF_COUNT2:D3DTTFF_DISABLE),"UV flags");
    Hr(device->SetTransform(D3DTS_TEXTURE0,&transform),"UV transform");
    surface_material::Contract contract;Check(surface_material::Read(device,contract),"read supported original contract");
    auto original=Render(vertices,nullptr);
    remixapi_InstanceInfoBlendEXT blend{};surface_material::Apply(contract,blend);
    Check(blend.tFactor==0x4060a0e0 && !blend.isTextureFactorBlend && !blend.isVertexColorBakedLighting,"preserve factor and avoid duplicate multiplication");
    auto program=std::string("ps_2_0\ndcl t0.xy\ndcl v0\ndcl_2d s0\ntexld r0, t0, s0\n")+
      Equation(blend.textureColorOperation,blend.textureColorArg1Source,blend.textureColorArg2Source,false)+
      Equation(blend.textureAlphaOperation,blend.textureAlphaArg1Source,blend.textureAlphaArg2Source,true)+"mov oC0, r1\n";
    Buffer *code=nullptr,*errors=nullptr;auto hr=assemble(program.c_str(),static_cast<UINT>(program.size()),nullptr,nullptr,0,&code,&errors);
    if(errors){fprintf(stderr,"%s",static_cast<const char*>(errors->GetBufferPointer()));errors->Release();}Hr(hr,"assemble interpreter");
    IDirect3DPixelShader9* shader=nullptr;Hr(device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()),&shader),"create interpreter");code->Release();
    float constants[12]={float((blend.tFactor>>16)&255)/255,float((blend.tFactor>>8)&255)/255,float(blend.tFactor&255)/255,float(blend.tFactor>>24)/255,1,1,1,1,2,4,0,0};
    Hr(device->SetPixelShaderConstantF(0,constants,3),"factor constants");
    Vertex translated[4];memcpy(translated,vertices,sizeof(vertices));
    for(unsigned v=0;v<4;++v) Check(surface_material::Coordinates(contract,contract.coordinates?vertices[v].other:vertices[v].uv,translated[v].uv),"translate UV");
    Hr(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE),"translated UV flags");
    Hr(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0),"translated UV set");
    auto result=Render(translated,shader);Hr(device->SetPixelShader(nullptr),"unbind interpreter");shader->Release();
    unsigned difference=0;
    for(unsigned y=2;y<30;++y) for(unsigned x=2;x<30;++x) for(unsigned shift=0;shift<32;shift+=8) {
      unsigned delta=unsigned(std::abs(int((original[y*32+x]>>shift)&255)-int((result[y*32+x]>>shift)&255)));
      if(delta>difference)difference=delta;
    }
    if(difference>1)fprintf(stderr,"RGB=%lu alpha=%lu args=%lu,%lu variant=%u delta=%u\n",rgb,alpha,arg[0],arg[1],variant,difference);
    Check(difference<=1,"native FFP matches exported material/UV in all RGBA channels");
    if(difference>maxDelta)maxDelta=difference;++pairs;
  }
  surface_material::Channel channel;
  Check(surface_material::Decode(D3DTOP_SELECTARG1,D3DTA_TEXTURE,0xffffffff,channel),"unused arg ignored");
  Check(surface_material::Decode(D3DTOP_MODULATE2X,D3DTA_TEXTURE,D3DTA_DIFFUSE,channel) && channel.operation==7,"RGB double modulation uses the explicit saturating operation");
  Check(!surface_material::Decode(D3DTOP_MODULATE4X,D3DTA_TEXTURE,D3DTA_DIFFUSE,channel),"non-equivalent RGB quadruple multiply rejected");
  Check(!surface_material::Decode(D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE,channel),"non-saturating RGB sum rejected");
  Check(!surface_material::Decode(D3DTOP_MODULATE,D3DTA_TEXTURE|D3DTA_COMPLEMENT,D3DTA_DIFFUSE,channel),"argument modifier rejected");
  Check(!surface_material::Decode(D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_SPECULAR,channel),"second color channel rejected");
  surface_material::Contract contract;float input[]={.2f,.3f},output[2];
  contract.transformFlags=D3DTTFF_COUNT3|D3DTTFF_PROJECTED;
  Check(!surface_material::Coordinates(contract,input,output),"projective interpolation not replaced by vertex division");
  input[0]=NAN;contract.transformFlags=0;Check(!surface_material::Coordinates(contract,input,output),"NaN UV rejected");
  Hr(device->SetRenderState(D3DRS_LIGHTING,TRUE),"lit fixture");Check(!surface_material::Read(device,contract),"lit FFP not guessed as albedo");
  Hr(device->SetRenderState(D3DRS_LIGHTING,FALSE),"unlit fixture");
  Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_MODULATE),"second stage fixture");Check(!surface_material::Read(device,contract),"second stage rejected");
  Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"reset second stage");
  Hr(device->SetRenderState(D3DRS_SPECULARENABLE,TRUE),"post-texture specular fixture");Check(!surface_material::Read(device,contract),"post-texture specular rejected");
  Hr(device->SetRenderState(D3DRS_SPECULARENABLE,FALSE),"reset specular");
  Hr(device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP),"temporary output fixture");Check(!surface_material::Read(device,contract),"temporary stage output rejected");
  Hr(device->SetTexture(0,nullptr),"unbind texture");texture->Release();readback->Release();target->Release();device->Release();d3d->Release();DestroyWindow(hwnd);
  printf("{\"checks\":%u,\"nativeRenderPairs\":%u,\"maxChannelDelta\":%u,\"status\":\"PASS\",\"oracle\":\"system D3D9 FFP vs exported descriptor interpreter; no Remix GPU equivalence claim\"}\n",checks,pairs,maxDelta);
  return 0;
}
