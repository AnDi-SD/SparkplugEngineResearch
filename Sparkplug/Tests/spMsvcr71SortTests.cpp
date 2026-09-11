#include "Analysis/Host/LegacySortPolicy.h"
#include "Code/Sparkplug/spCamera.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spRenderNode.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace sparkplug::reconstruction;
using Policy=sparkplug::host::LegacySortPolicy;
using Helper=sparkplug::evidence::pc::GeometryHelper4604F0;
namespace crt=sparkplug::host::msvcr71_7_10_7031_4;
namespace {
int checks=0;
void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
using Bytes=std::vector<unsigned char>;
std::uint32_t Word(const void* data){std::uint32_t value;std::memcpy(&value,data,4);return value;}
struct Reader {
    std::ifstream stream;
    explicit Reader(const std::filesystem::path& path):stream(path,std::ios::binary){if(!stream)throw std::runtime_error("Missing original sort capture");}
    Bytes Read(std::size_t size){Check(size<=65536,"bounded capture read");Bytes result(size);if(size&&!stream.read(reinterpret_cast<char*>(result.data()),size))throw std::runtime_error("Truncated capture");return result;}
    std::uint32_t U32(){return Word(Read(4).data());}
};
auto Key(const unsigned char* record) {
    sparkplug::evidence::pc::renderer_queue_math::AlphaKey key;
    std::memcpy(&key.distanceSquared,record+12,4);key.priority=Word(record+16);key.exactParticleSystem=record[20]!=0;return key;
}
struct Input {
    unsigned kind;spVertexBuffer* vertices;
    std::vector<std::array<std::uint32_t,2>> pairs;
    static int Compare(const void* context,const void* left,const void* right) {
        auto& input=*const_cast<Input*>(static_cast<const Input*>(context));
        if(!input.kind) {
            std::uint16_t a,b;std::memcpy(&a,left,2);std::memcpy(&b,right,2);input.pairs.push_back({a,b});
            return Helper::ComparePositionsForAnalysis(*input.vertices,a,b).value();
        }
        input.pairs.push_back({Word(left),Word(right)});
        return sparkplug::evidence::pc::renderer_queue_math::CompareAlpha(Key(static_cast<const unsigned char*>(left)),Key(static_cast<const unsigned char*>(right)));
    }
};
void AlphaConsumer(const Bytes& input,const Bytes& expected,std::size_t count) {
    struct Camera:spCamera {};
    std::array<Camera,12> cameras;
    std::array<spRenderNode,2> supports;
    std::array<spModel,3> objects;
    spRenderer::AlphaQueueForAnalysis queue;queue.count=static_cast<std::uint32_t>(count);
    for(std::size_t i=0;i<count;++i) {
        const auto* row=input.data()+24*i;
        queue.entries[i]={&cameras[Word(row)-1],&supports[(Word(row+4)-0x10000)/16],&objects[(Word(row+8)-0x20000)/16],Key(row)};
    }
    struct Sink {
        spRenderer::AlphaQueueForAnalysis& queue;std::array<Camera,12>& cameras;std::array<spRenderNode,2>& supports;
        std::vector<unsigned> render,prepare;
        static bool Prepare(void* context,spRenderNode& node) {
            auto& self=*static_cast<Sink*>(context);Check(self.queue.flushing,"Alpha flag is active during prepare");
            self.prepare.push_back(&node==&self.supports[0]?0:1);return false; // original flush ignores refusal
        }
        static bool Render(void* context,spRenderable&,spCamera* camera,spRenderNode*) {
            auto& self=*static_cast<Sink*>(context);Check(self.queue.flushing,"Alpha flag is active during render");
            for(unsigned i=0;i<self.cameras.size();++i)if(camera==&self.cameras[i]){self.render.push_back(i+1);return false;}
            throw std::runtime_error("Borrowed camera changed by sort");
        }
    } sink{queue,cameras,supports};
    spRenderer::AlphaDispatchForAnalysis dispatch{&Policy::Alpha,&Sink::Prepare,&Sink::Render,&sink};
    Check(spRenderer::FlushAlphaForAnalysis(queue,dispatch),"Shared Alpha flush accepts explicit CRT policy");
    std::vector<unsigned> expectedRender,expectedPrepare;
    for(std::size_t i=0;i<count;++i) {
        const auto* row=expected.data()+i*24;expectedRender.push_back(Word(row));
        const auto support=(Word(row+4)-0x10000)/16;
        if(expectedPrepare.empty()||expectedPrepare.back()!=support)expectedPrepare.push_back(support);
    }
    Check(sink.render==expectedRender&&sink.prepare==expectedPrepare,"Whole host records preserve native permutation and adjacent support reuse");
    Check(!queue.count&&!queue.flushing&&!queue.dispatching,"Alpha flush resets count and flags after sorting");
}
void Captures(const std::filesystem::path& path) {
    Reader reader(path);Check(reader.Read(4)==Bytes({'Q','S','T','1'}),"Original capture magic");
    const auto cases=reader.U32();Check(cases==10,"Ten bounded original cases");
    for(unsigned c=0;c<cases;++c) {
        const auto name=reader.Read(reader.U32());const auto kind=reader.U32(),count=reader.U32(),width=reader.U32();
        Check(kind<=1&&count<=12&&width==(kind?24u:2u),"Original capture shape");
        const auto vertexBytes=reader.Read(reader.U32()),input=reader.Read(count*width),expected=reader.Read(count*width);
        const auto calls=reader.U32();Check(calls<=512,"Bounded comparator trace");
        std::vector<std::array<std::uint32_t,2>> pairs;
        for(unsigned i=0;i<calls;++i){auto a=reader.U32(),b=reader.U32();pairs.push_back({a,b});}
        spVertexBuffer vertices;
        if(!kind) {
            std::vector<std::byte> bytes(vertexBytes.size());std::memcpy(bytes.data(),vertexBytes.data(),bytes.size());
            Check(vertices.InitializeFromDataForAnalysis(0,count,0,bytes),"Shared position-only VB");
        }
        Bytes guarded(input.size()+32,0xa5);std::copy(input.begin(),input.end(),guarded.begin()+16);
        Input context{kind,&vertices};
        Check(crt::Sort(guarded.data()+16,count,width,&Input::Compare,&context),"Versioned qsort completes");
        Check(std::equal(expected.begin(),expected.end(),guarded.begin()+16),"Exact original whole-record permutation");
        Check(context.pairs==pairs,"Exact original comparator argument order and count");
        Check(std::all_of(guarded.begin(),guarded.begin()+16,[](auto x){return x==0xa5;})&&
            std::all_of(guarded.end()-16,guarded.end(),[](auto x){return x==0xa5;}),"Sort does not touch guard bytes");
        if(kind)AlphaConsumer(input,expected,count);
        std::cout<<"  "<<std::string(name.begin(),name.end())<<": "<<calls<<" original comparator calls matched\n";
    }
    Check(reader.stream.peek()==std::char_traits<char>::eof(),"No unconsumed capture records");
}
void GeometryConsumer() {
    using Point=std::array<float,3>;
    const std::vector<Point> points{{-10,-10,10},{10,-10,10},{10,10,10},{-10,10,10},{-10,-10,10}};
    const std::array<std::uint16_t,6> input{0,1,2,4,2,3},expected{3,0,1,3,1,2};
    spIndexBuffer indices;spVertexBuffer vertices;
    Check(indices.InitializeForAnalysis(2,spIndexBuffer::eIndexBufferType::Type2,0),"Geometry IB");
    for(unsigned i=0;i<input.size();++i)Check(indices.SetIndexForAnalysis(i,input[i]),"Geometry input index");
    std::vector<std::byte> bytes(points.size()*12);std::memcpy(bytes.data(),points.data(),bytes.size());
    Check(vertices.InitializeFromDataForAnalysis(0,5,0,bytes),"Geometry VB");
    Helper helper;Helper::ObservationForAnalysis observation;std::string error;
    Check(helper.WeldForAnalysis(indices,vertices,Policy::GeometryDispatch(),observation,&error),"Original weld uses real portable CRT dependency");
    Check(observation.sortedIds==std::vector<std::uint16_t>({2,1,3,4,0})&&observation.compactionAllocationBytes==192,"Captured duplicate5 representative and allocation preserved");
    for(unsigned i=0;i<expected.size();++i)Check(indices.GetIndexForAnalysis(i)==expected[i],"Captured duplicate5 final index");
    const std::array<Point,4> expectedPoints{points[1],points[2],points[3],points[0]};
    Check(vertices.GetDataForAnalysis().size()==48&&std::memcmp(vertices.GetDataForAnalysis().data(),expectedPoints.data(),48)==0,"Captured duplicate5 final vertex bytes");
}
void Boundaries() {
    Check(crt::Sort(nullptr,0,24,nullptr,nullptr)&&crt::Sort(nullptr,1,24,nullptr,nullptr)&&crt::Sort(nullptr,12,0,nullptr,nullptr),"Original no-comparison early returns");
    Bytes bytes(64,0x5a);auto compare=[](const void*,const void*,const void*){return 0;};
    Check(!crt::Sort(nullptr,2,2,compare,nullptr)&&!crt::Sort(bytes.data(),2,2,nullptr,nullptr),"Host null guards");
    Check(!crt::Sort(bytes.data(),0x80000000ull,2,compare,nullptr)&&std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==0x5a;}),"Host extent guard precedes mutation");
}
}
int main(int argc,char** argv) {
    try {
        auto path=argc==2?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"Fixtures/msvcr71-qsort7107031.dat";
        Captures(path);GeometryConsumer();Boundaries();
        std::cout<<"PASS "<<checks<<"/"<<checks<<": versioned CRT qsort and both common consumers\n";return 0;
    } catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
