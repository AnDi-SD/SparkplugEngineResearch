#include "../Code/SparkplugDX/spDXRenderTarget.h"
#include "../Code/SparkplugDX/spDXCubeRenderTarget.h"
#include "../Code/SparkplugDX/spDXTexture.h"
#include "../Code/SparkplugDX/spDXCubeTexture.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool condition)
    { if (!condition) throw std::runtime_error("render-target lifecycle check failed"); }
    struct Fixture final
    {
        bool cube, failed, retained;
        std::uint32_t width = 0, height = 0, format = 0;
        unsigned textureRefs = 0, deviceRefs = 3;
        std::vector<unsigned> surfaceRefs;
        std::vector<std::string> events;
        DXTextureResourceAPIForAnalysis api;
        DXRenderTargetStateForAnalysis ordinary;
        DXCubeRenderTargetStateForAnalysis cubical;
        DXTextureResourceGlobalsForAnalysis globals;
        static constexpr DXResourceHandleForAnalysis Device = 100, Texture = 101, Surface = 200;
        std::uint32_t Hr() const { return failed ? 0x8876086Cu : 0; }
        template<class... T> void Event(const char* name, T... values)
        {
            std::ostringstream stream;
            stream << "[\"" << name << '"';
            ((stream << ',' << values), ...);
            stream << ']'; events.push_back(stream.str());
        }
        Fixture(bool isCube, bool hasFailure, bool hasExternalRefs)
            : cube(isCube), failed(hasFailure), retained(hasExternalRefs), surfaceRefs(isCube ? 6 : 1, 0)
        {
            ordinary.width = ordinary.height = ordinary.pixelFormat = 0;
            cubical.width = cubical.height = cubical.pixelFormat = 0;
            ordinary.backing.device = cubical.backing.device = Device;
            api.createTexture = [this](auto device, auto w, auto h, auto levels, auto usage,
                auto fmt, auto pool, auto& out, auto shared)
            { Check(!cube && device == Device); Create(w,h,levels,usage,fmt,pool,out,shared); return static_cast<std::int32_t>(Hr()); };
            api.createCubeTexture = [this](auto device, auto w, auto levels, auto usage,
                auto fmt, auto pool, auto& out, auto shared)
            { Check(cube && device == Device); Create(w,w,levels,usage,fmt,pool,out,shared); return static_cast<std::int32_t>(Hr()); };
            api.getSurfaceLevel = [this](auto texture, auto level, auto& out)
            { Check(!cube && texture == Texture && level == 0); Acquire(0,level,out); return static_cast<std::int32_t>(Hr()); };
            api.getCubeMapSurface = [this](auto texture, auto face, auto level, auto& out)
            { Check(cube && texture == Texture && face < 6 && level == 0); Acquire(face,level,out); return static_cast<std::int32_t>(Hr()); };
            api.getSurfaceDescription = [this](auto surface, auto& out)
            {
                const auto face = static_cast<unsigned>(surface - Surface);
                Check(face < surfaceRefs.size() && surfaceRefs[face] > 0);
                out = DXSurfaceDescriptionForAnalysis{format,width,height};
                Event("describe_surface",face,width,height,Hr()); return static_cast<std::int32_t>(Hr());
            };
            api.addRef = [this](auto texture)
            { Check(texture == Texture && textureRefs > 0); Event("texture_addref",++textureRefs); return textureRefs; };
            api.release = [this](auto handle)
            {
                if (handle == Device)
                { Check(deviceRefs > 1); Event("device_release",--deviceRefs); return deviceRefs; }
                if (handle == Texture)
                {
                    Check(textureRefs > 0); --textureRefs;
                    if (textureRefs == 0) for (auto& refs : surfaceRefs) { Check(refs > 0); --refs; }
                    Event("texture_release",textureRefs); return textureRefs;
                }
                const auto face = static_cast<unsigned>(handle - Surface);
                Check(face < surfaceRefs.size() && surfaceRefs[face] > 0);
                Event("surface_release",face,--surfaceRefs[face]); return surfaceRefs[face];
            };
            api.getLevelCount = [this](auto texture)
            { Check(texture == Texture && textureRefs > 0); Event("get_level_count",1); return 1u; };
        }
        void Create(unsigned w, unsigned h, unsigned levels, unsigned usage,
            unsigned fmt, unsigned pool, DXResourceHandleForAnalysis& out, DXResourceHandleForAnalysis shared)
        {
            Check(textureRefs == 0 && levels == 1 && usage == 1 && pool == 0 && shared == 0);
            width=w; height=h; format=fmt; textureRefs=retained ? 2u : 1u;
            surfaceRefs.assign(surfaceRefs.size(),retained ? 2u : 1u); out=Texture;
            Event("create",w,h,levels,usage,fmt,pool,shared,Hr());
        }
        void Acquire(unsigned face,unsigned level,DXResourceHandleForAnalysis& out)
        { Check(textureRefs > 0); ++surfaceRefs[face]; out=Surface+face; Event("acquire_surface",face,level,Hr()); }
        DXResourceResultForAnalysis Init(unsigned w,unsigned h,unsigned fmt)
        { return cube ? spDXCubeRenderTarget::InitializeDeviceTargetForAnalysis(cubical,Device,w,h,fmt,globals,api)
            : spDXRenderTarget::InitializeDeviceTargetForAnalysis(ordinary,Device,w,h,fmt,api); }
        bool Release()
        { return cube ? spDXCubeRenderTarget::ReleaseDeviceTargetForAnalysis(cubical,api)
            : spDXRenderTarget::ReleaseDeviceTargetForAnalysis(ordinary,api); }
        DXResourceResultForAnalysis Reinit()
        { return cube ? spDXCubeRenderTarget::ReinitializeDeviceTargetForAnalysis(cubical,Device,globals,api)
            : spDXRenderTarget::ReinitializeDeviceTargetForAnalysis(ordinary,Device,api); }
        std::string Snapshot(const char* operation, const char* result) const
        {
            const auto& backing=cube ? cubical.backing : ordinary.backing;
            std::ostringstream s;
            s << "{\"operation\":\"" << operation << "\",\"return\":" << result << ",\"events\":[";
            for (std::size_t i=0;i<events.size();++i) { if(i)s<<',';s<<events[i]; }
            s << "],\"target\":[" << (cube?cubical.width:ordinary.width) << ','
                << (cube?cubical.height:ordinary.height) << ',' << (cube?cubical.pixelFormat:ordinary.pixelFormat) << ",[";
            for(std::size_t i=0;i<surfaceRefs.size();++i)
            { if(i)s<<',';s<<((cube?cubical.surfaces[i]:ordinary.surface)!=0); }
            s << "]],\"backing\":[" << backing.width << ',' << backing.height << ',' << backing.format << ',' << backing.byteCount
                << ',' << (backing.texture!=0) << ',' << (backing.device!=0) << ',' << unsigned(backing.initialized)
                << ',' << unsigned(backing.field31) << ',' << (backing.palette!=0)
                << "],\"textureRefs\":" << textureRefs << ",\"surfaceRefs\":[";
            for(std::size_t i=0;i<surfaceRefs.size();++i) { if(i)s<<',';s<<surfaceRefs[i]; }
            s << "],\"deviceRefs\":" << deviceRefs << ",\"mipProcessing\":" << unsigned(globals.mipProcessing) << '}';
            return s.str();
        }
    };

    std::string Run(const std::string& mode)
    {
        const bool cube=mode.compare(0,4,"cube")==0,retained=mode.find("retained")!=std::string::npos;
        const bool failed=mode.find("failed-hresult")!=std::string::npos;
        Fixture f(cube,failed,retained);
        std::vector<std::string> steps;
        const auto init=f.Init(7,9,3);Check(init.completed&&init.nativeReturn);
        steps.push_back(f.Snapshot("Init","1"));f.events.clear();
        Check(f.Release());steps.push_back(f.Snapshot("Release","null"));f.events.clear();
        if(!retained)
        {
            const auto reinit=f.Reinit();Check(reinit.completed&&reinit.nativeReturn);
            steps.push_back(f.Snapshot("Reinit","1"));f.events.clear();
            const auto invalid=f.Init(1,2,2);Check(invalid.completed&&!invalid.nativeReturn);
            steps.push_back(f.Snapshot("InvalidInit","0"));
            Check(f.textureRefs==0);
            for(auto refs:f.surfaceRefs)Check(refs==0);
        }
        else
        {
            Check(f.textureRefs==1);
            for(auto refs:f.surfaceRefs)Check(refs==2);
        }
        std::ostringstream s;s<<"{\"mode\":\""<<mode<<"\",\"steps\":[";
        for(std::size_t i=0;i<steps.size();++i){if(i)s<<',';s<<steps[i];}s<<"]}";return s.str();
    }

    std::string Measure(const std::string& mode)
    {
        const bool attached=mode.compare(0,7,"attach-")==0;
        const std::string profile=attached ? mode.substr(7) : mode;
        Fixture f(false,profile=="rgba-failed-hresult",false);
        const std::uint32_t format=profile=="dxt1" ? 0x31545844u :
            profile=="dxt3" ? 0x33545844u : profile=="dxt5" ? 0x35545844u : 0x15u;
        const std::uint32_t count=profile=="zero" ? 0u : 3u;
        constexpr std::uint32_t widths[]{7,3,1},heights[]{9,4,2},pitches[]{32,16,8};
        std::vector<unsigned> refs(count,1);
        unsigned textureRefs=1;
        auto api=f.api;
        api.addRef=[&](auto texture)
        { Check(texture==Fixture::Texture);f.Event("texture_addref",++textureRefs);return textureRefs; };
        api.getLevelCount=[&](auto texture)
        { Check(texture==Fixture::Texture);f.Event("get_level_count",count);return count; };
        api.getSurfaceLevel=[&](auto texture,auto level,auto& out)
        {
            Check(texture==Fixture::Texture&&level<count);++refs[level];out=Fixture::Surface+level;
            f.Event("acquire_surface",level,f.Hr());return static_cast<std::int32_t>(f.Hr());
        };
        api.getSurfaceDescription=[&](auto surface,auto& out)
        {
            const auto level=static_cast<unsigned>(surface-Fixture::Surface);Check(level<count);
            out=DXSurfaceDescriptionForAnalysis{format,widths[level],heights[level]};
            f.Event("describe_surface",level,format,widths[level],heights[level],f.Hr());return static_cast<std::int32_t>(f.Hr());
        };
        api.lockSurface=[&](auto surface,auto& out,auto rect,auto flags)
        {
            const auto level=static_cast<unsigned>(surface-Fixture::Surface);Check(level<count&&rect==0&&flags==0);
            out=DXLockedRectForAnalysis{pitches[level],0};f.Event("lock_surface",level,pitches[level],rect,flags,f.Hr());
            return static_cast<std::int32_t>(f.Hr());
        };
        api.unlockSurface=[&](auto surface)
        { const auto level=static_cast<unsigned>(surface-Fixture::Surface);Check(level<count);f.Event("unlock_surface",level,f.Hr());return static_cast<std::int32_t>(f.Hr()); };
        api.release=[&](auto surface)
        {
            const auto level=static_cast<unsigned>(surface-Fixture::Surface);Check(level<count&&refs[level]>1);
            f.Event("surface_release",level,--refs[level]);return refs[level];
        };
        DXTextureResourceStateForAnalysis state;state.texture=attached ? 0 : Fixture::Texture;state.byteCount=0xA5A5A5A5u;
        const auto measured=attached ? spDXTexture::AttachDeviceTextureForAnalysis(state,7,9,
            spDXTexture::COMFormatToRuntimeForAnalysis(format),Fixture::Texture,0,0,api)
            : spDXTexture::MeasureDeviceByteCountForAnalysis(state,api);
        Check(measured.completed&&(attached ? !measured.nativeReturn : measured.nativeReturn));
        Check(state.byteCount==(count==0 ? 0u : format==0x15 ? 368u : format==0x31545844 ? 32u : 64u));
        for(auto ref:refs)Check(ref==1);
        std::ostringstream s;s<<"{\"byteCount\":"<<state.byteCount<<",\"nativeReturn\":"<<(attached?"null":"1")<<",\"events\":[";
        for(std::size_t i=0;i<f.events.size();++i){if(i)s<<',';s<<f.events[i];}
        s<<"],\"surfaceRefs\":[";for(std::size_t i=0;i<refs.size();++i){if(i)s<<',';s<<refs[i];}s<<']';
        if(attached)
        {
            Check(textureRefs==2);
            s<<",\"textureRefs\":"<<textureRefs<<",\"backing\":["<<state.width<<','<<state.height<<','<<state.format<<','
                <<state.byteCount<<','<<(state.texture!=0)<<','<<unsigned(state.initialized)<<','<<unsigned(state.field31)<<']';
        }
        s<<'}';
        return s.str();
    }

    void Guards()
    {
        Fixture noTexture(false,false,false);
        noTexture.api.createTexture=[](auto,auto,auto,auto,auto,auto,auto,auto&,auto){return -1;};
        const auto null=noTexture.Init(7,9,3);
        Check(!null.completed&&!null.nativeReturn&&noTexture.ordinary.width==0);
        Fixture noSurface(false,false,false);
        noSurface.api.getSurfaceLevel=[](auto,auto,auto&){return -1;};
        const auto surface=noSurface.Init(7,9,3);
        Check(!surface.completed&&surface.createdTexture==Fixture::Texture&&noSurface.textureRefs==1);
        Fixture noDescription(false,false,false);
        noDescription.api.getSurfaceDescription=[](auto,auto&){return -1;};
        const auto desc=noDescription.Init(7,9,3);
        Check(!desc.completed&&desc.createdTexture==Fixture::Texture&&noDescription.ordinary.surface!=0);
        Fixture palette(false,false,false);
        palette.ordinary.backing.palette=999;
        Check(!palette.Init(7,9,3).completed&&palette.events.empty());
        Fixture invalidReset(false,false,false);
        Check(!invalidReset.Reinit().completed&&invalidReset.events.empty());
        DXTextureResourceStateForAnalysis state; state.texture=1;state.palette=2;
        Check(!spDXTexture::AttachDeviceTextureForAnalysis(state,7,9,6,1,0,14,palette.api).completed);
        Check(state.texture==1&&state.palette==2);
        DXCubeRenderTargetStateForAnalysis faces;faces.surfaces[0]=200;faces.backing.texture=101;faces.backing.device=100;
        Check(!spDXCubeRenderTarget::ReleaseDeviceTargetForAnalysis(faces,palette.api));
        Check(faces.surfaces[0]==200&&palette.events.empty());
        for(const auto fmt:{0u,1u,2u,3u,4u,5u,6u,7u,0x19u,0xFFFFFFFFu})
        { const auto com=spDXTexture::RuntimeFormatToCOMForAnalysis(fmt);Check(spDXTexture::COMFormatToRuntimeForAnalysis(com)==fmt); }
        DXTextureResourceStateForAnalysis measurement;measurement.texture=1;measurement.byteCount=17;
        DXTextureResourceAPIForAnalysis bounded;bounded.getLevelCount=[](auto){return 33u;};
        Check(!spDXTexture::MeasureDeviceByteCountForAnalysis(measurement,bounded).completed&&measurement.byteCount==0);
        bounded.getLevelCount=[](auto){return 1u;};
        bounded.getSurfaceLevel=[](auto,auto,auto& out){out=2;return 0;};
        bounded.getSurfaceDescription=[](auto,auto&){return -1;};bounded.release=[](auto){throw std::runtime_error("unexpected release");return 0u;};
        const auto missing=spDXTexture::MeasureDeviceByteCountForAnalysis(measurement,bounded);
        Check(!missing.completed&&missing.acquiredMipSurfaces[0]==2&&measurement.byteCount==0);
    }
}
int main(int argc,char** argv)
{
    if(argc==3&&std::string(argv[1])=="--measure")
    {
        const std::string mode=argv[2];
        if(mode!="rgba"&&mode!="dxt1"&&mode!="dxt3"&&mode!="dxt5"&&mode!="zero"&&mode!="rgba-failed-hresult"
            &&mode!="attach-rgba"&&mode!="attach-dxt1")return 2;
        std::cout<<Measure(mode)<<'\n';return 0;
    }
    if(argc==3&&std::string(argv[1])=="--case")
    {
        const std::string mode=argv[2];
        if(mode!="ordinary"&&mode!="cube"&&mode!="ordinary-failed-hresult"
            &&mode!="cube-failed-hresult"&&mode!="ordinary-retained"&&mode!="cube-retained")return 2;
        std::cout<<Run(mode)<<'\n';return 0;
    }
    if(argc!=1)return 2;
    for(const auto* mode:{"ordinary","cube","ordinary-failed-hresult","cube-failed-hresult","ordinary-retained","cube-retained"})(void)Run(mode);
    for(const auto* mode:{"rgba","dxt1","dxt3","dxt5","zero","rgba-failed-hresult","attach-rgba","attach-dxt1"})(void)Measure(mode);
    Guards();std::cout<<"PASS render-target COM lifecycle and explicit incomplete boundaries\n";
}
