#include "Analysis/Host/RenderTopology.h"
#include <iostream>
using namespace sparkplug::host::render_topology;
namespace {
unsigned checks=0;using Indices=std::vector<std::uint32_t>;
void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void Reject(const Indices& values,std::uint32_t type,std::uint32_t vertices){
    bool refused=false;try{(void)Triangles(values,type,vertices);}catch(const std::runtime_error&){refused=true;}
    Check(refused,"Invalid emitted geometry is refused");
}
}
int main(){try{
    // Actual menu Mesh6 source indices, retained exactly by the original PC
    // reader/upload probe. Only the emitted two triangles fetch vertices.
    const Indices legacy{2,2,2,0,1,3,3,3,0xcdcd,0xcdcd};const auto before=legacy;
    Check(Triangles(legacy,3,4)==Indices({2,0,1,1,0,3}),"Original legacy strip maps to two valid triangles");
    Check(legacy==before,"Original index stream is not repaired or truncated");
    Check(Triangles({0,1,2,3},3,4)==Indices({0,1,2,2,1,3}),"Odd strip windows preserve winding");
    Check(Triangles({0,1,2,2,3,3,4,5},3,6)==Indices({0,1,2,4,3,5}),"Degenerate windows still advance parity");
    Check(Triangles({0xcdcd,0xcdcd,0xcdcd},3,0).empty(),"All-degenerate strip emits no vertex fetch");
    Check(Triangles({},3,0).empty()&&Triangles({0,1},3,0).empty(),"Short strip has no triangles");
    Check(Triangles({0,0,1,2,1,0},2,3)==Indices({0,0,1,2,1,0}),"List projection preserves authored list degenerates");
    Reject({0,1,0xcdcd},3,4);Reject({0xcdcd,1,2,2},3,4);
    Reject({0xcdcd,0xcdcd,0xcdcd},2,4);Reject({0,1},2,4);Reject({0,1,2},4,4);
    std::cout<<"PASS "<<checks<<" checks: host triangle projection preserves raw legacy indices\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
