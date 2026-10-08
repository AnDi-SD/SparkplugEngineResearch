#include "Code/SparkplugPC/spDXMouse.h"
#include "Analysis/PC/spDXMouseAbi.h"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* label)
    { ++checks; if (!value) throw std::runtime_error(label); }
    struct Step
    { std::uint32_t result, mutation; std::vector<spDXMouse::EventForAnalysis> events; };
    struct Boundary final : spDXMouse::BoundaryForAnalysis,
        spDXMouse::StartupBoundaryForAnalysis, spDXMouse::CursorBoundaryForAnalysis
    {
        std::vector<Step> steps;
        std::size_t cursor = 0;
        spDXMouse* mouse = nullptr;
        sparkplug::analysis::host::spDXMouseClientRectForAnalysis rectangle{0,0,640,480};
        std::vector<std::vector<std::uint32_t>> trace;
        std::array<std::array<std::uint32_t,2>,5> startupSteps{};
        unsigned startupCursor = 0;
        std::array<std::uint8_t,128> configuration{};
        std::uintptr_t createdDevice = 0x2222;
        unsigned acquireMutation = 0;
        unsigned released = 0;
        spDXMouse::PointForAnalysis screenPosition{21,31};
        std::uintptr_t GetInputInterface() noexcept override { return 0x1111; }
        void AddReference(std::uintptr_t) noexcept override {}
        void ReleaseReference(std::uintptr_t) noexcept override { ++released; }
        void Unacquire(std::uintptr_t) noexcept override {}
        std::uint32_t GetDeviceData(std::uintptr_t device, std::uint32_t size,
            spDXMouse::EventsForAnalysis& output, std::uint32_t& count, std::uint32_t flags) noexcept override
        {
            trace.push_back({1, std::uint32_t(device / 0x1111), size, count, flags});
            if (cursor >= steps.size()) std::terminate();
            const auto& step = steps[cursor++];
            count = std::uint32_t(step.events.size());
            for (std::size_t i=0; i<step.events.size(); ++i) output[i] = step.events[i];
            if (step.mutation) { auto state=mouse->GetStateForAnalysis(); state.device50=step.mutation*0x1111u; mouse->SetStateForAnalysis(state); }
            return step.result;
        }
        void Acquire(std::uintptr_t device) noexcept override
        { trace.push_back({2,std::uint32_t(device / 0x1111)}); Mutate(acquireMutation); }
        sparkplug::analysis::host::spDXMouseClientRectForAnalysis GetClientRectangle(std::uintptr_t window) noexcept override
        { trace.push_back({3,std::uint32_t(window / 0x1111)}); return rectangle; }
        void ReportNotAcquired() noexcept override { trace.push_back({4}); }
        void Mutate(unsigned marker) noexcept
        { if(marker){auto state=mouse->GetStateForAnalysis();state.device50=marker*0x1111u;mouse->SetStateForAnalysis(state);} }
        std::uint32_t NextStartupResult() noexcept
        { const auto step=startupSteps[startupCursor++];Mutate(step[1]);return step[0]; }
        std::uintptr_t GetApplicationWindow() noexcept override { return 0x5555; }
        std::uint32_t CreateMouseDevice(std::uintptr_t input, std::uint32_t guid,
            std::uintptr_t& device, std::uintptr_t outer) noexcept override
        { trace.push_back({5,std::uint32_t(input/0x1111),guid,std::uint32_t(outer)});device=createdDevice;return NextStartupResult(); }
        std::uint32_t GetMouseCapabilities(std::uintptr_t device, spDXMouse::CapabilitiesForAnalysis& capabilities) noexcept override
        { trace.push_back({6,std::uint32_t(device/0x1111),capabilities[0].value()});for(unsigned i=1;i<11;++i)capabilities[i]=100+i;return NextStartupResult(); }
        void ReadMouseExclusiveConfiguration(std::array<std::optional<std::uint8_t>,128>& output) noexcept override
        { trace.push_back({7,0x6f3170,0x6f3178,0x6db048,128,0x6f3188});for(unsigned i=0;i<128;++i)output[i]=configuration[i]; }
        std::uint32_t SetMouseDataFormat(std::uintptr_t device, std::uint32_t format) noexcept override
        { trace.push_back({8,std::uint32_t(device/0x1111),format});return NextStartupResult(); }
        std::uint32_t SetMouseCooperativeLevel(std::uintptr_t device, std::uintptr_t window, std::uint32_t flags) noexcept override
        { trace.push_back({9,std::uint32_t(device/0x1111),std::uint32_t(window/0x1111),flags});return NextStartupResult(); }
        std::uint32_t SetMouseBufferProperty(std::uintptr_t device, std::uintptr_t property,
            const sparkplug::analysis::host::spDXMouseBufferPropertyForAnalysis& value) noexcept override
        { trace.push_back({10,std::uint32_t(device/0x1111),std::uint32_t(property),value.size,value.headerSize,value.object,value.how,value.bufferSize});return NextStartupResult(); }
        void ReleaseMouseCapture() noexcept override { trace.push_back({11}); }
        void SetMouseCursorPosition(std::uint32_t x,std::uint32_t y) noexcept override { trace.push_back({12,x,y}); }
        void ShowMouseCursor(std::int32_t show) noexcept override { trace.push_back({13,std::uint32_t(show)}); }
        void SetMouseCapture(std::uintptr_t window) noexcept override { trace.push_back({14,std::uint32_t(window/0x1111)}); }
        spDXMouse::PointForAnalysis GetMouseCursorPosition() noexcept override { trace.push_back({15});return screenPosition; }
    };
    template<class Values> void PrintArray(const Values& values)
    {
        std::cout << '['; bool comma=false;
        for (const auto value : values) { if(comma)std::cout<<','; comma=true; std::cout<<std::uint32_t(value); }
        std::cout << ']';
    }
    void PrintResult(bool result, const spDXMouse& mouse, const Boundary& boundary)
    {
        const auto& cache = mouse.GetCacheForAnalysis().value();
        std::cout << '[' << result << ',' << unsigned(mouse.GetStateForAnalysis().acquired45) << ',';
        PrintArray(cache.relativeAxes);std::cout<<',';PrintArray(cache.buttons);std::cout<<',';
        PrintArray(cache.changedAxes);std::cout<<',';PrintArray(cache.changedButtons);std::cout<<',';
        PrintArray(cache.position);std::cout<<",[";
        for(std::size_t i=0;i<boundary.trace.size();++i){if(i)std::cout<<',';PrintArray(boundary.trace[i]);}
        std::cout << "]]\n";
    }
    int Batch()
    {
        unsigned operation, flag, stepCount, cases=0;
        while(std::cin>>operation>>flag)
        {
            if(operation>1 || flag>255 || ++cases>256)throw std::runtime_error("bounded mouse batch");
            Boundary boundary;spDXMouse mouse(boundary);boundary.mouse=&mouse;
            spDXMouse::CacheForAnalysis cache;
            for(auto& value:cache.relativeAxes)std::cin>>value;
            for(auto& value:cache.buttons){unsigned v;std::cin>>v;value=std::uint8_t(v);}
            for(auto& value:cache.changedAxes)std::cin>>value;
            for(auto& value:cache.changedButtons){unsigned v;std::cin>>v;value=std::uint8_t(v);}
            for(auto& value:cache.position)std::cin>>value;
            std::cin>>boundary.rectangle.right>>boundary.rectangle.bottom>>stepCount;
            if(!stepCount || stepCount>3)throw std::runtime_error("bounded mouse response script");
            for(unsigned i=0;i<stepCount;++i)
            {
                Step step;unsigned eventCount;std::cin>>step.result>>step.mutation>>eventCount;
                if(eventCount>1024 || step.mutation>3)throw std::runtime_error("bounded mouse event script");
                for(unsigned j=0;j<eventCount;++j){spDXMouse::EventForAnalysis e;std::cin>>e.offset>>e.data>>e.timestamp>>e.sequence>>e.applicationData;step.events.push_back(e);}
                boundary.steps.push_back(step);
            }
            if(!std::cin)throw std::runtime_error("truncated mouse case");
            mouse.SetRuntimeForAnalysis(0x4444,cache);auto state=mouse.GetStateForAnalysis();state.device50=0x2222;state.acquired45=std::uint8_t(flag);mouse.SetStateForAnalysis(state);
            const auto result=operation?mouse.PollForAnalysis():std::optional<bool>{mouse.ProcessEventsForAnalysis(boundary.steps[0].events.data(),boundary.steps[0].events.size())};
            if(!result)throw std::runtime_error("mouse batch admission");PrintResult(*result,mouse,boundary);
        }
        return 0;
    }
    void PrintTrace(const Boundary& boundary)
    { std::cout<<'[';for(std::size_t i=0;i<boundary.trace.size();++i){if(i)std::cout<<',';PrintArray(boundary.trace[i]);}std::cout<<']'; }
    int StartupBatch()
    {
        unsigned flag, cases=0;
        while(std::cin>>flag)
        {
            if(flag>255 || ++cases>256)throw std::runtime_error("bounded startup batch");
            Boundary boundary;spDXMouse mouse(boundary);boundary.mouse=&mouse;
            std::cin>>boundary.rectangle.left>>boundary.rectangle.top>>boundary.rectangle.right>>boundary.rectangle.bottom;
            for(unsigned i=0;i<5;++i){unsigned value;std::cin>>value;if(value>255)throw std::runtime_error("configuration byte");boundary.configuration[i]=std::uint8_t(value);}
            for(auto& step:boundary.startupSteps){std::cin>>step[0]>>step[1];if(step[1]>3)throw std::runtime_error("startup device mutation");}
            std::cin>>boundary.acquireMutation;
            if(!std::cin || boundary.acquireMutation>3)throw std::runtime_error("truncated startup");
            auto state=mouse.GetStateForAnalysis();state.acquired45=std::uint8_t(flag);mouse.SetStateForAnalysis(state);
            const auto result=mouse.StartupForAnalysis(0x4444,boundary);if(!result)throw std::runtime_error("startup domain");
            const auto& final=mouse.GetStateForAnalysis();
            std::cout<<'['<<*result<<','<<unsigned(final.field44)<<','<<unsigned(final.acquired45)<<','<<final.field48<<','
                <<std::uint32_t(final.device50/0x1111)<<','<<final.field80<<','<<unsigned(mouse.GetExclusiveFlagForAnalysis().value())<<',';
            PrintArray(mouse.GetPositionForAnalysis().value());std::cout<<",[";
            bool comma=false;for(const auto& value:mouse.GetCapabilitiesForAnalysis()){if(comma)std::cout<<',';comma=true;if(value)std::cout<<*value;else std::cout<<"null";}
            std::cout<<"],";PrintTrace(boundary);std::cout<<"]\n";
        }
        return 0;
    }
    int CursorBatch()
    {
        unsigned initial,requested,cases=0;spDXMouse::PointForAnalysis saved,screen;
        while(std::cin>>initial>>requested>>saved.x>>saved.y>>screen.x>>screen.y)
        {
            if(initial>255 || requested>255 || ++cases>256)throw std::runtime_error("bounded cursor batch");
            Boundary boundary;spDXMouse mouse(boundary);boundary.mouse=&mouse;boundary.screenPosition=screen;
            mouse.SetRuntimeForAnalysis(0x4444,{});mouse.SetCursorRuntimeForAnalysis(std::uint8_t(initial),saved);
            Check(mouse.SetCursorVisibleForAnalysis(std::uint8_t(requested),boundary),"cursor batch domain");
            const auto& position=mouse.GetCursorPositionForAnalysis().value();
            std::cout<<'['<<unsigned(mouse.GetCursorVisibleFlagForAnalysis().value())<<','<<position.x<<','<<position.y<<',';
            PrintTrace(boundary);std::cout<<"]\n";
        }
        return 0;
    }
    int QueriesBatch()
    {
        unsigned flag,cases=0;std::uint32_t code;
        while(std::cin>>flag>>code)
        {
            if(flag>255 || ++cases>256)throw std::runtime_error("bounded mouse query batch");
            Boundary boundary;spDXMouse mouse(boundary);boundary.mouse=&mouse;
            spDXMouse::CacheForAnalysis cache{{11,0xfffffff4u,0x80000000u},{0,1,2,3,1,0,255,1},
                {99,98,97},{1,2,3,4,5,6,7,8},{0xffffffffu,0x80000000u}};
            mouse.SetRuntimeForAnalysis(0x4444,cache);auto state=mouse.GetStateForAnalysis();state.acquired45=std::uint8_t(flag);mouse.SetStateForAnalysis(state);
            mouse.PhysicalSlot5ForAnalysis(code,0x11223344,0xffffffffu,17);
            const std::array<std::uint32_t,5> output{std::uint32_t(mouse.PhysicalSlot1ForAnalysis(code).value()),
                std::uint32_t(mouse.PhysicalSlot2ForAnalysis(code).value()),mouse.PhysicalSlot3ForAnalysis(code).value(),
                std::uint32_t(mouse.PhysicalSlot4ForAnalysis(code).value()),std::uint32_t(mouse.PhysicalSlot6ForAnalysis(code)==0.0f)};
            if(!boundary.trace.empty())throw std::runtime_error("native cached query foreign call");
            PrintArray(output);std::cout<<'\n';
        }
        return 0;
    }
    void Tests()
    {
        Boundary boundary;spDXMouse mouse(boundary);boundary.mouse=&mouse;
        Check(mouse.IsKindOf(spDXInputDevice::ClassID) && mouse.IsExactly(spDXMouse::ClassID), "original mouse identity");
        Check(!mouse.GetPositionForAnalysis() && !mouse.GetCacheForAnalysis()
            && mouse.GetPacketsForAnalysis().buttons==std::array<std::uint8_t,8>{}
            && mouse.GetPacketsForAnalysis().relativeAxes==std::array<std::uint32_t,3>{}
            && mouse.GetCursorVisibleFlagForAnalysis()==1 && mouse.GetExclusiveFlagForAnalysis()==0
            && mouse.GetCursorPositionForAnalysis()->x==0 && mouse.GetCursorPositionForAnalysis()->y==0,
            "original factory zero packets and cursor defaults while absolute position remains uninitialized");
        bool missingContext=false;
        try { (void)spRTTIManager::Instance().Create(spDXMouse::ClassID); }
        catch(const std::logic_error&) { missingContext=true; }
        Check(spDXMouse::StaticRTTI().factory && missingContext,"original nonnull RTTI factory requires explicit host context");
        {
            spDXMouse::FactoryContextForAnalysis context(boundary);
            auto registered=spRTTIManager::Instance().Create(spDXMouse::ClassID);
            Check(registered && registered->IsExactly(spDXMouse::ClassID),"bound RTTI factory uses recovered real mouse constructor");
            Boundary nested;
            {
                spDXMouse::FactoryContextForAnalysis inner(nested);
                Check(spDXMouse::FactoryContextForAnalysis::CurrentBoundaryForAnalysis()==&nested,"nested host context binds its own borrowed boundary");
            }
            Check(spDXMouse::FactoryContextForAnalysis::CurrentBoundaryForAnalysis()==&boundary,"nested host context restores previous boundary");
        }
        Check(!spDXMouse::FactoryContextForAnalysis::CurrentBoundaryForAnalysis(),"factory scope releases borrowed binding");
        Check(mouse.PollForAnalysis().value() && boundary.trace.empty(), "native missing-device early return");
        auto state=mouse.GetStateForAnalysis();state.device50=0x2222;mouse.SetStateForAnalysis(state);
        boundary.steps={{0,0,{}}};
        Check(!mouse.PollForAnalysis() && boundary.trace==std::vector<std::vector<std::uint32_t>>{{1,2,20,1024,0}},
            "successful original COM poll precedes guard on unknown absolute position");
        boundary.trace.clear();boundary.cursor=0;
        boundary.steps={{1,0,{}}};
        Check(mouse.PollForAnalysis().value() && !mouse.GetPositionForAnalysis()
            && mouse.GetPacketsForAnalysis().buttons==std::array<std::uint8_t,8>{},
            "overflow poll needs no uninitialized absolute-position read");
        boundary.trace.clear();boundary.cursor=0;
        spDXMouse::CacheForAnalysis cache{};cache.buttons[1]=1;cache.position={300,200};mouse.SetRuntimeForAnalysis(0x4444,cache);
        boundary.steps={{0,0,{{0,8,0,0,0},{4,std::uint32_t(-250),0,0,0},{12,128,0,0,0},{12,0,0,0,0},{19,128,0,0,0}}}};
        Check(mouse.PollForAnalysis().value(), "successful real buffered poll");
        const auto& changed=mouse.GetCacheForAnalysis().value();
        Check(changed.relativeAxes[0]==8 && changed.relativeAxes[1]==std::uint32_t(-250) && changed.position[0]==308 && changed.position[1]==0,
            "axis wrapping and window clamping");
        Check(changed.buttons[0]==0 && changed.changedButtons[0]==1 && changed.buttons[1]==1 && changed.buttons[7]==1,
            "ordered button records and untouched button retention");
        Check(mouse.PhysicalSlot1ForAnalysis(107).value() && mouse.PhysicalSlot2ForAnalysis(100).value() && mouse.PhysicalSlot3ForAnalysis(108).value()==308 && mouse.PhysicalSlot4ForAnalysis(109).value()==-250,
            "poll then physical cached queries");
        boundary.trace.clear();boundary.cursor=0;boundary.steps={{0x8007001e,3,{}},{1,0,{}}};
        Check(mouse.PollForAnalysis().value() && mouse.GetCacheForAnalysis()->buttons[1]==0 && mouse.GetStateForAnalysis().acquired45==1,
            "lost input then overflow clears packets and preserves acquired flag");
        Check(boundary.trace==std::vector<std::vector<std::uint32_t>>{{1,2,20,1024,0},{2,3},{1,3,20,1024,0}},"COM pointer reread and exact retry sequence");
        boundary.trace.clear();boundary.cursor=0;boundary.steps={{0x80004005,0,{}},{0x80004005,0,{}},{0x80004005,0,{}}};
        Check(!mouse.PollForAnalysis().value() && mouse.GetStateForAnalysis().acquired45==0 && boundary.trace.back()==std::vector<std::uint32_t>{4},
            "three errors log and deactivate");
        Check(!mouse.PhysicalSlot1ForAnalysis(107).value() && mouse.PhysicalSlot3ForAnalysis(108).value()==0,
            "failure state gates subsequent queries");
        boundary.rectangle={123,456,0,0};Check(mouse.ProcessEventsForAnalysis(nullptr,0) && mouse.GetCacheForAnalysis()->position[0]==0xffffffffu,
            "original upper-bound-first empty rectangle behavior");
        Boundary startup;spDXMouse started(startup);startup.mouse=&started;startup.rectangle={123,45,0,-20};
        startup.configuration[0]='t';startup.configuration[1]='r';startup.configuration[2]='u';startup.configuration[3]='e';
        for(auto& step:startup.startupSteps)step[0]=0x80004005;startup.startupSteps[1][1]=3;
        Check(started.StartupForAnalysis(0x4444,startup).value(),"startup ignores failing HRESULT with established outputs");
        Check(started.GetCacheForAnalysis() && started.GetPacketsForAnalysis().buttons==std::array<std::uint8_t,8>{}
            && started.GetPositionForAnalysis().value()==std::array<std::uint32_t,2>{std::uint32_t(-61),std::uint32_t(-32)},
            "startup centers signed wrapped rectangle and retains original zero packets");
        Check(started.GetExclusiveFlagForAnalysis()==1 && started.GetStateForAnalysis().field44==1 && started.GetStateForAnalysis().acquired45==0
            && started.GetStateForAnalysis().field80==1,"startup final flags and acquired retention");
        Check(startup.trace==std::vector<std::vector<std::uint32_t>>{{3,5},{5,1,0x727ea0,0},{6,2,44},
            {7,0x6f3170,0x6f3178,0x6db048,128,0x6f3188},{8,3,0x712b6c},{9,3,4,5},{10,3,1,20,16,0,0,1024},{2,3}},
            "startup exact call order, app/runtime window split and COM device rereads");
        Boundary missing;spDXMouse unsafe(missing);missing.mouse=&unsafe;missing.createdDevice=0;
        Check(!unsafe.StartupForAnalysis(0x4444,missing) && missing.trace.size()==2 && unsafe.GetStateForAnalysis().field44==0,
            "unsafe native missing-device dereference remains explicit host guard");
        Boundary mismatch;spDXMouse nonexclusive(mismatch);mismatch.mouse=&nonexclusive;mismatch.configuration={'t','r','u','e','x'};
        Check(nonexclusive.StartupForAnalysis(0x4444,mismatch).value() && nonexclusive.GetExclusiveFlagForAnalysis()==0
            && mismatch.trace[5]==std::vector<std::uint32_t>{9,2,4,6},"configuration compares exact five bytes including terminator");
        startup.trace.clear();Check(started.SetCursorVisibleForAnalysis(1,startup) && startup.trace.empty(),
            "original factory visibility one makes same-value cursor call a no-op");
        started.SetCursorRuntimeForAnalysis(1,{4,5});startup.trace.clear();
        Check(started.SetCursorVisibleForAnalysis(1,startup) && startup.trace.empty(),"unchanged cursor byte suppresses foreign calls");
        Check(started.SetCursorVisibleForAnalysis(0,startup) && startup.trace==std::vector<std::vector<std::uint32_t>>{{14,4},{15},{13,0}}
            && started.GetCursorPositionForAnalysis()->x==21,"cursor hide captures current screen position");
        startup.trace.clear();Check(started.SetCursorVisibleForAnalysis(255,startup) && startup.trace==std::vector<std::vector<std::uint32_t>>{{11},{12,21,31},{13,1}}
            && started.GetCursorVisibleFlagForAnalysis()==255,"cursor show uses any nonzero byte and retains exact flag");
        started.SetExclusiveFlagForAnalysis(255);Check(started.GetExclusiveFlagForAnalysis()==255,"original direct exclusive-byte setter");
        Boundary integrated;spDXMouse logicalMouse(integrated);integrated.mouse=&logicalMouse;
        const auto logicalQueries=logicalMouse.GetQueriesForAnalysis();
        Check(logicalQueries.slot1 && logicalQueries.slot2 && logicalQueries.slot3
            && logicalQueries.slot4 && logicalQueries.slot5 && logicalQueries.slot6,
            "borrowed mouse provider binds all six original physical slots");
        integrated.configuration={'t','r','u','e',0};
        Check(logicalMouse.StartupForAnalysis(0x4444,integrated).value(),"logical mouse startup");
        integrated.steps={{0,0,{{12,128,0,0,0},{4,std::uint32_t(-250),0,0,0},
            {8,7,0,0,0},{19,128,0,0,0}}}};
        Check(logicalMouse.PollForAnalysis().value(),"logical mouse buffered poll");
        logicalMouse.AppendBindingForAnalysis(100,1);
        logicalMouse.AppendBindingForAnalysis(107,1);
        logicalMouse.AppendBindingForAnalysis(108,2);
        logicalMouse.AppendBindingForAnalysis(109,2);
        logicalMouse.AppendBindingForAnalysis(109,3);
        logicalMouse.AppendBindingForAnalysis(110,4);
        const auto operationTrace=integrated.trace;
        Check(logicalMouse.QuerySlot1ForAnalysis(1,logicalQueries).value()
            && logicalMouse.QuerySlot2ForAnalysis(1,logicalQueries).value(),
            "inherited logical booleans consume mouse current and changed buttons");
        Check(logicalMouse.QuerySlot3ForAnalysis(2,logicalQueries).value()==320
            && logicalMouse.QuerySlot4ForAnalysis(3,logicalQueries).value()==-250
            && logicalMouse.QuerySlot4ForAnalysis(4,logicalQueries).value()==7,
            "inherited logical scalars use first binding and live clamped position or relative axis");
        Check(logicalMouse.CommandSlot5ForAnalysis(1,17,0xffffffffu,0x12345678u,logicalQueries)
            && logicalMouse.QuerySlot6ForAnalysis(1,logicalQueries).value()==0.0f
            && integrated.trace==operationTrace,
            "logical command and float use the original empty and zero mouse slots without a foreign call");
        Check(logicalMouse.QuerySlot3ForAnalysis(999,logicalQueries).value()==0
            && !logicalMouse.QuerySlot1ForAnalysis(999,logicalQueries).value(),
            "unbound logical query keeps common input zero result");
        integrated.cursor=0;integrated.steps={{0x80004005,0,{}},{0x80004005,0,{}},{0x80004005,0,{}}};
        Check(!logicalMouse.PollForAnalysis().value()
            && !logicalMouse.QuerySlot1ForAnalysis(1,logicalQueries).value()
            && logicalMouse.QuerySlot3ForAnalysis(2,logicalQueries).value()==0
            && logicalMouse.QuerySlot4ForAnalysis(3,logicalQueries).value()==0,
            "same borrowed callbacks observe later poll failure and original acquired gate");
        Boundary unknown;spDXMouse unknownPosition(unknown);unknown.mouse=&unknownPosition;
        auto unknownState=unknownPosition.GetStateForAnalysis();unknownState.acquired45=1;unknownPosition.SetStateForAnalysis(unknownState);
        unknownPosition.AppendBindingForAnalysis(108,1);
        const auto unknownQueries=unknownPosition.GetQueriesForAnalysis();
        bool unknownRejected=false;
        try { (void)unknownPosition.QuerySlot3ForAnalysis(1,unknownQueries); }
        catch(const std::logic_error&) { unknownRejected=true; }
        Check(unknownRejected && unknown.trace.empty() && !unknownPosition.GetPositionForAnalysis(),
            "active logical position query rejects unspecified original absolute X/Y without a fabricated value");
        unknownState.acquired45=0;unknownPosition.SetStateForAnalysis(unknownState);
        Check(unknownPosition.QuerySlot3ForAnalysis(1,unknownQueries).value()==0,
            "inactive original position slot returns zero before reading unspecified X/Y");
        spCloneManager manager;started.SetName("mouse clone");
        auto cloned=manager.Clone(started);Check(cloned && cloned->IsExactly(spDXMouse::ClassID)
            && std::string(static_cast<spDXMouse*>(cloned.get())->GetName())=="mouse clone"
            && !static_cast<spDXMouse*>(cloned.get())->GetCacheForAnalysis()
            && static_cast<spDXMouse*>(cloned.get())->GetExclusiveFlagForAnalysis()==0,
            "real recovered clone factory creates fresh packets/runtime then maps and copies only inherited name");
        started.SetCloneFactoryForAnalysis([]{return std::unique_ptr<spDXMouse>{};});Check(!manager.Clone(started),"native factory miss returns null clone");
        struct CopyFailure final : spDXMouse
        {
            using spDXMouse::spDXMouse;
            mutable bool mapped = false;
            bool vfunc_14(spBaseObject& destination,spCloneManager& cloneManager) const override
            { mapped = cloneManager.FindClone(*this)==&destination;return false; }
        };
        CopyFailure failure(startup);failure.SetCloneFactoryForAnalysis([&]{return std::make_unique<spDXMouse>(startup);});
        const auto releases=startup.released;
        Check(!manager.Clone(failure) && failure.mapped && startup.released==releases+1 && !manager.FindClone(failure),
            "clone maps before virtual Copy and destroys failed destination before root-map cleanup");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string(argv[1])=="--batch")return Batch();
        if(argc==2 && std::string(argv[1])=="--startup-batch")return StartupBatch();
        if(argc==2 && std::string(argv[1])=="--cursor-batch")return CursorBatch();
        if(argc==2 && std::string(argv[1])=="--queries-batch")return QueriesBatch();
        Tests();std::cout<<"PASS "<<checks<<'/'<<checks<<": DX mouse startup, polling, cursor and clone-dispatch checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
