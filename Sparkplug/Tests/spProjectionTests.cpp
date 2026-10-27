#include "Code/Sparkplug/spProjection.h"
#include "Code/Sparkplug/spBoxProjection.h"
#include "Code/SparkplugPC/spPCProjectionFX.h"
#include "Analysis/PC/spProjectionAbi.h"
#include <bit>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
    template<class T>std::string Numbers(const T& values)
    {
        std::ostringstream out;out<<'[';bool first=true;
        for(auto value:values){if(!first)out<<',';first=false;out<<+value;}
        return out.str()+']';
    }
    template<std::size_t N>std::array<std::uint32_t,N> Bits(const std::array<float,N>& values)
    {std::array<std::uint32_t,N> result;for(unsigned i=0;i<N;++i)result[i]=std::bit_cast<std::uint32_t>(values[i]);return result;}
    std::string State(const spBoxProjection& object)
    {
        const auto& s=object.GetProjectionStateForAnalysis();const auto* fx=object.GetPCFxForAnalysis();
        std::ostringstream out;out<<std::boolalpha<<"{\"classID\":"<<object.vfunc_18().classID
            <<",\"field28\":"<<object.GetField28ForAnalysis()<<",\"runtimeMode14\":"<<object.GetProjectionRuntimeModeForAnalysis()
            <<",\"alphaSort\":"<<unsigned(object.IsAlphaSortEnabledForAnalysis())<<",\"priority\":"<<object.GetPriorityForAnalysis()
            <<",\"materialNull\":"<<!object.GetMaterialForAnalysis()<<",\"fogNull\":"<<!object.GetFogForAnalysis()
            <<",\"flags58\":"<<Numbers(s.flags58)<<",\"values5C\":"<<Numbers(s.words5C)<<",\"word6C\":"<<s.word6C
            <<",\"sphere\":"<<Numbers(Bits(s.sphere70))<<",\"bound84Null\":"<<(object.GetBound84ForAnalysis()==nullptr)
            <<",\"list90Count\":"<<object.GetOpaqueRecordCountForAnalysis()<<",\"helperPresent\":"<<(fx!=nullptr)<<",\"helperInitialized\":";
        if(fx)out<<unsigned(fx->InitializedByteForAnalysis());else out<<"null";
        out<<",\"word98\":"<<s.word98<<",\"center\":"<<Numbers(Bits(object.GetCenterForAnalysis()))
            <<",\"size\":"<<Numbers(Bits(object.GetSizeForAnalysis()))<<'}';return out.str();
    }
    void Replay(const std::string& line)
    {
        std::istringstream input(line);std::string mode;std::uint32_t debug28;
        spBoxProjection::Vector3 center,size;input>>mode>>debug28;
        for(auto& v:center){std::uint32_t word;input>>word;v=std::bit_cast<float>(word);}
        for(auto& v:size){std::uint32_t word;input>>word;v=std::bit_cast<float>(word);}
        Check(bool(input),"Projection input contains mode, captured common constructor datum and six raw geometry words");
        spProjection::SetFactoryConstructorDataForAnalysis(spProjectionConstructorDataForAnalysis{debug28});
        auto owner=spRTTIManager::Instance().Create(spBoxProjection::ClassID);auto* object=dynamic_cast<spBoxProjection*>(owner.get());
        Check(object&&object->IsKindOf(spProjection::ClassID)&&object->IsKindOf(spRenderable::ClassID),"Real shared Box factory/physical base chain");
        const auto initial=State(*object);Check(object->SetGeometryForFixture(center,size),"Admitted integer geometry input");
        (void)object->GetBoundingSphereForAnalysis();spRenderable::BoundsPosition minimum,maximum;object->GetBoundsForAnalysis(minimum,maximum);
        std::string other="null",returned="null";bool cloneMap=false;spCloneManager manager;std::unique_ptr<spBaseObject> otherOwner;
        if(mode=="copy"||mode=="clone")
        {
            auto& state=object->ProjectionStateForFixture();state.flags58={0xA5,0x5A,0xFF};
            state.words5C={0x7FC12345,0x80000000,0x01234567,0xFFFFFFFF};state.word6C=0x13579BDF;state.word98=0x2468ACE0;
            object->SetField28ForAnalysis(0x12345678);
            if(mode=="copy")
            {
                auto target=std::make_unique<spBoxProjection>();Check(target->SetGeometryForFixture({100,200,300},{10,20,30}),"Copy destination existing geometry");
                (void)target->GetBoundingSphereForAnalysis();Check(object->vfunc_14(*target,manager),"Inherited actual Renderable Copy succeeds");
                returned="1";otherOwner=std::move(target);
            }
            else
            {otherOwner=object->vfunc_10(manager);cloneMap=manager.FindClone(*object)==otherOwner.get();Check(cloneMap,"Actual shared CloneManager pair");}
            auto* target=dynamic_cast<spBoxProjection*>(otherOwner.get());Check(target,"Typed destination");other=State(*target);
        }
        else if(mode=="fx")
        {
            Check(object->InitializePCForAnalysis()==spProjectionFxInitializationResultForAnalysis::Completed,"Real owned-PCFX no-material setup");returned="1";
        }
        std::cout<<std::boolalpha<<"{\"mode\":\""<<mode<<"\",\"center\":"<<Numbers(Bits(center))<<",\"size\":"<<Numbers(Bits(size))
            <<",\"initial\":"<<initial<<",\"after\":"<<State(*object)<<",\"bounds\":{\"minimum\":"<<Numbers(Bits(minimum))
            <<",\"maximum\":"<<Numbers(Bits(maximum))<<"},\"other\":"<<other<<",\"returned\":"<<returned<<",\"cloneMap\":"<<cloneMap<<'}';
        otherOwner.reset();owner.reset();spProjection::SetFactoryConstructorDataForAnalysis(std::nullopt);
    }
    void Guards()
    {
        static_assert(std::is_abstract_v<spProjection>);
        Check(!spProjection::StaticRTTI().factory,"Abstract Projection has no registry factory");
        (void)spBoxProjection::StaticRTTI();spProjection::SetFactoryConstructorDataForAnalysis(std::nullopt);
        bool refused=false;try{(void)spRTTIManager::Instance().Create(spBoxProjection::ClassID);}catch(const std::logic_error&){refused=true;}
        Check(refused,"Factory cold common-data acquisition is an explicit analytical refusal");
        spBoxProjection box(spProjectionConstructorDataForAnalysis{0x12345678});const auto before=box.GetCenterForAnalysis();
        Check(!box.SetGeometryForFixture({0.5f,0,0},{1,1,1})&&box.GetCenterForAnalysis()==before,"Unqualified fractional geometry refuses before mutation");
        Check(!box.SetGeometryForFixture({1025,0,0},{1,1,1})&&box.GetCenterForAnalysis()==before,"Bounded input refusal preserves existing geometry");
        spRenderable::BoundsPosition lo,hi;box.GetBoundsForAnalysis(lo,hi);Check(lo[0]==-0.5f&&hi[0]==0.5f,"Default box is centered with half unit extents");
        spBoxProjection alias(spProjectionConstructorDataForAnalysis{0});
        Check(alias.SetGeometryForFixture(alias.GetSizeForAnalysis(),alias.GetCenterForAnalysis())
            &&alias.GetCenterForAnalysis()==spBoxProjection::Vector3{1,1,1}
            &&alias.GetSizeForAnalysis()==spBoxProjection::Vector3{0,0,0},"Fixture setter snapshots aliased valid inputs before either store");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit")
        {std::string line;while(std::getline(std::cin,line))if(!line.empty()){Replay(line);std::cout<<'\n';}return 0;}
        Guards();std::cout<<"Projection/Box integer-geometry and explicit constructor-data guards PASS\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
