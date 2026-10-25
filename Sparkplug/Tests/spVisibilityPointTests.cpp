#include "Analysis/PC/spVisibilityPointMath.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace math=sparkplug::evidence::pc::visibility_point_math;
namespace
{
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
float Float(std::uint32_t bits){float result;std::memcpy(&result,&bits,4);return result;}
void Batch()
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=256,"bounded point query count");std::cout<<'[';
    for(std::size_t c=0;c<count;++c)
    {
        std::size_t planeCount,pointCount;math::PlaneSet planes;
        Check(bool(std::cin>>planeCount>>planes.activeCount>>pointCount)&&planeCount<=64&&pointCount<=64,"bounded query dimensions");
        planes.planes.resize(planeCount);for(auto& plane:planes.planes){for(auto& value:plane.equation){std::uint32_t word;Check(bool(std::cin>>word),"point query plane words");value=Float(word);}unsigned flag;Check(bool(std::cin>>flag)&&flag<=255,"point query enabled byte");plane.enabled=flag!=0;}
        std::vector<math::Vector3> points(pointCount);for(auto& point:points)for(auto& value:point){std::uint32_t word;Check(bool(std::cin>>word),"point query position words");value=Float(word);}
        const auto outside=math::IsOutsidePoints(planes,points),inside=math::IsFullyInsidePoints(planes,points);
        Check(outside.completed&&inside.completed,"point query outside supported number domain");if(c)std::cout<<',';std::cout<<'['<<outside.value<<','<<inside.value<<']';
    }
    std::cout<<"]\n";
}
void Directed()
{
    math::PlaneSet planes{{{{0,0,1,10},true}},1};
    const auto query=[&](std::vector<math::Vector3> points,bool outside,bool inside){const auto a=math::IsOutsidePoints(planes,points),b=math::IsFullyInsidePoints(planes,points);Check(a.completed&&b.completed&&a.value==outside&&b.value==inside,"native point sign set");};
    query({{0,0,5},{0,0,20}},false,false);query({{0,0,10},{0,0,20}},false,true);query({{0,0,10},{0,0,5}},true,false);query({},false,false);
    query({{0,0,std::numeric_limits<float>::quiet_NaN()},{0,0,20}},false,true);
    planes.activeCount=0;query({{0,0,5}},false,true);planes.activeCount=1;
    const auto incomplete=math::IsOutsidePoints(planes,std::vector<math::Vector3>{{0,0,std::numeric_limits<float>::infinity()}});Check(!incomplete.completed,"infinite input is explicit incomplete numeric domain");
    planes={{{{1,1,1,0},true}},1};query({{float(std::uint64_t(1)<<60),1,-float(std::uint64_t(1)<<60)}},false,true);
    planes={{{{0,0,1,0},true}},1};query({{0,0,.001f}},false,false);query({{0,0,std::nextafter(.001f,1.f)}},false,true);query({{0,0,std::nextafter(-.001f,-1.f)}},true,false);
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--points-batch"){Batch();return 0;}
    Directed();std::cout<<"PASS "<<checks<<"/"<<checks<<": plane-set point queries\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
