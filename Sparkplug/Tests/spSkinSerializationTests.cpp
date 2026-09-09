#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
    Bytes Unhex(const std::string& text)
    {Check(text.size()%2==0,"hex input length");Bytes bytes;for(std::size_t i=0;i<text.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;}
    std::string Hex(const Bytes& bytes)
    {constexpr char digits[]="0123456789abcdef";std::string text;for(auto b:bytes){text+=digits[b>>4];text+=digits[b&15];}return text;}
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"fixture capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"fixture rewind");}
    Bytes Data(spMemoryStream& stream)
    {std::uint32_t size=0;Check(stream.GetSize(&size),"stream size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    template<class T>Bytes Bits(const T& value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);return Bytes(p,p+sizeof(value));}
    std::string Capture(const std::string& mode,const Bytes& directory,const Bytes& payload)
    {
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;Open(index,directory);
        auto* fat=manager.GetFATForAnalysis();Check(fat->LoadIndexForAnalysis(index),"actual reconstructed FAT parser");
        Check(manager.RegisterForAnalysis(spSkin::ClassID,std::make_shared<spSkinSerializer>(),0xff,3),"register Skin");
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"register Node");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        if(mode=="prebound"){auto node=std::make_shared<spNode>();fat->FindByIDForAnalysis(7)->object=node.get();context.externalOwners.push_back(node);}
        spSkin skin;spSkinSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        Check(skin.GetWeightCountForAnalysis()==4&&skin.GetBoneCountForAnalysis()==0,"original factory defaults");
        const bool result=serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),skin,&error);
        Check(result==(mode!="null")&&context.failed==!result,"native reader result");
        std::uint32_t position=0;Check(input.GetCurrentPosition(position),"reader cursor");
        Check(position==payload.size()-(mode=="null"),"native final cursor");
        Bytes matrices;std::ostringstream bones;bones<<'[';bool first=true;
        for(const auto& binding:skin.GetBoneBindingsForAnalysis())
        {
            Check(binding.GetBoneForAnalysis().get()==fat->FindByIDForAnalysis(7)->object,"one canonical borrowed reference with explicit host owner");
            const auto bits=Bits(binding.inverseBindMatrix);matrices.insert(matrices.end(),bits.begin(),bits.end());
            if(!first)bones<<',';first=false;
            bones<<"[7,\""<<Hex(Bits(binding.GetBoneForAnalysis()->GetWorldPositionForAnalysis()))<<"\"]";
        }
        bones<<']';std::string output="null";
        spSkin inspected;spSkinSerializer::InspectionForAnalysis observation;spMemoryStream inspection;Open(inspection,payload);
        const bool inspectedResult=serializer.InspectPayloadForAnalysis(inspection,static_cast<std::uint32_t>(payload.size()),inspected,observation,&error);
        Check(inspectedResult==result,"Skin inspection matches original shared reader status");
        Check(inspected.GetBoneCountForAnalysis()==0,"inspection never attaches substitute Node palette");
        if(result)
        {
            Check(observation.bones.size()==skin.GetBoneCountForAnalysis(),"inspected palette count matches loaded palette");
            Check((observation.fieldMask?observation.weights:inspected.GetWeightCountForAnalysis())==skin.GetWeightCountForAnalysis(),"inspected weight count uses original default or last field");
            Bytes observedMatrices;for(const auto& bone:observation.bones)
            {Check(bone.reference.id==7,"canonical inspected bone ID");const auto bits=Bits(bone.inverseBind);observedMatrices.insert(observedMatrices.end(),bits.begin(),bits.end());}
            Check(observedMatrices==matrices,"inspected matrices retain all original bits including nonaffine/NaN/signed zero");
        }
        if(result)
        {
            fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
            Check(spSerializer::IndexReferenceForAnalysis(manager,&skin),"recursive inherited fields and bone index");
            Check(fat->GetResourceCountForAnalysis()==(skin.GetBoneCountForAnalysis()?2u:1u),"canonical IDs for repeated bones");
            spMemoryStream stream;Open(stream);
            Check(serializer.WritePayloadWithContextForAnalysis(manager,stream,skin,&error),error.c_str());
            output='"'+Hex(Data(stream))+'"';
        }
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(payload)<<"\","<<result<<','<<position<<','
            <<skin.GetWeightCountForAnalysis()<<','<<skin.GetBoneCountForAnalysis()<<",\""<<Hex(matrices)<<"\","<<bones.str()<<','<<output<<']';return row.str();
    }
    std::string Model(const Bytes& payload)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spModel model;spModelSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        if(!serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),model,&error))throw std::runtime_error(error);
        spModel partial;spModelSerializer::InspectionForAnalysis observation;Open(input,payload);
        if(!serializer.InspectPayloadForAnalysis(input,static_cast<std::uint32_t>(payload.size()),partial,observation,&error))throw std::runtime_error(error);
        Check(model.IsAlphaSortEnabledForAnalysis()==partial.IsAlphaSortEnabledForAnalysis()
            &&model.GetPriorityForAnalysis()==partial.GetPriorityForAnalysis()
            &&model.GetProjectionGroupForAnalysis()==partial.GetProjectionGroupForAnalysis(),"Model inspector shares scalar assignments and defaults");
        spMemoryStream output;Open(output);
        if(!serializer.WritePayloadForAnalysis(output,model,&error))throw std::runtime_error(error);
        std::ostringstream row;row<<'['<<model.IsAlphaSortEnabledForAnalysis()<<','<<model.GetPriorityForAnalysis()<<','
            <<model.GetProjectionGroupForAnalysis()<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    void Guards()
    {
        std::weak_ptr<spNode> releasedRoot;
        std::weak_ptr<spSkin> releasedSkin;
        {
            auto root=std::make_shared<spNode>();auto render=std::make_shared<spRenderNode>();
            auto skin=std::make_shared<spSkin>();releasedRoot=root;releasedSkin=skin;
            Check(root->AttachChildForAnalysis(render)&&render->AttachRenderableForAnalysis(skin),"ancestor owns the Skin graph");
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            context.externalOwners.push_back(root);
            Check(manager.GetFATForAnalysis()->IndexObjectForAnalysis(spNode::ClassID,*root),"prebound ancestor FAT entry");
            Bytes payload{0,0,0xa0,80};
            for(auto value:{4u,1u,1u,0u}){const auto bytes=Bits(value);payload.insert(payload.end(),bytes.begin(),bytes.end());}
            payload.insert(payload.end(),64,0);payload.push_back(0);
            spMemoryStream stream;Open(stream,payload);std::string error;
            Check(spSkinSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(payload.size()),*skin,&error),error.c_str());
            Check(skin->GetBoneBindingsForAnalysis()[0].GetBoneForAnalysis()==root,"decoded Skin borrows its ancestor");
        }
        Check(releasedRoot.expired()&&releasedSkin.expired(),"loaded ancestor-bone edge creates no ownership cycle");
        spSkin::BoneBinding expired(nullptr,{});
        {auto owner=std::make_shared<spNode>();expired=spSkin::BoneBinding::BorrowedForAnalysis(owner,{});}
        Check(!expired.GetBoneForAnalysis(),"expired borrowed bone resolves safely to null");
        // Host-only envelope checks: original ignores matrix ReadData failure
        // and may overflow count arithmetic. These are declared deviations.
        for(const auto* text:{"0000a00800000000ffffffff00","0000a0070000000000000000","00000000"})
        {
            auto payload=Unhex(text);spMemoryStream stream;Open(stream,payload);
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spSkin skin;std::string error;
            Check(!spSkinSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(payload.size()),skin,&error)&&context.failed,"invalid Skin envelope rejected");
            Check(skin.GetWeightCountForAnalysis()==4&&skin.GetBoneCountForAnalysis()==0,"failed envelope leaves palette unchanged");
        }
        spSkin skin;spSkinSerializer serializer;spMemoryStream stream;Open(stream);std::string error;
        Check(serializer.WritePayloadForAnalysis(stream,skin,&error),"empty palette requires no graph manager");
        Check(Hex(Data(stream))=="6201000000630000000000e1040000000300000000e008000000040000000000000000","original full empty Skin wire capture");
        for(const auto* text:{"0000","63070000000000","62020000006309000000620000000000610500000000"})(void)Model(Unhex(text));
    }
    std::string Clone(const std::string& mode)
    {
        spCloneManager manager;spSkin source;auto bone=std::make_shared<spNode>();
        bone->SetPositionForAnalysis({1,2,3});bone->MarkLocalTransformDirtyForAnalysis();Check(bone->UpdateWorldForAnalysis(),"source cached world input");
        const std::size_t count=mode=="empty"?0u:(mode=="repeat"||mode=="direct-repeat"||mode=="child"?2u:1u);
        std::shared_ptr<spNode> child,mapped;
        if(mode=="child")
        {child=std::make_shared<spNode>();child->SetPositionForAnalysis({4,5,6});Check(bone->AttachChildForAnalysis(child),"source Node child attach");}
        std::vector<spSkin::BoneBinding> bindings;
        for(std::size_t i=0;i<count;++i)
        {
            spSkin::Matrix4 matrix{};
            if(mode=="raw-bits")
            {std::array<std::uint32_t,16> bits{0x80000000,0x7fc12345,0x7f800000,0xff800000};for(std::uint32_t j=0;j<12;++j)bits[j+4]=j;std::memcpy(matrix.data(),bits.data(),64);}
            else for(std::size_t j=0;j<16;++j)matrix[j]=static_cast<float>(j+i);
            bindings.push_back({mode=="child"&&i==1?child:bone,matrix});
        }
        Check(source.SetPaletteForAnalysis(3,std::move(bindings)),"explicit source palette");source.SetProjectionGroupForAnalysis(17);
        if(mode=="mapped")
        {mapped=std::make_shared<spNode>();mapped->SetPositionForAnalysis({7,8,9});Check(manager.RegisterSharedCloneForAnalysis(*bone,mapped),"explicit live mapped owner");}
        std::unique_ptr<spBaseObject> result;
        if(mode=="copy-populated")
        {auto skin=std::make_unique<spSkin>();Check(skin->SetPaletteForAnalysis(9,{{bone,{}}}),"old destination palette");Check(source.vfunc_14(*skin,manager),"actual copy API");result=std::move(skin);}
        else result=mode=="direct-repeat"?source.vfunc_10(manager):manager.Clone(source);
        auto* clone=dynamic_cast<spSkin*>(result.get());Check(clone&&clone->GetBoneCountForAnalysis()==count,"whole Skin clone result");
        Check(!manager.FindClone(source)&&!manager.FindClone(*bone),"root transaction map cleared");
        std::vector<const spNode*> unique;
        for(const auto& binding:clone->GetBoneBindingsForAnalysis())
            if(std::find(unique.begin(),unique.end(),binding.GetBoneForAnalysis().get())==unique.end())unique.push_back(binding.GetBoneForAnalysis().get());
        Bytes matrices;std::ostringstream nodes;nodes<<'[';bool first=true;
        for(const auto& binding:clone->GetBoneBindingsForAnalysis())
        {
            const auto* node=binding.GetBoneForAnalysis().get();Check(node!=bone.get()&&node!=child.get(),"unmapped bone cloned independently of source");
            const auto bits=Bits(binding.inverseBindMatrix);matrices.insert(matrices.end(),bits.begin(),bits.end());
            const auto parent=std::find(unique.begin(),unique.end(),node->GetParentForAnalysis());
            if(!first)nodes<<',';first=false;
            nodes<<'['<<(std::find(unique.begin(),unique.end(),node)-unique.begin())<<','<<(node==mapped.get())<<','
                <<(parent==unique.end()?-1:static_cast<int>(parent-unique.begin()))<<",\""<<Hex(Bits(node->GetPositionForAnalysis()))
                <<"\",\""<<Hex(Bits(node->GetWorldPositionForAnalysis()))<<"\","<<node->GetFlagsForAnalysis()<<']';
        }
        nodes<<']';
        // Ownership test independent of the native borrowed-pointer ABI:
        // resulting bones must remain valid when every source owner is gone.
        Check(source.SetPaletteForAnalysis(4,{}),"release source palette");bone.reset();child.reset();mapped.reset();
        for(const auto& binding:clone->GetBoneBindingsForAnalysis())Check(binding.GetBoneForAnalysis()->IsKindOf(spNode::ClassID),"clone retains real destination owner");
        std::ostringstream row;row<<"[\""<<mode<<"\",1,"<<clone->GetWeightCountForAnalysis()<<','<<count<<','<<clone->GetProjectionGroupForAnalysis()
            <<",\""<<Hex(matrices)<<"\","<<nodes.str()<<']';return row.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--capture")
        {std::string directory,payload;std::cin>>directory>>payload;std::cout<<Capture(argv[2],Unhex(directory),Unhex(payload))<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--clone"){std::cout<<Clone(argv[2])<<'\n';return 0;}
        if(argc==2&&std::string(argv[1])=="--model")
        {std::string payload;std::cin>>payload;std::cout<<Model(Unhex(payload))<<'\n';return 0;}
        Guards();for(const auto* mode:{"empty","one","mapped","repeat","direct-repeat","child","copy-populated","raw-bits"})(void)Clone(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": Skin envelope guards, native wire and clone ownership\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
