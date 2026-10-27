#include "Code/Sparkplug/spPyramidProjection.h"
#include "Code/SparkplugPC/spPCProjectionFX.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
    std::uint32_t Word(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
    float Float(std::uint32_t word){float value;std::memcpy(&value,&word,4);return value;}
    template<class Values> void Words(std::ostream& out,const Values& values)
    {out<<'[';bool comma=false;for(float value:values){if(comma)out<<',';comma=true;out<<Word(value);}out<<']';}
    std::string Own(const spPyramidProjection& object)
    {
        const auto& s=object.GetProjectionStateForAnalysis();const auto& p=object.GetPyramidStateForAnalysis();std::ostringstream out;out<<std::boolalpha;
        out<<"{\"flags58\":["<<unsigned(s.flags58[0])<<','<<unsigned(s.flags58[1])<<','<<unsigned(s.flags58[2])<<"],\"words5C\":[";
        for(unsigned i=0;i<4;++i){if(i)out<<',';out<<s.words5C[i];}
        out<<"],\"word6C\":"<<s.word6C<<",\"sphere\":";Words(out,s.sphere70);
        out<<",\"bound84Null\":"<<(object.GetBound84ForAnalysis()==nullptr)<<",\"recordCount\":"<<object.GetOpaqueRecordCountForAnalysis()
            <<",\"helperPresent\":"<<(object.GetPCFxForAnalysis()!=nullptr)<<",\"helperInitialized\":";
        if(const auto* fx=object.GetPCFxForAnalysis())out<<unsigned(fx->InitializedByteForAnalysis());else out<<"null";
        out<<",\"word98\":"<<s.word98<<",\"flip9C\":"<<unsigned(p.flip9C)<<",\"center\":";Words(out,object.GetCenterForAnalysis());
        out<<",\"size\":";Words(out,object.GetSizeForAnalysis());out<<",\"wordB8\":"<<p.wordB8<<",\"byteCC\":"<<unsigned(p.byteCC)<<'}';return out.str();
    }
    std::string State(const spPyramidProjection& object)
    {
        std::ostringstream out;out<<std::boolalpha<<"{\"classID\":"<<object.vfunc_18().classID<<",\"field28\":"<<object.GetField28ForAnalysis()
            <<",\"runtimeMode14\":"<<object.GetProjectionRuntimeModeForAnalysis()<<",\"alphaSort\":"<<(object.IsAlphaSortEnabledForAnalysis()?1:0)
            <<",\"priority\":"<<object.GetPriorityForAnalysis()<<",\"materialNull\":"<<!object.GetMaterialForAnalysis()<<",\"fogNull\":"<<!object.GetFogForAnalysis();
        auto own=Own(object);out<<','<<own.substr(1);return out.str();
    }
    void Replay(const std::string& mode,const spPyramidProjection::Vector3& center,const spPyramidProjection::Vector3& size,unsigned flip)
    {
        Check(flip<256,"Raw flip byte input");spProjection::SetFactoryConstructorDataForAnalysis(spProjectionConstructorDataForAnalysis{0xFF000000});
        auto object=std::make_unique<spPyramidProjection>();const auto initial=State(*object);
        Check(object->IsExactly(spPyramidProjection::ClassID)&&object->IsKindOf(spProjection::ClassID)&&object->IsKindOf(spRenderable::ClassID),"Typed Pyramid physical source family");
        Check(object->SetGeometryForFixture(center,size),"Admitted Pyramid input domain");object->PyramidStateForFixture().flip9C=static_cast<std::uint8_t>(flip);
        if(mode=="opaque-own"){object->PyramidStateForFixture().wordB8=0x7FC12345;object->PyramidStateForFixture().byteCC=0xA5;}
        const auto before=State(*object);spRenderable::BoundsPosition minimum{},maximum{};object->GetBoundsForAnalysis(minimum,maximum);
        Check(State(*object)==before,"Bounds do not update sphere or other fields");
        std::ostringstream bounds;bounds<<"{\"minimum\":";Words(bounds,minimum);bounds<<",\"maximum\":";Words(bounds,maximum);bounds<<'}';
        (void)object->GetBoundingSphereForAnalysis();const auto afterGeometry=State(*object);
        auto expected=before;auto observed=afterGeometry;
        auto withoutSphere=[](std::string value){const auto first=value.find("\"sphere\":");const auto last=value.find(']',first);Check(first!=std::string::npos&&last!=std::string::npos,"Snapshot sphere extent");value.erase(first,last-first+1);return value;};
        Check(withoutSphere(expected)==withoutSphere(observed),"Sphere changes only its cache");
        std::unique_ptr<spPyramidProjection> destination;bool cloneMap=false,returned=false,hasReturn=false,copyPreserved=false;
        if(mode=="copy"||mode=="clone")
        {
            auto& shared=object->ProjectionStateForFixture();shared.flags58={0xA5,0x5A,0xFF};shared.words5C={0x7FC12345,0x80000000,0x01234567,0xFFFFFFFF};
            shared.word6C=0x13579BDF;shared.word98=0x2468ACE0;object->SetField28ForAnalysis(0x12345678);object->SetPriorityForAnalysis(0x22334455);
            object->PyramidStateForFixture().wordB8=0x7FC12345;object->PyramidStateForFixture().byteCC=0x5A;
            spCloneManager manager;
            if(mode=="copy")
            {
                destination=std::make_unique<spPyramidProjection>();Check(destination->SetGeometryForFixture({100,200,300},{10,2,30}),"Destination geometry");
                auto& own=destination->PyramidStateForFixture();own.flip9C=255;own.wordB8=0x12345678;own.byteCC=0x99;(void)destination->GetBoundingSphereForAnalysis();
                const auto ownBefore=Own(*destination);returned=object->vfunc_14(*destination,manager);hasReturn=true;copyPreserved=Own(*destination)==ownBefore;Check(returned&&copyPreserved,"Inherited Copy preserves own Projection/Pyramid fields");
            }
            else
            {
                auto clone=object->vfunc_10(manager);auto* typed=dynamic_cast<spPyramidProjection*>(clone.get());Check(typed&&manager.FindClone(*object)==typed,"Real shared CloneManager pair");
                destination.reset(static_cast<spPyramidProjection*>(clone.release()));cloneMap=true;
            }
        }
        else if(mode=="fx")
        {returned=object->InitializePCForAnalysis()==spProjectionFxInitializationResultForAnalysis::Completed;hasReturn=true;Check(returned&&object->GetPCFxForAnalysis(),"Actual shared owned no-material FX initialization");}
        std::string other=destination?State(*destination):"null";const auto after=State(*object);
        destination.reset();object.reset();spProjection::SetFactoryConstructorDataForAnalysis(std::nullopt);
        std::cout<<std::boolalpha<<"{\"mode\":\""<<mode<<"\",\"initial\":"<<initial<<",\"afterGeometry\":"<<afterGeometry<<",\"after\":"<<after<<",\"bounds\":"<<bounds.str()
            <<",\"other\":"<<other<<",\"returned\":";
        if(hasReturn)std::cout<<(returned?1:0);else std::cout<<"null";
        std::cout<<",\"cloneMap\":"<<cloneMap<<",\"copyOwnPreserved\":";if(mode=="copy")std::cout<<copyPreserved;else std::cout<<"null";
        std::cout<<",\"boundsDoNotChangeObject\":true,\"sphereChangesOnlyCache\":true,\"emptyOwnedLifecycleCompleted\":true}\n";
    }
    void Guards()
    {
        spProjection::SetFactoryConstructorDataForAnalysis(std::nullopt);bool refused=false;try{spPyramidProjection value;}catch(const std::logic_error&){refused=true;}Check(refused,"Factory requires captured constructor data");
        spProjection::SetFactoryConstructorDataForAnalysis(spProjectionConstructorDataForAnalysis{0xFF000000});
        auto factory=spRTTIManager::Instance().Create(spPyramidProjection::ClassID);
        Check(factory&&dynamic_cast<spPyramidProjection*>(factory.get())&&factory->IsKindOf(spProjection::ClassID),"Typed reflective Pyramid factory");
        factory.reset();spProjection::SetFactoryConstructorDataForAnalysis(std::nullopt);
        spPyramidProjection value(spProjectionConstructorDataForAnalysis{0xFF000000});
        Check(value.SetGeometryForFixture({-0.,0.,-0.},{-0.,-2.,0.}),"Signed zero and negative power-of-two height");
        const auto before=State(value);
        for(float height:{0.0f,-0.0f,3.0f,0.5f,2048.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            Check(!value.SetGeometryForFixture({1,2,3},{4,height,8})&&State(value)==before,"Unsafe/nonadmitted height refuses atomically");
        Check(!value.SetGeometryForFixture({0,0,1025},{1,2,1})&&State(value)==before,"Out-of-range center refuses atomically");
        Check(!value.SetGeometryForFixture({0,0,0},{1.5f,2,1})&&State(value)==before,"Noninteger width refuses atomically");
        value.PyramidStateForFixture().flip9C=0xA5;const auto& sphere=value.GetBoundingSphereForAnalysis();Check(Word(sphere[0])==0x80000000&&Word(sphere[2])==0x80000000&&sphere[3]==-1,"Raw flip and negative radius preserved");
        Check(value.SetGeometryForFixture({2,4,6},{8,16,32}),"Aliased fixture input setup");
        Check(value.SetGeometryForFixture(value.GetSizeForAnalysis(),value.GetCenterForAnalysis())
            &&value.GetCenterForAnalysis()==spPyramidProjection::Vector3{8,16,32}&&value.GetSizeForAnalysis()==spPyramidProjection::Vector3{2,4,6},"Fixture input references are captured before either store");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit")
        {
            std::string line;while(std::getline(std::cin,line))
            {
                if(line.empty())continue;std::string mode;unsigned flip;std::array<std::uint32_t,6> words{};std::istringstream input(line);Check(bool(input>>mode),"Pyramid mode");
                for(auto& word:words)Check(bool(input>>word),"Pyramid float words");Check(bool(input>>flip),"Pyramid flip byte");
                Replay(mode,{Float(words[0]),Float(words[1]),Float(words[2])},{Float(words[3]),Float(words[4]),Float(words[5])},flip);
            }
            return 0;
        }
        Guards();
        Replay("ctor",{0,0,0},{1,1,1},0);Replay("flipped-zero-center",{0,0,0},{0,1,2},1);
        Replay("copy",{11,12,13},{2,4,8},0xA5);Replay("clone",{11,12,13},{2,4,8},0xA5);Replay("fx",{0,0,0},{1,1,1},0);
        std::cout<<"PASS Pyramid bounded geometry, inherited Copy/Clone, shared FX and explicit guards\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
