#include "Analysis/PC/spRendererQueueMath.h"
#include "Analysis/PC/SparkplugAbi.h"
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
    void UnitTests()
    {
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
