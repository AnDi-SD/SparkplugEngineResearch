#include "Code/Sparkplug/spParticleSystem.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spFunctionEval.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;
unsigned checks=0,compared=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
struct Reader {
    Bytes data;std::size_t at=0;
    explicit Reader(Bytes bytes):data(std::move(bytes)){}
    template<class T> T Raw(){Check(at+sizeof(T)<=data.size(),"Bounded capture read");T value;std::memcpy(&value,data.data()+at,sizeof(T));at+=sizeof(T);return value;}
    Bytes Block(){auto n=Raw<std::uint32_t>();Check(n<=65536&&at+n<=data.size(),"Bounded capture block");Bytes b(data.begin()+at,data.begin()+at+n);at+=n;return b;}
};
std::uint32_t Bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
void EqualWord(std::uint32_t actual,std::uint32_t expected,const std::string& label,unsigned index) {
    ++compared;
    if(actual!=expected){std::cerr<<label<<'['<<index<<"] actual="<<std::hex<<actual<<" expected="<<expected<<std::dec<<'\n';throw std::runtime_error("Original particle word mismatch");}
}
void Captures(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);Check(bool(file),"Particle reference fixture exists");
    Reader capture(Bytes{std::istreambuf_iterator<char>(file),{}});
    Check(capture.Raw<std::uint32_t>()==0x31495450,"Particle Init fixture PTI1");
    const auto cases=capture.Raw<std::uint32_t>();Check(cases>=4&&cases<=32,"Bounded original case count");
    for(unsigned ordinal=0;ordinal<cases;++ordinal) {
        const auto text=capture.Block();const std::string label(text.begin(),text.end());
        Reader parameter(capture.Block()),world(capture.Block());spParticleSystem particle;auto& p=particle.Parameters();
        (void)parameter.Raw<std::uint8_t>();(void)parameter.Raw<std::uint32_t>();
        p.flags=parameter.Raw<decltype(p.flags)>();p.rate=parameter.Raw<float>();p.times=parameter.Raw<decltype(p.times)>();
        p.colors=parameter.Raw<decltype(p.colors)>();p.acceleration=parameter.Raw<decltype(p.acceleration)>();
        p.direction=parameter.Raw<decltype(p.direction)>();p.velocity=parameter.Raw<decltype(p.velocity)>();
        p.angle=parameter.Raw<decltype(p.angle)>();p.scale=parameter.Raw<decltype(p.scale)>();p.sphere=parameter.Raw<decltype(p.sphere)>();
        p.regionType=parameter.Raw<std::uint32_t>();constexpr unsigned sizes[]={0,3,6,4,8,4,5,6};
        Check(p.regionType<=7,"Region tag bounded");for(unsigned n=0;n<sizes[p.regionType];++n)p.region.push_back(parameter.Raw<float>());
        (void)parameter.Raw<std::array<std::uint32_t,5>>();Check(parameter.at==parameter.data.size(),"All original parameter bytes accounted for");
        auto node=std::make_shared<spRenderNode>();node->SetPositionForAnalysis(world.Raw<spNode::Vector3>());
        node->SetScaleForAnalysis(world.Raw<spNode::Vector3>());node->SetOrientationForAnalysis(world.Raw<spNode::Matrix3>());
        node->MarkLocalTransformDirtyForAnalysis();
        Check(node->UpdateWorldForAnalysis(),"Explicit captured RenderNode world input");particle.SetRenderNodeForAnalysis(node);
        spFunctionEval::RandomStateForAnalysis random,expectedRandom;random.index=capture.Raw<std::uint32_t>();
        auto bytes=capture.Block();Check(bytes.size()==sizeof(random.state),"Full original PRNG state");std::memcpy(random.state.data(),bytes.data(),bytes.size());
        expectedRandom.index=capture.Raw<std::uint32_t>();bytes=capture.Block();Check(bytes.size()==sizeof(expectedRandom.state),"Full expected PRNG state");std::memcpy(expectedRandom.state.data(),bytes.data(),bytes.size());
        const auto count=capture.Raw<std::uint32_t>(),first=capture.Raw<std::uint32_t>(),boundary=capture.Raw<std::uint32_t>();
        const auto pool=capture.Raw<std::array<std::uint32_t,5>>();Reader records(capture.Block()),links(capture.Block());const auto written=capture.Block();
        Check(count<=1024&&records.data.size()==count*32&&links.data.size()==count*12&&written.size()==count,"Exact original storage extents");
        std::string error;Check(particle.InitializeForAnalysis(&random,&error),error.c_str());
        Check(particle.IsInitializedForAnalysis()&&particle.GetRecordsForAnalysis().size()==count,"Initialized shared CPU particle owner");
        EqualWord(particle.GetFirstForAnalysis(),first,label+" first",0);EqualWord(particle.GetBoundaryForAnalysis(),boundary,label+" boundary",0);
        for(unsigned n=0;n<5;++n)EqualWord(particle.GetPoolStateForAnalysis()[n],pool[n],label+" pool",n);
        for(unsigned n=0;n<count;++n) {
            for(unsigned word=0;word<8;++word) {
                const auto expected=records.Raw<std::uint32_t>();
                if(written[n]||word==7)EqualWord(Bits(particle.GetRecordsForAnalysis()[n][word]),expected,label+" record",n*8+word);
                else Check(Bits(particle.GetRecordsForAnalysis()[n][word])==0,"Host zeros unknown original allocator bytes");
            }
            for(unsigned word=0;word<3;++word)EqualWord(particle.GetLinksForAnalysis()[n][word],links.Raw<std::uint32_t>(),label+" link",n*3+word);
            Check(particle.GetRecordWrittenForAnalysis()[n]==written[n],"Record validity agrees with actual original emission");
        }
        EqualWord(random.index,expectedRandom.index,label+" random index",0);
        for(unsigned n=0;n<624;++n)EqualWord(random.state[n],expectedRandom.state[n],label+" random state",n);
        const auto before=random;Check(!particle.InitializeForAnalysis(&random,&error),"Reinitialize refused before mutation");
        Check(random.state==before.state&&random.index==before.index,"Rejected Init consumes no shared randomness");
        std::cout<<"PASS "<<label<<" capacity "<<count<<" active "<<pool[0]<<'\n';
    }
    Check(capture.at==capture.data.size(),"No trailing unaccounted fixture bytes");
}
void Guards() {
    using Random=spFunctionEval::RandomStateForAnalysis;
    const auto prepare=[](spParticleSystem& p){p.Parameters().regionType=1;p.Parameters().region={1,2,3};
        p.Parameters().times={-1,1};p.Parameters().rate=3;};
    for(unsigned variant=0;variant<7;++variant) {
        spParticleSystem p;prepare(p);Random random;std::string error;
        auto node=std::make_shared<spRenderNode>();p.SetRenderNodeForAnalysis(node);
        if(variant==0)p.SetRenderNodeForAnalysis({});
        if(variant==1)p.Parameters().rate=1;
        if(variant==2)p.Parameters().rate=1025;
        if(variant==3)p.Parameters().rate=std::numeric_limits<float>::infinity();
        if(variant==4)p.Parameters().region.pop_back();
        if(variant==5)p.Parameters().region[0]=65537;
        if(variant==6)p.Parameters().angle[1]=360001;
        Check(!p.InitializeForAnalysis(&random,&error)&&!error.empty(),"Malformed/unsupported Init input fails explicitly");
        Check(random.index==625&&random.state==Random{}.state&&p.GetRecordsForAnalysis().empty(),"Preflight failure preserves PRNG and pool");
    }
    spParticleSystem p;prepare(p);p.Parameters().flags[0]=0;std::string error;Random random;
    Check(p.InitializeForAnalysis(&random,&error),"Non-loop pool needs no RenderNode or random draws");
    Check(p.GetPoolStateForAnalysis()[1]==3&&random.index==625,"All non-loop records start free");
    Check(!p.PrepareNonLoopingForAnalysis(),"Old count-only adapter cannot mutate a live CPU owner");
    // Default Init must use the SAME global sequence as FunctionEval. Supplying
    // an explicit equivalent state is only a test input, never a per-object RNG.
    auto node=std::make_shared<spRenderNode>();spParticleSystem a,b;prepare(a);prepare(b);
    a.SetRenderNodeForAnalysis(node);b.SetRenderNodeForAnalysis(node);
    auto& shared=spFunctionEval::SharedRandomForAnalysis();const auto saved=shared;shared=Random{};Random explicitState;
    Check(a.InitializeForAnalysis(nullptr,&error)&&b.InitializeForAnalysis(&explicitState,&error),"Implicit and explicit shared-generator routes initialize");
    Check(a.GetRecordsForAnalysis()==b.GetRecordsForAnalysis()&&shared.index==explicitState.index&&shared.state==explicitState.state,"Original global PRNG identity is preserved");
    shared=saved;
}
}
int main(int argc,char** argv) {
    try {
        Captures(argc==2?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"Fixtures/particle-init.dat");
        Guards();
        std::cout<<"PASS "<<checks<<" particle CPU checks; "<<compared<<" original initialized words\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
