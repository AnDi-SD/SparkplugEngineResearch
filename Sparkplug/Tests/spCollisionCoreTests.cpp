// Golden fields/states come from probe_pc_collision_core.py (pristine PC).
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spCollisionInfoSerializer.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/Sparkplug/spOBBBVSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    int checks=0;
    void Check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
    Bytes Unhex(const char* s)
    {
        const auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};Bytes bytes;
        for(;*s;s+=2)bytes.push_back(std::uint8_t(digit(s[0])*16+digit(s[1])));return bytes;
    }
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"memory stream allocation");
        if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart,0),"memory stream rewind");
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size=0;Check(stream.GetSize(&size),"stream size");
        const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};
    }
    void Fields()
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spCollisionInfo info;spCollisionInfoSerializer serializer;spMemoryStream stream;Open(stream);std::string error;
        Check(info.GetGroupForAnalysis()==1&&!info.GetPrimitiveForAnalysis(),"PC CollisionInfo defaults");
        Check(serializer.WritePayloadForAnalysis(stream,info,&error),error);
        Check(Data(stream)==Unhex("6101000000a2280000000000000000000000000000000000000000000000000000803f0000803f0000803f0000803f00"),"PC default writer includes group and transform");
        const auto input=Unhex("a10478563412a2280000e040000000410000104100000000000000000000003f0000003f00000040000040400000804000");
        Open(stream,input);Check(serializer.ReadPayloadForAnalysis(context,stream,std::uint32_t(input.size()),info,&error),error);
        Check(info.GetGroupForAnalysis()==0x12345678,"PC group remains UInt32");
        Check(info.GetOrientationForAnalysis()==spNode::Matrix3{.5f,.5f,0,-.5f,.5f,0,0,0,1},"PC nonunit quaternion stays unnormalized");
        Open(stream);Check(serializer.WritePayloadForAnalysis(stream,info,&error),error);
        Check(Data(stream)==Unhex("6178563412a2280000e040000000410000104100000000000000003acd933ed7b35d3f00000040000040400000804000"),"PC transform writer uses common matrix conversion");
        Check(!info.UpdateWorldForAnalysis(),"host rejects native null-primitive dereference");

        auto primitive=std::make_shared<spOBBBV>();spOBBBVSerializer obbSerializer;
        const auto obbInput=Unhex("a00c0000803f0000004000004040a10c000000400000804000008040a21000000000000000000000003f0000003f00");
        Open(stream,obbInput);Check(obbSerializer.ReadPayloadForAnalysis(context,stream,std::uint32_t(obbInput.size()),*primitive,&error),error);
        Check(primitive->GetBoundingCenterForAnalysis()==spNode::Vector3{1,2,3}&&primitive->GetBoundingRadiusForAnalysis()==3,"PC OBB reader source/mirror and derived radius");
        Open(stream);Check(obbSerializer.WritePayloadForAnalysis(stream,*primitive,&error),error);
        Check(Data(stream)==Unhex("a00c0000803f0000004000004040a10c000000400000804000008040a21000000000000000003acd933ed7b35d3f00"),"PC OBB wire output");
        auto copiedBase=primitive->Clone();auto* copied=dynamic_cast<spOBBBV*>(copiedBase.get());
        Check(copied&&copied->GetSizeForAnalysis()==spNode::Vector3{1,1,1}&&copied->GetBoundingRadiusForAnalysis()==0,"PC OBB clone keeps constructor geometry");
        info.SetPrimitiveForAnalysis(primitive);auto cloneBase=info.Clone();auto* clone=dynamic_cast<spCollisionInfo*>(cloneBase.get());
        Check(clone&&clone->GetPrimitiveForAnalysis()==primitive.get()&&clone->GetGroupForAnalysis()==info.GetGroupForAnalysis(),"PC CollisionInfo clone shares primitive and copies group");
        Check(clone->GetPositionForAnalysis()==spNode::Vector3{}&&clone->GetScaleForAnalysis()==spNode::Vector3{1,1,1},"PC CollisionInfo clone leaves default PRS");
    }
    void WorldAndLifetime()
    {
        auto primitive=std::make_shared<spOBBBV>();primitive->SetPositionForAnalysis({1,2,3});primitive->SetSizeForAnalysis({2,4,4});
        primitive->SetOrientationForAnalysis({.5f,.5f,0,-.5f,.5f,0,0,0,1});
        auto collision=std::make_shared<spCollisionInfo>();collision->SetPrimitiveForAnalysis(primitive);
        spNode node;node.SetPositionForAnalysis({7,8,9});node.SetScaleForAnalysis({2,3,4});
        node.SetOrientationForAnalysis({0,1,0,-1,0,0,0,0,1});node.MarkLocalTransformDirtyForAnalysis();
        Check(node.AttachCollisionForAnalysis(collision),"attach collision");
        Check(!node.AttachCollisionForAnalysis(collision),"host rejects duplicate direct ownership");
        Check(node.UpdateWorldForAnalysis(),"update represented Node collision transform");
        Check(collision->GetPositionForAnalysis()==spNode::Vector3{5,9,12},"PC OBB transforms CollisionInfo position without scale");
        Check(collision->GetOrientationForAnalysis()==spNode::Matrix3{-.5f,.5f,0,-.5f,-.5f,0,0,0,1},"PC OBB matrix order");
        Check(collision->GetBoundingCenterForAnalysis()==spNode::Vector3{5,9,12}&&collision->GetBoundingRadiusForAnalysis()==3,"PC collision sphere before OBB transform");
        Check(primitive->GetPositionForAnalysis()==spNode::Vector3{1,2,3},"world update preserves local primitive");
        auto graphClone=node.Clone();auto* clonedNode=dynamic_cast<spNode*>(graphClone.get());
        Check(clonedNode&&clonedNode->GetCollisionCountForAnalysis()==1,"Node deep clones collision owner");
        Check(clonedNode->GetCollisionForAnalysis(0)!=collision.get()&&clonedNode->GetCollisionForAnalysis(0)->GetPrimitiveForAnalysis()==primitive.get(),"Node clones CollisionInfo but shares primitive");
        Check(node.DetachCollisionForAnalysis(*collision)==collision&&!collision->GetNodeForAnalysis(),"reciprocal detach");
        Check(collision->UpdateWorldForAnalysis()&&collision->GetBoundingCenterForAnalysis()==spNode::Vector3{3.5f,8.5f,15},"PC unowned update retains PRS and recomputes sphere");
        {spNode owner;Check(owner.AttachCollisionForAnalysis(collision),"reattach");}
        Check(!collision->GetNodeForAnalysis(),"destroyed Node clears retained host collision back pointer");
    }
    void Graph(const char* output)
    {
        spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),255,3),"Node registration");
        Check(manager.RegisterForAnalysis(spCollisionInfo::ClassID,std::make_shared<spCollisionInfoSerializer>(),255,3),"Collision registration");
        Check(manager.RegisterForAnalysis(spOBBBV::ClassID,std::make_shared<spOBBBVSerializer>(),255,3),"OBB registration");
        manager.SetDispatchContextForAnalysis(2,2);
        spNode root;root.SetPositionForAnalysis({7,8,9});root.SetScaleForAnalysis({2,3,4});
        auto collision=std::make_shared<spCollisionInfo>();auto primitive=std::make_shared<spOBBBV>();
        primitive->SetPositionForAnalysis({1,2,3});primitive->SetSizeForAnalysis({2,4,4});collision->SetPrimitiveForAnalysis(primitive);
        Check(root.AttachCollisionForAnalysis(collision),"source graph collision");
        std::string error;Bytes file;
        Check(manager.BuildResourceFileForAnalysis(root,file,0,4096,&error),error);
        spMemoryStream stream;Open(stream,file);
        auto context=std::make_unique<spSerializerReadContextForAnalysis>(manager,resources);
        auto* loaded=dynamic_cast<spNode*>(manager.LoadResourcesForAnalysis(stream,*context,&error));
        Check(loaded&&!context->failed,error);
        Check(context->createdObjects.size()==3&&loaded->GetCollisionCountForAnalysis()==1,"whole common file loader retains all three objects");
        auto* info=loaded->GetCollisionForAnalysis(0);auto* obb=dynamic_cast<spOBBBV*>(info->GetPrimitiveForAnalysis());
        Check(obb&&obb->GetSizeForAnalysis()==spNode::Vector3{2,4,4},"typed OBB relationship");
        Check(info->GetNodeForAnalysis()==loaded&&info->GetPositionForAnalysis()==spNode::Vector3{8,10,12},"whole read/attach/final Node world update");
        auto retained=std::dynamic_pointer_cast<spNode>(context->ShareObjectForAnalysis(loaded));context.reset();
        Check(retained&&retained->GetCollisionForAnalysis(0)->GetPrimitiveForAnalysis()==obb,"all graph owners survive file context release");
        if(output){std::ofstream out(output,std::ios::binary);out.write(reinterpret_cast<const char*>(file.data()),file.size());Check(bool(out),"write cross-check specimen");}
    }
}
int main(int argc,char** argv)
{
    try{Fields();WorldAndLifetime();Graph(argc==2?argv[1]:nullptr);std::cout<<"PASS "<<checks<<" collision core checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
