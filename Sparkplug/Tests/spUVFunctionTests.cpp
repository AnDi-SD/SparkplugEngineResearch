#include "Code/Sparkplug/spUVController.h"
#include "Code/Sparkplug/spUVControllerSerializer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/Sparkplug/spAnimationManager.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/Sparkplug/spAnimTexController.h"
#include <algorithm>
#include <optional>
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spUVFunctionAbi.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& out,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(T));}
    void Field(Bytes& out,std::uint8_t id,const Bytes& bytes){Check(bytes.size()<256&&!bytes.empty(),"tiny field");out.push_back(static_cast<std::uint8_t>(0xa0+id));out.push_back(static_cast<std::uint8_t>(bytes.size()));out.insert(out.end(),bytes.begin(),bytes.end());}
    template<class T>void Field(Bytes& out,std::uint8_t id,const T& value){Bytes data;Add(data,value);Field(out,id,data);}
    void Open(spMemoryStream& stream,const Bytes& data={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(data.size())),"capacity");if(!data.empty())std::memcpy(stream.GetBuffer(),data.data(),data.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    std::string Hex(const Bytes& bytes){std::string out;constexpr char digits[]="0123456789abcdef";for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(auto v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::string Run(const std::string& mode)
    {
        const auto split=mode.find('-');const auto kind=mode.substr(0,split),which=mode.substr(split+1);
        spFunctionEval::SharedRandomForAnalysis().Seed(5489);
        spAnimationManager animationManager;auto uv=std::make_shared<spUVController>();auto& trans=uv->GetTransformForAnalysis();
        std::array<float,7> values{0,0,0,1,1,1,0};spTransformEval::Vector3 pivot{},axis{0,0,1};
        if(which=="translate"){values[0]=2;values[1]=3;values[2]=4;}
        if(which=="scale"){values[3]=2;values[4]=3;values[5]=4;}
        if(which=="rotation"||which=="pivot"||which=="nonunit"||which=="zeroaxis")values[6]=.25F;
        if(which=="pivot"||which=="compound")pivot={.5F,1,2};
        if(which=="compound")values={2,3,4,2,3,4,.125F};
        if(which=="nonunit")axis={1,2,3};if(which=="zeroaxis")axis={0,0,0};
        for(std::size_t i=0;i<values.size();++i)
        {
            auto state=trans.GetFunctionsForAnalysis()[i].GetStateForAnalysis();state.yOffset=values[i];
            if(which=="animated"||which=="random"){state.functionType=which=="random"?6u:8u;state.pitch=static_cast<float>(i+1)*.5F;}
            trans.GetFunctionsForAnalysis()[i].SetStateForAnalysis(state);
        }
        trans.SetPivotForAnalysis(pivot);trans.SetAxisForAnalysis(axis);spMaterialTexture holder;
        const spUVController::Matrix3 saved=which=="compound"?spUVController::Matrix3{2,.5F,9,.25F,3,8,.5F,.75F,7}:spUVController::Matrix3{1,0,0,0,1,0,0,0,1};
        holder.SetStaticUVTransformForAnalysis(saved);holder.SetOwnedUVControllerForAnalysis(uv);
        std::ostringstream states;states<<std::setprecision(17)<<'[';Bytes input,output;
        if(kind=="codec")
        {
            Bytes functions;
            for(std::size_t i=0;i<values.size();++i)
            {
                if(which=="animated"||which=="random")Field(functions,0,which=="random"?6u:8u);
                if(values[i])Field(functions,4,values[i]);
                if(which=="animated"||which=="random")Field(functions,5,static_cast<float>(i+1)*.5F);
                functions.push_back(0);
            }
            for(float v:pivot)Add(functions,v);for(float v:axis)Add(functions,v);
            Bytes nested;Field(nested,0,functions);nested.push_back(0);Field(input,0,nested);input.push_back(0);
            spMemoryStream source;Open(source,input);spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spUVControllerSerializer serializer;std::string error;
            Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),*uv,&error),error.c_str());
            spMemoryStream destination;Open(destination);Check(serializer.WritePayloadForAnalysis(destination,*uv,&error),error.c_str());
            std::uint32_t size=0;Check(destination.GetSize(&size),"output size");const auto* data=static_cast<const std::uint8_t*>(destination.GetBuffer());output.assign(data,data+size);
        }
        else
        {
            bool first=true;
            for(float delta:{0.F,.25F,.75F,-.5F})
            {
                std::vector<float> state;
                if(kind=="prs")
                {
                    spTransformEval::SampleForAnalysis sample;Check(trans.EvaluateSampleForAnalysis(delta,sample),"scalar→PRS");
                    state.insert(state.end(),sample.position.begin(),sample.position.end());state.insert(state.end(),sample.rotation.begin(),sample.rotation.end());state.insert(state.end(),sample.scale.begin(),sample.scale.end());
                    state.push_back(sample.hasPosition?1.F:0.F);state.push_back(sample.hasRotation?1.F:0.F);state.push_back(sample.hasScale?1.F:0.F);
                }
                else if(kind=="matrix")
                {spTransFunctionEval::Matrix4 matrix;Check(trans.EvaluateMatrixForAnalysis(delta,matrix),"scalar→matrix");state.assign(matrix.begin(),matrix.end());}
                else
                {uv->ApplyForAnalysis(delta);Check(uv->UpdateForRenderForAnalysis(),"direct UV update");const auto& matrix=holder.GetUVTransformForAnalysis();state.assign(matrix.begin(),matrix.end());}
                std::array<float,7> times{};for(std::size_t i=0;i<7;++i)times[i]=trans.GetFunctionsForAnalysis()[i].GetStateForAnalysis().time;
                if(!first)states<<',';first=false;states<<'['<<delta<<',';Array(states,state);states<<',';Array(states,times);states<<']';
            }
        }
        states<<']';std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<states.str()<<",\""<<Hex(output)<<"\"]";return row.str();
    }
    std::string MaterialGraph(const std::string& mode)
    {
        Bytes functions;
        for(float value:{2.F,3.F,0.F,1.F,1.F,1.F,.25F}){if(value)Field(functions,4,value);functions.push_back(0);}
        Add(functions,std::array<float,6>{.5F,1,2,0,0,1});Bytes trans;Field(trans,0,functions);trans.push_back(0);
        Bytes controllerBytes;Add(controllerBytes,spUVController::ClassID);Add(controllerBytes,0x4f4f4253u);Field(controllerBytes,0,trans);controllerBytes.push_back(0);
        Bytes ref;Add(ref,7u);Add(ref,static_cast<std::uint32_t>(controllerBytes.size()));ref.insert(ref.end(),controllerBytes.begin(),controllerBytes.end());
        Bytes input;Field(input,3,2u);Field(input,4,spStdLayer::ClassID);
        Bytes matrix;Add(matrix,1u);Add(matrix,std::array<float,9>{2,.5F,0,.25F,3,0,.5F,.75F,1});Field(input,9,matrix);Field(input,12,ref);
        if(mode=="repeat"||mode=="two-layers")
        {if(mode=="two-layers")Field(input,4,spStdLayer::ClassID);ref.clear();Add(ref,7u);Add(ref,0u);Field(input,12,ref);}
        if(mode=="null-after")Field(input,12,0u);input.push_back(0);
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spUVController::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(controllerBytes.size()));
        spAnimationManager animations;spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spMaterialData::ClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"material codec registry");
        Check(manager.RegisterForAnalysis(spUVController::ClassID,std::make_shared<spUVControllerSerializer>(),0xff,3),"UV codec registry");manager.SetDispatchContextForAnalysis(2,1);
        auto* fat=manager.GetFATForAnalysis();spMemoryStream index;Open(index,directory);Check(fat->LoadIndexForAnalysis(index),"UV graph directory");
        spSerializerReadContextForAnalysis context(manager,resources);spMaterialData material;spMaterialDataSerializer serializer;spMemoryStream source;Open(source,input);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),material,&error),error.c_str());
        auto* controller=dynamic_cast<spUVController*>(fat->FindByIDForAnalysis(7)->object);Check(controller!=nullptr,"actual UV header factory");
        auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));Check(pass&&pass->GetLayerCountForAnalysis()==(mode=="two-layers"?2u:1u),"expected standard layers");
        for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
        {auto* holder=pass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis().get();Check(holder->GetUVControllerForAnalysis()==controller,"canonical UV identity including NULL preserve");Check(holder->GetTextureStatesForAnalysis()[8]==2,"UV coordinate mode2");}
        controller->ApplyForAnalysis(1);Check(pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis()->UpdateUVAnimationForAnalysis(),"first holder drives last-bound UV");
        std::ostringstream state;state<<std::setprecision(17)<<'[';
        for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i){if(i)state<<',';Array(state,pass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis()->GetUVTransformForAnalysis());}state<<']';
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);Check(spSerializer::IndexReferenceForAnalysis(manager,&material),"material/UV recursive index");
        Check(fat->GetResourceCountForAnalysis()==2,"one material plus one shared UV");spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,material,&error),error.c_str());
        std::uint32_t size=0;Check(output.GetSize(&size),"graph output size");const auto* bytes=static_cast<const std::uint8_t*>(output.GetBuffer());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<state.str()<<",\""<<Hex(Bytes(bytes,bytes+size))<<"\"]";return row.str();
    }
    struct TransformSink
    {
        std::array<spDXRenderer::TextureMatrix4ForAnalysis,2> caches{};
        std::vector<std::pair<std::uint32_t,spDXRenderer::TextureMatrix4ForAnalysis>> events;
        bool fail=false;
        spAnimTexController* animation=nullptr;
        std::optional<float> submittedPhase;
        static std::int32_t Device(void* context,std::uint32_t stage,const spDXRenderer::TextureMatrix4ForAnalysis& matrix) noexcept
        {auto& sink=*static_cast<TransformSink*>(context);sink.events.emplace_back(stage,matrix);if(sink.animation)sink.submittedPhase=sink.animation->GetPlaybackTimeForAnalysis();return sink.fail?static_cast<std::int32_t>(0x80004005u):0;}
        static bool Platform(void* context,std::uint32_t stage,const spDXRenderer::TextureMatrix3ForAnalysis& matrix)
        {auto& sink=*static_cast<TransformSink*>(context);if(stage>=sink.caches.size())return false;return spDXRenderer::ApplyTextureTransform3ForAnalysis(sink.caches[stage],stage,matrix,&Device,&sink);}
    };
    std::string Renderer(const std::string& mode)
    {
        spAnimationManager animations;spMaterialTexture holder;std::unique_ptr<spMaterialTexture> second;std::shared_ptr<spUVController> uv;
        TransformSink sink;sink.fail=mode.find("-fail")!=std::string::npos;
        std::shared_ptr<spAnimTexController> animation;std::ostringstream phases;phases<<std::setprecision(17)<<'[';
        spDXRenderer::TextureMatrix4ForAnalysis matrix{};for(std::size_t i=0;i<16;++i)matrix[i]=static_cast<float>(i+1);
        spDXRenderer::TextureMatrix3ForAnalysis matrix3{};std::copy_n(matrix.begin(),9,matrix3.begin());
        if(mode=="static"||mode=="static-fail")holder.SetStaticUVTransformForAnalysis(matrix3);
        if(mode.rfind("uv-",0)==0)
        {
            uv=std::make_shared<spUVController>();holder.SetOwnedUVControllerForAnalysis(uv);
            auto s=uv->GetTransformForAnalysis().GetFunctionsForAnalysis()[0].GetStateForAnalysis();s.functionType=8;s.pitch=2;uv->GetTransformForAnalysis().GetFunctionsForAnalysis()[0].SetStateForAnalysis(s);
            if(mode=="uv-shared"){second=std::make_unique<spMaterialTexture>();second->SetOwnedUVControllerForAnalysis(uv);}
        }
        if(mode=="uv-both"||mode=="anim-only")
        {animation=std::make_shared<spAnimTexController>();Check(animation->GetTextureTrackForAnalysis().SetKeysForAnalysis({1},{nullptr}),"one finite NULL texture key");holder.SetOwnedAnimTextureControllerForAnalysis(animation);sink.animation=animation.get();}
        std::size_t step=0,updates=0;std::ostringstream states;states<<std::setprecision(17)<<'[';
        for(float delta:{1.F,0.F,.5F})
        {
            if(uv&&mode!="uv-idle")uv->ApplyForAnalysis(delta);
            if(animation)animation->ApplyForAnalysis(delta);sink.submittedPhase.reset();
            if(uv&&uv->GetAppliedTimeForAnalysis()!=uv->GetAccumulatedTimeForAnalysis())++updates;
            if(mode.rfind("matrix4",0)==0)Check(spDXRenderer::ApplyTextureTransform4ForAnalysis(sink.caches[step%2],static_cast<std::uint32_t>(step%2),matrix,&TransformSink::Device,&sink),"PC4x4 boundary");
            else if(mode=="matrix3")Check(TransformSink::Platform(&sink,static_cast<std::uint32_t>(step%2),matrix3),"PC3x3 boundary");
            else Check(holder.UpdateForRenderForAnalysis(static_cast<std::uint32_t>(step%2),&TransformSink::Platform,&sink),"whole material UV/render boundary");
            if(step)states<<',';states<<"[[";Array(states,holder.GetUVTransformForAnalysis());if(second){states<<',';Array(states,second->GetUVTransformForAnalysis());}
            states<<"],[";Array(states,sink.caches[0]);states<<',';Array(states,sink.caches[1]);states<<"]]";
            if(animation){if(step)phases<<',';phases<<'[';if(sink.submittedPhase)phases<<*sink.submittedPhase;else phases<<"null";phases<<','<<animation->GetPlaybackTimeForAnalysis()<<']';}++step;
        }
        states<<']';std::ostringstream row;row<<std::setprecision(17)<<"[\""<<mode<<"\","<<states.str()<<",[";
        bool first=true;for(const auto& [stage,value]:sink.events){if(!first)row<<',';first=false;row<<'['<<stage<<',';Array(row,value);row<<']';}row<<"],"<<updates;if(animation)row<<','<<phases.str()<<']';row<<']';return row.str();
    }
    void Guards()
    {
        spAnimationManager manager;auto uv=std::make_shared<spUVController>();spMaterialTexture first;
        TransformSink sink;spDXRenderer::TextureMatrix4ForAnalysis original{};original.fill(7);auto cached=original;
        Check(!spDXRenderer::ApplyTextureTransform4ForAnalysis(cached,0,{},nullptr,nullptr)&&cached==original,"missing device host guard preserves cache");
        Check(spRenderer::GetPlatformInterfaceSlotForAnalysis(spRendererPlatformForAnalysis::PC,spRendererPlatformOperationForAnalysis::SetUVTransform3x3)==24,"PC3x3 is not4x4 slot23");
        Check(spRenderer::GetPlatformInterfaceSlotForAnalysis(spRendererPlatformForAnalysis::PS2,spRendererPlatformOperationForAnalysis::SetUVTransform3x3)==23,"independently documented PS2 UV slot23");
        Check(!uv->UpdateForRenderForAnalysis(),"unbound UV safe refusal");
        const spUVController::Matrix3 a{2,3,0,4,5,0,6,7,1},b{8,9,0,10,11,0,12,13,1};
        first.SetStaticUVTransformForAnalysis(a);first.SetOwnedUVControllerForAnalysis(uv);
        Check(uv->GetSavedTransformForAnalysis()==a&&first.GetTextureStatesForAnalysis()[8]==2,"native UV bind snapshot and state8");
        first.SetStaticUVTransformForAnalysis(b);first.SetOwnedUVControllerForAnalysis(uv);
        Check(uv->GetSavedTransformForAnalysis()==b,"same-owner alias refreshes baseline");
        {spMaterialTexture second;second.SetStaticUVTransformForAnalysis(a);first.SetStaticUVTransformForAnalysis(a);second.SetOwnedUVControllerForAnalysis(uv);
        Check(first.GetUVTransformForAnalysis()==b&&uv->GetSavedTransformForAnalysis()==a,"rebinding restores former holder and snapshots new");}
        Check(!uv->GetMaterialForAnalysis(),"host detaches dying last-bound holder without touching destroyed storage");
        first.SetTextureStateForAnalysis(8,0xabcdef08);first.SetOwnedUVControllerForAnalysis(uv);first.SetOwnedUVControllerForAnalysis(nullptr);
        Check(first.GetTextureStatesForAnalysis()[8]==0xabcdef08&&!uv->GetMaterialForAnalysis(),"preserve-bit8 and safe clear");
        first.SetTextureStateForAnalysis(8,0);first.SetOwnedUVControllerForAnalysis(uv);
        Check(first.UpdateUVAnimationForAnalysis()&&uv->GetAppliedTimeForAnalysis()==0,"equal clocks skip recompute");
        uv->ApplyForAnalysis(.25F);Check(first.UpdateUVAnimationForAnalysis()&&uv->GetAppliedTimeForAnalysis()==.25F,"changed clock consumes UV elapsed");
        auto s=uv->GetTransformForAnalysis().GetFunctionsForAnalysis()[0].GetStateForAnalysis();s.functionType=3;s.frequency=0;s.reciprocal=std::numeric_limits<float>::infinity();uv->GetTransformForAnalysis().GetFunctionsForAnalysis()[0].SetStateForAnalysis(s);
        uv->ApplyForAnalysis(1);Check(!first.UpdateUVAnimationForAnalysis(),"unsafe scalar dependency refuses UV update");
        for(const Bytes bytes:{Bytes{0xa0,2,0,0,0},Bytes{0xa0,1,0,0},Bytes{0}})
        {
            spMemoryStream source;Open(source,bytes);spSerializerManager serializers;spResourceManager resources;spSerializerReadContextForAnalysis context(serializers,resources);spUVControllerSerializer codec;std::string error;
            const bool result=codec.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(bytes.size()),*uv,&error);
            Check(result==(bytes.size()!=5),"nested exact envelope/empty original section semantics");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--uv"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--material-uv"){std::cout<<MaterialGraph(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--uv-renderer"){std::cout<<Renderer(argv[2])<<'\n';return 0;}
        for(const auto* kind:{"prs","matrix","uv"})for(const auto* which:{"identity","translate","scale","rotation","pivot","compound","nonunit","zeroaxis","animated","random"})(void)Run(std::string(kind)+'-'+which);
        for(const auto* which:{"default","compound","animated","random"})(void)Run(std::string("codec-")+which);
        for(const auto* mode:{"inline","repeat","null-after","two-layers"})(void)MaterialGraph(mode);
        for(const auto* mode:{"none","static","static-fail","uv-idle","uv-active","uv-shared","uv-both","anim-only","matrix3","matrix4","matrix4-fail"})(void)Renderer(mode);
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC UV/TransFunction shared evaluator/codec and binding guards\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
