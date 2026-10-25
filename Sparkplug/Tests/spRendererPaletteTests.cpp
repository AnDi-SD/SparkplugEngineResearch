#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/Sparkplug/spPalette.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/SparkplugDX/spDXCubeTexture.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Renderer=spDXRenderer;
    void Check(bool condition)
    {if(!condition)throw std::runtime_error("renderer palette check failed");}
    struct Fixture
    {
        Renderer::DevicePalettePoolForAnalysis pool;
        spPalette palette;
        bool failed=false,valid=true;
        std::vector<std::string> events;
        std::array<std::byte,1024> entries{};
        explicit Fixture(const std::string& mode)
        {
            for(std::size_t i=0;i<entries.size();++i)entries[i]=std::byte((i*13+7)&255);
            Check(palette.SetEntriesForAnalysis(entries.data(),entries.size()));
            pool.nextIndex=mode=="counter-wrap"?0xFFFFFFFFu:17u;
            if(mode=="reuse-duplicates")pool.freeIndices={7,9,7};
            else if(mode.compare(0,7,"recycle")==0)pool.freeIndices={7,9};
            if(mode=="recycle")palette.SetDeviceIndexForAnalysis(23);
            failed=mode=="failed-hresult";
        }
        static std::int32_t Submit(void* context,std::uintptr_t device,
            std::uint32_t index,const std::byte* entries) noexcept
        {
            auto& self=*static_cast<Fixture*>(context);
            self.valid&=device==100&&entries!=nullptr;
            if(entries)for(std::size_t i=0;i<self.entries.size();++i)self.valid&=entries[i]==self.entries[i];
            const std::uint32_t hr=self.failed?0x8876086Cu:0u;
            // SHA of the independently generated synthetic1024byte fixture,
            // after checking every consumed byte; no game payload is embedded.
            std::ostringstream s;s<<"[\"set_palette_entries\","<<index
                <<",\"5c4ec75221d7de1d903915d958d14e9f35a6e52c1c10e13d2e39a88d4e6dda8a\","<<hr<<']';
            self.events.push_back(s.str());return static_cast<std::int32_t>(hr);
        }
        std::string Capture(const std::string& mode)
        {
            const auto result=mode.compare(0,7,"recycle")==0
                ?Renderer::RecyclePaletteIndexForAnalysis(pool,palette)
                :Renderer::RegisterPaletteForAnalysis(pool,100,palette,Submit,this);
            Check(result.completed&&result.nativeReturn&&valid);
            std::ostringstream s;s<<"{\"next\":"<<pool.nextIndex<<",\"free\":[";
            bool first=true;for(auto index:pool.freeIndices){if(!first)s<<',';first=false;s<<index;}
            s<<"],\"paletteIndex\":"<<palette.GetIndexForAnalysis()<<",\"nativeReturn\":1,\"events\":[";
            for(std::size_t i=0;i<events.size();++i){if(i)s<<',';s<<events[i];}s<<"]}";return s.str();
        }
    };
    void Guards()
    {
        Fixture f("new");spPalette unknown;
        Check(!Renderer::RegisterPaletteForAnalysis(f.pool,100,unknown,Fixture::Submit,&f).completed);
        Check(f.pool.nextIndex==17&&f.events.empty());
        Check(!Renderer::RegisterPaletteForAnalysis(f.pool,0,f.palette,Fixture::Submit,&f).completed);
        Check(!Renderer::RegisterPaletteForAnalysis(f.pool,100,f.palette,nullptr,&f).completed);
        f.palette.SetDeviceIndexForAnalysis(23);f.pool.maximumFreeIndices=0;
        Check(!Renderer::RecyclePaletteIndexForAnalysis(f.pool,f.palette).completed&&f.pool.freeIndices.empty());
        f.palette.SetDeviceIndexForAnalysis(0xFFFFFFFFu);
        Check(Renderer::RecyclePaletteIndexForAnalysis(f.pool,f.palette).completed&&f.pool.freeIndices.empty());
        Fixture pending("reuse-duplicates");const auto before=pending.pool.freeIndices;
        Check(Renderer::RegisterPaletteForAnalysis(pending.pool,100,pending.palette,Fixture::Submit,&pending).completed);
        Check(before==std::list<std::uint32_t>({7,9,7})&&pending.pool.freeIndices==std::list<std::uint32_t>{9}
            &&pending.pool.nextIndex==17&&pending.palette.GetIndexForAnalysis()==7&&pending.events.size()==1);
    }
    template<class Texture> void Ownership()
    {
        Texture texture;DXPalettePoolForAnalysis pool;
        auto old=std::make_unique<spPalette>();old->SetDeviceIndexForAnalysis(5);
        Check(texture.ReplaceDevicePaletteForAnalysis(pool,old).completed&&!old);
        auto next=std::make_unique<spPalette>();next->SetDeviceIndexForAnalysis(23);
        const auto* newIdentity=next.get();
        Check(texture.ReplaceDevicePaletteForAnalysis(pool,next).completed&&!next);
        Check(texture.GetPaletteForAnalysis()==newIdentity&&pool.freeIndices==std::list<std::uint32_t>{23});
        std::unique_ptr<spPalette> null;
        Check(!texture.ReplaceDevicePaletteForAnalysis(pool,null).completed);
        Check(texture.GetPaletteForAnalysis()==newIdentity&&pool.freeIndices==std::list<std::uint32_t>{23});
        // Deliberately malformed duplicate owner is refused before mutation;
        // release its ownership token afterwards so the real owner deletes once.
        std::unique_ptr<spPalette> alias(const_cast<spPalette*>(newIdentity));
        Check(!texture.ReplaceDevicePaletteForAnalysis(pool,alias).completed);
        Check(texture.GetPaletteForAnalysis()==newIdentity&&pool.freeIndices==std::list<std::uint32_t>{23});
        (void)alias.release();
        auto unassigned=std::make_unique<spPalette>();const auto* identity=unassigned.get();
        Check(texture.ReplaceDevicePaletteForAnalysis(pool,unassigned).completed&&!unassigned);
        Check(texture.GetPaletteForAnalysis()==identity&&pool.freeIndices==std::list<std::uint32_t>{23});
    }
    template<class Texture> std::string OwnerCapture(const std::string& mode)
    {
        Texture texture;DXPalettePoolForAnalysis pool;pool.nextIndex=17;
        if(mode.compare(0,7,"replace")==0)
        {
            auto old=std::make_unique<spPalette>();old->SetDeviceIndexForAnalysis(5);
            texture.AdoptPaletteForAnalysis(std::move(old)); // Explicit initial fixture state.
        }
        std::unique_ptr<spPalette> incoming;
        if(mode!="empty")
        {
            incoming=std::make_unique<spPalette>();
            if(mode=="adopt"||mode=="replace-assigned")incoming->SetDeviceIndexForAnalysis(23);
        }
        const auto* identity=incoming.get();
        const auto result=texture.ReplaceDevicePaletteForAnalysis(pool,incoming);
        Check(result.completed&&!result.nativeReturn&&!incoming&&texture.GetPaletteForAnalysis()==identity);
        std::ostringstream s;s<<"{\"current\":"<<(identity!=nullptr)<<",\"paletteIndex\":";
        if(identity)s<<identity->GetIndexForAnalysis();else s<<"null";
        s<<",\"next\":"<<pool.nextIndex<<",\"free\":[";bool first=true;
        for(auto index:pool.freeIndices){if(!first)s<<',';first=false;s<<index;}s<<"],\"events\":[]}";
        return s.str();
    }
    template<class Texture> struct AttachmentFixture
    {
        Texture texture;
        DXPalettePoolForAnalysis pool;
        DXTextureResourceStateForAnalysis state;
        DXTextureResourceGlobalsForAnalysis globals;
        DXTextureResourceAPIForAnalysis api;
        std::unique_ptr<spPalette> incoming;
        const spPalette* oldIdentity=nullptr;
        const spPalette* newIdentity=nullptr;
        std::array<std::byte,1024> entries{};
        std::vector<std::string> events;
        std::uint32_t oldRefs=1,newRefs=1;
        bool failed=false,valid=true;
        static constexpr bool cube=std::is_same_v<Texture,spDXCubeTexture>;
        explicit AttachmentFixture(const std::string& mode)
        {
            pool.nextIndex=17;
            if(mode=="replace-reuse")pool.freeIndices={7,9,7};
            if(mode!="new")
            {
                auto old=std::make_unique<spPalette>();old->SetDeviceIndexForAnalysis(5);
                oldIdentity=old.get();texture.AdoptPaletteForAnalysis(std::move(old));
            }
            incoming=std::make_unique<spPalette>();newIdentity=incoming.get();
            for(std::size_t i=0;i<entries.size();++i)entries[i]=std::byte((i*13+7)&255);
            Check(incoming->SetEntriesForAnalysis(entries.data(),entries.size()));
            if(mode=="replace-assigned")incoming->SetDeviceIndexForAnalysis(23);
            failed=mode=="failed-hresult";
            // Current global renderer device100 is distinct from saved backing38.
            state.device=99;state.texture=101;state.palette=reinterpret_cast<std::uintptr_t>(oldIdentity);
            state.width=1;state.height=2;state.format=99;state.byteCount=123;
            state.initialized=4;state.field31=7;
            api.release=[this](std::uintptr_t handle)
            {
                Check(handle==101&&oldRefs>0);--oldRefs;
                events.push_back("[\"release_old_texture\","+std::to_string(oldRefs)+","+View()+"]");return oldRefs;
            };
            api.addRef=[this](std::uintptr_t handle)
            {
                Check(handle==102&&newRefs>0);++newRefs;
                events.push_back("[\"addref_texture\","+std::to_string(newRefs)+","+View()+"]");return newRefs;
            };
            api.getLevelCount=[this](std::uintptr_t handle)
            {
                Check(handle==102);events.push_back("[\"get_level_count\",1,"+View()+"]");return 1u;
            };
            api.setPaletteEntries=Submit;api.paletteContext=this;
        }
        std::string View() const
        {
            std::ostringstream s;
            const auto* current=texture.GetPaletteForAnalysis();
            s<<'['<<(state.texture==0?0:state.texture==101?1:2)<<','
                <<(state.palette==0?0:state.palette==reinterpret_cast<std::uintptr_t>(oldIdentity)?1:2)<<',';
            if(current)s<<current->GetIndexForAnalysis();else s<<"null";
            s<<','<<state.width<<','<<state.height<<','<<state.format<<','<<state.byteCount<<','
                <<static_cast<unsigned>(state.initialized)<<','<<static_cast<unsigned>(state.field31)<<','<<pool.nextIndex<<",[";
            bool first=true;for(auto index:pool.freeIndices){if(!first)s<<',';first=false;s<<index;}
            s<<"],"<<static_cast<unsigned>(globals.mipProcessing)<<']';return s.str();
        }
        static std::int32_t Submit(void* context,std::uintptr_t device,
            std::uint32_t index,const std::byte* data) noexcept
        {
            auto& self=*static_cast<AttachmentFixture*>(context);
            self.valid&=device==100&&data!=nullptr;
            if(data)for(std::size_t i=0;i<self.entries.size();++i)self.valid&=data[i]==self.entries[i];
            const std::uint32_t hr=self.failed?0x8876086Cu:0u;
            self.events.push_back("[\"set_palette_entries\","+std::to_string(index)
                +",\"5c4ec75221d7de1d903915d958d14e9f35a6e52c1c10e13d2e39a88d4e6dda8a\","
                +std::to_string(hr)+","+self.View()+"]");return static_cast<std::int32_t>(hr);
        }
        DXResourceResultForAnalysis Attach()
        {
            if constexpr(cube)return texture.AttachOwnedDeviceTextureForAnalysis(
                state,7,9,0x17,102,incoming,84,pool,100,globals,api);
            else return texture.AttachOwnedDeviceTextureForAnalysis(
                state,7,9,6,102,incoming,14,pool,100,api);
        }
        std::string Capture()
        {
            const auto result=Attach();
            Check(result.completed&&!result.nativeReturn&&!incoming&&valid&&oldRefs==0&&newRefs==2
                &&texture.GetPaletteForAnalysis()==newIdentity&&state.device==99);
            std::ostringstream s;s<<"{\"view\":"<<View()<<",\"events\":[";
            for(std::size_t i=0;i<events.size();++i){if(i)s<<',';s<<events[i];}
            s<<"],\"oldTextureRefs\":"<<oldRefs<<",\"newTextureRefs\":"<<newRefs<<'}';return s.str();
        }
    };
    template<class Texture> void AttachmentGuards()
    {
        AttachmentFixture<Texture> mismatch("new");mismatch.state.palette=999;
        Check(!mismatch.Attach().completed&&mismatch.events.empty()&&mismatch.oldRefs==1&&mismatch.incoming);
        AttachmentFixture<Texture> nullNew("replace-assigned");nullNew.incoming.reset();
        Check(!nullNew.Attach().completed&&nullNew.events.empty()&&nullNew.oldRefs==1);
        AttachmentFixture<Texture> alias("replace-assigned");alias.incoming.reset(
            const_cast<spPalette*>(alias.texture.GetPaletteForAnalysis()));
        Check(!alias.Attach().completed&&alias.events.empty()&&alias.oldRefs==1);
        (void)alias.incoming.release(); // Duplicate token must not delete the real owner.
        AttachmentFixture<Texture> missing("new");missing.api.setPaletteEntries=nullptr;
        Check(!missing.Attach().completed&&missing.events.size()==1&&missing.oldRefs==0&&missing.newRefs==1
            &&!missing.incoming&&missing.texture.GetPaletteForAnalysis()==missing.newIdentity
            &&missing.state.texture==102&&missing.state.width==1&&missing.pool.nextIndex==17);
    }
}
int main(int argc,char** argv)
{
    if(argc==4&&std::string(argv[1])=="--attach")
    {
        const std::string type=argv[2],mode=argv[3];
        if(mode!="new"&&mode!="replace-assigned"&&mode!="replace-reuse"&&mode!="failed-hresult")return 2;
        if(type=="ordinary"){AttachmentFixture<spDXTexture> f(mode);std::cout<<f.Capture()<<'\n';}
        else if(type=="cube"){AttachmentFixture<spDXCubeTexture> f(mode);std::cout<<f.Capture()<<'\n';}else return 2;
        return 0;
    }
    if(argc==4&&std::string(argv[1])=="--owner")
    {
        const std::string type=argv[2],mode=argv[3];
        if(mode!="empty"&&mode!="adopt"&&mode!="replace-assigned"&&mode!="replace-unassigned")return 2;
        if(type=="ordinary")std::cout<<OwnerCapture<spDXTexture>(mode)<<'\n';
        else if(type=="cube")std::cout<<OwnerCapture<spDXCubeTexture>(mode)<<'\n';else return 2;
        return 0;
    }
    if(argc==3&&std::string(argv[1])=="--case")
    {
        const std::string mode=argv[2];
        if(mode!="new"&&mode!="reuse-duplicates"&&mode!="recycle"&&mode!="recycle-unassigned"&&mode!="failed-hresult"&&mode!="counter-wrap")return 2;
        Fixture f(mode);std::cout<<f.Capture(mode)<<'\n';return 0;
    }
    if(argc!=1)return 2;
    for(const auto* mode:{"new","reuse-duplicates","recycle","recycle-unassigned","failed-hresult","counter-wrap"})
    {Fixture f(mode);(void)f.Capture(mode);}
    Guards();Ownership<spDXTexture>();Ownership<spDXCubeTexture>();
    for(const auto* mode:{"empty","adopt","replace-assigned","replace-unassigned"})
    {(void)OwnerCapture<spDXTexture>(mode);(void)OwnerCapture<spDXCubeTexture>(mode);}
    for(const auto* mode:{"new","replace-assigned","replace-reuse","failed-hresult"})
    {AttachmentFixture<spDXTexture> ordinary(mode);(void)ordinary.Capture();
     AttachmentFixture<spDXCubeTexture> cube(mode);(void)cube.Capture();}
    AttachmentGuards<spDXTexture>();AttachmentGuards<spDXCubeTexture>();
    std::cout<<"PASS renderer palette pool, ownership, joined COM attachment and unsafe-input guards\n";
}
