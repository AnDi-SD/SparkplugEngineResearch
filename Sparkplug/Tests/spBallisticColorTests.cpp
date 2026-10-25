#include "Analysis/PC/spBallisticColor.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace sparkplug::evidence::pc;
namespace {
float Float(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(int argc,char** argv)
{
    try {
        if(argc==2&&std::string(argv[1])=="--batch") {
            std::uint32_t time,birth,life,stored,begin,end;
            while(std::cin>>time>>birth>>life>>stored>>begin>>end) {
                std::uint32_t out=0xdeadbeef;
                bool ok=EvaluateBallisticCpuColorForAnalysis(Float(time),Float(birth),Float(life),stored!=0,begin,end,out);
                std::cout<<ok<<','<<out<<'\n';
            }
            return 0;
        }
        std::uint32_t out=0xdeadbeef;
        Check(EvaluateBallisticCpuColorForAnalysis(1,0,3,false,0x7f132737,0xe24f65a3,out)
            &&out==0xa0263a5a,"native one-third packed-color boundary");
        Check(EvaluateBallisticCpuColorForAnalysis(1,0,3,true,0x7f132737,0xe24f65a3,out)
            &&out==0xa0263a5a,"stored-position uses same extended normalized age");
        out=0xdeadbeef;
        Check(!EvaluateBallisticCpuColorForAnalysis(1,0,0,false,0,0,out)&&out==0xdeadbeef,"invalid lifetime preserves output");
        Check(!EvaluateBallisticCpuColorForAnalysis(std::numeric_limits<float>::infinity(),0,1,false,0,0,out)
            &&out==0xdeadbeef,"nonfinite clock preserves output");
        std::cout<<"PASS: bounded Ballistic packed color\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
