#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spDXMaterialDataSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugDX/spDXMaterialSerializer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/Sparkplug/spTextureData.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/SparkplugDX/spDXTextureSerializer.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spModelSerializer.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& out,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(value));}
    void Field(Bytes& bytes,std::uint8_t id,const Bytes& data){Check(!data.empty()&&data.size()<256,"small field");bytes.push_back(0xa0+id);bytes.push_back(static_cast<std::uint8_t>(data.size()));bytes.insert(bytes.end(),data.begin(),data.end());}
    template<class T>void Field(Bytes& bytes,std::uint8_t id,const T& value){Bytes data;Add(data,value);Field(bytes,id,data);}
    std::string Hex(const Bytes& bytes){constexpr char d[]="0123456789abcdef";std::string out;for(auto v:bytes){out+=d[v>>4];out+=d[v&15];}return out;}
    void Open(spMemoryStream& stream,const Bytes& data={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(data.size())),"capacity");if(!data.empty())std::memcpy(stream.GetBuffer(),data.data(),data.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    Bytes Data(spMemoryStream& stream){std::uint32_t size=0;Check(stream.GetSize(&size),"size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    Bytes State(const spMaterialData& material)
    {
        Bytes state;Add(state,material.GetRenderStatesForAnalysis());Add(state,material.GetVertexAlphaByteForAnalysis());
        Add(state,material.GetDiffuseColorForAnalysis());Add(state,material.GetAmbientColorForAnalysis());
        Add(state,material.GetSpecularColorForAnalysis());Add(state,material.GetEmissiveColorForAnalysis());Add(state,material.GetSpecularPowerForAnalysis());return state;
    }
    Bytes Input(const std::string& mode)
    {
        Bytes bytes;
        if(mode=="values"||mode=="repeat")
        {
            if(mode=="repeat"){Field(bytes,1,std::uint8_t(1));Field(bytes,18,Bytes{'i','g','n','o','r','e','d'});}
            Field(bytes,0,std::array<std::uint32_t,11>{9,8,7,6,5,4,3,2,1,0,0xffffffff});Field(bytes,1,std::uint8_t(2));
            Field(bytes,2,std::array<std::uint32_t,5>{0x12345678,0xabcdef01,0xff102030,0x87654321,0x40600000});
            if(mode=="repeat")Field(bytes,1,std::uint8_t(0));
        }
        else if(mode=="null-controller")Field(bytes,6,0u);
        else if(mode=="pass-only")Field(bytes,3,2u);
        else Check(mode=="empty","explicit material mode");
        bytes.push_back(0);return bytes;
    }
    std::string Scalar(const std::string& mode)
    {
        const auto bytes=Input(mode);spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);spMaterialData material;spMaterialDataSerializer serializer;
        spMemoryStream input;Open(input,bytes);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),material,&error),error.c_str());
        std::uint32_t position=0;Check(input.GetCurrentPosition(position)&&position==bytes.size()&&!context.failed,"exact bounded material input");
        const auto state=State(material);spMemoryStream output;Open(output);
        Check(serializer.WritePayloadForAnalysis(output,material,&error),error.c_str());Check(state==State(material),"writer preserves material");
        Check(serializer.IndexRelationshipsWithContextForAnalysis(manager,material),"scalar/pass-only index requires no invented resource");
        spMemoryStream dxOutput;Open(dxOutput);Check(spDXMaterialDataSerializer{}.WritePayloadWithContextForAnalysis(manager,dxOutput,material,&error),error.c_str());
        Check(Data(output)==Data(dxOutput),"common and DX marker use one common material codec");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\",1,"<<position<<",\""<<Hex(state)<<"\","<<material.GetPassCountForAnalysis()<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    std::string Layer(const std::string& mode)
    {
        Bytes bytes;Field(bytes,3,2u);Field(bytes,4,spStdLayer::ClassID);
        const std::array<std::uint32_t,9> states{9,8,7,6,5,4,3,2,1};
        if(mode=="states"||mode=="uv"||mode=="uv-zero"||mode=="uv-repeat"||mode=="repeat")Field(bytes,17,states);
        if(mode=="repeat")Field(bytes,17,std::array<std::uint32_t,9>{0,1,2,3,4,5,6,7,8});
        if(mode=="legacy")Field(bytes,8,states);
        if(mode=="uv"||mode=="uv-zero"||mode=="uv-repeat")
        {
            Bytes uv;Add(uv,mode=="uv-zero"?0u:2u);Add(uv,std::array<float,9>{1,2,3,4,5,6,7,8,9});Field(bytes,9,uv);
            if(mode=="uv-repeat"){uv.clear();Add(uv,0u);Add(uv,std::array<float,9>{9,8,7,6,5,4,3,2,1});Field(bytes,9,uv);}
        }
        bytes.push_back(0);spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spMaterialData material;spMaterialDataSerializer serializer;spMemoryStream input;Open(input,bytes);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),material,&error),error.c_str());
        Check(material.GetPassCountForAnalysis()==1,"one owning pass");const auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));
        Check(pass&&pass->GetLayerCountForAnalysis()==1,"one direct-owned layer");const auto* layer=dynamic_cast<const spStdLayer*>(pass->GetLayerForAnalysis(0).get());
        Check(layer&&layer->GetMaterialTextureForAnalysis(),"actual standard-layer factory owns texture");const auto& texture=*layer->GetMaterialTextureForAnalysis();
        Bytes state;for(std::size_t i=0;i<9;++i)Add(state,texture.GetTextureStatesForAnalysis()[i]);Add(state,texture.GetUVTransformForAnalysis());Add(state,std::uint8_t(texture.HasStaticTransformForAnalysis()));
        Check(serializer.IndexRelationshipsWithContextForAnalysis(manager,material),"standard layer without external refs indexes");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,material,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\",\""<<Hex(state)<<"\",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    std::string Graph(const std::string& mode)
    {
        Bytes body;Add(body,spMaterialData::ClassID);Add(body,0x4f4f4253u);Field(body,3,2u);Field(body,4,spStdLayer::ClassID);
        Field(body,17,std::array<std::uint32_t,9>{9,8,7,6,5,4,3,2,1});Bytes uv;Add(uv,2u);Add(uv,std::array<float,9>{1,2,3,4,5,6,7,8,9});Field(body,9,uv);
        Field(body,2,std::array<std::uint32_t,5>{0x12345678,0xabcdef01,0xff102030,0x87654321,0x40600000});body.push_back(0);
        Bytes reference;Add(reference,7u);Add(reference,mode=="prebound"?0u:static_cast<std::uint32_t>(body.size()));if(mode!="prebound")reference.insert(reference.end(),body.begin(),body.end());
        Bytes payload;Field(payload,0,reference);
        if(mode=="repeat"){reference.clear();Add(reference,7u);Add(reference,0u);Field(payload,0,reference);}
        if(mode=="clear")Field(payload,0,0u);payload.push_back(0);payload.push_back(0);
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spMaterialData::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(body.size()));
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();
        Check(fat->LoadIndexForAnalysis(index),"material graph FAT");Check(manager.RegisterForAnalysis(spModel::ClassID,std::make_shared<spModelSerializer>(),0xff,3),"register Model");
        Check(manager.RegisterForAnalysis(spMaterialData::ClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"register MaterialData codec");manager.SetDispatchContextForAnalysis(2,1);
        Check(manager.RegisterForAnalysis(spDXMaterial::ClassID,std::make_shared<spDXMaterialSerializer>(),0xff,3),"actual6D4C80 runtime material serializer pair");
        spSerializerReadContextForAnalysis context(manager,resources);
        if(mode=="prebound"){auto material=std::make_shared<spMaterialData>();fat->FindByIDForAnalysis(7)->object=material.get();context.externalOwners.push_back(material);}
        spModel model;spModelSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),model,&error),error.c_str());
        auto* created=fat->FindByIDForAnalysis(7)->object;Check(created&&bool(context.ShareObjectForAnalysis(created)),"canonical material source owner");
        auto* active=dynamic_cast<spMaterial*>(model.GetMaterialForAnalysis().get());Check(bool(active)==(mode!="clear"),"native clear/prebound/repeat result");
        Bytes state;if(active)
        {
            Check(active->IsExactly(mode=="prebound"?spMaterialData::ClassID:spDXMaterial::ClassID),"wire MaterialData becomes runtime DXMaterial only when actual header factory runs");
            Check(active==created&&active->GetPassCountForAnalysis()==(mode=="prebound"?0u:1u),"same material alias and correct pass count");
            if(active->GetPassCountForAnalysis())
            {
                const auto* pass=static_cast<spMaterialPassLayer*>(active->GetPassForAnalysis(0));const auto& texture=*pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis();
                for(std::size_t i=0;i<9;++i)Add(state,texture.GetTextureStatesForAnalysis()[i]);Add(state,texture.GetUVTransformForAnalysis());Add(state,std::uint8_t(texture.HasStaticTransformForAnalysis()));
            }
        }
        // Native clear has freed the material while FAT still aliases it.
        // Source context deliberately keeps that object alive, never emulates UAF.
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);Check(spSerializer::IndexReferenceForAnalysis(manager,&model),"recursive material graph save index");
        Check(fat->GetResourceCountForAnalysis()==(active?2u:1u),"pass/layer inline objects do not get separate FAT entries");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,model,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(payload)<<"\","<<(state.empty()?"null":'"'+Hex(state)+'"')<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    std::string TextureGraph(const std::string& mode)
    {
        Bytes body;Add(body,spDXTexture::ClassID);Add(body,0x4f4f4253u);
        Add(body,1u);Add(body,1u);Add(body,3u);Add(body,std::uint8_t(0));Add(body,1u);Add(body,0x04030201u);
        Bytes ref;Add(ref,7u);Add(ref,static_cast<std::uint32_t>(body.size()));ref.insert(ref.end(),body.begin(),body.end());
        Bytes payload;if(mode!="orphan"){Field(payload,3,2u);Field(payload,4,spStdLayer::ClassID);}Field(payload,10,ref);
        if(mode=="repeat"||mode=="two-layers")
        {if(mode=="two-layers")Field(payload,4,spStdLayer::ClassID);ref.clear();Add(ref,7u);Add(ref,0u);Field(payload,10,ref);}
        if(mode=="null-after")Field(payload,10,0u);payload.push_back(0);
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spDXTexture::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(body.size()));
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();
        Check(fat->LoadIndexForAnalysis(index),"texture FAT");
        Check(manager.RegisterForAnalysis(spMaterialData::ClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"register material codec");
        Check(manager.RegisterForAnalysis(spDXTexture::ClassID,std::make_shared<spDXTextureSerializer>(),0xff,3),"actual runtime DXTexture codec pair");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t row)noexcept{return row+4;};
        spMaterialData material;spMaterialDataSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),material,&error),error.c_str());
        auto* loaded=fat->FindByIDForAnalysis(7)->object;std::ostringstream state;state<<'[';
        if(mode=="orphan")Check(!loaded&&material.GetPassCountForAnalysis()==0,"orphan field does not create a texture");
        else
        {
            Check(loaded&&loaded->IsExactly(spDXTexture::ClassID)&&context.ShareObjectForAnalysis(loaded),"shared canonical DX texture");
            const auto* pass=static_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));
            Check(pass->GetLayerCountForAnalysis()==(mode=="two-layers"?2u:1u),"expected owning layer count");
            for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
            {
                const auto& holder=*pass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis();
                const bool active=holder.GetFallBackTextureForAnalysis()==loaded;
                Check(active==(mode!="null-after"),"NULL field10 clears; repeat and aliases preserve canonical pointer");
                if(i)state<<',';state<<(active?"true":"false");
                Check(!active||holder.GetFallBackTextureOwnerForAnalysis().get()==loaded,"active fallback retains canonical owner");
            }
        }
        state<<']';fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
        Check(spSerializer::IndexReferenceForAnalysis(manager,&material),"recursive material-to-texture index");
        Check(fat->GetResourceCountForAnalysis()==((mode=="orphan"||mode=="null-after")?1u:2u),"two layers share one FAT texture entry");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,material,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(payload)<<"\","<<state.str()<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    void DXIdentityAndCopy()
    {
        spDXMaterial material;Check(material.IsExactly(spDXMaterial::ClassID)&&material.IsKindOf(spMaterial::ClassID)
            &&!material.IsKindOf(spMaterialData::ClassID)&&!material.IsKindOf(spNamedObject::ClassID),"DX engine identity is sibling of MaterialData, physical name does not change engine RTTI");
        Check(material.GetAmbientColorForAnalysis()==spMaterial::ColorRGBA{0,0,0,0}
            &&material.GetEmissiveColorForAnalysis()==spMaterial::ColorRGBA{0,0,0,0},"actual transparent DX black defaults");
        Check(!material.HasInitializedSpecularPowerForAnalysis(),"DX power is unspecified, host zero not advertised as native default");
        spMemoryStream output;Open(output);std::string error;
        Check(!spDXMaterialSerializer{}.WritePayloadForAnalysis(output,material,&error)&&Data(output).empty(),"unset DX power rejected before output");
        for(int kind=0;kind<3;++kind)
        {
            Bytes bytes;Add(bytes,kind==2?spDXMaterial::ClassID:0x12345678u);Add(bytes,0xabcdef01u);
            spMemoryStream input;Open(input,bytes);spSerializerObjectHeaderForAnalysis observed;
            std::unique_ptr<spBaseObject> created;
            if(kind==0)created=spMaterialDataSerializer{}.ReadObjectHeaderAndCreateForAnalysis(input,&observed);
            if(kind==1)created=spDXMaterialDataSerializer{}.ReadObjectHeaderAndCreateForAnalysis(input,&observed);
            if(kind==2)created=spDXMaterialSerializer{}.ReadObjectHeaderAndCreateForAnalysis(input,&observed);
            Check(created&&created->IsExactly(spDXMaterial::ClassID)&&observed.marker==0xabcdef01,"data hook ignores identity, runtime hook uses common RTTI; both ignore marker");
        }
        const spMaterial::ColorRGBA changed{.25F,.5F,.75F,1};material.SetDiffuseColorForAnalysis(changed);material.SetSpecularPowerForAnalysis(3.5F);
        material.SetRenderOverrideByteForAnalysis(2);material.SetVertexAlphaByteForAnalysis(3);(void)material.SetRenderStateForAnalysis(0,19);
        auto owner=material.Clone();auto* clone=dynamic_cast<spDXMaterial*>(owner.get());
        Check(clone&&clone->GetDiffuseColorForAnalysis()==changed&&clone->GetSpecularPowerForAnalysis()==3.5F&&clone->HasInitializedSpecularPowerForAnalysis(),"DX scalar clone copies initialized payload, unlike Data blank clone");
        Check(clone->GetRenderOverrideByteForAnalysis()==2&&clone->GetVertexAlphaByteForAnalysis()==3&&clone->GetRenderStateForAnalysis(0)==19,"DX scalar clone preserves raw bytes and states");
        Check(material.SetPassForAnalysis(0,std::make_shared<spMaterialPassLayer>()),"nonempty unsupported clone shape");
        Check(!material.Clone(),"never alias unknown DX pass clone graph as if fully restored");
    }
    void TextureFallbackOwners()
    {
        spMaterialTexture material;
        auto texture=std::make_shared<spTextureData>();std::weak_ptr<spTextureData> weak=texture;
        material.SetOwnedFallBackTextureForAnalysis(texture);
        Check(material.GetFallBackTextureForAnalysis()==texture.get()&&texture.use_count()==2,"fallback retains canonical texture owner");
        material.SetOwnedFallBackTextureForAnalysis(texture);
        Check(texture.use_count()==2,"same-pointer owned assignment retains one edge");
        material.SetFallBackTextureForAnalysis(texture.get());
        Check(texture.use_count()==2,"legacy same-pointer setter does not drop existing canonical owner");
        texture.reset();Check(!weak.expired(),"fallback survives outside owner");
        auto owner=material.Clone();auto* clone=dynamic_cast<spMaterialTexture*>(owner.get());
        Check(clone&&clone->GetFallBackTextureForAnalysis()==material.GetFallBackTextureForAnalysis(),"native clone shares texture instead of cloning its data");
        Check(clone->GetFallBackTextureOwnerForAnalysis().use_count()==2,"clone retains a second canonical edge");
        material.SetOwnedFallBackTextureForAnalysis(nullptr);Check(!weak.expired(),"clearing first holder preserves clone fallback");
        owner.reset();Check(weak.expired(),"last holder releases canonical texture");
        auto old=std::make_shared<spTextureData>();weak=old;material.SetOwnedFallBackTextureForAnalysis(old);old.reset();
        auto replacement=std::make_shared<spTextureData>();material.SetOwnedFallBackTextureForAnalysis(replacement);
        Check(weak.expired()&&material.GetFallBackTextureForAnalysis()==replacement.get(),"rebind releases old and retains replacement");
        spTextureData borrowed;material.SetFallBackTextureForAnalysis(&borrowed);
        Check(!material.GetFallBackTextureOwnerForAnalysis()&&replacement.use_count()==1,"raw legacy fixture explicitly remains borrowed");
        material.SetUVControllerForAnalysis(&borrowed);
        Check(!material.Clone(),"unknown nonempty controller clone is rejected, not shallow-aliased");
    }
    void FailuresAndOwners()
    {
        for(int mode=0;mode<7;++mode)
        {
            Bytes bytes;
            if(mode==0)bytes={0xa0,44,0,0};
            if(mode==1){Field(bytes,1,2u);bytes.push_back(0);}
            if(mode==2){Field(bytes,2,0u);bytes.push_back(0);}
            if(mode==3){Field(bytes,4,spStdLayer::ClassID);bytes.push_back(0);}
            if(mode==4){Field(bytes,0,std::array<std::uint32_t,11>{});}
            if(mode==5){for(int i=0;i<9;++i)Field(bytes,3,1u);bytes.push_back(0);}
            if(mode==6){Field(bytes,6,std::array<std::uint32_t,2>{1,0});bytes.push_back(0);}
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spMaterialData material;spMemoryStream input;Open(input,bytes);std::string error;
            Check(!spMaterialDataSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),material,&error)&&context.failed&&!error.empty(),"bounded malformed/unrestored material rejected explicitly");
        }
        spMaterialData material;auto a=std::make_shared<spMaterialPassLayer>(),b=std::make_shared<spMaterialPassLayer>();
        std::weak_ptr<spMaterialPassLayer> weak=a;
        Check(material.SetPassForAnalysis(0,a)&&material.SetPassForAnalysis(1,b),"own material passes");a.reset();Check(!weak.expired(),"material retains pass");
        Check(material.SetPassForAnalysis(0,nullptr)&&weak.expired()&&material.GetPassCountForAnalysis()==1&&material.GetPassForAnalysis(0)==b.get(),"native slot removal shifts owned tail");
        Check(!material.SetPassForAnalysis(8,b),"host guards native unbounded index");
        Check(b->SetLayerForAnalysis(0,std::make_unique<spMaterialTextureLayer>()),"unsupported base layer");
        spMemoryStream output;Open(output);std::string error;
        Check(!spMaterialDataSerializer{}.WritePayloadForAnalysis(output,material,&error)&&Data(output).empty(),"unknown layer rejected before writing partial material");
        struct CountedLayer:spMaterialTextureLayer{bool& destroyed;explicit CountedLayer(bool& value):destroyed(value){}~CountedLayer() override{destroyed=true;}};
        bool deleted=false;spMaterialPassLayer layerPass;
        Check(layerPass.SetLayerForAnalysis(2,std::make_unique<CountedLayer>(deleted))&&layerPass.GetLayerCountForAnalysis()==3&&!deleted,"direct layer slot owns a unique object");
        Check(layerPass.SetLayerForAnalysis(2,nullptr)&&deleted&&layerPass.GetLayerCountForAnalysis()==3,"NULL layer replacement destroys without shrinking count");
        Check(layerPass.SetLayerForAnalysis(7,nullptr)&&layerPass.GetLayerCountForAnalysis()==8,"NULL inactive layer slot grows count unlike Material pass setter");
        Bytes wrongUV;Field(wrongUV,3,1u);Field(wrongUV,4,spStdLayer::ClassID);Field(wrongUV,9,std::array<float,9>{});wrongUV.push_back(0);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream input;Open(input,wrongUV);
        spMaterialData malformed;Check(!spMaterialDataSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(wrongUV.size()),malformed,&error)&&context.failed,"UV36 missing flag rejected; native may read across field and report success with diagnostic");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--material"){std::cout<<Scalar(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--material-layer"){std::cout<<Layer(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--material-graph"){std::cout<<Graph(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--material-texture"){std::cout<<TextureGraph(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"empty","values","repeat","null-controller","pass-only"})(void)Scalar(mode);
        for(const auto* mode:{"default","states","repeat","legacy","uv","uv-zero","uv-repeat"})(void)Layer(mode);
        for(const auto* mode:{"inline","repeat","clear","prebound"})(void)Graph(mode);
        for(const auto* mode:{"inline","repeat","two-layers","null-after","orphan"})(void)TextureGraph(mode);
        DXIdentityAndCopy();TextureFallbackOwners();FailuresAndOwners();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC material codecs, actual DX identity, owners and safe bounds\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
