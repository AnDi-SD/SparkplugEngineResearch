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
#include <string_view>

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
void FrameAndPacking() {
    using Random=spFunctionEval::RandomStateForAnalysis;
    using Particle=spParticleSystem;
    auto node=std::make_shared<spRenderNode>();Check(node->UpdateWorldForAnalysis(),"Frame node world");
    const auto prepare=[&](Particle& particle,float rate=4) {
        auto& p=particle.Parameters();p.flags={0,0,0};p.rate=rate;p.times={-1,1};
        p.regionType=1;p.region={0,0,0};p.direction={1,0,0};p.velocity={2,2};p.angle={0,0};
        p.acceleration={{{1,0,0},{1,0,0}}};particle.SetRenderNodeForAnalysis(node);
    };
    Particle p;prepare(p);Random random;std::string error;
    Check(p.InitializeForAnalysis(&random,&error),"Frame non-loop Init");
    Particle::ProductionForAnalysis production;
    Check(p.ProduceParticlesForAnalysis(3,0,production,&random,&error)
        && production.limitedRequest==3 && production.created==3,"Concrete producer fills free ring");
    Check(p.GetPoolStateForAnalysis()[0]==3 && p.GetPoolStateForAnalysis()[1]==1,"Producer owned active/free state");
    Particle::FrameOptionsForAnalysis options;options.integrationRate=0;
    const auto initialRecords=p.GetRecordsForAnalysis();
    Check(p.UpdateForAnalysis(1,options,&random,&error) && p.GetPoolStateForAnalysis()[0]==3,
        "Strict retirement preserves exact lifetime endpoint");
    Check(p.UpdateForAnalysis(.25f,options,&random,&error) && p.GetPoolStateForAnalysis()[0]==2
        && p.GetRecordsForAnalysis()==initialRecords,"Physical retirement retains record bytes");

    Particle emitted;prepare(emitted);Check(emitted.InitializeForAnalysis(&random,&error),"Emission Init");
    Check(emitted.ProduceParticlesForAnalysis(3,0,production,&random,&error),"Emission prefill");
    options.emissionEnabled=true;
    Check(emitted.UpdateForAnalysis(1,options,&random,&error) && emitted.GetPoolStateForAnalysis()[0]==4
        && emitted.GetFrameStateForAnalysis().accumulator==0,"Frame debit uses original request, not one actual free slot");
    // The producer's original return and the actual write count differ at128.
    Particle exact;prepare(exact,128);Check(exact.InitializeForAnalysis(&random,&error),"Exact128 Init");
    const auto beforeRandom=random;
    Check(exact.ProduceParticlesForAnalysis(128,0,production,&random,&error)
        && production.limitedRequest==128 && production.created==0
        && exact.GetPoolStateForAnalysis()[0]==0 && random.state==beforeRandom.state
        && random.index==beforeRandom.index,"Producer preserves empty last128 batch and native return");

    Particle loop;prepare(loop,3);loop.Parameters().flags[0]=1;
    Check(loop.InitializeForAnalysis(&random,&error),"Loop Init via common producer");
    loop.SetFrameStateForAnalysis({0,.75f,0});unsigned modifierCalls=0;
    options.modifiers={{[](void* c,float){++*static_cast<unsigned*>(c);},&modifierCalls}};
    Check(loop.UpdateForAnalysis(3,options,&random,&error) && loop.GetFrameStateForAnalysis().clock==2.75f
        && loop.GetFrameStateForAnalysis().delta==1 && modifierCalls==0,"Loop full clock, clamped delta, one subtraction and no modifier");

    Particle integrated;prepare(integrated);integrated.Parameters().flags[2]=1;
    Check(integrated.InitializeForAnalysis(&random,&error)
        && integrated.ProduceParticlesForAnalysis(1,0,production,&random,&error),"Integrated physical record Init");
    struct ModifierContext {Particle* particle;unsigned calls=0;} modifier{&integrated};
    options.emissionEnabled=false;
    options.modifiers={{[](void* c,float delta){auto& m=*static_cast<ModifierContext*>(c);++m.calls;
        const auto index=m.particle->GetLinksForAnalysis()[m.particle->GetFirstForAnalysis()][0];
        auto record=m.particle->GetRecordsForAnalysis()[index];record[3]=10;
        if(delta<0 || !m.particle->SetRecordForAnalysis(index,record))throw std::runtime_error("Modifier fixture");},&modifier}};
    Check(integrated.UpdateForAnalysis(.1f,options,&random,&error),"Frame executes explicit modifier then integration");
    const auto physical=integrated.GetLinksForAnalysis()[integrated.GetFirstForAnalysis()][0];
    Check(modifier.calls==1 && integrated.GetRecordsForAnalysis()[physical][0]==1
        && integrated.GetRecordsForAnalysis()[physical][3]==10.01f,"Modifier result feeds same-step concrete record integration");
    options.modifiers.clear();
    Check(integrated.UpdateForAnalysis(0,options,&random,&error)
        && integrated.GetRecordsForAnalysis()[physical][0]==1
        && integrated.GetRecordsForAnalysis()[physical][3]>10.01f,"Zero delta still changes velocity once");

    struct DrawContext {bool worldReply=true;unsigned worlds=0,colors=0,draws=0;std::vector<unsigned> vertices;
        std::vector<sparkplug::evidence::pc::BallisticCpuPackedVertexForAnalysis> stream;} draw;
    Particle::CpuDrawDispatchForAnalysis dispatch;
    dispatch.context=&draw;
    dispatch.setWorld=[](void* c,const std::array<float,16>& world){auto& d=*static_cast<DrawContext*>(c);++d.worlds;
        Check(world[12]==7 && world[13]==8 && world[14]==9,"Local draw SetWorld uses translation only");return d.worldReply;};
    dispatch.color=[](void* c,float,float,float,bool,std::uint32_t,std::uint32_t,std::uint32_t& out)
        {++static_cast<DrawContext*>(c)->colors;out=0x01020304;return true;};
    dispatch.uploadAndDraw=[](void* c,const sparkplug::evidence::pc::BallisticCpuPackedVertexForAnalysis* v,
        std::uint32_t count,const std::uint16_t* indices,std::uint32_t indexCount,std::uint32_t kind,std::uint32_t primitiveCount){
        auto& d=*static_cast<DrawContext*>(c);++d.draws;d.vertices.push_back(count);
        Check(kind==2 && primitiveCount==count/2 && indexCount==count/4*6,"Concrete external draw counts");
        if(count){Check(indices[0]==0 && indices[5]==3 && v[0].color==0x01020304,"Concrete packed spans before upload");d.stream.assign(v,v+count);}return false;};
    Particle::CpuDrawInputForAnalysis input;input.nodePosition={7,8,9};input.vertexCapacity=8;input.indexCapacity=12;
    Particle::CpuDrawStateForAnalysis drawState;drawState.effectField1C=9;draw.worldReply=false;
    Check(!p.DrawCpuForAnalysis(input,dispatch,drawState,&error) && drawState.effectField1C==9
        && draw.draws==0,"SetWorld failure preserves byte and prevents packing/submission");
    draw.worldReply=true;
    Check(emitted.DrawCpuForAnalysis(input,dispatch,drawState,&error) && drawState.effectField1C==0
        && drawState.submittedBatches==2 && drawState.rejectedSubmissionReplies==2
        && draw.vertices==std::vector<unsigned>{8,0} && draw.colors==2,"Concrete batches retain empty final draw and ignore per-batch false");
    dispatch.color=nullptr;
    Check(!integrated.DrawCpuForAnalysis(input,dispatch,drawState,&error),"Nonempty core draw requires explicit color provider");
}
int ClosureCase(std::string_view name) {
    Check(name=="init-directed" || name=="refill" || name=="retire" || name=="modifier"
        || name=="zero" || name=="loop-once" || name=="draw-empty","Explicit closure case");
    spParticleSystem particle;auto node=std::make_shared<spRenderNode>();
    node->SetPositionForAnalysis({7,8,9});node->MarkLocalTransformDirtyForAnalysis();
    Check(node->UpdateWorldForAnalysis(),"Closure world input");particle.SetRenderNodeForAnalysis(node);
    auto& p=particle.Parameters();p.flags={std::uint8_t(name=="init-directed"),0,0};
    p.rate=4;p.times={-1,1};p.regionType=1;p.region={1,2,3};p.direction={0,0,1};
    p.velocity={2,2};p.angle={0,0};p.acceleration={{{1,.5f,0},{1,.5f,0}}};p.colors={0,0};
    spFunctionEval::RandomStateForAnalysis random;random.Seed(5489);std::string error;
    Check(particle.InitializeForAnalysis(&random,&error),error.c_str());
    if(name!="init-directed") {
        spParticleSystem::ProductionForAnalysis production;
        const auto initial=name=="modifier" || name=="zero"?1u:name=="draw-empty"?4u:3u;
        Check(particle.ProduceParticlesForAnalysis(initial,0,production,&random,&error),error.c_str());
        p.flags[0]=name=="loop-once";p.flags[2]=name=="modifier" || name=="zero";
    }
    struct Context {spParticleSystem* particle;std::vector<std::uint32_t> modifierDelta,world;
        struct Batch {std::uint32_t kind,count;std::vector<std::uint32_t> words;std::vector<std::uint16_t> indices;};
        std::vector<Batch> batches;} context{&particle};
    if(name!="init-directed") {
        particle.SetFrameStateForAnalysis({0,name=="loop-once"?.75f:0,0});
        spParticleSystem::FrameOptionsForAnalysis options;options.integrationRate=0;options.emissionEnabled=name=="refill";
        if(name=="modifier")options.modifiers={{[](void* c,float delta){auto& x=*static_cast<Context*>(c);
            x.modifierDelta.push_back(Bits(delta));const auto n=x.particle->GetLinksForAnalysis()[x.particle->GetFirstForAnalysis()][0];
            auto record=x.particle->GetRecordsForAnalysis()[n];record[3]=10;
            Check(x.particle->SetRecordForAnalysis(n,record),"Closure modifier record");},&context}};
        const float delta=name=="refill"?1.f:name=="retire"?1.25f:name=="modifier"?.125f:name=="loop-once"?3.f:0;
        Check(particle.UpdateForAnalysis(delta,options,&random,&error),error.c_str());
    }
    spParticleSystem::CpuDrawInputForAnalysis input;input.nodePosition={7,8,9};input.vertexCapacity=8;input.indexCapacity=12;
    spParticleSystem::CpuDrawDispatchForAnalysis dispatch;dispatch.context=&context;
    dispatch.setWorld=[](void* c,const std::array<float,16>& world){auto& out=static_cast<Context*>(c)->world;
        for(float f:world)out.push_back(Bits(f));return true;};
    dispatch.color=[](void*,float,float,float,bool,std::uint32_t a,std::uint32_t b,std::uint32_t& out)
        {Check(!a&&!b,"Exact zero-color provider domain");out=0;return true;};
    dispatch.uploadAndDraw=[](void* c,const sparkplug::evidence::pc::BallisticCpuPackedVertexForAnalysis* vertices,
        std::uint32_t count,const std::uint16_t* indices,std::uint32_t indexCount,std::uint32_t kind,std::uint32_t primitives){
        Context::Batch b{kind,primitives};const auto* bytes=reinterpret_cast<const std::uint8_t*>(vertices);
        for(std::size_t n=0;n<std::size_t(count)*6;++n){std::uint32_t u;std::memcpy(&u,bytes+4*n,4);b.words.push_back(u);}
        if(indexCount)b.indices.assign(indices,indices+indexCount);static_cast<Context*>(c)->batches.push_back(std::move(b));return false;};
    spParticleSystem::CpuDrawStateForAnalysis state;state.effectField1C=9;
    Check(particle.DrawCpuForAnalysis(input,dispatch,state,&error),error.c_str());
    const auto array=[](const auto& values){std::cout<<'[';bool first=true;
        for(auto value:values){std::cout<<(first?"":",")<<std::uint32_t(value);first=false;}std::cout<<']';};
    std::cout<<"{\"first\":"<<particle.GetFirstForAnalysis()<<",\"boundary\":"<<particle.GetBoundaryForAnalysis()<<",\"pool\":";
    array(particle.GetPoolStateForAnalysis());std::cout<<",\"records\":[";
    bool first=true;for(const auto& record:particle.GetRecordsForAnalysis()){std::cout<<(first?"":",");first=false;
        std::array<std::uint32_t,8> words{};for(unsigned n=0;n<8;++n)words[n]=Bits(record[n]);array(words);}
    std::cout<<"],\"links\":[";first=true;for(const auto& link:particle.GetLinksForAnalysis()){std::cout<<(first?"":",");first=false;array(link);}
    std::cout<<"],\"randomIndex\":"<<random.index<<",\"randomState\":";array(random.state);
    std::cout<<",\"modifierDelta\":";array(context.modifierDelta);std::cout<<",\"world\":";array(context.world);
    std::cout<<",\"effectField1C\":"<<unsigned(state.effectField1C)<<",\"batches\":[";first=true;
    for(const auto& b:context.batches){std::cout<<(first?"":",")<<"{\"kind\":"<<b.kind<<",\"primitives\":"<<b.count<<",\"words\":";
        first=false;array(b.words);std::cout<<",\"indices\":";array(b.indices);std::cout<<'}';}std::cout<<"]}\n";return 0;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string_view(argv[1])=="--closure-case")return ClosureCase(argv[2]);
        Captures(argc==2?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"Fixtures/particle-init.dat");
        Guards();
        FrameAndPacking();
        std::cout<<"PASS "<<checks<<" particle CPU checks; "<<compared<<" original initialized words\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
