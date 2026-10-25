#include "Analysis/PC/spRendererQueueMath.h"
#include "Analysis/PC/SparkplugAbi.h"
#include "Analysis/Host/LegacySortPolicy.h"
#include "Code/Sparkplug/spModel.h"
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace math = sparkplug::evidence::pc::renderer_queue_math;
namespace
{
    using Phase = spRenderable::CallbackPhaseForAnalysis;
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    template <std::size_t N> void Read(std::array<float, N>& values)
    {
        for (auto& value : values)
            if (!(std::cin >> value))
                throw std::runtime_error("incomplete batch row");
    }
    struct Context
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> calls;
        std::int32_t direct = 1;
        int directCalls = 0;
        bool testGuards = false;
        bool guardsPassed = false;
    };
    struct User
    {
        Context* context;
        std::uint32_t id;
        std::int32_t result;
    };
    std::int32_t Group(spRenderable* model, spCamera*, void*, std::uint32_t ordinal, void* data)
    {
        auto& user = *static_cast<User*>(data);
        user.context->calls.emplace_back(ordinal, user.id);
        if (user.context->testGuards)
            user.context->guardsPassed =
                !model->ClearGroupCallbacksForAnalysis(Phase::Pre) &&
                !model->AddGroupCallbackForAnalysis(Phase::Pre, Group, data) &&
                !model->DispatchCallbackPhaseForAnalysis(Phase::Post, nullptr, nullptr);
        return user.result;
    }
    std::int32_t Direct(spRenderable*, spCamera*, void* data)
    {
        auto& context = *static_cast<Context*>(data);
        ++context.directCalls;
        return context.direct;
    }
    void Configure(spModel& model, Phase phase, Context& context, std::vector<User>& users,
                   bool enabled)
    {
        model.SetDirectCallbackForAnalysis(phase, Direct);
        for (auto& user : users)
            model.AddGroupCallbackForAnalysis(phase, Group, &user);
        model.SetGroupCallbacksEnabledForAnalysis(phase, enabled);
        context.calls.clear();
    }
    using R=spRenderer;
    template<class T>T* Token(std::uintptr_t value){return reinterpret_cast<T*>(value);}
    template<class T>std::uintptr_t Identity(T* value){return reinterpret_cast<std::uintptr_t>(value);}
    struct GeneralSink
    {
        std::vector<std::array<std::uintptr_t,4>> events;
        static void Sort(void* opaque,R::GeneralEntryForAnalysis* records,std::size_t count,
            R::GeneralDispatchForAnalysis::Less less)
        {
            auto& self=*static_cast<GeneralSink*>(opaque);self.events.push_back({1,count,0,0});
            sparkplug::host::LegacySortPolicy::General(nullptr,records,count,less);
        }
        static bool Prepare(void* opaque,void* support)
        {static_cast<GeneralSink*>(opaque)->events.push_back({2,Identity(support),0,0});return false;}
        static bool Render(void* opaque,spRenderable* object,spCamera* camera,void* support)
        {static_cast<GeneralSink*>(opaque)->events.push_back({3,Identity(object),Identity(camera),Identity(support)});return false;}
        static void Phase(void* opaque,std::uint32_t slot,spCamera* camera)
        {static_cast<GeneralSink*>(opaque)->events.push_back({4,slot,Identity(camera),0});}
        R::GeneralDispatchForAnalysis Dispatch(){return {Sort,Prepare,Render,Phase,this};}
    };
    R::GeneralQueueInputForAnalysis QueueInput(std::uintptr_t object,std::uintptr_t support,
        std::uintptr_t camera,std::uint32_t material,std::uintptr_t mesh,
        std::uint32_t mode=0,bool enabled=true)
    {
        // Opaque borrowed identity fixture: no object is dereferenced or owned
        // by these queue methods. Native differential supplies original tokens.
        return {{Token<spRenderable>(object),Token<void>(support),Token<spCamera>(camera),material,mesh},enabled,mode};
    }
    void GeneralQueueTests()
    {
        R::GeneralQueueStateForAnalysis state;GeneralSink sink;
        Check(!R::EnqueueGeneralForAnalysis(state,nullptr)&&state.general.empty(),"C050-disabled gate before null input");
        state.enabled=true;
        auto first=QueueInput(1,11,21,0x80000000,20,0xffffffff,false);
        auto second=QueueInput(2,11,22,1,30),third=QueueInput(3,12,23,1,10);
        Check(R::EnqueueGeneralForAnalysis(state,&first)&&R::EnqueueGeneralForAnalysis(state,&second)
            &&R::EnqueueGeneralForAnalysis(state,&third),"General branch ignores bucket support/mode inputs");
        const auto capacity=state.general.capacity();
        Check(R::FlushGeneralForAnalysis(state,nullptr,{},sink.Dispatch()),"General flush ignores false setup/render replies");
        Check(sink.events==std::vector<std::array<std::uintptr_t,4>>{{1,3,0,0},{2,12,0,0},{3,3,23,12},
            {2,11,0,0},{3,2,22,11},{3,1,21,11}},"unsigned material then mesh order and adjacent support reuse");
        Check(state.general.empty()&&state.general.capacity()==capacity&&state.enabled&&!state.dispatching,
            "General flush retains storage and C050 but clears logical count");
        sink.events.clear();Check(R::FlushGeneralForAnalysis(state,nullptr,{},sink.Dispatch())
            &&sink.events==std::vector<std::array<std::uintptr_t,4>>{{1,0,0,0}},"Empty general flush still calls sort");
        Check(R::EnqueueGeneralForAnalysis(state,&second),"General append before explicit logical clear");
        R::ClearGeneralForAnalysis(state);Check(state.general.empty()&&state.general.capacity()==capacity,"455FE0 clear retains capacity");
        state.nineBucketMode=true;
        auto disabled=QueueInput(0,0,0,0,0,0xffffffff,false);
        Check(R::EnqueueGeneralForAnalysis(state,&disabled),"disabled bucket support succeeds before mode/pointer reads");
        auto invalid=QueueInput(1,11,0,0,0,9);
        Check(!R::EnqueueGeneralForAnalysis(state,&invalid),"out-of-range native mode is explicit host refusal");
        for(unsigned mode:{0u,2u,8u})
        {auto input=QueueInput(mode+1,mode+11,0,0xffffffff,0xffffffff,mode);Check(R::EnqueueGeneralForAnalysis(state,&input),"raw bucket mode selects matching vector");}
        state.general.push_back(second.general); // native common tail must clear this even in bucket branch
        for(unsigned mask=0;mask<32;++mask)
        {
            sink.events.clear();std::array<bool,5> flags{};for(unsigned i=0;i<5;++i)flags[i]=(mask&(1u<<i))!=0;
            Check(R::FlushGeneralForAnalysis(state,Token<spCamera>(99),flags,sink.Dispatch()),"PC bucket no-op phases flush");
            std::vector<std::array<std::uintptr_t,4>> expected;
            const auto add=[&](unsigned slot){expected.push_back({4,slot,99,0});};
            if(flags[0])add(0x20);if(flags[1])add(0x24);if(flags[2])add(0x28);add(0x3c);add(0x40);
            if(flags[3])add(0x2c);if(flags[4])add(0x30);
            Check(sink.events==expected&&state.buckets[0].size()==1&&state.buckets[2].size()==1
                &&state.buckets[8].size()==1&&state.general.empty(),"exact optional phase order; bucket contents survive; general cleared");
        }
        R::ReleaseGeneralForAnalysis(state);Check(!state.general.capacity()&&state.buckets[8].size()==1,"456B10 releases only general vector storage");
        const auto bucketCapacity=state.buckets[8].capacity();R::ClearBucketQueuesForAnalysis(state);
        Check(state.buckets[8].empty()&&state.buckets[8].capacity()==bucketCapacity,"explicit Scene/caller bucket clear retains capacity");
        state.nineBucketMode=false;state.dispatching=true;
        Check(!R::EnqueueGeneralForAnalysis(state,&second)&&!R::FlushGeneralForAnalysis(state,nullptr,{},sink.Dispatch()),"host enqueue/flush reentry guard");
        state.dispatching=false;
    }
    template<class Values>void JsonValues(const Values& values)
    {std::cout<<'[';bool first=true;for(const auto& value:values){if(!first)std::cout<<',';first=false;std::cout<<value;}std::cout<<']';}
    void QueueBatch()
    {
        unsigned bucketMode,enabled,mask,count;
        while(std::cin>>bucketMode>>enabled>>mask>>count)
        {
            if(count>8||mask>31)throw std::runtime_error("bounded queue batch max8/flags31");
            R::GeneralQueueStateForAnalysis state;state.enabled=enabled!=0;state.nineBucketMode=bucketMode!=0;
            std::vector<unsigned> accepted;
            for(unsigned i=0;i<count;++i)
            {
                std::uintptr_t object,support,camera,mesh;std::uint32_t material,mode;unsigned supportEnabled;
                if(!(std::cin>>object>>support>>camera>>material>>mesh>>mode>>supportEnabled))throw std::runtime_error("bad queue input row");
                const auto input=QueueInput(object,support,camera,material,mesh,mode,supportEnabled!=0);
                accepted.push_back(R::EnqueueGeneralForAnalysis(state,&input));
            }
            std::cout<<'[';JsonValues(accepted);std::cout<<",[";bool first=true;
            for(const auto& entry:state.general)
            {if(!first)std::cout<<',';first=false;JsonValues(std::array<std::uintptr_t,5>{Identity(entry.renderable),Identity(entry.support),Identity(entry.camera),entry.materialKey,entry.meshIdentity});}
            std::cout<<"],[";
            for(unsigned i=0;i<9;++i)
            {if(i)std::cout<<',';std::cout<<'[';first=true;for(const auto& entry:state.buckets[i]){if(!first)std::cout<<',';first=false;JsonValues(std::array<std::uintptr_t,2>{Identity(entry.renderable),Identity(entry.support)});}std::cout<<']';}
            GeneralSink sink;std::array<bool,5> flags{};for(unsigned i=0;i<5;++i)flags[i]=(mask&(1u<<i))!=0;
            const auto capacity=state.general.capacity();const bool flushed=R::FlushGeneralForAnalysis(state,Token<spCamera>(99),flags,sink.Dispatch());
            std::cout<<"],[";first=true;for(const auto& event:sink.events){if(!first)std::cout<<',';first=false;JsonValues(event);}
            std::cout<<"],"<<flushed<<','<<state.general.size()<<",[";
            for(unsigned i=0;i<9;++i){if(i)std::cout<<',';std::cout<<state.buckets[i].size();}
            std::cout<<"],"<<(state.general.capacity()==capacity)<<','<<state.enabled<<"]\n";
        }
    }
    void UnitTests()
    {
        GeneralQueueTests();
        for (const auto phase : {Phase::Pre, Phase::Post})
        {
            spModel model;
            Context context;
            std::vector<User> users{{&context, 11, -1},
                                    {&context, 22, 256},
                                    {&context, 33, -1},
                                    {&context, 44, 0},
                                    {&context, 55, 1}};
            Configure(model, phase, context, users, true);
            Check(model.DispatchCallbackPhaseForAnalysis(phase, nullptr, &context),
                  "group stop still permits direct");
            Check(context.calls ==
                      std::vector<std::pair<std::uint32_t, std::uint32_t>>{
                          {0, 11}, {1, 22}, {2, 33}, {3, 44}},
                  "native ordinal advances after stable erase");
            Check(model.GetGroupCallbackCountForAnalysis(phase) == 3 && context.directCalls == 1,
                  "three records retained and direct callback reached");
            context.direct = 256;
            context.calls.clear();
            Check(!model.DispatchCallbackPhaseForAnalysis(phase, nullptr, &context),
                  "direct tests low byte, group tests full word");
            Check(context.calls ==
                      std::vector<std::pair<std::uint32_t, std::uint32_t>>{{0, 22}, {1, 44}},
                  "second phase starts ordinal0 with compacted records");
            model.SetGroupCallbacksEnabledForAnalysis(phase, false);
            context.calls.clear();
            context.direct = -1;
            Check(model.DispatchCallbackPhaseForAnalysis(phase, nullptr, &context) &&
                      context.calls.empty() && model.GetGroupCallbackCountForAnalysis(phase) == 3,
                  "disable view of group does not clear records");
            model.SetField28ForAnalysis(0x12345678);
            auto clone = model.Clone();
            auto* cloned = dynamic_cast<spModel*>(clone.get());
            Check(cloned && cloned->GetField28ForAnalysis() == 0x12345678 &&
                      cloned->GetGroupCallbackCountForAnalysis(phase) == 0,
                  "copy raw28 but not callback records");
        }
        spModel model;
        Context context;
        context.testGuards = true;
        std::vector<User> users{{&context, 1, 1}};
        Configure(model, Phase::Pre, context, users, true);
        Check(model.DispatchCallbackPhaseForAnalysis(Phase::Pre, nullptr, &context) &&
                  context.guardsPassed,
              "explicit host reentry/mutation guards keep vector traversal valid");
        Check(!model.AddGroupCallbackForAnalysis(Phase::Pre, nullptr, nullptr),
              "null callback guard");
        const math::Matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const auto key =
            math::BuildAlphaKey({1, 2, 3}, identity, identity, false, 13, 0xfffffff9, false);
        Check(key.distanceSquared == 14 && key.priority == 6,
              "alpha metric and wrapped unsigned priority");
        Check(
            math::BuildAlphaKey({1, 2, 3}, identity, identity, true, 0, 0, false).distanceSquared ==
                9,
            "camera231 z squared");
        Check(math::CompareAlpha(key, key) == 1, "native equality comparator is plus1");
        Check(math::CompareAlpha({99, 0, false}, {1, 99, true}) == -1, "ordinary before particle");
        Check(math::CompareAlpha({9, 0, true}, {4, 99, true}) == -1, "particle priorities ignored");
        Check(math::CompareAlpha({std::numeric_limits<float>::quiet_NaN(), 0, false},
                                 {1, 0, false}) == 1,
              "unordered distance yields plus1");
        Check(math::LessGeneral(1, 9, 2, 0) && math::LessGeneral(1, 9, 1, 10) &&
                  !math::LessGeneral(1, 9, 1, 9),
              "general ascending lexicographic unsigned key");
    }
    void BatchKeys()
    {
        math::Vector3 center;
        math::Matrix4 world, view;
        while (std::cin >> center[0])
        {
            std::cin >> center[1] >> center[2];
            Read(world);
            Read(view);
            unsigned branch, particle;
            std::uint32_t priority, base;
            if (!(std::cin >> branch >> priority >> base >> particle))
                throw std::runtime_error("bad key flags");
            const auto key = math::BuildAlphaKey(center, world, view, branch != 0, priority, base,
                                                 particle != 0);
            std::cout << std::setprecision(9) << key.distanceSquared << ',' << key.priority << ','
                      << key.exactParticleSystem << '\n';
        }
    }
    void BatchCallbacks()
    {
        unsigned phaseValue, enabled, count;
        std::int32_t direct;
        while (std::cin >> phaseValue >> enabled >> direct >> count)
        {
            if (count > 8)
                throw std::runtime_error("bounded callback batch max8");
            const auto phase = phaseValue ? Phase::Post : Phase::Pre;
            spModel model;
            Context context;
            context.direct = direct;
            std::vector<User> users;
            users.reserve(count);
            for (unsigned i = 0; i < count; ++i)
            {
                std::int32_t result;
                if (!(std::cin >> result))
                    throw std::runtime_error("missing callback result");
                users.push_back({&context, i + 1, result});
            }
            Configure(model, phase, context, users, enabled != 0);
            for (unsigned round = 0; round < 2; ++round)
            {
                context.calls.clear();
                context.directCalls = 0;
                const bool result =
                    model.DispatchCallbackPhaseForAnalysis(phase, nullptr, &context);
                if (round)
                    std::cout << ',';
                std::cout << result << ',' << model.GetGroupCallbackCountForAnalysis(phase) << ','
                          << context.directCalls << ',' << context.calls.size();
                for (const auto& call : context.calls)
                    std::cout << ',' << call.first << ',' << call.second;
            }
            std::cout << '\n';
        }
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--keys-batch")
        {
            BatchKeys();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--callbacks-batch")
        {
            BatchCallbacks();
            return 0;
        }
        if(argc==2&&std::string(argv[1])=="--queues-batch")
        {QueueBatch();return 0;}
        UnitTests();
        std::cout << "PASS " << checks << '/' << checks
                  << ": renderer protocol CPU/source checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
