#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    template<class T>void Add(Bytes& b,const T& value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);b.insert(b.end(),p,p+sizeof(value));}
    std::string Hex(const Bytes& bytes)
    {constexpr char digits[]="0123456789abcdef";std::string result;for(auto v:bytes){result+=digits[v>>4];result+=digits[v&15];}return result;}
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"fixture allocation");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    Bytes Data(spMemoryStream& stream)
    {std::uint32_t size=0;Check(stream.GetSize(&size),"size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    std::uint32_t Position(spStream& stream)
    {std::uint32_t position=0;Check(stream.GetCurrentPosition(position),"tell");return position;}
    void Field(Bytes& out,std::uint8_t id,const Bytes& data)
    {Check(!data.empty()&&data.size()<256,"tiny field fixture");out.push_back(0xa0+id);out.push_back(static_cast<std::uint8_t>(data.size()));out.insert(out.end(),data.begin(),data.end());}
    template<class T>void Field(Bytes& out,std::uint8_t id,const T& value)
    {Bytes data;Add(data,value);Field(out,id,data);}
    Bytes State(const spNode& node)
    {Bytes b;Add(b,node.GetPositionForAnalysis());Add(b,node.GetScaleForAnalysis());Add(b,node.GetOrientationForAnalysis());Add(b,node.GetWorldPositionForAnalysis());Add(b,node.GetWorldScaleForAnalysis());Add(b,node.GetWorldOrientationForAnalysis());return b;}
    Bytes ReaderInput(int mode)
    {
        Bytes b;
        if(mode==1){Field(b,0,spNode::Vector3{1,2,3});Field(b,1,std::array<float,4>{0,0,.5f,.5f});Field(b,2,spNode::Vector3{2,3,4});Field(b,3,std::uint8_t(0xa5));Field(b,4,std::uint8_t(1));Field(b,8,std::uint8_t(0));}
        if(mode==2)for(auto id:{3,4,8})Field(b,std::uint8_t(id),std::uint8_t(0));
        if(mode==3)for(auto value:{1,0})for(auto id:{3,4,8})Field(b,std::uint8_t(id),std::uint8_t(value));
        if(mode==4){Field(b,9,std::uint8_t('X'));Field(b,0,spNode::Vector3{1,2,3});Field(b,0,spNode::Vector3{4,5,6});}
        b.push_back(0);return b;
    }
    std::string Readers()
    {
        const char* names[]={"empty","transforms","false-flags","repeat-flags","unknown-repeat"};
        std::ostringstream rows;rows<<'[';
        for(int mode=0;mode<5;++mode)
        {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spNode node;spNodeSerializer serializer;auto bytes=ReaderInput(mode);spMemoryStream stream;Open(stream,bytes);std::string error;
            Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),node,&error),error.c_str());
            Check(!context.failed&&Position(stream)==bytes.size(),"Node field envelope consumed");
            Check(node.IsAnimatedForAnalysis(),"false Animated never clears true default");
            Check(node.IsBoneForAnalysis()==(mode==1),"Bone toggles both directions");
            Check(node.IsStaticForAnalysis()==(mode==1||mode==3),"false Static preserves previous state");
            Check(node.GetPositionForAnalysis()==node.GetWorldPositionForAnalysis(),"reader executes final world propagation");
            if(mode==1)Check(node.GetOrientationForAnalysis()==spNode::Matrix3{.5f,.5f,0,-.5f,.5f,0,0,0,1},"nonunit quaternion is not normalized");
            if(mode)rows<<',';
            rows<<"[\""<<names[mode]<<"\",\""<<Hex(bytes)<<"\","<<Position(stream)<<','<<node.GetFlagsForAnalysis()<<",\""<<Hex(State(node))<<"\"]";
        }
        rows<<']';return rows.str();
    }
    std::string Writers()
    {
        const char* names[]={"defaults","transforms","flags-off","threshold"};std::ostringstream rows;rows<<'[';
        for(int mode=0;mode<4;++mode)
        {
            spNode node;spNodeSerializer serializer;spMemoryStream stream;Open(stream);std::string error;
            if(mode==1){node.SetPositionForAnalysis({1,2,3});node.SetScaleForAnalysis({2,3,4});node.SetOrientationForAnalysis({0,1,0,-1,0,0,0,0,1});node.SetBoneForAnalysis(true);node.SetStaticForAnalysis(true);}
            if(mode==2)node.SetAnimatedForAnalysis(false);
            if(mode==3){spNode::Vector3 value{};const std::array<std::uint32_t,3> bits{0x3a83126f,0x3a831270,0};std::memcpy(value.data(),bits.data(),12);node.SetPositionForAnalysis(value);}
            const auto before=State(node);const auto flags=node.GetFlagsForAnalysis();
            Check(serializer.WritePayloadForAnalysis(stream,node,&error),error.c_str());
            Check(before==State(node)&&flags==node.GetFlagsForAnalysis(),"writer preserves source object");
            auto output=Data(stream);Check(!output.empty()&&output.back()==0,"writer terminator");
            if(mode==0||mode==2)Check(output==Bytes{0x28,std::uint8_t(mode==0),0},"exact mandatory bool encoding");
            if(mode)rows<<',';rows<<"[\""<<names[mode]<<"\",\""<<Hex(output)<<"\"]";
        }
        rows<<']';return rows.str();
    }
    void Directory(spSerializerManager& manager,std::uint32_t size=23)
    {
        Bytes b;Add(b,1u);Add(b,7u);Add(b,std::uint16_t(0));Add(b,spNode::ClassID);Add(b,0u);Add(b,size);
        spMemoryStream input;Open(input,b);Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(input),"Node directory");
        manager.SetDispatchContextForAnalysis(2,1);
    }
    Bytes ChildReference(bool inlineBody)
    {
        Bytes b;Add(b,7u);Add(b,inlineBody?23u:0u);
        if(inlineBody){Add(b,spNode::ClassID);Add(b,0x4f4f4253u);Field(b,0,spNode::Vector3{1,2,3});b.push_back(0);}
        return b;
    }
    void ChildReads()
    {
        for(int mode=0;mode<4;++mode)
        {
            spSerializerManager manager;spResourceManager resources;Directory(manager);
            Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"register Node reader");
            spSerializerReadContextForAnalysis context(manager,resources);std::shared_ptr<spNode> external;
            if(mode>=2){external=std::make_shared<spNode>();external->SetPositionForAnalysis({1,2,3});external->MarkLocalTransformDirtyForAnalysis();manager.GetFATForAnalysis()->FindByIDForAnalysis(7)->object=external.get();if(mode==2)context.externalOwners.push_back(external);}
            Bytes payload;Field(payload,0,spNode::Vector3{10,0,0});Field(payload,5,ChildReference(mode<2));if(mode==1)Field(payload,5,ChildReference(false));payload.push_back(0);
            spMemoryStream input;Open(input,payload);spNode parent;std::string error;
            const auto ok=spNodeSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),parent,&error);
            if(mode==3){Check(!ok&&context.failed&&parent.GetChildCountForAnalysis()==0,"borrowed owning edge without explicit shared owner is rejected");continue;}
            Check(ok&&!context.failed,error.c_str());Check(parent.GetChildCountForAnalysis()==1,"repeat reference is same-parent no-op");
            auto* child=parent.GetChildForAnalysis(0);Check(child&&child->GetParentForAnalysis()==&parent,"reciprocal owning edge");
            Check(child->GetWorldPositionForAnalysis()==spNode::Vector3{11,2,3},"parent transform propagates into resolved child");
            Check(bool(context.ShareObjectForAnalysis(child)),"child shares canonical host owner");
        }
    }
    std::string Graph()
    {
        spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"register Node graph writer");
        manager.SetDispatchContextForAnalysis(2,2);
        spNode root;auto child=std::make_shared<spNode>();child->SetPositionForAnalysis({1,2,3});Check(root.AttachChildForAnalysis(child),"source graph");
        Check(spSerializer::IndexReferenceForAnalysis(manager,&root),"common recursive indexing");
        auto* fat=manager.GetFATForAnalysis();Check(fat->GetResourceCountForAnalysis()==2,"two distinct indexed objects");
        Check(spSerializer::IndexReferenceForAnalysis(manager,&root)&&fat->GetResourceCountForAnalysis()==2,"index repeats do not recurse/duplicate");
        spMemoryStream stream;Open(stream);std::string error;
        Check(spSerializer::WriteReferenceForAnalysis(manager,stream,&root,&error),error.c_str());const auto bytes=Data(stream);
        auto* childEntry=fat->FindByObjectForAnalysis(*child);auto* rootEntry=fat->FindByObjectForAnalysis(root);
        Check(rootEntry->id==1&&rootEntry->offset==8&&rootEntry->size==49&&childEntry->id==2&&childEntry->offset==31&&childEntry->size==25,"exact native nested graph FAT extents");
        Check(spSerializer::WriteReferenceForAnalysis(manager,stream,&root,&error),"repeat root reference");
        auto repeat=bytes;Add(repeat,1u);Add(repeat,0u);Check(Data(stream)==repeat,"repeat graph payload not emitted again");
        // Explicit TEST FFPS envelope around writer-produced object bytes.
        Bytes whole;constexpr std::uint32_t origin=72;
        for(auto word:{0x53504646u,0x26u,0u,origin+49u,2u,origin,49u})Add(whole,word);
        Add(whole,2u);
        for(auto* entry:{rootEntry,childEntry}){Add(whole,entry->id);Add(whole,std::uint16_t(0));Add(whole,spNode::ClassID);Add(whole,entry->offset-8);Add(whole,entry->size);}
        Add(whole,0u);whole.insert(whole.end(),bytes.begin()+8,bytes.end());fat->ClearResourceEntriesForAnalysis();
        auto context=std::make_unique<spSerializerReadContextForAnalysis>(manager,resources);spMemoryStream input;Open(input,whole);
        auto* loaded=dynamic_cast<spNode*>(manager.LoadResourcesForAnalysis(input,*context,&error));
        Check(loaded&&!context->failed,error.c_str());Check(context->createdObjects.size()==2&&loaded->GetChildCountForAnalysis()==1,"whole Node FFPS through common factory/reader/ref core");
        Check(loaded->GetChildForAnalysis(0)->GetWorldPositionForAnalysis()==spNode::Vector3{1,2,3},"whole graph world transform");
        auto retained=std::dynamic_pointer_cast<spNode>(context->ShareObjectForAnalysis(loaded));std::weak_ptr<spBaseObject> weakChild=context->ShareObjectForAnalysis(loaded->GetChildForAnalysis(0));
        context.reset();Check(!weakChild.expired()&&retained->GetChildForAnalysis(0)->GetParentForAnalysis()==retained.get(),"root shared handle keeps child alive after context teardown");
        retained.reset();Check(weakChild.expired(),"last root owner releases complete represented tree");
        return Hex(bytes);
    }
    void BillboardInputs()
    {
        // Native NULL-camera identity and finite billboard math were already
        // established by probe_pc_node_world; test their reader integration.
        const spNode::Matrix3 identity{1,0,0,0,1,0,0,0,1},degenerate{};
        for(std::uint32_t axis:{1u,2u})for(int mode=0;mode<3;++mode)
        {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            context.cameraOrientation=mode==0?nullptr:(mode==1?&identity:&degenerate);
            Bytes payload;Field(payload,6,axis);payload.push_back(0);spMemoryStream input;Open(input,payload);
            spNode node;std::string error;const auto ok=spNodeSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),node,&error);
            Check(ok==(mode!=2)&&context.failed==(mode==2),"billboard NULL succeeds, explicit degeneracy fails safely");
            Check(node.GetBillboardAxisForAnalysis()==axis,"billboard field retained");
            if(ok)Check(node.GetWorldOrientationForAnalysis()==(mode==0?identity:spNode::Matrix3{-1,0,0,0,1,0,0,0,-1}),"reader applies known billboard world math");
        }
    }
    void Failures()
    {
        for(int mode=0;mode<6;++mode)
        {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            Bytes input;
            if(mode==0)input={0xa0,12};
            if(mode==1){Field(input,0,spNode::Vector3{1,2,3});}
            if(mode==2)input={0,0};
            if(mode==3){Field(input,7,0u);input.push_back(0);}
            if(mode==4){Field(input,0,spNode::Vector3{std::numeric_limits<float>::quiet_NaN(),0,0});input.push_back(0);}
            if(mode==5){Field(input,5,0u);input.push_back(0);}
            spMemoryStream source;Open(source,input);spNode node;std::string error;
            Check(!spNodeSerializer{}.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),node,&error)&&context.failed&&!error.empty(),"unsafe/unsupported Node input explicitly rejected");
        }
        spRenderNode derived;spNodeSerializer serializer;spMemoryStream output;Open(output);std::string error;
        Check(!serializer.WritePayloadForAnalysis(output,derived,&error)&&Data(output).empty(),"explicit base serializer cannot silently write only the derived object's base section");
    }
    void Corpus(const char* path)
    {
        std::ifstream file(path,std::ios::binary|std::ios::ate);Check(bool(file),"open readonly tiny Node corpus");
        const auto length=file.tellg();Check(length>0&&length<=512,"bounded Node corpus file size");
        Bytes bytes(static_cast<std::size_t>(length));file.seekg(0);Check(bool(file.read(reinterpret_cast<char*>(bytes.data()),length)),"read unchanged corpus bytes");
        spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"register corpus Node serializer");
        spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream source;Open(source,bytes);std::string error;
        auto* root=dynamic_cast<spNode*>(manager.LoadResourcesForAnalysis(source,context,&error));Check(root&&!context.failed,error.c_str());
        Check(Position(source)==bytes.size()-source.GetLogicalOriginForAnalysis(),"whole source corpus consumed");
        struct Row{const spNode* node;std::vector<std::size_t> children;};std::vector<Row> rows;
        std::function<std::size_t(const spNode*)> visit=[&](const spNode* node){
            Check(rows.size()<32,"tiny Node tree bound");for(const auto& row:rows)Check(row.node!=node,"acyclic corpus tree");
            const auto index=rows.size();rows.push_back({node,{}});
            for(std::size_t i=0;i<node->GetChildCountForAnalysis();++i){const auto childIndex=visit(node->GetChildForAnalysis(i));rows[index].children.push_back(childIndex);}
            return index;};
        visit(root);std::cout<<'[';
        for(std::size_t i=0;i<rows.size();++i)
        {
            const auto& row=rows[i];if(i)std::cout<<',';std::cout<<'[';
            if(row.node->GetName())std::cout<<'"'<<row.node->GetName()<<'"';else std::cout<<"null";
            std::cout<<','<<row.node->GetFlagsForAnalysis()<<",\""<<Hex(State(*row.node))<<"\",[";
            for(std::size_t j=0;j<row.children.size();++j){if(j)std::cout<<',';std::cout<<row.children[j];}
            std::cout<<"]]";
        }
        std::cout<<"]\n";
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string_view(argv[1])=="--asset"){Corpus(argv[2]);return 0;}
        const auto readers=Readers(),writers=Writers();ChildReads();const auto graph=Graph();BillboardInputs();Failures();
        if(argc==2&&std::string_view(argv[1])=="--capture")std::cout<<"{\"readers\":"<<readers<<",\"writers\":"<<writers<<",\"graph\":\""<<graph<<"\"}\n";
        else std::cout<<"PASS "<<checks<<'/'<<checks<<": Node sections, recursive references, graph ownership and whole FFPS\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
