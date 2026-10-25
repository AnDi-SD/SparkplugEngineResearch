#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/Sparkplug/spTextureData.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value){if(!value)throw std::runtime_error("native texture upload check failed");}
    std::string Hex(const std::vector<std::byte>& bytes)
    {
        constexpr char digits[]="0123456789abcdef";std::string result;
        for(auto byte:bytes){const auto value=std::to_integer<unsigned>(byte);result+=digits[value>>4];result+=digits[value&15];}
        return result;
    }
    struct Level
    {
        std::uint32_t width=0,height=0,pitch=0,rows=0,refs=0;
        std::vector<std::byte> pixels;
        bool locked=false;
    };
    struct Fixture
    {
        spTextureData data;
        DXTextureResourceStateForAnalysis state;
        DXTextureResourceGlobalsForAnalysis globals;
        DXTextureResourceAPIForAnalysis api;
        std::vector<Level> levels;
        std::vector<std::string> events;
        std::uint32_t textureRefs=0,dimension=0,format=0,flags=0;
        bool failedCreate=false,noDescription=false;
        explicit Fixture(const std::string& mode)
        {
            const auto dash=mode.find('-');const auto kind=mode.substr(0,dash);
            Check(dash!=std::string::npos&&dash+1<mode.size());dimension=static_cast<unsigned>(mode[dash+1]-'0');
            Check(dimension==2||dimension==4);
            if(kind=="raw"){flags=0;format=0x15;}
            else if(kind=="dxt1"){flags=1;format=0x31545844;}
            else if(kind=="dxt3"){flags=2;format=0x33545844;}
            else if(kind=="dxt5"){flags=3;format=0x35545844;}else Check(false);
            failedCreate=mode=="raw-2-failed-create";noDescription=mode=="raw-2-no-desc";
            Check(mode==kind+"-"+std::to_string(dimension)||failedCreate||noDescription);
            std::vector<spTextureData::NativeMipForAnalysis> mips;
            for(auto size=dimension;;size=std::max(1u,size>>1))
            {
                const auto stride=flags?std::max(1u,size>>2)*(flags==1?8u:16u):size*4;
                const auto rows=flags?std::max(1u,size>>2):size;
                spTextureData::NativeMipForAnalysis mip;mip.width=size;mip.rows=rows;mip.rowStride=stride;
                mip.bytes.resize(std::size_t(stride)*rows);
                for(std::size_t i=0;i<mip.bytes.size();++i)mip.bytes[i]=std::byte((i*17+mips.size()*5+3)&255);
                mips.push_back(std::move(mip));
                Level level;level.width=level.height=size;level.rows=rows;level.pitch=stride+4;
                level.pixels.assign(std::size_t(level.pitch)*rows,std::byte{0xA5});levels.push_back(std::move(level));
                if(size==1)break;
            }
            Check(data.SetNativeMipDataForAnalysis(dimension,dimension,flags,3,std::move(mips)));
            state.device=100;state.field18=2;state.field1C=7;state.textureFlags=9;
            state.width=3;state.height=5;state.format=0xAABBCCDD;state.byteCount=0x11223344;
            state.initialized=4;state.field31=8;
            api.createTexture=[this](std::uintptr_t device,std::uint32_t width,std::uint32_t height,
                std::uint32_t count,std::uint32_t usage,std::uint32_t fmt,std::uint32_t pool,
                std::uintptr_t& out,std::uintptr_t shared)
            {
                Check(device==100&&width==dimension&&height==dimension&&count==0&&usage==0
                    &&fmt==format&&pool==1&&shared==0&& &out==&state.texture&&textureRefs==0);
                events.push_back("[\"CreateTexture\","+std::to_string(width)+","+std::to_string(height)
                    +",0,0,"+std::to_string(fmt)+",1,0]");out=101;textureRefs=1;
                for(auto& level:levels){Check(level.refs==0);level.refs=1;}
                if(failedCreate)events.push_back("[\"CreateTextureHRESULT\",2289436780]");
                return static_cast<std::int32_t>(failedCreate?0x8876086Cu:0u);
            };
            api.getSurfaceLevel=[this](std::uintptr_t texture,std::uint32_t level,std::uintptr_t& out)
            {
                Check(texture==101&&textureRefs&&level<levels.size());++levels[level].refs;out=200+level;
                events.push_back("[\"GetSurfaceLevel\","+std::to_string(level)+"]");return 0;
            };
            api.getSurfaceDescription=[this](std::uintptr_t surface,std::optional<DXSurfaceDescriptionForAnalysis>& out)
            {
                const auto index=Index(surface);const auto& level=levels[index];
                if(noDescription){events.push_back("[\"GetDescNotWritten\",2289436780]");return static_cast<std::int32_t>(0x8876086Cu);}
                out=DXSurfaceDescriptionForAnalysis{format,level.width,level.height};
                events.push_back("[\"GetDesc\","+std::to_string(format)+","+std::to_string(level.width)+","+std::to_string(level.height)+"]");return 0;
            };
            api.lockSurface=[this](std::uintptr_t surface,std::optional<DXLockedRectForAnalysis>& out,
                std::uintptr_t rectangle,std::uint32_t lockFlags)
            {
                const auto index=Index(surface);auto& level=levels[index];Check(!level.locked&&!rectangle&&lockFlags==0x10);
                level.locked=true;out=DXLockedRectForAnalysis{level.pitch,reinterpret_cast<std::uintptr_t>(level.pixels.data()),level.pixels.size()};
                events.push_back("[\"LockRect\","+std::to_string(level.pitch)+",16,"+std::to_string(index)+"]");return 0;
            };
            api.unlockSurface=[this](std::uintptr_t surface)
            {
                const auto index=Index(surface);Check(levels[index].locked);levels[index].locked=false;
                events.push_back("[\"UnlockRect\","+std::to_string(index)+"]");return 0;
            };
            api.release=[this](std::uintptr_t handle)
            {
                if(handle==101)
                {
                    Check(textureRefs>0);--textureRefs;
                    if(!textureRefs)for(auto& level:levels){Check(level.refs>0);--level.refs;}
                    events.push_back("[\"ReleaseTexture\"]");return textureRefs;
                }
                const auto index=Index(handle);Check(levels[index].refs>1);--levels[index].refs;
                events.push_back("[\"ReleaseSurface\","+std::to_string(index)+"]");return levels[index].refs;
            };
            api.getLevelCount=[this](std::uintptr_t texture)
            {
                Check(texture==101&&textureRefs>0);events.push_back("[\"GetLevelCount\","+std::to_string(levels.size())+"]");
                return static_cast<std::uint32_t>(levels.size());
            };
        }
        std::size_t Index(std::uintptr_t handle) const
        {Check(handle>=200&&handle<200+levels.size()&&levels[handle-200].refs>0);return handle-200;}
        DXResourceResultForAnalysis Upload()
        {return spDXTexture::UploadNativeDeviceTextureForAnalysis(state,data,globals,api);}
        void Cleanup()
        {
            for(std::size_t i=0;i<levels.size();++i)
            {
                if(levels[i].locked)(void)api.unlockSurface(200+i);
                while(levels[i].refs>1)(void)api.release(200+i);
            }
            if(textureRefs)(void)api.release(101);
            Check(!textureRefs);for(const auto& level:levels)Check(!level.refs&&!level.locked);
        }
        std::string Capture()
        {
            const auto result=Upload();Check(result.completed&&result.nativeReturn&&state.texture==101&&textureRefs==1);
            std::ostringstream s;s<<"{\"nativeReturn\":1,\"base\":["<<state.field18<<','<<unsigned(state.field1C)<<','
                <<state.textureFlags<<','<<unsigned(state.initialized)<<','<<state.width<<','<<state.height
                <<"],\"dx\":["<<state.format<<','<<state.byteCount<<','<<unsigned(state.field31)<<','<<state.palette<<"],\"copies\":[";
            for(std::size_t i=0;i<levels.size();++i){if(i)s<<',';s<<'"'<<Hex(levels[i].pixels)<<'"';}
            s<<"],\"events\":[";for(std::size_t i=0;i<events.size();++i){if(i)s<<',';s<<events[i];}
            s<<"],\"textureRefs\":"<<textureRefs<<",\"surfaceRefs\":[";
            for(std::size_t i=0;i<levels.size();++i){if(i)s<<',';s<<levels[i].refs;}
            s<<"],\"mipProcessing\":"<<unsigned(globals.mipProcessing)<<'}';Cleanup();return s.str();
        }
    };
    void Guards()
    {
        Fixture missing("raw-2");missing.api.lockSurface={};
        Check(!missing.Upload().completed&&missing.events.empty()&&!missing.state.texture);
        Fixture shortSpan("raw-2");auto lock=shortSpan.api.lockSurface;
        shortSpan.api.lockSurface=[lock](auto surface,auto& out,auto rect,auto flags)
        {const auto hr=lock(surface,out,rect,flags);out->accessibleBytes=1;return hr;};
        const auto rejected=shortSpan.Upload();Check(!rejected.completed&&rejected.acquiredMipSurfaces[0]==200
            &&shortSpan.levels[0].locked&&shortSpan.state.width==3&&shortSpan.state.format==0xAABBCCDD);
        for(auto b:shortSpan.levels[0].pixels)Check(b==std::byte{0xA5});shortSpan.Cleanup();
        Fixture alias("raw-2");const auto bytes=alias.data.GetNativeMipsForAnalysis()[0].bytes;
        alias.api.lockSurface=[&alias](auto,auto& out,auto,auto)
        {
            const auto& source=alias.data.GetNativeMipsForAnalysis()[0];
            out=DXLockedRectForAnalysis{source.rowStride,reinterpret_cast<std::uintptr_t>(source.bytes.data()),source.bytes.size()};return 0;
        };
        Check(!alias.Upload().completed&&alias.data.GetNativeMipsForAnalysis()[0].bytes==bytes);alias.Cleanup();
        Fixture partial("raw-4");auto mips=partial.data.GetNativeMipsForAnalysis();mips.pop_back();
        Check(partial.data.SetNativeMipDataForAnalysis(4,4,0,3,std::move(mips)));
        const auto stopped=partial.Upload();Check(!stopped.completed&&!stopped.nativeReturn&&partial.globals.mipProcessing==1
            &&partial.state.field18==2&&partial.state.width==3&&partial.state.initialized==4);
        partial.Cleanup();
        Fixture cross("raw-2");cross.data.SetField68ForAnalysis(false);
        Check(!cross.Upload().completed&&cross.events.empty()&&!cross.state.texture);
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")
        {Fixture fixture(argv[2]);std::cout<<fixture.Capture()<<'\n';return 0;}
        if(argc!=1)return 2;
        for(const auto* mode:{"raw-2","raw-4","dxt1-2","dxt1-4","dxt3-2","dxt3-4","dxt5-2","dxt5-4","raw-2-failed-create","raw-2-no-desc"})
        {Fixture fixture(mode);(void)fixture.Capture();}
        Guards();std::cout<<"PASS native texture COM upload, pitched row copies, metadata and explicit incomplete boundaries\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
