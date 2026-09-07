#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spMaterialColorController.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::array<std::uint32_t,17> Words(const spDXRenderer::LightingStateForAnalysis& s)
    {
        std::array<std::uint32_t,17> result{};unsigned i=0;
        for(const auto* c:{&s.diffuse,&s.ambient,&s.specular,&s.emissive})for(float v:*c)result[i++]=Bits(v);
        result[16]=Bits(s.specularPower);return result;
    }
    std::array<std::uint32_t,17> Words(const spDXMaterial& m)
    {
        spDXRenderer::LightingStateForAnalysis s;s.diffuse=m.GetDiffuseColorForAnalysis();s.ambient=m.GetAmbientColorForAnalysis();
        s.specular=m.GetSpecularColorForAnalysis();s.emissive=m.GetEmissiveColorForAnalysis();s.specularPower=m.GetSpecularPowerForAnalysis();return Words(s);
    }
    std::string Install(const std::string& mode)
    {
        auto controller=std::make_shared<spMaterialColorController>();
        std::vector<std::unique_ptr<spDXMaterial>> materials;materials.push_back(std::make_unique<spDXMaterial>());materials[0]->SetSpecularPowerForAnalysis(8);
        if(mode!="install-none")materials[0]->SetMaterialColorControllerForAnalysis(controller);
        if(mode=="install-shared"){materials.push_back(std::make_unique<spDXMaterial>());materials[1]->SetSpecularPowerForAnalysis(16);materials[1]->SetMaterialColorControllerForAnalysis(controller);}
        for(unsigned i=0;i<4;++i){auto& c=controller->GetColorsForAnalysis()[i];c.SetColorsForAnalysis(0x80402010+i,0xff112244+i);auto f=c.GetFunctionForAnalysis().GetStateForAnalysis();f.functionType=7;f.yOffset=i*.25F-.5F;c.GetFunctionForAnalysis().SetStateForAnalysis(f);}
        auto alpha=controller->GetAlphaForAnalysis().GetStateForAnalysis();alpha.functionType=8;alpha.yOffset=0;alpha.pitch=1;controller->GetAlphaForAnalysis().SetStateForAnalysis(alpha);
        spDXRenderer::LightingStateForAnalysis state;spDXMaterial* owner=nullptr;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;
        for(auto event:std::array<std::pair<unsigned,float>,6>{{{0,.25F},{0,.5F},{1,.25F},{1,0.F},{2,.5F},{3,0.F}}})
        {
            controller->ApplyForAnalysis(event.second);
            for(unsigned index=0;index<materials.size();++index)
            {
                auto& material=*materials[index];const auto before=Words(material);bool evaluated=false;
                const bool result=spDXRenderer::InstallMaterialForAnalysis(state,owner,&material,event.first,&evaluated);
                Check(result&&owner==&material&&Words(state)==before,"material installed before frame evaluation");
                if(!first)out<<',';first=false;out<<'['<<event.first<<','<<event.second<<','<<index<<','<<result<<','<<(evaluated?"true":"false")<<',';
                Array(out,before);out<<',';Array(out,Words(state));out<<",[";
                for(unsigned i=0;i<materials.size();++i){if(i)out<<',';Array(out,Words(*materials[i]));}
                out<<"],[";for(unsigned i=0;i<materials.size();++i){if(i)out<<',';out<<materials[i]->GetOpaqueRuntimeFieldForAnalysis();}
                out<<"],["<<Bits(controller->GetAppliedTimeForAnalysis())<<','<<Bits(controller->GetAccumulatedTimeForAnalysis())<<"]]";
            }
        }
        out<<"]]";return out.str();
    }
    struct Sink
    {
        spDXRenderer::MaterialStateWordsForAnalysis raw{};spDXRenderer::LightingStateForAnalysis lighting;
        std::array<std::uint32_t,256> device{};bool fail=false;
        struct Event{unsigned index,value;spDXRenderer::MaterialStateWordsForAnalysis raw;std::array<std::uint32_t,17> colors;};
        std::array<Event,32> events{};unsigned count=0;
        static std::int32_t Device(void* ptr,unsigned index,unsigned value) noexcept
        {auto& s=*static_cast<Sink*>(ptr);if(s.count<32)s.events[s.count++]={index,value,s.raw,Words(s.lighting)};return s.fail?-1:0;}
        static std::int32_t Dispatch(void* ptr,unsigned index,unsigned value) noexcept
        {auto& s=*static_cast<Sink*>(ptr);return spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(s.device[index],index,value,Device,ptr)?0:-1;}
    };
    std::string Batch(const std::string& mode)
    {
        spDXMaterial material;Sink sink;sink.fail=mode=="batch-failed";sink.raw.fill(0xcccccccc);sink.device.fill(0xa5a5a5a5);
        unsigned i=1;for(auto* c:{&sink.lighting.diffuse,&sink.lighting.ambient,&sink.lighting.specular,&sink.lighting.emissive})for(float& v:*c)v=float(i++)/16;
        sink.lighting.specularPower=8;sink.lighting.packedColorC194=0x80402010;sink.lighting.diffuseSource=77;sink.lighting.ambientSource=88;
        spDXRenderer::MaterialStateWordsForAnalysis alternate{0,1,0,0,0,0,7,6,5,17,1};
        const spDXRenderer::MaterialStateWordsForAnalysis* sources[]{nullptr,&alternate};
        spDXRenderer::MaterialStateOverridesForAnalysis overrides;overrides.sources=sources;overrides.count=2;
        for(unsigned index=1;index<11;++index)overrides.selectors[index-1]=index%2;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned iteration=0;iteration<4;++iteration)
        {
            if(iteration==2)Check(material.SetRenderStateForAnalysis(8,6),"change lighting input");
            if(iteration==3){Check(material.SetRenderStateForAnalysis(1,1),"change shading input");alternate[8]=2;}
            sink.count=0;const bool result=spDXRenderer::ApplyMaterialStateSetForAnalysis(sink.raw,material.GetRenderStatesForAnalysis(),mode=="batch-override"?&overrides:nullptr,sink.lighting,Sink::Dispatch,&sink);
            Check(result,"native material state batch");if(iteration)out<<',';
            out<<'['<<iteration<<','<<result<<',';Array(out,sink.raw);out<<',';Array(out,Words(sink.lighting));out<<','<<sink.lighting.diffuseSource<<','<<sink.lighting.ambientSource<<",[";
            for(unsigned j=0;j<sink.count;++j){if(j)out<<',';const auto& e=sink.events[j];out<<"[\"render\","<<e.index<<','<<e.value<<',';Array(out,e.raw);out<<',';Array(out,e.colors);out<<']';}out<<"]]";
        }
        out<<"]]";return out.str();
    }
    struct PassSink
    {
        std::array<spDXRenderer::TextureMatrix4ForAnalysis,2> cache{};
        std::vector<std::pair<unsigned,spDXRenderer::TextureMatrix4ForAnalysis>> events;bool fail=false;
        static std::int32_t Device(void* ptr,unsigned stage,const spDXRenderer::TextureMatrix4ForAnalysis& matrix) noexcept
        {auto& s=*static_cast<PassSink*>(ptr);s.events.emplace_back(stage,matrix);return s.fail?-1:0;}
        static bool UV(void* ptr,unsigned stage,const std::array<float,9>& matrix)
        {auto& s=*static_cast<PassSink*>(ptr);if(stage>=2)throw std::runtime_error("bounded two-stage UV input");return spDXRenderer::ApplyTextureTransform3ForAnalysis(s.cache[stage],stage,matrix,Device,ptr);}
    };
    std::string PassUpdate(const std::string& mode)
    {
        spMaterialPassLayer pass;PassSink sink;sink.events.reserve(8);sink.fail=mode=="failed";
        const unsigned count=mode=="empty"?0:mode=="eight-no-uv"?8:2;
        for(unsigned index=0;index<count;++index)
        {
            auto layer=std::make_unique<spStdLayer>();
            if(mode!="eight-no-uv"){std::array<float,9> matrix{};for(unsigned v=0;v<9;++v)matrix[v]=float(v+1+index*10);layer->GetMaterialTextureForAnalysis()->SetStaticUVTransformForAnalysis(matrix);}
            Check(pass.SetLayerForAnalysis(index,std::move(layer)),"native pass layers");
        }
        const unsigned stage=mode=="two-stage0"?0:mode=="two-stage1"?1:0xffffffff;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned repeat=0;repeat<2;++repeat)
        {
            sink.events.clear();Check(pass.UpdateForRenderForAnalysis(stage,PassSink::UV,&sink),"original pass texture update");
            if(repeat)out<<',';out<<'['<<repeat<<",[";
            for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<'['<<sink.events[i].first<<',';Array(out,sink.events[i].second);out<<']';}out<<"]]";
        }
        out<<"]]";return out.str();
    }
    std::string Run(const std::string& mode){return mode.rfind("install-",0)==0?Install(mode):Batch(mode);}
    struct BindingSink
    {
        std::array<std::uintptr_t,8> identities{};std::uint32_t palette=0xffffffff;bool fail=false;
        struct Event{bool isPalette;unsigned stage;std::uintptr_t value;std::array<std::uintptr_t,8> identities;unsigned palette;};
        std::array<Event,4> events{};unsigned count=0;
        static std::int32_t Bind(void* ptr,unsigned stage,std::uintptr_t handle) noexcept
        {auto& s=*static_cast<BindingSink*>(ptr);if(s.count<4)s.events[s.count++]={false,stage,handle,s.identities,s.palette};return s.fail?-1:0;}
        static std::int32_t Palette(void* ptr,unsigned index) noexcept
        {auto& s=*static_cast<BindingSink*>(ptr);if(s.count<4)s.events[s.count++]={true,0,index,s.identities,s.palette};return s.fail?-1:0;}
        static void Snapshot(std::ostream& out,const std::array<std::uintptr_t,8>& cache,unsigned palette)
        {out<<"[["<<cache[0]<<','<<cache[1]<<','<<cache[7]<<"],"<<palette<<']';}
    };
    std::string Binding(const std::string& mode)
    {
        BindingSink sink;sink.fail=mode=="failed";
        std::vector<std::pair<unsigned,unsigned>> inputs{{0,0},{0,1},{0,1},{1,1},{7,2},{0,2},{0,0},{0,0},{1,0},{7,0}};
        if(mode=="palette-direct")inputs={{0,1},{0,1},{0,0},{0,2},{0,2},{0,0}};
        std::ostringstream out;out<<"[\""<<mode<<"\",[";unsigned step=0;
        for(auto [stage,ordinal]:inputs)
        {
            sink.count=0;bool result=false;
            if(mode=="palette-direct")result=spDXRenderer::SelectPaletteForAnalysis(sink.palette,ordinal?std::optional<unsigned>(ordinal==1?7:3):std::nullopt,BindingSink::Palette,&sink);
            else
            {
                spDXRenderer::ResolvedTextureBindingForAnalysis input;input.identity=ordinal;input.deviceTexture=ordinal;
                if(ordinal==1&&(mode=="palette"||mode=="failed"))input.paletteIndex=7;
                result=spDXRenderer::BindResolvedTextureForAnalysis(sink.identities[stage],sink.palette,stage,input,mode=="debug"&&step>=2&&step<6,BindingSink::Bind,BindingSink::Palette,&sink);
            }
            Check(result,"resolved texture/palette native wrapper success");if(step)out<<',';
            out<<'['<<step<<','<<stage<<','<<ordinal<<','<<result<<',';BindingSink::Snapshot(out,sink.identities,sink.palette);out<<",[";
            for(unsigned i=0;i<sink.count;++i)
            {
                const auto& e=sink.events[i];if(i)out<<',';out<<"[\""<<(e.isPalette?"palette":"texture")<<"\",";
                if(!e.isPalette)out<<e.stage<<',';out<<e.value<<',';BindingSink::Snapshot(out,e.identities,e.palette);out<<']';
            }
            out<<"]]";++step;
        }
        out<<"]]";return out.str();
    }
    std::array<unsigned,72> Flatten(const spDXRenderer::TextureStateBlockForAnalysis& block)
    {std::array<unsigned,72> result{};unsigned i=0;for(const auto& row:block)for(auto v:row)result[i++]=v;return result;}
    struct PassStateSink
    {
        spDXRenderer::PassTextureStateForAnalysis state;spDXRenderer::MaterialStateWordsForAnalysis materialRaw{};
        std::array<unsigned,256> device{};bool fail=false;
        struct Event{unsigned kind,count;std::array<std::uintptr_t,3> args;std::array<unsigned,72> raw;};
        std::array<Event,128> events{};unsigned count=0;
        std::array<unsigned,72> Raw() const
        {spDXRenderer::TextureStateBlockForAnalysis block{};for(unsigned i=0;i<8;++i)block[i]=state.cache[i].raw;return Flatten(block);}
        std::int32_t Emit(unsigned kind,unsigned argc,std::array<std::uintptr_t,3> args) noexcept
        {if(count<128)events[count++]={kind,argc,args,Raw()};return fail?-1:0;}
        static std::int32_t Texture(void* ptr,unsigned stage,std::uintptr_t handle) noexcept
        {return static_cast<PassStateSink*>(ptr)->Emit(0,2,{stage,handle,0});}
        static std::int32_t State(void* ptr,bool sampler,unsigned stage,unsigned index,unsigned value) noexcept
        {return static_cast<PassStateSink*>(ptr)->Emit(sampler?2:1,3,{stage,index,value});}
        static std::int32_t Render(void* ptr,unsigned index,unsigned value) noexcept
        {return static_cast<PassStateSink*>(ptr)->Emit(3,2,{index,value,0});}
        static std::int32_t Dispatch(void* ptr,unsigned index,unsigned value) noexcept
        {auto& s=*static_cast<PassStateSink*>(ptr);return spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(s.device[index],index,value,Render,ptr)?0:-1;}
    };
    std::string PassStates(const std::string& mode)
    {
        PassStateSink sink;sink.fail=mode=="failed";spDXMaterial material;
        spMaterialPassLayer pass;std::array<spMaterialTexture,2> defaults;
        const std::array<std::array<unsigned,9>,4> presets{{
            {{0,1,2,0,1,0x11223344,1,0,2}},{{0,0,0,2,2,0,0,0,0}},
            {{0,3,4,1,0,0x55667788,2,1,3}},{{0,5,6,0,1,0x12345678,3,9,0x102}}}};
        for(unsigned d=0;d<2;++d)for(unsigned i=0;i<9;++i)defaults[d].SetTextureStateForAnalysis(i,presets[d][i]);
        const unsigned count=mode=="empty"?0:mode=="one"?1:2;
        for(unsigned index=0;index<count;++index)
        {auto layer=std::make_unique<spStdLayer>();for(unsigned i=0;i<9;++i)layer->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(i,presets[index+2][i]);Check(pass.SetLayerForAnalysis(index,std::move(layer)),"pass state layer input");}
        for(unsigned stage=0;stage<8;++stage)
        {
            sink.state.desiredTextures[stage]=sink.state.boundTextures[stage]=0x12340000+stage;
            sink.state.cache[stage].raw.fill(0xcccccccc);sink.state.cache[stage].coordinateIndex=0xeeeeeeee;sink.state.cache[stage].transformFlags=0xdddddddd;
        }
        std::array<spDXRenderer::ResolvedTextureBindingForAnalysis,8> bindings{};
        std::array<spDXRenderer::TextureStateBlockForAnalysis,2> blocks{};spDXRenderer::PassTextureOverridesForAnalysis overrides;overrides.sources=blocks.data();overrides.count=2;
        for(unsigned stage=0;stage<8;++stage)for(unsigned index=0;index<9;++index){blocks[1][stage][index]=presets[2][index];overrides.selectors[stage][index]=index%2;}
        spDXRenderer::MaterialStateWordsForAnalysis alternate{};alternate[7]=6;
        const spDXRenderer::MaterialStateWordsForAnalysis* sources[]{nullptr,&alternate};spDXRenderer::MaterialStateOverridesForAnalysis blendOverrides;blendOverrides.sources=sources;blendOverrides.count=2;blendOverrides.selectors[6]=1;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned iteration=0;iteration<3;++iteration)
        {
            sink.count=0;if(iteration==2&&count)pass.GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(3,2);
            if(mode.rfind("blend",0)==0)pass.SetFinalBlendOperationForAnalysis(std::array<unsigned,3>{0,3,6}[iteration]);
            if(mode=="blend-override"&&iteration==2)alternate[7]=3;
            bool result=spDXRenderer::ApplyPassTextureStatesForAnalysis(sink.state,pass,{&defaults[0],&defaults[1]},bindings,mode=="override"?&overrides:nullptr,mode=="shader-override",false,PassStateSink::Texture,nullptr,PassStateSink::State,&sink);
            if(result&&mode.rfind("blend",0)==0)result=spDXRenderer::ApplyPassBlendForAnalysis(sink.materialRaw,material,pass,mode=="blend-override"?&blendOverrides:nullptr,PassStateSink::Dispatch,&sink);
            Check(result,"actual pass state chain");if(iteration)out<<',';out<<'['<<iteration<<','<<result<<',';Array(out,Flatten(sink.state.desired));out<<',';Array(out,sink.Raw());
            for(unsigned field=0;field<2;++field){out<<",[";for(unsigned s=0;s<8;++s){if(s)out<<',';out<<(field?sink.state.cache[s].transformFlags:sink.state.cache[s].coordinateIndex);}out<<']';}
            out<<",[";for(unsigned s=0;s<8;++s){if(s)out<<',';out<<unsigned(sink.state.dirty[s]);}out<<"],"<<material.GetRenderStateForAnalysis(7)<<','<<sink.materialRaw[7]<<",[";
            constexpr const char* names[]{"texture","stage","sampler","render"};
            for(unsigned i=0;i<sink.count;++i){if(i)out<<',';const auto& e=sink.events[i];out<<"[\""<<names[e.kind]<<'"';for(unsigned a=0;a<e.count;++a)out<<','<<e.args[a];out<<',';Array(out,e.raw);out<<']';}out<<"]]";
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--pass")std::cout<<PassUpdate(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--binding")std::cout<<Binding(argv[2])<<'\n';
        else if(argc==3&&std::string(argv[1])=="--pass-states")std::cout<<PassStates(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"install-none","install-color","install-shared","batch-default","batch-override","batch-failed"})Run(mode);
            spDXMaterial material;spDXMaterial* owner=nullptr;spDXRenderer::LightingStateForAnalysis state;
            Check(!spDXRenderer::InstallMaterialForAnalysis(state,owner,&material,0)&&!owner,"uninitialized power is not a native default");
            Check(!spDXRenderer::InstallMaterialForAnalysis(state,owner,nullptr,0),"NULL material host guard");
            spDXRenderer::MaterialStateWordsForAnalysis raw{};spDXRenderer::MaterialStateOverridesForAnalysis bad;bad.selectors[0]=1;
            Check(!spDXRenderer::ApplyMaterialStateSetForAnalysis(raw,material.GetRenderStatesForAnalysis(),&bad,state,nullptr,nullptr)&&raw[8]==255,"unsafe override slot host guard after lighting invalidation");
            for(const char* mode:{"empty","two-auto","two-stage0","two-stage1","eight-no-uv","failed"})PassUpdate(mode);
            spMaterialPassLayer sparse;Check(sparse.SetLayerForAnalysis(1,std::make_unique<spStdLayer>()),"sparse count");
            Check(!sparse.UpdateForRenderForAnalysis(0xffffffff,PassSink::UV,nullptr),"NULL occupied layer host guard");
            spMaterialTextureLayer bare;Check(!bare.UpdateForRenderForAnalysis(0,PassSink::UV,nullptr),"NULL material texture host guard");
            for(const char* mode:{"plain","palette","failed","debug","palette-direct"})Binding(mode);
            unsigned palette=0;Check(!spDXRenderer::SelectPaletteForAnalysis(palette,7,nullptr,nullptr)&&palette==0,"NULL palette device callback host guard");
            for(const char* mode:{"empty","one","two","override","shader-override","failed","blend","blend-override"})PassStates(mode);
            std::cout<<"PASS "<<checks<<'/'<<checks<<": material installation and state application\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
