#include "Code/Sparkplug/spCapsuleBV.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spNode.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    using Capsule=spCapsuleBV;
    int checks=0;
    void Check(bool condition,const char* message)
    {++checks;if(!condition)throw std::runtime_error(message);}
    std::uint32_t Bits(float value)
    {std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    float ReadFloat(std::istream& input)
    {
        std::uint32_t bits;
        if(!(input>>bits))throw std::runtime_error("Missing float bits");
        float value;std::memcpy(&value,&bits,4);return value;
    }
    template<std::size_t Size>bool SameBits(const std::array<float,Size>& first,const std::array<float,Size>& second)
    {for(std::size_t i=0;i<Size;++i)if(Bits(first[i])!=Bits(second[i]))return false;return true;}
    bool SameState(const Capsule::StateForAnalysis& first,const Capsule::StateForAnalysis& second)
    {
        return Bits(first.length)==Bits(second.length)&&Bits(first.radius)==Bits(second.radius)&&
            SameBits(first.orientation,second.orientation)&&SameBits(first.first,second.first)&&
            SameBits(first.second,second.second)&&SameBits(first.position,second.position);
    }
    void MainChecks()
    {
        Capsule capsule;
        const Capsule::StateForAnalysis defaults;
        Check(SameState(capsule.GetStateForAnalysis(),defaults),"PC and PS2 authored shape and cached segment defaults");
        auto created=Capsule::StaticRTTI().factory();auto* factoryCapsule=dynamic_cast<Capsule*>(created.get());
        Check(factoryCapsule&&SameState(factoryCapsule->GetStateForAnalysis(),defaults),"registered factory returns the complete authored default state");
        Check(capsule.IsKindOf(Capsule::ClassID)&&capsule.IsKindOf(spBoundingVolume::ClassID)&&
            capsule.IsKindOf(spBaseObject::ClassID),"native registered Capsule/BV/Base RTTI graph");
        auto state=defaults;state.length=2;state.radius=-7;state.position={1,2,3};
        capsule.SetStateForAnalysis(state);
        Capsule::Vector3 position{10,20,30};
        Capsule::Matrix3 orientation{1,0,0,0,1,0,0,0,1};
        const Capsule::Vector3 scale{2,3,4};
        const Capsule& view=capsule;
        Check(view.TryUpdateCollisionTransformForAnalysis(position,orientation,scale),"finite transform accepted");
        auto after=capsule.GetStateForAnalysis();
        Check(position==Capsule::Vector3{12,26,42}&&after.first==Capsule::Vector3{12,14,42}&&
            after.second==Capsule::Vector3{12,38,42},"local scaled center and scaled signed-max length update caches");
        Check(after.orientation==state.orientation&&after.position==state.position&&after.length==2&&after.radius==-7&&
            capsule.GetBoundingCenterForAnalysis()==Capsule::Vector3{}&&capsule.GetBoundingRadiusForAnalysis()==0,
            "transform preserves authored shape, radius and inherited bounds");
        capsule.SetStateForAnalysis(defaults);position={};orientation=defaults.orientation;
        capsule.UpdateCollisionTransformForAnalysis(position,orientation,{-2,-3,-4});
        after=capsule.GetStateForAnalysis();
        Check(after.first==Capsule::Vector3{0,-3,0}&&after.second==Capsule::Vector3{0,3,0},
            "maximum scale is signed, not maximum absolute magnitude");
        state=defaults;state.radius=std::numeric_limits<float>::quiet_NaN();
        capsule.SetStateForAnalysis(state);position={};orientation=defaults.orientation;
        Check(capsule.TryUpdateCollisionTransformForAnalysis(position,orientation,{1,1,1})&&
            capsule.GetStateForAnalysis().first==defaults.first,"radius is unread even when nonfinite");
        state=defaults;state.length=17;state.radius=19;state.orientation={2,3,5,7,11,13,17,19,23};
        state.position={29,31,37};state.first={41,43,47};state.second={53,59,61};
        capsule.SetStateForAnalysis(state);capsule.SetName("authored capsule");
        spCloneManager manager;
        auto clone=manager.Clone(capsule);auto* typed=dynamic_cast<Capsule*>(clone.get());
        Check(typed&&SameState(typed->GetStateForAnalysis(),defaults)&&
            std::string(typed->GetName())=="authored capsule","Clone copies physical name and resets shape and cache");
        Check(SameState(capsule.GetStateForAnalysis(),state)&&manager.FindClone(capsule)==nullptr,
            "completed clone preserves source and releases root mapping");
        Capsule target;auto destinationState=defaults;destinationState.length=-2;destinationState.radius=5;
        destinationState.position={7,11,13};destinationState.first={17,19,23};destinationState.second={29,31,37};
        target.SetStateForAnalysis(destinationState);target.SetName("destination");
        Check(capsule.vfunc_14(target,manager)&&SameState(target.GetStateForAnalysis(),destinationState)&&
            std::string(target.GetName())=="authored capsule","inherited copy preserves destination geometry and cache");
        position={3,5,7};orientation=defaults.orientation;
        const auto oldPosition=position;const auto oldOrientation=orientation;const auto oldState=capsule.GetStateForAnalysis();
        Check(!capsule.TryUpdateCollisionTransformForAnalysis(position,orientation,
            {1,std::numeric_limits<float>::infinity(),1})&&position==oldPosition&&orientation==oldOrientation&&
            SameState(capsule.GetStateForAnalysis(),oldState),"explicit finite qualification preserves complete operation on unsupported input");
        state=defaults;state.length=std::numeric_limits<float>::max();
        state.first={-0.0F,0,-0.0F};state.second={0,-0.0F,0};capsule.SetStateForAnalysis(state);
        position={-0.0F,0,-0.0F};orientation=defaults.orientation;const auto zeroPosition=position;
        Check(!capsule.TryUpdateCollisionTransformForAnalysis(position,orientation,{2,2,2})&&
            SameBits(position,zeroPosition)&&SameBits(orientation,defaults.orientation)&&SameState(capsule.GetStateForAnalysis(),state),
            "finite overflow refusal preserves signed-zero bits and complete cached state");
        state=defaults;state.length=2;state.position={1,2,3};capsule.SetStateForAnalysis(state);
        position={2,3,4};orientation=defaults.orientation;
        Check(capsule.TryUpdateCollisionTransformForAnalysis(position,orientation,position)&&
            position==Capsule::Vector3{4,9,16}&&capsule.GetStateForAnalysis().first==Capsule::Vector3{4,-135,16}&&
            capsule.GetStateForAnalysis().second==Capsule::Vector3{4,153,16},
            "position-scale alias observes published translated position before the segment rereads scale");
        clone.reset();
        auto another=manager.Clone(capsule);
        Check(another&&another->vfunc_18().classID==Capsule::ClassID,"clone lifetime remains reusable after disposal");
    }
    void Case(const std::string& line)
    {
        std::istringstream input(line);Capsule capsule;auto state=capsule.GetStateForAnalysis();
        state.length=ReadFloat(input);state.radius=ReadFloat(input);
        for(auto& value:state.orientation)value=ReadFloat(input);
        for(auto& value:state.position)value=ReadFloat(input);
        Capsule::Vector3 position;Capsule::Matrix3 orientation;Capsule::Vector3 scale;
        for(auto& value:position)value=ReadFloat(input);
        for(auto& value:orientation)value=ReadFloat(input);
        for(auto& value:scale)value=ReadFloat(input);
        unsigned aliasScale;
        if(!(input>>aliasScale)||aliasScale>1)throw std::runtime_error("Invalid or missing alias selector");
        std::string extra;if(input>>extra)throw std::runtime_error("Trailing case fields");
        capsule.SetStateForAnalysis(state);
        if(!capsule.TryUpdateCollisionTransformForAnalysis(position,orientation,aliasScale?position:scale))
            throw std::runtime_error("Unsupported native comparison input");
        state=capsule.GetStateForAnalysis();
        bool first=true;std::cout<<'[';
        auto write=[&](const auto& values){for(float value:values){if(!first)std::cout<<',';first=false;std::cout<<Bits(value);}};
        write(position);write(orientation);write(state.first);write(state.second);write(aliasScale?position:scale);
        std::cout<<"]\n";
    }
    void CollisionOperation()
    {
        auto capsule=std::make_shared<Capsule>();auto state=capsule->GetStateForAnalysis();
        state.length=2;state.position={1,2,3};capsule->SetStateForAnalysis(state);
        auto collision=std::make_shared<spCollisionInfo>();collision->SetPrimitiveForAnalysis(capsule);
        spNode node;node.SetPositionForAnalysis({10,20,30});node.SetScaleForAnalysis({2,3,4});
        node.MarkLocalTransformDirtyForAnalysis();
        Check(node.AttachCollisionForAnalysis(collision)&&node.UpdateWorldForAnalysis(),
            "Node owns CollisionInfo and completes Capsule transform");
        Check(collision->GetNodeForAnalysis()==&node&&collision->GetPositionForAnalysis()==Capsule::Vector3{12,26,42}&&
            collision->GetScaleForAnalysis()==Capsule::Vector3{2,3,4}&&
            collision->GetBoundingCenterForAnalysis()==Capsule::Vector3{10,20,30}&&
            capsule->GetStateForAnalysis().first==Capsule::Vector3{12,14,42}&&
            capsule->GetStateForAnalysis().second==Capsule::Vector3{12,38,42},
            "whole collision operation retains original sphere-before-shape ordering");
        const auto position=collision->GetPositionForAnalysis(),scale=collision->GetScaleForAnalysis();
        const auto orientation=collision->GetOrientationForAnalysis();
        const auto center=collision->GetBoundingCenterForAnalysis();
        const float radius=collision->GetBoundingRadiusForAnalysis();const auto cache=capsule->GetStateForAnalysis();
        node.SetScaleForAnalysis({2,std::numeric_limits<float>::infinity(),4});node.MarkLocalTransformDirtyForAnalysis();
        Check(!node.UpdateWorldForAnalysis(),"Node propagates unsupported Capsule transform as failure");
        Check(collision->GetPositionForAnalysis()==position&&collision->GetOrientationForAnalysis()==orientation&&
            collision->GetScaleForAnalysis()==scale&&collision->GetBoundingCenterForAnalysis()==center&&
            collision->GetBoundingRadiusForAnalysis()==radius&&SameState(capsule->GetStateForAnalysis(),cache),
            "failed collision operation preserves CollisionInfo and endpoint caches");
        node.SetScaleForAnalysis({1,1,1});node.MarkLocalTransformDirtyForAnalysis();
        Check(node.UpdateWorldForAnalysis()&&capsule->GetStateForAnalysis().first==Capsule::Vector3{11,21,33},
            "a fresh supported operation recovers after finite rejection");
        auto graph=node.Clone();auto* cloned=dynamic_cast<spNode*>(graph.get());
        Check(cloned&&cloned->GetCollisionCountForAnalysis()==1&&
            cloned->GetCollisionForAnalysis(0)->GetPrimitiveForAnalysis()==capsule.get(),
            "Node graph clone retains original shared primitive ownership");
        Check(node.DetachCollisionForAnalysis(*collision)==collision&&!collision->GetNodeForAnalysis(),
            "completed collision owner lifetime clears reciprocal Node pointer");
    }
    // Test-only analytical primitive observes the successful virtual-call
    // boundary; it does not stand in for an original engine class.
    class PublicationObserver final : public spBoundingVolume
    {
    public:
        const spCollisionInfo* info=nullptr;
        mutable bool observed=false;
        PublicationObserver(){boundingCenter_={1,2,3};boundingRadius_=7;}
        void UpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept override
        {(void)TryUpdateCollisionTransformForAnalysis(position,orientation,scale);}
        bool TryUpdateCollisionTransformForAnalysis(Vector3& position,Matrix3& orientation,
            const Vector3& scale) const noexcept override
        {
            observed=info && &position==&info->GetPositionForAnalysis()&&
                &orientation==&info->GetOrientationForAnalysis() && &scale==&info->GetScaleForAnalysis()&&
                position==Vector3{10,20,30}&&scale==Vector3{2,3,4}&&
                info->GetBoundingCenterForAnalysis()==Vector3{11,22,33}&&info->GetBoundingRadiusForAnalysis()==7;
            position[0]+=5;return true;
        }
    };
    void PublicationOrder()
    {
        auto observer=std::make_shared<PublicationObserver>();
        auto collision=std::make_shared<spCollisionInfo>();observer->info=collision.get();
        collision->SetPrimitiveForAnalysis(observer);
        spNode node;Check(node.AttachCollisionForAnalysis(collision),"attach publication-order fixture");
        node.SetPositionForAnalysis({10,20,30});node.SetScaleForAnalysis({2,3,4});node.MarkLocalTransformDirtyForAnalysis();
        Check(node.UpdateWorldForAnalysis()&&observer->observed&&collision->GetPositionForAnalysis()==Capsule::Vector3{15,20,30},
            "successful virtual call sees published info PRS/bounds and original member references");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--cases")
        {std::string line;while(std::getline(std::cin,line))if(!line.empty())Case(line);return 0;}
        MainChecks();CollisionOperation();PublicationOrder();std::cout<<"PASS "<<checks<<" Capsule checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
