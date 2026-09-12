#include "Code/Sparkplug/spMeshDataSerializer.h"
#include "Code/Sparkplug/spMeshData.h"
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spPS2MeshDataSerializer.h"
#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string_view>
#include <sstream>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    template<class T> void Add(Bytes& bytes,T value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    void Word(Bytes& b,std::size_t at,std::uint32_t word)
    {Check(at+4<=b.size(),"bounded fixture patch");std::memcpy(b.data()+at,&word,4);}
    void Load(spMemoryStream& stream,const Bytes& b)
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(b.size())),"bounded fixture input");if(!b.empty())std::memcpy(stream.GetBuffer(),b.data(),b.size());}
    Bytes Data(spMemoryStream& stream)
    {std::uint32_t n=0;Check(stream.GetSize(&n),"fixture size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return Bytes(p,p+n);}
    Bytes Payload(bool native=true,bool packed=false)
    {
        Bytes b;
        if(native){for(auto v:{packed?0x1002u:0x112u,3u,packed?84u:96u,6u})Add(b,v);Add(b,std::uint8_t(0));}
        for(auto v:{2u,1u,0u})Add(b,v);
        for(auto v:{0,1,2})Add(b,std::uint16_t(v));
        for(auto v:{packed?0x20u:0x840u,3u,0u})Add(b,v);
        if(packed)for(int i=0;i<3;++i){for(int j=0;j<3;++j)Add(b,float(i+j));for(auto v:{1,2,3,255})Add(b,std::uint8_t(v));}
        else for(int i=0;i<24;++i)Add(b,float(i));
        return b;
    }
    Bytes Object(const Bytes& payload,bool native=true,bool unknown=false)
    {
        Bytes b;Add(b,0x33c34cf0u);Add(b,0x4f4f4253u);
        if(unknown){b.push_back(0x22);b.push_back(0xab);}
        Check(payload.size()<256,"tiny field envelope");b.push_back(native?0xa1:0xa0);b.push_back(std::uint8_t(payload.size()));
        b.insert(b.end(),payload.begin(),payload.end());b.push_back(0);return b;
    }
    template<class T> std::string Hex(const std::vector<T>& bytes)
    {
        constexpr char digits[]="0123456789abcdef";std::string result;
        for(auto value:bytes){const auto byte=static_cast<std::uint8_t>(value);result.push_back(digits[byte>>4]);result.push_back(digits[byte&15]);}
        return result;
    }
    Bytes Unhex(const std::string& text)
    {
        Check(text.size()%2==0&&text.size()<4096,"bounded even hex input");Bytes bytes;
        for(std::size_t i=0;i<text.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));
        return bytes;
    }
    Bytes WriteMesh(const Bytes& ib,const Bytes& vb,unsigned policy,bool base=false)
    {
        spMemoryStream i,v,out;Load(i,ib);Load(v,vb);Check(out.Open(nullptr),"writer output stream");
        spIndexBuffer indices;spVertexBuffer vertices;spMeshData mesh;
        Check(indices.ReadForAnalysis(i,static_cast<std::uint32_t>(ib.size())),"CPU writer indices");
        Check(vertices.ReadForAnalysis(v,static_cast<std::uint32_t>(vb.size())),"CPU writer vertices");
        Check(mesh.InitializeForAnalysis(indices,vertices),"CPU writer mesh owns deep copies");
        spSerializerManager manager;Check(manager.SetSerializationPolicyForAnalysis(policy),"bounded writer policy");
        spDXMeshDataSerializer dx;spMeshDataSerializer common;spSerializer& serializer=base?static_cast<spSerializer&>(common):dx;std::string error;
        Check(serializer.WritePayloadWithContextForAnalysis(manager,out,mesh,&error),error.c_str());
        const auto bytes=Data(out);
        if(base&&policy==1){Check(bytes==Bytes{0},"base native-only policy emits only section terminator");return bytes;}
        // Verify the new writer also feeds the existing production PC reader.
        spMemoryStream input;Load(input,bytes);spResourceManager resources;spPCRenderer renderer;
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;
        spDXMesh restored;Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),restored,&error),error.c_str());
        const auto header=spDXMeshDataSerializer::BuildNativePayloadHeaderForAnalysis(mesh);
        Check(restored.GetVertexByteSizeForAnalysis()==header.vertexDataSize&&restored.GetIndexByteSizeForAnalysis()==header.indexDataSize,"writer/reader native buffer extents");
        Check(Hex(restored.GetDXIndexBufferForAnalysis()->GetDataForAnalysis())==Hex(std::vector<std::uint8_t>(ib.begin()+12,ib.end())),"writer/reader exact index data");
        return bytes;
    }
    std::string GraphMesh(const Bytes& ib,const Bytes& vb,unsigned policy,bool base,const std::string& mode)
    {
        spMemoryStream i,v,out;Load(i,ib);Load(v,vb);Check(out.Open(nullptr),"graph output");
        spIndexBuffer indices;spVertexBuffer vertices;spMeshData mesh;
        Check(indices.ReadForAnalysis(i,static_cast<unsigned>(ib.size()))&&vertices.ReadForAnalysis(v,static_cast<unsigned>(vb.size()))&&mesh.InitializeForAnalysis(indices,vertices),"graph CPU mesh input");
        spSerializerManager manager;Check(manager.SetSerializationPolicyForAnalysis(policy),"graph policy");manager.SetDispatchContextForAnalysis(2,2);
        std::shared_ptr<spSerializer> serializer=base?std::static_pointer_cast<spSerializer>(std::make_shared<spMeshDataSerializer>()):std::make_shared<spDXMeshDataSerializer>();
        Check(manager.RegisterForAnalysis(spMeshData::ClassID,serializer,0xff,3),"explicit CPU MeshData exporter dispatch");
        Check(spSerializer::IndexReferenceForAnalysis(manager,&mesh),"whole generic mesh index");auto* fat=manager.GetFATForAnalysis();
        Check(fat->GetNextResourceIDForAnalysis()==2,"one graph ID; no CPU buffer references");
        std::string error;Check(spSerializer::WriteReferenceForAnalysis(manager,out,&mesh,&error),error.c_str());
        const auto* entry=fat->FindByObjectForAnalysis(mesh);Check(entry!=nullptr,"saved graph entry");
        const std::array<unsigned,5> metadata{entry->id,entry->classID,entry->offset,entry->size,entry->payloadWritten?1u:0u};
        Check(spSerializer::WriteReferenceForAnalysis(manager,out,&mesh,&error)&&spSerializer::WriteReferenceForAnalysis(manager,out,nullptr,&error),"generic repeat/null references");
        std::ostringstream result;result<<"[\"graph:"<<(base?"base:":"")<<mode<<"\","<<policy<<",\""<<Hex(ib)<<"\",\""<<Hex(vb)<<"\",\""<<Hex(Data(out))<<"\",[";
        for(unsigned n=0;n<5;++n){if(n)result<<',';result<<metadata[n];}result<<"]]";return result.str();
    }
    void Writers()
    {
        for(bool base:{false,true})for(bool packed:{false,true})for(unsigned policy:{0u,1u,2u})
        {
            const auto input=Payload(false,packed);
            const auto output=WriteMesh(Bytes(input.begin(),input.begin()+18),Bytes(input.begin()+18,input.end()),policy,base);
            Check(output.front()==(policy==1?(base?0:0xe1):0xe0)&&output.back()==0,"native fixed-u32 field reservation and terminator");
        }
        spDXMeshDataSerializer serializer;spMeshData empty;spMemoryStream stream;Check(stream.Open(nullptr),"empty guard stream");
        std::string error;Check(!serializer.WritePayloadForAnalysis(stream,empty,&error)&&!error.empty(),"uninitialized CPU writer guard");
        spSerializerManager manager;Check(manager.SetSerializationPolicyForAnalysis(1),"empty base policy fixture");
        spPS2MeshDataSerializer unverified;
        Check(!unverified.WritePayloadWithContextForAnalysis(manager,stream,empty,&error)&&!error.empty(),"unverified derived PS2 writer cannot inherit successful PC omission");
        std::uint32_t size=1;Check(stream.GetSize(&size)&&size==0,"derived rejection precedes output mutation");
        const auto packed=Payload(false,true);
        for(bool base:{false,true})for(unsigned policy:{0u,1u})
            (void)GraphMesh(Bytes(packed.begin(),packed.begin()+18),Bytes(packed.begin()+18,packed.end()),policy,base,"packed");
    }
    void CaptureMeshRead(const Bytes& bytes)
    {
        spMemoryStream input;Load(input,bytes);spSerializerManager manager;spResourceManager resources;spPCRenderer renderer;
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;
        spDXMesh mesh;spDXMeshDataSerializer serializer;std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),mesh,&error),error.c_str());
        std::cout<<"[["<<mesh.GetFVFCodeForAnalysis()<<','<<mesh.GetVertexStrideForAnalysis()<<','
            <<mesh.GetVertexByteSizeForAnalysis()<<','<<mesh.GetIndexByteSizeForAnalysis()<<','<<mesh.GetComponentWeightCountForAnalysis()
            <<"],\""<<Hex(mesh.GetDXVertexBufferForAnalysis()->GetDataForAnalysis())<<"\",\""
        <<Hex(mesh.GetDXIndexBufferForAnalysis()->GetDataForAnalysis())<<"\"]";
    }
    // Own bounded transport for comparing the shared class to original PC/PS2
    // layout calls. Each row is a sequence of reinitializations of one object.
    void VertexLayoutNativeRegressions()
    {
        // Complete original PC 0x45FEA0 and PS2 0x15C8E0 calls agree.
        spVertexBuffer buffer;
        Check(buffer.InitializeForAnalysis(0x2104, 1), "gapped native component mask");
        Check(buffer.GetComponentFlagsForAnalysis() == 0x2104
            && buffer.GetVertexStrideForAnalysis() == 16
            && buffer.GetComponentCountForAnalysis() == 4
            && buffer.GetComponentOffsetsForAnalysis()[9] == 3
            && buffer.GetComponentOffsetsForAnalysis()[3] == 0
            && buffer.GetComponentOffsetsForAnalysis()[14] == 0,
            "weight/UV gaps terminate groups; color remains at byte 12 and raw flags survive");
        Check(buffer.InitializeForAnalysis(0x940, 1)
            && buffer.InitializeForAnalysis(0x2408, 1)
            && buffer.GetVertexStrideForAnalysis() == 24
            && buffer.GetComponentOffsetsForAnalysis()[7] == 3
            && buffer.GetComponentOffsetsForAnalysis()[9] == 6
            && buffer.GetComponentOffsetsForAnalysis()[12] == 7
            && buffer.GetComponentOffsetsForAnalysis()[11] == 3,
            "reinitialization retains absent offsets and overwrites the present field");
        auto copy = buffer.CopyBufferForAnalysis();
        Check(copy && copy->GetDataForAnalysis() == buffer.GetDataForAnalysis()
            && copy->GetComponentOffsetsForAnalysis()[9] == 0,
            "copy creates a fresh layout; absent stale offsets are not vertex data");
    }
    void CaptureVertexLayouts()
    {
        unsigned sequences=0;Check(bool(std::cin>>sequences)&&sequences<=128,"bounded layout sequences");
        std::cout<<'[';
        for(unsigned s=0;s<sequences;++s)
        {
            unsigned count=0;Check(bool(std::cin>>count)&&count<=32,"bounded layout calls");
            spVertexBuffer buffer;if(s)std::cout<<',';std::cout<<'[';
            for(unsigned i=0;i<count;++i)
            {
                std::uint32_t mask=0;Check(bool(std::cin>>mask),"layout mask");
                Check(buffer.InitializeForAnalysis(mask,0),"shared layout initialization");
                if(i)std::cout<<',';
                std::cout<<'['<<buffer.GetVertexStrideForAnalysis()<<','<<buffer.GetComponentCountForAnalysis();
                for(auto offset:buffer.GetComponentOffsetsForAnalysis())std::cout<<','<<offset;
                std::cout<<']';
            }
            std::cout<<']';
        }
        std::cout<<"]\n";
    }
    void Readers(bool capture=false)
    {
        const char* names[]={"dx-native","dx-packed","base-cross","dx-cross","dx-unknown","dx-both"};
        if(capture)std::cout<<'[';
        for(int mode=0;mode<6;++mode)
        {
            spSerializerManager manager;spResourceManager resources;spPCRenderer renderer;
            manager.SetDispatchContextForAnalysis(mode==3?1:2,1);
            spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;
            const bool native=mode!=2&&mode!=3,packed=mode==1;
            auto payload=Payload(native,packed);
            if(native){Word(payload,0,0xdeadbeef);Word(payload,4,999);Word(payload,8,777);Word(payload,12,555);payload[16]=0xa5;}
            auto input=Object(payload,native,mode==4);
            if(mode==4)input[9]='X';
            if(mode==5){const auto cross=Payload(false,false);Bytes prefix{0xa0,static_cast<std::uint8_t>(cross.size())};prefix.insert(prefix.end(),cross.begin(),cross.end());input.insert(input.begin()+8,prefix.begin(),prefix.end());}
            // Original specialised header ignores ID/marker, always DXMesh.
            Word(input,0,0x12345678);Word(input,4,0xdeadbeef);
            spMemoryStream stream;Load(stream,input);
            spMeshDataSerializer base;spDXMeshDataSerializer dx;spSerializer& serializer=mode==2?static_cast<spSerializer&>(base):dx;
            auto object=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);
            auto* mesh=dynamic_cast<spDXMesh*>(object.get());Check(mesh!=nullptr,"specialized header creates DXMesh, not wire RTTI class");
            std::string error;Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(input.size()-8),*mesh,&error),error.c_str());
            Check(mesh->GetVertexByteSizeForAnalysis()==(packed?84u:96u)&&mesh->GetIndexByteSizeForAnalysis()==6,"exact expanded vertex and index byte sizes");
            Check(mesh->GetVertexStrideForAnalysis()==(packed?28u:32u)&&mesh->GetFVFCodeForAnalysis()==(packed?0x1002u:0x112u),"converted format/stride");
            Check(mesh->GetVertexDeclarationForAnalysis()==renderer.GetVertexDeclarationForAnalysis(packed?0x20:0x840),"reader uses common renderer declaration cache");
            Check(mesh->GetDXIndexBufferForAnalysis()->GetDataForAnalysis().size()==6,"index buffer storage");
            const auto& data=mesh->GetDXVertexBufferForAnalysis()->GetDataForAnalysis();
            std::vector<float> wanted;
            if(packed)for(int i=0;i<3;++i){for(int j=0;j<3;++j)wanted.push_back(float(i+j));for(float v:{1.f,2.f,3.f,255.f})wanted.push_back(v);}
            else for(int i=0;i<24;++i)wanted.push_back(float(i));
            Check(data.size()==wanted.size()*4&&std::memcmp(data.data(),wanted.data(),data.size())==0,"exact CPU output/unnormalized packed expansion");
            std::uint32_t end=0;Check(stream.GetCurrentPosition(end)&&end==input.size(),"known and unknown fields consumed exactly");
            if(capture)
            {
                if(mode)std::cout<<',';
                std::cout<<"[\""<<names[mode]<<"\",\""<<Hex(input)<<"\","<<end<<','
                    <<mesh->GetFVFCodeForAnalysis()<<','<<mesh->GetVertexStrideForAnalysis()<<','
                    <<mesh->GetVertexByteSizeForAnalysis()<<','<<mesh->GetIndexByteSizeForAnalysis()<<','
                    <<mesh->GetComponentWeightCountForAnalysis()<<",\""<<Hex(data)<<"\",\""
                    <<Hex(mesh->GetDXIndexBufferForAnalysis()->GetDataForAnalysis())<<"\"]";
            }
        }
        if(capture)std::cout<<"]\n";
    }
    Bytes WholeFile()
    {
        // TEST FFPS envelope, not a claimed reconstruction of whole Save.
        spAnimation animation;spMemoryStream fields;Check(fields.Open(nullptr),"animation writer stream");
        Check(spAnimationSerializer{}.WriteFieldsForAnalysis(fields,animation),"existing native-compatible animation writer");
        Bytes root;Add(root,spAnimation::ClassID);Add(root,0x4f4f4253u);auto data=Data(fields);root.insert(root.end(),data.begin(),data.end());
        const auto mesh=Object(Payload());const std::uint32_t origin=36+18*3;
        const std::uint32_t total=static_cast<std::uint32_t>(root.size()+mesh.size()*2);
        Bytes b;for(auto v:{0x53504646u,0x26u,0u,origin+total,2u,origin,total})Add(b,v);
        Add(b,3u);std::uint32_t offset=0;
        for(std::uint32_t i=0;i<3;++i)
        {
            Add(b,i+1);Add(b,std::uint16_t(0));Add(b,i?spMeshDataSerializer::TargetClassID:spAnimation::ClassID);
            Add(b,offset);const auto size=static_cast<std::uint32_t>(i?mesh.size():root.size());Add(b,size);offset+=size;
        }
        Add(b,0u);b.insert(b.end(),root.begin(),root.end());b.insert(b.end(),mesh.begin(),mesh.end());b.insert(b.end(),mesh.begin(),mesh.end());return b;
    }
    void WholeLoad()
    {
        const auto bytes=WholeFile();spSerializerManager manager;spResourceManager resources;spPCRenderer renderer;
        Check(manager.RegisterForAnalysis(spAnimation::ClassID,std::make_shared<spAnimationSerializer>(),0xff,3),"register root reader");
        Check(manager.RegisterForAnalysis(spMeshDataSerializer::TargetClassID,std::make_shared<spDXMeshDataSerializer>(),2,1),"register concrete PC mesh reader");
        auto first=std::make_unique<spSerializerReadContextForAnalysis>(manager,resources);
        auto second=std::make_unique<spSerializerReadContextForAnalysis>(manager,resources);
        first->pcRenderer=second->pcRenderer=&renderer;
        Check(spRTTIManager::Instance().Find(spAnimation::ClassID)
            &&spRTTIManager::Instance().Find(spMeshData::ClassID),"root and mesh RTTI available independent of TU startup order");
        spMemoryStream a,b;Load(a,bytes);Load(b,bytes);std::string error;
        auto* root=manager.LoadResourcesForAnalysis(a,*first,&error);
        Check(dynamic_cast<spAnimation*>(root)&&!first->failed,error.empty()?"whole FFPS with root and PC mesh batch":error.c_str());
        Check(first->createdObjects.size()==3&&first->createdObjects.back().get()==root,"hook-created meshes do not consume outer root selection");
        auto* one=dynamic_cast<spDXMesh*>(first->createdObjects[0].get());auto* two=dynamic_cast<spDXMesh*>(first->createdObjects[1].get());
        Check(one&&two&&one!=two&&one->GetDXVertexBufferForAnalysis()==two->GetDXVertexBufferForAnalysis(),"whole loader produces two mesh ranges in one buffer");
        Check(two->GetVertexBeginForAnalysis()==3&&two->GetIndexBeginForAnalysis()==3,"native second mesh range");
        Check(first->activeMeshCombiner==nullptr&&first->depth==0&&manager.GetFATForAnalysis()->GetResourceCountForAnalysis()==0,"FAT/scoped combiner cleared while resources survive");
        Check(renderer.GetVertexDeclarationCountForAnalysis()==1,"common declaration cache across meshes");
        auto* secondRoot=manager.LoadResourcesForAnalysis(b,*second,&error);
        Check(secondRoot&&secondRoot!=root&&second->createdObjects.size()==3&&!second->failed,"second complete file while first stays alive");
        auto* surviving=dynamic_cast<spDXMesh*>(second->createdObjects[0].get());
        Check(surviving&&surviving->GetVertexDeclarationForAnalysis()==one->GetVertexDeclarationForAnalysis(),"two files share renderer format, not owned geometry");
        first.reset();Check(surviving->GetDXVertexBufferForAnalysis()->GetDataForAnalysis().size()==192,"second file survives first context teardown");
        spMemoryStream denied;Load(denied,bytes);spSerializerReadContextForAnalysis noRenderer(manager,resources);
        Check(!manager.LoadResourcesForAnalysis(denied,noRenderer,&error)&&noRenderer.failed&&!error.empty(),"missing explicit renderer rejects whole mesh batch");
    }
    void Failures()
    {
        for(int mode=0;mode<7;++mode)
        {
            auto payload=Payload();if(mode==0)Word(payload,21,0xffffffff);if(mode==1)Word(payload,39,0xffffffff);
            auto bytes=Object(payload);if(mode==2)bytes.pop_back();if(mode==3)bytes.push_back(0xff);
            if(mode==4)bytes[8]=0xa0;if(mode==5)bytes[9]=0xff;
            spSerializerManager manager;spResourceManager resources;spPCRenderer renderer;
            manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;
            spMemoryStream input;Load(input,bytes);spDXMeshDataSerializer serializer;auto object=serializer.ReadObjectHeaderAndCreateForAnalysis(input);
            std::string error;auto size=mode==6?0xffffffffu:static_cast<std::uint32_t>(bytes.size()-8);
            Check(!serializer.ReadPayloadForAnalysis(context,input,size,*object,&error)&&!error.empty(),"bounded malformed payload rejected without huge allocation");
        }
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string_view(argv[1])=="--vertex-layouts"){CaptureVertexLayouts();return 0;}
        if(argc==2&&std::string_view(argv[1])=="--read-payload")
        {std::string bytes;Check(bool(std::cin>>bytes)&&bytes.size()<4096,"bounded mesh payload");CaptureMeshRead(Unhex(bytes));std::cout<<'\n';return 0;}
        if(argc==5&&std::string_view(argv[1])=="--write-graph")
        {std::string ib,vb;std::cin>>ib>>vb;std::cout<<GraphMesh(Unhex(ib),Unhex(vb),std::stoul(argv[3]),std::string_view(argv[4])=="base",argv[2])<<'\n';return 0;}
        if(argc==4&&(std::string_view(argv[1])=="--write"||std::string_view(argv[1])=="--write-roundtrip"||std::string_view(argv[1])=="--write-base"))
        {std::string ib,vb;std::cin>>ib>>vb;const auto policy=std::stoul(argv[3]);const bool base=std::string_view(argv[1])=="--write-base";const auto output=WriteMesh(Unhex(ib),Unhex(vb),policy,base);std::cout<<"[\""<<(base?"base:":"")<<argv[2]<<"\","<<policy<<",\""<<ib<<"\",\""<<vb<<"\",\""<<Hex(output)<<'"';
            if(std::string_view(argv[1])=="--write-roundtrip"){std::cout<<',';CaptureMeshRead(output);}std::cout<<"]\n";return 0;}
        if(argc==2&&std::string_view(argv[1])=="--capture"){Readers(true);return 0;}VertexLayoutNativeRegressions();Readers();Writers();WholeLoad();Failures();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC mesh reader/writer and whole composed FFPS\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
