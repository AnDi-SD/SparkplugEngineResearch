#include "Code/Sparkplug/spMaterialColorController.h"
#include "Code/Sparkplug/spAnimationManager.h"
#include "Code/Sparkplug/spMatColorControllerSerializer.h"
#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
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
    template<class T>void Add(Bytes& bytes,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(T));}
    void Field(Bytes& bytes,std::uint8_t id,std::uint32_t raw){bytes.push_back(static_cast<std::uint8_t>(0xa0+id));bytes.push_back(4);Add(bytes,raw);}
    void Field(Bytes& bytes,std::uint8_t id,const Bytes& payload){Check(payload.size()<256,"bounded field");bytes.push_back(static_cast<std::uint8_t>(0xa0+id));bytes.push_back(static_cast<std::uint8_t>(payload.size()));bytes.insert(bytes.end(),payload.begin(),payload.end());}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    void Open(spMemoryStream& stream,const Bytes& bytes={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    std::string Hex(const Bytes& bytes){constexpr char d[]="0123456789abcdef";std::string result;for(auto b:bytes){result+=d[b>>4];result+=d[b&15];}return result;}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& value:values){if(!first)out<<',';first=false;out<<value;}out<<']';}
    spMaterialColorController::Colors Colors(const spMaterial& m){return {m.GetAmbientColorForAnalysis(),m.GetDiffuseColorForAnalysis(),m.GetSpecularColorForAnalysis(),m.GetEmissiveColorForAnalysis()};}
    void PrintColors(std::ostream& out,const spMaterialColorController::Colors& colors){out<<'[';bool first=true;for(const auto& color:colors){if(!first)out<<',';first=false;Array(out,color);}out<<']';}
    void Mutate(spMaterial& m,float start)
    {
        const auto next=[&](float base){return spMaterial::ColorRGBA{start+base,start+base+1,start+base+2,start+base+3};};
        m.SetDiffuseColorForAnalysis(next(0));m.SetAmbientColorForAnalysis(next(4));m.SetSpecularColorForAnalysis(next(8));m.SetEmissiveColorForAnalysis(next(12));m.SetSpecularPowerForAnalysis(start+16);
    }
    std::string Run(const std::string& mode)
    {
        spFunctionEval::SharedRandomForAnalysis().Seed(5489);spMaterialColorController controller;spMaterialData material;controller.BindMaterialForAnalysis(&material);
        Check(controller.GetMaterialForAnalysis()==&material,"borrowed material");Check(controller.GetSavedColorsForAnalysis()==Colors(material),"binder snapshots four colors");
        Bytes input,output;std::ostringstream states;states<<std::setprecision(17)<<'[';
        if(mode=="bind")
        {
            const auto saved=Colors(material);Mutate(material,0);controller.BindMaterialForAnalysis(&material);const auto refreshed=Colors(material);
            Check(controller.GetSavedColorsForAnalysis()==refreshed,"alias snapshots without restoring");Mutate(material,20);spMaterialData other;controller.BindMaterialForAnalysis(&other);
            Check(Colors(material)==refreshed,"rebind restores old four colors");PrintColors(states,saved);states<<',';PrintColors(states,refreshed);states<<',';PrintColors(states,Colors(material));states<<',';PrintColors(states,Colors(other));
            controller.BindMaterialForAnalysis(nullptr);Check(controller.GetMaterialForAnalysis()==nullptr,"null detaches");
        }
        else if(mode.rfind("codec",0)==0)
        {
            Bytes nested;for(unsigned i=0;i<4;++i)
            {if(mode=="codec-values"){Field(nested,0,0x80402010+i);Field(nested,1,0xff112244+i);Field(nested,2,7);Field(nested,6,Bits(i*.25F-.5F));}nested.push_back(0);}
            if(mode=="codec-values"){Field(nested,0,7);Field(nested,4,Bits(.25F));}nested.push_back(0);input={0xa0,static_cast<std::uint8_t>(nested.size())};input.insert(input.end(),nested.begin(),nested.end());input.push_back(0);
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMatColorControllerSerializer codec;spMemoryStream source;Open(source,input);std::string error;
            Check(codec.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),controller,&error),error.c_str());spMemoryStream destination;Open(destination);Check(codec.WritePayloadForAnalysis(destination,controller,&error),error.c_str());
            std::uint32_t size=0;Check(destination.GetSize(&size),"writer size");const auto* p=static_cast<const std::uint8_t*>(destination.GetBuffer());output.assign(p,p+size);
        }
        else
        {
            for(unsigned i=0;i<4;++i)
            {
                auto& color=controller.GetColorsForAnalysis()[i];auto s=color.GetFunctionForAnalysis().GetStateForAnalysis();
                if(mode!="default"){color.SetColorsForAnalysis(0x80402010+i,0xff112244+i);s.yOffset=i*.25F-.5F;}
                if(mode=="all"||mode=="random"||(mode=="diffuse"&&i==1))s.functionType=mode=="random"?6:7;color.GetFunctionForAnalysis().SetStateForAnalysis(s);
            }
            auto s=controller.GetAlphaForAnalysis().GetStateForAnalysis();
            if(mode=="all"||mode=="random"||mode=="alpha-negative"||mode=="alpha-high"){s.functionType=mode=="random"?6:7;s.yOffset=mode=="alpha-negative"?-.5F:mode=="alpha-high"?2.F:.25F;}
            if(mode=="type0-ignored")s.yOffset=.25F;controller.GetAlphaForAnalysis().SetStateForAnalysis(s);
            bool first=true;for(float delta:{.25F,.5F,-.25F})
            {
                controller.ApplyForAnalysis(delta);Check(controller.UpdateForRenderForAnalysis(),"finite material color update");
                Check(controller.GetAppliedTimeForAnalysis()==controller.GetAccumulatedTimeForAnalysis(),"render clock consumed");
                if(!first)states<<',';first=false;states<<'['<<delta<<',';PrintColors(states,Colors(material));states<<",[";
                for(const auto& color:controller.GetColorsForAnalysis())states<<color.GetFunctionForAnalysis().GetStateForAnalysis().time<<',';
                states<<controller.GetAlphaForAnalysis().GetStateForAnalysis().time<<"]]";
            }
        }
        states<<']';std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<states.str()<<",\""<<Hex(output)<<"\"]";return row.str();
    }
    void Configure(spMaterialColorController& controller,const std::string& mode)
    {
        for(unsigned i=0;i<4;++i)
        {auto& c=controller.GetColorsForAnalysis()[i];c.SetColorsForAnalysis(0x80402010+i,0xff112244+i);auto s=c.GetFunctionForAnalysis().GetStateForAnalysis();s.functionType=mode=="random"?6:7;s.yOffset=i*.25F-.5F;c.GetFunctionForAnalysis().SetStateForAnalysis(s);}
        auto s=controller.GetAlphaForAnalysis().GetStateForAnalysis();s.functionType=mode=="random"?6:mode=="linear"?8:7;s.yOffset=mode=="linear"?0.F:.25F;s.pitch=1;controller.GetAlphaForAnalysis().SetStateForAnalysis(s);
    }
    std::string Frame(const std::string& mode)
    {
        spFunctionEval::SharedRandomForAnalysis().Seed(5489);auto controller=std::make_shared<spMaterialColorController>();Configure(*controller,mode);
        std::vector<std::unique_ptr<spDXMaterial>> materials;materials.push_back(std::make_unique<spDXMaterial>());
        if(mode!="none")materials.back()->SetMaterialColorControllerForAnalysis(controller);
        if(mode=="shared"){materials.push_back(std::make_unique<spDXMaterial>());materials.back()->SetMaterialColorControllerForAnalysis(controller);}
        struct Event{unsigned frame;float delta;bool force;};const Event events[]{{0,.25F,false},{0,.5F,false},{0,0,true},{1,.25F,false},{1,0,true},{2,0,false}};
        std::ostringstream out;out<<std::setprecision(17)<<std::boolalpha<<"[\""<<mode<<"\",[";bool first=true;
        for(const auto& e:events)
        {
            controller->ApplyForAnalysis(e.delta);std::vector<bool> calls;std::vector<unsigned> stamps;
            for(const auto& m:materials){bool called=false;Check(m->UpdateColorForFrameForAnalysis(e.frame,e.force,&called),"DX frame color gate");calls.push_back(called);stamps.push_back(m->GetOpaqueRuntimeFieldForAnalysis());}
            if(!first)out<<',';first=false;out<<'['<<e.frame<<','<<e.delta<<','<<unsigned(e.force)<<',';Array(out,calls);out<<',';Array(out,stamps);out<<",[";
            for(std::size_t i=0;i<materials.size();++i){if(i)out<<',';PrintColors(out,Colors(*materials[i]));}
            out<<"],["<<controller->GetAppliedTimeForAnalysis()<<','<<controller->GetAccumulatedTimeForAnalysis()<<"]]";
        }
        out<<"]]";return out.str();
    }
    std::string Links(const std::string& mode)
    {
        Bytes ref;Add(ref,7u);Add(ref,0u);Bytes input;Field(input,6,ref);
        if(mode=="repeat")Field(input,6,ref);if(mode=="null-after")Field(input,6,0u);input.push_back(0);
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spMaterialColorController::ClassID);Add(directory,0u);Add(directory,54u);
        spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spMaterialData::ClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"material registry");
        Check(manager.RegisterForAnalysis(spMaterialColorController::ClassID,std::make_shared<spMatColorControllerSerializer>(),0xff,3),"color registry");manager.SetDispatchContextForAnalysis(2,1);
        auto* fat=manager.GetFATForAnalysis();spMemoryStream index;Open(index,directory);Check(fat->LoadIndexForAnalysis(index),"color directory");
        spSerializerReadContextForAnalysis context(manager,resources);auto controller=std::make_shared<spMaterialColorController>();Configure(*controller,"constant");
        // Links fixture alpha has native default pitch0, unlike frame fixture.
        auto alpha=controller->GetAlphaForAnalysis().GetStateForAnalysis();alpha.pitch=0;controller->GetAlphaForAnalysis().SetStateForAnalysis(alpha);
        fat->FindByIDForAnalysis(7)->object=controller.get();context.externalOwners.push_back(controller);
        std::vector<std::unique_ptr<spMaterialData>> materials;spMaterialDataSerializer serializer;std::string error;
        for(unsigned i=0;i<(mode=="shared"?2u:1u);++i)
        {materials.push_back(std::make_unique<spMaterialData>());spMemoryStream source;Open(source,input);Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),*materials.back(),&error),error.c_str());Check(materials.back()->GetMaterialColorControllerForAnalysis()==controller.get(),"canonical prebound color edge");}
        Check(controller->GetMaterialForAnalysis()==materials.back().get(),"last-bound canonical material");controller->ApplyForAnalysis(1);Check(controller->UpdateForRenderForAnalysis(),"prebound graph runtime");
        std::ostringstream states;states<<std::setprecision(17)<<'[';for(std::size_t i=0;i<materials.size();++i){if(i)states<<',';PrintColors(states,Colors(*materials[i]));}states<<']';
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);Check(spSerializer::IndexReferenceForAnalysis(manager,materials.front().get()),"material color recursive index");Check(fat->GetResourceCountForAnalysis()==2,"material and controller index exactly once");
        spMemoryStream destination;Open(destination);Check(serializer.WritePayloadWithContextForAnalysis(manager,destination,*materials.front(),&error),error.c_str());std::uint32_t size=0;Check(destination.GetSize(&size),"graph bytes");const auto* p=static_cast<const std::uint8_t*>(destination.GetBuffer());
        std::ostringstream out;out<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<states.str()<<",\""<<Hex(Bytes(p,p+size))<<"\"]";return out.str();
    }
    std::string Corpus(const std::string& hex)
    {
        Check(hex.size()%2==0&&hex.size()<=128,"small raw corpus payload");Bytes input;
        for(std::size_t i=0;i<hex.size();i+=2)input.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));
        spMaterialColorController controller;spMaterialData material;material.SetDiffuseColorForAnalysis({1,1,1,.375F});controller.BindMaterialForAnalysis(&material);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMatColorControllerSerializer codec;spMemoryStream source;Open(source,input);std::string error;
        Check(codec.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),controller,&error),error.c_str());
        std::ostringstream states;states<<std::setprecision(17)<<'[';bool first=true;
        for(float delta:{.25F,.5F,.25F})
        {
            controller.ApplyForAnalysis(delta);Check(controller.UpdateForRenderForAnalysis(),"real wire on explicit type0 input");
            if(!first)states<<',';first=false;states<<'['<<delta<<',';PrintColors(states,Colors(material));states<<",[";
            for(const auto& color:controller.GetColorsForAnalysis())states<<color.GetFunctionForAnalysis().GetStateForAnalysis().time<<',';
            states<<controller.GetAlphaForAnalysis().GetStateForAnalysis().time<<"],["<<controller.GetAppliedTimeForAnalysis()<<','<<controller.GetAccumulatedTimeForAnalysis()<<"]]";
        }
        states<<']';spMemoryStream destination;Open(destination);Check(codec.WritePayloadForAnalysis(destination,controller,&error),error.c_str());std::uint32_t size=0;Check(destination.GetSize(&size),"corpus write size");const auto* p=static_cast<const std::uint8_t*>(destination.GetBuffer());Bytes output(p,p+size);Check(output==input,"exact real payload roundtrip");
        return "[\"corpus\",\""+Hex(input)+"\","+states.str()+",\""+Hex(output)+"\"]";
    }
    void FactoryDefaults()
    {
        // Actual 41A580 -> 437550, run2: 480B, four embedded ColorFunc and five
        // FunctionEval objects. 437610(flag1) destroys/unlinks and frees once.
        spAnimationManager animations;
        (void)spMaterialColorController::StaticRTTI();
        auto owner=spRTTIManager::Instance().Create(spMaterialColorController::ClassID);
        auto* controller=dynamic_cast<spMaterialColorController*>(owner.get());
        Check(controller&&controller->vfunc_18().classID==spMaterialColorController::ClassID,"actual PC material color factory creates the registered class");
        Check(animations.GetControllerCountForAnalysis()==1,"factory registers one controller; embedded leaves do not register");
        Check(controller->IsEnabledForAnalysis()&&controller->GetAccumulatedTimeForAnalysis()==0&&controller->GetAppliedTimeForAnalysis()==0,"actual base enabled and render clocks default");
        const spMaterial::ColorRGBA black{0,0,0,1};
        Check(!controller->GetMaterialForAnalysis()&&controller->GetSavedColorsForAnalysis()==spMaterialColorController::Colors{black,black,black,black},"actual null material and four saved opaque black colors");
        const auto functionDefaults=[](const spFunctionEval& value)
        {
            const auto& s=value.GetStateForAnalysis();
            return s.time==0&&s.frequency==1&&s.reciprocal==1&&s.amplitude==1&&s.xOffset==0&&s.yOffset==0
                &&s.pitch==0&&s.clampLimit==0&&!s.clampEnabled&&s.functionType==0;
        };
        bool colorsMatch=true;
        for(const auto& color:controller->GetColorsForAnalysis())
            colorsMatch=colorsMatch&&color.GetColor1ForAnalysis()==0xFF000000&&color.GetColor2ForAnalysis()==0xFF000000&&functionDefaults(color.GetFunctionForAnalysis());
        Check(colorsMatch&&functionDefaults(controller->GetAlphaForAnalysis()),"actual embedded PC endpoints and all five evaluator defaults");
        // Original absence of field0 leaves the actual constructor defaults;
        // the shared codec must now accept an object made by the RTTI factory.
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spMemoryStream stream;Open(stream,Bytes{0});spMatColorControllerSerializer codec;std::string error;
        Check(codec.ReadPayloadForAnalysis(context,stream,1,*controller,&error),error.c_str());
        Check(controller->GetSavedColorsForAnalysis()==spMaterialColorController::Colors{black,black,black,black}
            &&functionDefaults(controller->GetAlphaForAnalysis()),"empty original section preserves constructor state");
        Check(!controller->Clone(),"factory support does not invent the separate clone operation");
        owner.reset();Check(animations.GetControllerCountForAnalysis()==0,"factory owner destruction unregisters exactly its controller");
    }
    void Guards()
    {
        spMaterialColorController controller;Check(!controller.UpdateForRenderForAnalysis(),"unbound update rejected without dereference");
        spMaterialData material;controller.BindMaterialForAnalysis(&material);controller.ApplyForAnalysis(std::numeric_limits<float>::infinity());Check(!controller.UpdateForRenderForAnalysis(),"nonfinite render clock rejected");
        for(const Bytes bytes:{Bytes{0xa0,1,0,0},Bytes{0xa0,5,0,0,0,0,0,0,0},Bytes{0xa0,5,0,0,0,0},Bytes{0xa0,6,0,0,0,0,0,0,0}})
        {spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream stream;Open(stream,bytes);spMatColorControllerSerializer codec;std::string error;Check(!codec.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),controller,&error)&&context.failed,"packed five-section bounds enforced");}
        auto shared=std::make_shared<spMaterialColorController>();
        {spMaterialData owner;owner.SetMaterialColorControllerForAnalysis(shared);Check(shared->GetMaterialForAnalysis()==&owner,"canonical setter binds");Mutate(owner,0);owner.SetMaterialColorControllerForAnalysis(shared);Check(shared->GetSavedColorsForAnalysis()==Colors(owner),"canonical alias rebind");Check(shared->Clone()==nullptr,"unknown controller clone factory refused");const auto clone=owner.Clone();Check(clone&&dynamic_cast<spMaterialData*>(clone.get())->GetMaterialColorControllerForAnalysis()==nullptr,"CPU MaterialData preserves its native blank-clone rule");}
        Check(shared->GetMaterialForAnalysis()==nullptr,"host holder destructor removes stale backlink");
        spMaterialData replacement;replacement.SetMaterialColorControllerForAnalysis(shared);replacement.SetMaterialColorControllerForAnalysis(nullptr);Check(shared->GetMaterialForAnalysis()==nullptr,"host NULL release detaches retained canonical controller without restore");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--material-color"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--frame"){std::cout<<Frame(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--links"){std::cout<<Links(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--corpus"){std::cout<<Corpus(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"default","type0-ignored","all","diffuse","alpha-negative","alpha-high","random","bind","codec-default","codec-values"})(void)Run(mode);
        for(const auto* mode:{"none","constant","linear","random","shared"})(void)Frame(mode);
        for(const auto* mode:{"prebound","repeat","null-after","shared"})(void)Links(mode);
        (void)Corpus("e02800000063cdcccc3d0063cdcccc3d0063cdcccc3d0063cdcccc3d0061cdcccc3d6200000000640000803f0000");
        FactoryDefaults();Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC material color factory/consumers/common codec\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
