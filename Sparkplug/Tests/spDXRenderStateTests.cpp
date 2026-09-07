#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

using sparkplug::reconstruction::spDXRenderer;
namespace
{
    struct Context
    {
        std::uint32_t count = 0, index = 0, value = 0;
        std::int32_t result = 0;
    };
    std::int32_t Submit(void* data, std::uint32_t index, std::uint32_t value) noexcept
    {
        auto& context = *static_cast<Context*>(data);
        ++context.count;
        context.index = index;
        context.value = value;
        return context.result;
    }
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    void UnitTests()
    {
        std::uint32_t cached = 0;
        Context context;
        context.result = -1;
        Check(spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(cached, 53, 1, Submit, &context),
              "device failure ignored");
        Check(cached == 1 && context.count == 1 && context.index == 53 && context.value == 1,
              "cache updated despite HRESULT");
        Check(
            spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(cached, 53, 1, Submit, &context) &&
                context.count == 1,
            "equal cache suppresses failed-state retry");
        Check(
            spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(cached, 53, 0, Submit, &context) &&
                cached == 0 && context.count == 2,
            "changed state resubmitted");
        Check(spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(cached, 53, 0, nullptr, nullptr),
              "equal cache does not need callback");
        Check(
            !spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(cached, 53, 1, nullptr, nullptr) &&
                cached == 0,
            "changed null callback host-only guard");
    }
    void Batch()
    {
        std::uint32_t cached, index, first, second, result;
        while (std::cin >> cached >> index >> first >> second >> result)
        {
            Context context;
            context.result = static_cast<std::int32_t>(result);
            const std::uint32_t values[2]{first, second};
            for (unsigned round = 0; round < 2; ++round)
            {
                const bool ok = spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(
                    cached, index, values[round], Submit, &context);
                if (context.count > 2)
                    throw std::runtime_error("bounded calls");
                if (round)
                    std::cout << ',';
                std::cout << ok << ',' << cached << ',' << context.count << ',' << context.index
                          << ',' << context.value;
            }
            std::cout << '\n';
        }
    }
    struct MappingSink
    {
        std::array<std::uint32_t,256> device{}; // declared test subset, not native extent/default
        std::uint32_t current=0;bool fail=false;
        std::vector<std::array<std::uint32_t,4>> events;
        static std::int32_t Device(void* ptr,std::uint32_t index,std::uint32_t value) noexcept
        {auto& s=*static_cast<MappingSink*>(ptr);s.events.push_back({index,value,s.current,s.device[index]});return s.fail?-1:0;}
        static std::int32_t Dispatch(void* ptr,std::uint32_t index,std::uint32_t value) noexcept
        {auto& s=*static_cast<MappingSink*>(ptr);return spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(s.device[index],index,value,Device,ptr)?0:-1;}
    };
    std::string MaterialMap(const std::string& mode)
    {
        using namespace sparkplug::reconstruction;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        if(mode.rfind("layer-",0)==0)
        {
            spStdLayer layer;if(mode=="layer-values")for(unsigned i=0;i<9;++i)layer.GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(i,0x10000000+i);
            for(std::uint32_t stage:{0u,1u,2u,0xffffffffu})
            {
                std::array<std::uint32_t,9> values{};Check(layer.CopyTextureStatesForAnalysis(stage,values),"copy actual PC nine-state prefix");
                if(!first)out<<',';first=false;out<<'['<<stage<<",1,[";for(unsigned i=0;i<9;++i){if(i)out<<',';out<<values[i];}out<<"]]";
            }
        }
        else
        {
            MappingSink sink;sink.fail=mode=="state-fail";sink.events.reserve(4);if(mode=="state-poison")sink.device.fill(0xa5a5a5a5);
            std::vector<std::pair<std::uint32_t,std::uint32_t>> inputs;
            auto add=[&](std::uint32_t index,std::initializer_list<std::uint32_t> values){for(auto value:values)inputs.emplace_back(index,value);};
            add(0,{17});add(1,{0,1,2});add(2,{0,1});add(3,{0,1,2});add(4,{0,1,2});add(5,{0,1,2});
            for(auto index:{6u,7u})for(unsigned value=0;value<8;++value)inputs.emplace_back(index,value);
            add(9,{0,255,0x12345678});for(unsigned value=0;value<8;++value)inputs.emplace_back(10,value);add(11,{19});
            for(auto [index,value]:inputs)for(unsigned repeat=0;repeat<(mode=="state-repeat"?2u:1u);++repeat)
            {
                sink.events.clear();sink.current=0;
                const bool result=spDXRenderer::ApplyMaterialRenderStateForAnalysis(sink.current,index,value,&MappingSink::Dispatch,&sink);
                Check(result==(index!=0&&index!=11)&&sink.current==value,"raw state stored before dispatch or bad-index return");if(repeat)Check(sink.events.empty(),"repeat suppressed by device state cache");
                if(!first)out<<',';first=false;out<<'['<<index<<','<<value<<','<<result<<','<<sink.current<<",[";
                bool firstEvent=true;for(auto event:sink.events){if(!firstEvent)out<<',';firstEvent=false;out<<"[\"render\","<<event[0]<<','<<event[1]<<','<<event[2]<<','<<event[3]<<']';}out<<"]]";
            }
        }
        out<<"]]";return out.str();
    }
    struct TextureSink
    {
        std::array<spDXRenderer::TextureStageCacheForAnalysis,8> cache{};
        struct Event{bool sampler;std::array<std::uint32_t,6> words;};
        std::array<Event,3> events{};unsigned count=0;std::uint32_t activeState=0;bool fail=false;
        static std::int32_t Submit(void* ptr,bool sampler,std::uint32_t stage,std::uint32_t index,std::uint32_t value) noexcept
        {
            auto& s=*static_cast<TextureSink*>(ptr);const auto& c=s.cache[stage];
            if(s.count<3)s.events[s.count++]={sampler,{stage,index,value,c.raw[s.activeState],c.coordinateIndex,c.transformFlags}};
            return s.fail?-1:0;
        }
    };
    std::string TextureMap(const std::string& mode)
    {
        std::vector<std::pair<std::uint32_t,std::uint32_t>> inputs;
        auto add=[&](std::uint32_t index,std::initializer_list<std::uint32_t> values){for(auto value:values)inputs.emplace_back(index,value);};
        if(mode=="ops")for(auto index:{1u,2u})for(unsigned v=0;v<16;++v)inputs.emplace_back(index,v);
        else if(mode=="address")for(auto index:{3u,4u})add(index,{0,1,2,3,0xffffffff});
        else if(mode=="border")add(5,{0,0xff000000,0x12345678,0xffffffff});
        else if(mode=="filter")add(6,{0,1,2,3,4,0xffffffff});
        else if(mode=="uv-index"||mode=="hardware"){for(unsigned v=0;v<14;++v)inputs.emplace_back(7,v);add(7,{0xffffffff});}
        else if(mode=="uv-flags"||mode=="hardware-flags"){for(unsigned v=0;v<17;++v)inputs.emplace_back(8,v);add(8,{0xffffffff});}
        else if(mode=="failed")for(unsigned i=1;i<9;++i)for(unsigned v=0;v<4;++v)inputs.emplace_back(i,v);
        else throw std::runtime_error("unknown finite texture map mode");
        TextureSink sink;sink.fail=mode=="failed";const bool overrideCoordinates=mode.rfind("hardware",0)==0;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        for(auto stage:{0u,1u,7u})for(auto [index,value]:inputs)for(unsigned repeat=0;repeat<2;++repeat)
        {
            sink.count=0;sink.activeState=index;auto& c=sink.cache[stage];
            const bool result=spDXRenderer::ApplyTextureStateForAnalysis(c,stage,index,value,overrideCoordinates,TextureSink::Submit,&sink);
            Check(result&&c.raw[index]==(mode=="hardware"?stage:value),"native texture raw cache and special override");
            if(repeat)Check(sink.count==(index==7||index==8?0u:index==6?3u:1u),"only coordinate/transform states deduplicate");
            if(!first)out<<',';first=false;
            out<<'['<<stage<<','<<index<<','<<value<<','<<result<<','<<c.raw[index]<<','<<c.coordinateIndex<<','<<c.transformFlags<<",[";
            for(unsigned i=0;i<sink.count;++i){if(i)out<<',';out<<"[\""<<(sink.events[i].sampler?"sampler":"stage")<<'"';for(auto word:sink.events[i].words)out<<','<<word;out<<']';}
            out<<"]]";
        }
        out<<"]]";return out.str();
    }
    std::uint32_t RawFloat(float value){std::uint32_t bits=0;std::memcpy(&bits,&value,4);return bits;}
    float FloatFromRaw(std::uint32_t bits){float value=0;std::memcpy(&value,&bits,4);return value;}
    std::string LightingSnapshot(const spDXRenderer::LightingStateForAnalysis& state)
    {
        std::ostringstream out;out<<"[[";bool first=true;
        for(const auto* color:{&state.diffuse,&state.ambient,&state.specular,&state.emissive})for(float v:*color)
        {if(!first)out<<',';first=false;out<<RawFloat(v);}
        out<<','<<RawFloat(state.specularPower)<<"],"<<state.diffuseSource<<','<<state.ambientSource<<']';return out.str();
    }
    struct LightingSink
    {
        spDXRenderer::LightingStateForAnalysis state;std::uint32_t rawMode=0;
        std::array<std::uint32_t,256> cache{};bool fail=false,sourceOnly=false;
        struct Event{std::uint32_t index,value,mode,old;spDXRenderer::LightingStateForAnalysis state;};
        std::array<Event,8> events{};unsigned count=0;
        static std::int32_t Device(void* ptr,std::uint32_t index,std::uint32_t value) noexcept
        {auto& s=*static_cast<LightingSink*>(ptr);if(s.count<8)s.events[s.count++]={index,value,s.rawMode,s.cache[index],s.state};return s.fail?-1:0;}
        static std::int32_t Dispatch(void* ptr,std::uint32_t index,std::uint32_t value) noexcept
        {auto& s=*static_cast<LightingSink*>(ptr);return spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(s.cache[index],index,value,Device,ptr)?0:-1;}
    };
    std::string LightingMap(const std::string& mode)
    {
        LightingSink sink;sink.fail=mode=="failed";sink.sourceOnly=mode=="sources";sink.cache.fill(0xa5a5a5a5);
        sink.state.packedColorC194=0x80402010;sink.state.diffuseSource=77;sink.state.ambientSource=88;
        unsigned j=1;for(auto* color:{&sink.state.diffuse,&sink.state.ambient,&sink.state.specular,&sink.state.emissive})for(auto& value:*color)value=float(j++)/16.F;
        const auto power=mode=="power-zero"?0u:mode=="power-negative"?0xbf800000u:mode=="power-nan"?0x7fc12345u:mode=="power-inf"?0x7f800000u:0x41000000u;
        sink.state.specularPower=FloatFromRaw(power);
        std::vector<std::pair<std::uint32_t,std::uint32_t>> inputs;
        if(sink.sourceOnly)for(auto entry:{0x4bddb0u,0x4bde00u})for(auto value:{10u,10u,11u,11u,12u,12u,9u,13u,0xffffffffu})inputs.emplace_back(entry,value);
        else for(auto value:{0u,1u,2u,3u,4u,5u,6u,7u,8u,0xffffffffu,6u,1u,3u})inputs.emplace_back(0x4b0ad0u,value);
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        for(auto [entry,value]:inputs)
        {
            sink.count=0;const auto before=LightingSnapshot(sink.state);bool result=false;
            if(sink.sourceOnly)
            {auto& raw=entry==0x4bddb0?sink.state.diffuseSource:sink.state.ambientSource;result=spDXRenderer::ApplyMaterialColorSourceForAnalysis(raw,entry==0x4bde00,value,LightingSink::Dispatch,&sink);}
            else result=spDXRenderer::ApplyMaterialRenderStateForAnalysis(sink.rawMode,8,value,LightingSink::Dispatch,&sink,&sink.state);
            Check(result,"actual lighting/source always success");if(!first)out<<',';first=false;
            out<<'['<<entry<<','<<value<<','<<result<<','<<before<<','<<LightingSnapshot(sink.state)<<",[";
            for(unsigned i=0;i<sink.count;++i)
            {
                const auto& event=sink.events[i];if(i)out<<',';out<<"[\"render\","<<event.index<<','<<event.value;
                if(!sink.sourceOnly)out<<','<<event.mode<<','<<event.old;
                out<<','<<LightingSnapshot(event.state)<<']';
            }
            out<<"]]";
        }
        out<<"]]";return out.str();
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            Batch();
        else if(argc==3&&std::string(argv[1])=="--material")std::cout<<MaterialMap(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--texture")std::cout<<TextureMap(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--lighting")std::cout<<LightingMap(argv[2])<<'\n';
        else
        {
            UnitTests();
            for(const char* mode:{"layer-default","layer-values","state-map","state-fail","state-poison","state-repeat"})MaterialMap(mode);
            using namespace sparkplug::reconstruction;
            spMaterialTextureLayer empty;std::array<std::uint32_t,9> states{};states.fill(0x12345678);
            Check(!empty.CopyTextureStatesForAnalysis(0,states)&&states[0]==0x12345678,"NULL nested texture host-only guard");
            std::uint32_t entry=0;MappingSink sink;
            Check(!spDXRenderer::ApplyMaterialRenderStateForAnalysis(entry,8,0,&MappingSink::Dispatch,&sink),"lighting8 requires explicit state, not guessed defaults");
            for(auto index:{2u,3u,6u,10u})Check(!spDXRenderer::ApplyMaterialRenderStateForAnalysis(entry,index,0xffffffff,&MappingSink::Dispatch,&sink),"unsafe native lookup index guarded");
            for(const char* mode:{"ops","address","border","filter","uv-index","uv-flags","hardware","hardware-flags","failed"})TextureMap(mode);
            TextureSink textures;auto& c=textures.cache[0];
            Check(!spDXRenderer::ApplyTextureStateForAnalysis(c,8,1,0,false,TextureSink::Submit,&textures),"stage bound host guard");
            Check(!spDXRenderer::ApplyTextureStateForAnalysis(c,0,9,0,false,TextureSink::Submit,&textures),"raw state index bound host guard");
            Check(!spDXRenderer::ApplyTextureStateForAnalysis(c,0,1,16,false,TextureSink::Submit,&textures)&&c.raw[1]==16,"native unsafe op table guarded after raw cache store");
            for(const char* mode:{"modes","failed","power-zero","power-negative","power-nan","power-inf","sources"})LightingMap(mode);
            std::cout << "PASS " << checks << '/' << checks << ": DX state cache\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
