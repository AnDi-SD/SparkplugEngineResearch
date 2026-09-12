// Own bounded D3D9 software-vertex-processing oracle. Executes supplied shader
// bytecode through the system runtime; contains no recovered lighting formula.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <string>

template<class T> struct Com {
    T* value=nullptr;
    ~Com(){if(value)value->Release();}
    T** out(){return &value;}
    T* operator->()const{return value;}
};
static void Check(HRESULT hr,const char* label){if(FAILED(hr))throw std::runtime_error(std::string(label)+" HRESULT="+std::to_string(hr));}
static std::vector<unsigned char> Read(const wchar_t* path,size_t maximum){
    FILE* f=nullptr;if(_wfopen_s(&f,path,L"rb")||!f)throw std::runtime_error("input open");
    std::vector<unsigned char> bytes(maximum+1);const size_t size=fread(bytes.data(),1,bytes.size(),f);const bool error=ferror(f)!=0;fclose(f);
    if(error||size>maximum)throw std::runtime_error("bounded input size");bytes.resize(size);return bytes;
}
struct Vertex {float position[4],weights[4],indices[4],normal[4],color[4],uv[3];};
struct Output {float position[4];DWORD color;}; // Legacy ProcessVertices FVF output.
int wmain(int argc,wchar_t** argv){
    if(argc!=5){std::fprintf(stderr,"usage: oracle shader.bin constants.bin vertices.bin output.bin\n");return 2;}
    HMODULE module=nullptr;HWND window=nullptr;
    try{
        const auto code=Read(argv[1],65536),constants=Read(argv[2],4096),vertices=Read(argv[3],sizeof(Vertex)*64);
        if(code.size()<8||code.size()%4||constants.empty()||constants.size()%16||vertices.empty()||vertices.size()%sizeof(Vertex))throw std::runtime_error("input layout");
        wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH))throw std::runtime_error("system directory");
        const std::wstring dll=std::wstring(system)+L"\\d3d9.dll";module=LoadLibraryW(dll.c_str());
        if(!module)throw std::runtime_error("system D3D9 unavailable");
        auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));
        if(!create)throw std::runtime_error("Direct3DCreate9 unavailable");
        const wchar_t* klass=L"SparkplugShaderVertexOracle";
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=klass;
        if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("window class");
        window=CreateWindowExW(0,klass,L"Shader oracle",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);
        if(!window)throw std::runtime_error("hidden window");
        {
            Com<IDirect3D9> d3d;d3d.value=create(D3D_SDK_VERSION);if(!d3d.value)throw std::runtime_error("D3D9 create");
            D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=16;pp.BackBufferHeight=16;pp.BackBufferFormat=D3DFMT_UNKNOWN;
            Com<IDirect3DDevice9> device;Check(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,device.out()),"CreateDevice");
            Com<IDirect3DVertexShader9> shader;Check(device->CreateVertexShader(reinterpret_cast<const DWORD*>(code.data()),shader.out()),"CreateVertexShader");
            const D3DVERTEXELEMENT9 elements[]={
                {0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
                {0,16,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BLENDWEIGHT,0},
                {0,32,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BLENDINDICES,0},
                {0,48,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},
                {0,64,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},
                {0,80,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
            Com<IDirect3DVertexDeclaration9> inputDecl;Check(device->CreateVertexDeclaration(elements,inputDecl.out()),"input declaration");
            const UINT count=static_cast<UINT>(vertices.size()/sizeof(Vertex));
            Com<IDirect3DVertexBuffer9> input,output;
            Check(device->CreateVertexBuffer(static_cast<UINT>(vertices.size()),D3DUSAGE_SOFTWAREPROCESSING,0,D3DPOOL_SYSTEMMEM,input.out(),nullptr),"input buffer");
            Check(device->CreateVertexBuffer(count*sizeof(Output),D3DUSAGE_SOFTWAREPROCESSING,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,D3DPOOL_SYSTEMMEM,output.out(),nullptr),"output buffer");
            void* data=nullptr;Check(input->Lock(0,0,&data,0),"input lock");memcpy(data,vertices.data(),vertices.size());Check(input->Unlock(),"input unlock");
            Check(device->SetVertexDeclaration(inputDecl.value),"set declaration");Check(device->SetVertexShader(shader.value),"set shader");
            Check(device->SetVertexShaderConstantF(0,reinterpret_cast<const float*>(constants.data()),static_cast<UINT>(constants.size()/16)),"set constants");
            Check(device->SetStreamSource(0,input.value,0,sizeof(Vertex)),"set stream");
            Check(device->ProcessVertices(0,0,count,output.value,nullptr,0),"ProcessVertices");
            std::vector<unsigned char> result(count*sizeof(Output));Check(output->Lock(0,0,&data,D3DLOCK_READONLY),"output lock");memcpy(result.data(),data,result.size());Check(output->Unlock(),"output unlock");
            FILE* file=nullptr;if(_wfopen_s(&file,argv[4],L"wb")||!file)throw std::runtime_error("output open");
            const bool written=fwrite(result.data(),1,result.size(),file)==result.size();const int closed=fclose(file);if(!written||closed)throw std::runtime_error("output write");
            std::printf("PASS system D3D9 ProcessVertices: %u vertices, FVF packed ARGB colors\n",count);
        }
        DestroyWindow(window);FreeLibrary(module);return 0;
    }catch(const std::exception& e){if(window)DestroyWindow(window);if(module)FreeLibrary(module);std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
