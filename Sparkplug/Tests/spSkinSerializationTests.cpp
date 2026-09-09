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
    void RenderableScalarWriterSlices()
    {
        // CP10 secondary47F7A0 -> target1402630 always writes these fields.
        // Exact slices also preserved in sealed model-skin-reader captures:
        // model-empty-original.log SHA256 7A860B605673A3B0E56BE658FF2F9E333
        // EBE096901B04D2DA81B3C6DDA7B1016: 6201000000630000000000.
        // model-repeat-original.log SHA256 B2A96A45887B8CA39AB2BA91DF78B881
        // B1D625D36445FF3471BEA7374540E89D: 620000000063ffffffff00.
        for(const bool alpha:{false,true})for(const std::uint32_t priority:{0u,UINT32_MAX})
        {
            spModel object;object.SetAlphaSortEnabledForAnalysis(alpha);object.SetPriorityForAnalysis(priority);
            spMemoryStream output;Open(output);std::string error;
            Check(spRenderableSerializer::WriteAlphaSortEnableFieldForAnalysis(output,object,&error),error.c_str());
            Check(Hex(Data(output))==(alpha?"6201000000":"6200000000"),"alpha slice matches original true/false UInt32 field, without suppression or terminator");
            Open(output);Check(spRenderableSerializer::WritePriorityFieldForAnalysis(output,object,&error),error.c_str());
            Check(Hex(Data(output))==(priority?"63ffffffff":"6300000000"),"priority slice matches original zero/full-u32 field, without suppression or terminator");
            Check(object.IsAlphaSortEnabledForAnalysis()==alpha&&object.GetPriorityForAnalysis()==priority,"scalar writer slices preserve actual object state");
        }
    }
    void RenderableScalarObservations()
    {
        struct Expected{std::uint32_t field,offset;};std::vector<Expected> expected;
        Bytes renderable;
        const auto field=[](Bytes& bytes,std::uint8_t id,const Bytes& value)
        {
            Check(value.size()<256,"bounded observation fixture field");bytes.push_back(0xa0+id);
            bytes.push_back(static_cast<std::uint8_t>(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());
        };
        const auto scalar=[&](std::uint8_t id,std::uint32_t value)
        {
            expected.push_back({id,static_cast<std::uint32_t>(renderable.size()+2)});
            field(renderable,id,Bits(value));
        };
        scalar(3,UINT32_MAX);scalar(2,256);
        field(renderable,18,Bits(std::array<std::uint32_t,2>{7,0}));
        scalar(2,0);scalar(3,7);scalar(2,2);renderable.push_back(0);
        Bytes model;field(model,2,Bytes{0xff});field(model,3,Bits(std::array<std::uint32_t,2>{5,0}));
        field(model,1,Bits(11u));model.push_back(0);
        Bytes skin;field(skin,2,Bits(0u));field(skin,3,Bits(UINT32_MAX));
        field(skin,0,Bits(std::array<std::uint32_t,2>{4,0}));skin.push_back(0);
        Bytes payload=renderable;payload.insert(payload.end(),model.begin(),model.end());payload.insert(payload.end(),skin.begin(),skin.end());
        Bytes prefixed{0x42,0x42,0x42,0x42,0x42};prefixed.insert(prefixed.end(),payload.begin(),payload.end());
        spMemoryStream input;Open(input,prefixed);Check(input.Seek(spStream::SeekSource::essStart,5),"nonzero Skin stream start");
        spSkin object;spSkinSerializer::InspectionForAnalysis observation;std::string error;
        Check(spSkinSerializer{}.InspectPayloadForAnalysis(input,static_cast<std::uint32_t>(payload.size()),object,observation,&error),error.c_str());
        const auto& rows=observation.model.renderable.scalarFields;
        Check(rows.size()==expected.size(),"only actual Renderable scalar assignments are observed across three sections");
        for(std::size_t i=0;i<expected.size();++i)
        {
            const auto& row=rows[i];const auto& want=expected[i];
            Check(static_cast<std::uint32_t>(row.field)==want.field&&row.payloadOffset==want.offset
                &&row.payloadSize==4&&row.assignmentOrder==i&&row.owner==static_cast<const spRenderable*>(&object),
                "Renderable row has exact relative extent, actual base owner and repeated-assignment order");
        }
        Check(object.IsAlphaSortEnabledForAnalysis()&&object.GetPriorityForAnalysis()==7,
            "repeated actual Renderable scalars are last-wins and normalize nonzero alpha");
        Check(object.GetProjectionGroupForAnalysis()==11&&observation.weights==4&&observation.bones.empty(),
            "Model/Skin retain their own semantics despite unknown fields2/3");
        Check(observation.model.renderable.fieldMask==12&&observation.model.fieldMask==2&&observation.fieldMask==1,
            "field masks remain scoped to the actual inherited sections");
        std::uint32_t position=0;Check(input.GetCurrentPosition(position)&&position==prefixed.size(),"three sections consume their exact complete extent");
        // Observation reset belongs to the existing whole-inspection entry.
        const Bytes empty{0,0,0};Open(input,empty);spSkin second;
        Check(spSkinSerializer{}.InspectPayloadForAnalysis(input,static_cast<std::uint32_t>(empty.size()),second,observation,&error),error.c_str());
        Check(observation.model.renderable.scalarFields.empty()&&second.IsAlphaSortEnabledForAnalysis()
            &&second.GetPriorityForAnalysis()==0,"reused inspection clears borrowed rows and does not invent missing scalar assignments");
    }
    void PaletteWriterSlices()
    {
        spSkin empty;spMemoryStream output;Open(output);std::string error;
        Check(spSkinSerializer::WritePaletteFieldWithContextForAnalysis(nullptr,output,empty,&error),error.c_str());
        // Exact CP62 original field; the final section terminator is excluded.
        Check(Hex(Data(output))=="e0080000000400000000000000","empty palette retains UInt32 length, default weights and null-manager support");
        spSerializerManager manager;spResourceManager resources;
        Open(output);Check(spSkinSerializer::WritePaletteFieldWithContextForAnalysis(manager,output,empty,&error),error.c_str());
        Check(Hex(Data(output))=="e0080000000400000000000000","reference manager overload writes the same empty field");
        // Original skin-raw-bits capture, sealed 2026-09-09: input contains an
        // actual inline Node; ordinary full read/index keeps its real owner.
        // No prebound-ID API or substitute inspection palette is introduced.
        const auto payload=Unhex("0000a067ffffffff010000000700000017000000650f5c6953424f4fa00c0000803f000000400000404000000000804523c17f0000807f000080ff000000000100000002000000030000000400000005000000060000000700000008000000090000000a0000000b00000000");
        const auto directory=Unhex("01000000070000000000650f5c690000000017000000");
        spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();
        Check(fat->LoadIndexForAnalysis(index),"palette original input FAT");
        Check(manager.RegisterForAnalysis(spSkin::ClassID,std::make_shared<spSkinSerializer>(),0xff,3),"palette Skin registration");
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"palette Node registration");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        spSkin skin;spMemoryStream input;Open(input,payload);
        Check(spSkinSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),skin,&error),error.c_str());
        Check(skin.GetWeightCountForAnalysis()==UINT32_MAX&&skin.GetBoneCountForAnalysis()==1,"original full UInt32 weights and one actual bone");
        Open(output);Check(!spSkinSerializer::WritePaletteFieldWithContextForAnalysis(nullptr,output,skin,&error)&&Data(output).empty(),
            "nonempty palette without manager is rejected before field output");
        Open(output);Check(!spSkinSerializer{}.WritePayloadForAnalysis(output,skin,&error)&&Data(output).empty(),
            "full nonempty writer retains pre-inherited-output manager guard");
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
        Check(spSerializer::IndexReferenceForAnalysis(manager,&skin),"ordinary original recursive Skin and bone indexing");
        Open(output);Check(spSkinSerializer::WritePaletteFieldWithContextForAnalysis(manager,output,skin,&error),error.c_str());
        // model-skin-reader/skin-raw-bits-original.log SHA256
        // 679A92D3630615D9FAA46154B3172395D3492FD273CE75C24350EB94E44CDEAA.
        Check(Hex(Data(output))=="e069000000ffffffff010000000200000019000000650f5c6953424f4fa00c0000803f0000004000004040280100000000804523c17f0000807f000080ff000000000100000002000000030000000400000005000000060000000700000008000000090000000a0000000b000000",
            "palette helper matches exact original indexed Node/reference/raw-matrix field bytes");
    }
    void PaletteFieldObservations()
    {
        const auto field=[](Bytes& bytes,std::uint8_t id,const Bytes& payload,std::uint8_t width)
        {
            bytes.push_back(static_cast<std::uint8_t>(id|((width==2?6u:7u)<<5)));
            const auto size=static_cast<std::uint32_t>(payload.size());
            for(std::uint8_t i=0;i<width;++i)bytes.push_back(static_cast<std::uint8_t>(size>>(8*i)));
            bytes.insert(bytes.end(),payload.begin(),payload.end());
        };
        Bytes payload;field(payload,0,Bits(0u),2);payload.push_back(0); // Renderable field0, not palette.
        field(payload,0,Bits(std::array<std::uint32_t,2>{77,0}),4);payload.push_back(0); // Model field0, not palette.
        field(payload,18,Bits(std::array<std::uint32_t,2>{0,0}),2); // Unknown eight-byte collision.
        const auto first=static_cast<std::uint32_t>(payload.size());
        field(payload,0,Bits(std::array<std::uint32_t,2>{0,0}),2); // Zero weights still is an actual palette field.
        const auto second=static_cast<std::uint32_t>(payload.size());
        const auto matrix=Unhex("000000804523c17f0000807f000080ff000000000100000002000000030000000400000005000000060000000700000008000000090000000a0000000b000000");
        Bytes palette=Bits(std::array<std::uint32_t,4>{UINT32_MAX,1,9,0});palette.insert(palette.end(),matrix.begin(),matrix.end());
        field(payload,0,palette,4);payload.push_back(0);
        Bytes prefixed{0xfe,0xed,0xfa,0xce,0x7f};prefixed.insert(prefixed.end(),payload.begin(),payload.end());
        spMemoryStream input;Open(input,prefixed);Check(input.Seek(spStream::SeekSource::essStart,5),"nonzero palette input start");
        spSkin partial;spSkinSerializer::InspectionForAnalysis observation;std::string error;
        Check(spSkinSerializer{}.InspectPayloadForAnalysis(input,static_cast<std::uint32_t>(payload.size()),partial,observation,&error),error.c_str());
        Check(observation.paletteFields.size()==2,"only the two actual Skin palette fields are observed");
        const auto& a=observation.paletteFields[0];const auto& b=observation.paletteFields[1];
        Check(a.headerOffset==first&&a.payloadOffset==first+3&&a.payloadSize==8&&a.assignmentOrder==0,
            "zero-weight palette has exact wide header/payload location independent of stream start");
        Check(b.headerOffset==second&&b.payloadOffset==second+5&&b.payloadSize==80&&b.assignmentOrder==1,
            "repeated palette retains its own UInt32 header and encounter order");
        Check(observation.weights==UINT32_MAX&&observation.bones.size()==1&&observation.bones[0].reference.id==9
            &&Bits(observation.bones[0].inverseBind)==matrix,"last palette assignment preserves unrestricted weights and raw matrix bits");
        Check(partial.GetBoneCountForAnalysis()==0,"palette observation does not invent substitute Node owners");
        std::uint32_t position=0;Check(input.GetCurrentPosition(position)&&position==prefixed.size(),"palette inspection consumes all three sections exactly");
        payload.pop_back();const auto third=static_cast<std::uint32_t>(payload.size());
        field(payload,0,Bits(std::array<std::uint32_t,2>{0,0}),4);payload.push_back(0);Open(input,payload);spSkin cleared;
        Check(spSkinSerializer{}.InspectPayloadForAnalysis(input,static_cast<std::uint32_t>(payload.size()),cleared,observation,&error),error.c_str());
        Check(observation.paletteFields.size()==3&&observation.paletteFields.back().headerOffset==third
            &&observation.paletteFields.back().assignmentOrder==2&&observation.weights==0&&observation.bones.empty(),
            "later zero-weight empty field replaces the palette and remains observed");
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
        Guards();RenderableScalarWriterSlices();RenderableScalarObservations();PaletteWriterSlices();PaletteFieldObservations();for(const auto* mode:{"empty","one","mapped","repeat","direct-repeat","child","copy-populated","raw-bits"})(void)Clone(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": Skin envelope guards, native wire and clone ownership\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
