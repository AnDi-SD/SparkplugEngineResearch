#include "Code/Sparkplug/spActor.h"
#include "pcActorScenario.h"
#include "pcActorControlScenario.h"
#include "pcActorControlPipeline.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* text)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(text);
    }
    using Action = spActor::ActionForAnalysis;
    using Kind = spActor::ActionKindForAnalysis;
    using Payload = spActor::EventPayloadForAnalysis;
    using Request = spActor::StartRequestForAnalysis;
    spTransformTrackEval& Eval(spActor& actor, std::size_t index)
    {
        return *dynamic_cast<spTransformTrackEval*>(
            actor.GetOwnedControllerForAnalysis(index)->GetEvaluatorForAnalysis());
    }
    std::unique_ptr<spAnimation> Animation(spAnimationManager& manager, float offset = 0)
    {
        auto animation = std::make_unique<spAnimation>();
        Check(animation->SetTotalTimeForAnalysis(2), "animation duration");
        int ordinal = 0;
        for (const char* name : {"Head", "Spine"})
        {
            auto* track = animation->AppendTrackForAnalysis();
            track->SetName(name);
            spAnimTrack::TrackDataForAnalysis data{};
            const float start = offset + ordinal++;
            data[0][0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{
                1,
                {0, 1, 2},
                {start, start * 2, start * 3, start + 10, start * 2 + 20, start * 3 + 30,
                 start + 20, start * 2 + 40, start * 3 + 60}};
            Check(track->SetKeysForAnalysis(std::move(data)) && track->BindNameForAnalysis(manager),
                  "prepared owned track");
        }
        return animation;
    }
    struct Fixture
    {
        spAnimationManager manager;
        std::array<std::unique_ptr<spAnimation>, 3> animations;
        std::shared_ptr<spNode> root = std::make_shared<spNode>();
        std::array<std::shared_ptr<spNode>, 2> nodes{std::make_shared<spNode>(),
                                                     std::make_shared<spNode>()};
        std::unique_ptr<spActor> actor;
        std::vector<Action> actions;
        std::string error;
        Fixture()
        {
            for (std::size_t i = 0; i < animations.size(); ++i)
                animations[i] = Animation(manager, static_cast<float>(i * 100));
            root->SetName("Head"); // wrapper must skip eligible supplied root
            nodes[0]->SetName("Head");
            nodes[1]->SetName("Spine");
            Check(root->AttachChildForAnalysis(nodes[0]) &&
                      nodes[0]->AttachChildForAnalysis(nodes[1]),
                  "three-level tree");
            actor = std::make_unique<spActor>();
            Check(actor->BindDescendantsForAnalysis(*root, &error), "actor descendant binding");
        }
        Request RequestFor(std::size_t index = 0, std::uint32_t fade = 0)
        {
            Request request;
            request.animation = animations[index].get();
            request.fadeMode = fade;
            request.weight = .25f;
            request.fallbackFadeInRate = .5f;
            request.fallbackFadeOutRate = .75f;
            return request;
        }
        std::optional<std::size_t> Start(std::size_t index = 0, std::uint32_t fade = 0)
        {
            auto request = RequestFor(index, fade);
            return actor->StartForAnalysis(request, actions, &error);
        }
    };
    void DiscoveryAndFrame()
    {
        Fixture f;
        Check(f.actor->GetOwnedControllerCountForAnalysis() == 2 &&
                  f.actor->GetOwnedControllerForAnalysis(0)->GetNodeForAnalysis() ==
                      f.nodes[0].get() &&
                  f.actor->GetOwnedControllerForAnalysis(1)->GetNodeForAnalysis() ==
                      f.nodes[1].get(),
              "preorder descendants, root skipped");
        Check(!(f.root->GetFlagsForAnalysis() & 0x2000) &&
                  (f.nodes[0]->GetFlagsForAnalysis() & 0x2000),
              "exact ownership mark");
        Check(f.actor->BindDescendantsForAnalysis(*f.root) &&
                  f.actor->GetOwnedControllerCountForAnalysis() == 2,
              "repeat wrapper skips marked nodes");
        Check(f.manager.FindNameForAnalysis("Head")->references == 4,
              "three resources plus one node reference");
        Check(!f.actor->SetControllerBindingsForAnalysis({}),
              "manual seam cannot replace owned controllers");
        auto request = f.RequestFor();
        request.fadeInDuration = 2;
        request.fadeOutDuration = .25f;
        request.initialTime = .5f;
        request.callbackCookie = 0x1234;
        f.animations[0]->SetPriorityGroupForAnalysis(3);
        Check(f.actor->StartForAnalysis(request, f.actions, &f.error) == 0 && f.error.empty(),
              "first start selected0");
        const auto& state = *f.actor->GetPlaybackForAnalysis(0);
        Check(request.weight == 1 && state.weight == 1 && state.fadeInRate == .5f &&
                  state.fadeOutRate == 4 && state.normalizedProgress == .25f &&
                  state.sampleTime == 0 && state.callbackCookie == 0x1234,
              "original start request mutation/rates/progress and inherited sample time");
        Check(state.bindingUseCount == 2 && state.priority == 0x03000001 &&
                  state.fadeThreshold == 1.75f,
              "state use count and group/frame priority");
        Check(f.actions.size() == 3 && f.actions[0].eventCode == 2 &&
                  f.actions[0].payload == Payload::Animation && f.actions[1].eventCode == 4 &&
                  f.actions[1].payload == Payload::Playback && f.actions[2].kind == Kind::Flush,
              "start and gained event carry distinct payload roles before flush");
        Check(Eval(*f.actor, 0).GetPhysicalInputsForAnalysis()[0].playback == &state.evaluation &&
                  state.evaluation.bindingUseCount == &state.bindingUseCount,
              "committed input pointers reference live actor state");
        Check(f.manager.AdvanceFrameForAnalysis(.5f),
              "actual manager to owned actor/controller/evaluator tick");
        Check(state.sampleTime == 1 &&
                  f.nodes[0]->GetPositionForAnalysis() == spNode::Vector3{10, 20, 30} &&
                  f.nodes[1]->GetPositionForAnalysis() == spNode::Vector3{11, 22, 33},
              "prepared keys applied to both native-named nodes");
        Check(f.root->UpdateWorldForAnalysis() &&
                  f.nodes[1]->GetWorldPositionForAnalysis() == spNode::Vector3{21, 42, 63},
              "owned frame reaches parent/child world update");
        Check(f.actor->RebindForAnalysis({}, f.actions, &f.error) && state.bindingUseCount == 4 &&
                  f.actions.empty(),
              "exclusive rebind preserves original counter asymmetry");
        auto clone = f.actor->Clone();
        auto* cloned = dynamic_cast<spActor*>(clone.get());
        Check(cloned && cloned->GetOwnedControllerCountForAnalysis() == 0 &&
                  !cloned->GetPlaybackForAnalysis(0)->animation,
              "actor clone does not duplicate tree/input payload");
        f.actor.reset();
        Check(!(f.nodes[0]->GetFlagsForAnalysis() & 0x2000) &&
                  f.manager.FindNameForAnalysis("Head")->references == 3,
              "actor destruction clears mark and only its registry reference");
    }
    void RestartAndStop()
    {
        Fixture f;
        Check(f.Start(0, 2) == 0, "blended start0");
        auto& first = *f.actor->GetPlaybackForAnalysis(0);
        first.weight = .625f;
        first.sampleTime = .875f;
        first.stopAfterFade = true;
        auto request = f.RequestFor(0);
        Check(f.actor->StartForAnalysis(request, f.actions, &f.error) == 0 && request.weight == 1 &&
                  first.weight == .625f && first.sampleTime == .875f && first.stopAfterFade,
              "active restart preserves state fields");
        Check(f.actions.size() == 2 && f.actions[0].eventCode == 2 &&
                  f.actions[1].kind == Kind::Flush,
              "restart has no duplicate gained event");
        Check(f.actor->StopForAnalysis(f.animations[2].get(), false, f.actions, &f.error) &&
                  f.actions.empty(),
              "missing Stop no-op");
        Check(f.actor->StopForAnalysis(f.animations[0].get(), true, f.actions, &f.error) &&
                  f.actions.size() == 1 && f.actions[0].kind == Kind::Flush &&
                  first.bindingUseCount == 0 && !first.running && first.status == 3,
              "suppressed Stop clears state then flushes");
        Check(Eval(*f.actor, 0).GetInputCountForAnalysis() == 1 &&
                  !Eval(*f.actor, 0).GetPhysicalInputsForAnalysis()[0].playback,
              "Stop clears pointers, not input count");
        Check(f.actor->StopForAnalysis(f.animations[0].get(), false, f.actions, &f.error) &&
                  f.actions.size() == 2 && f.actions[0].kind == Kind::Flush &&
                  f.actions[1].kind == Kind::ImmediateEvent && f.actions[1].eventCode == 3,
              "matching inactive Stop flushes before immediate3");
        Check(f.Start(0, 2) == 0 && f.Start(1, 2) == 1, "two blended states");
        Check(f.actor->StopForAnalysis(f.animations[0].get(), false, f.actions, &f.error) &&
                  f.actor->GetPlaybackForAnalysis(1)->bindingUseCount == 2 &&
                  Eval(*f.actor, 0).GetPhysicalInputsForAnalysis()[0].playback ==
                      &f.actor->GetPlaybackForAnalysis(1)->evaluation,
              "remaining blended state promoted with balanced counter");
        Check(f.actor->StopAllForAnalysis(f.actions, &f.error) && f.actions.size() == 2 &&
                  f.actions[1].playbackIndex == 1,
              "StopAll skips previously inactive state");
        Check(f.actor->StopAllForAnalysis(f.actions, &f.error) && f.actions.empty(),
              "empty StopAll no-op");
    }
    void ThirdInputHostFence()
    {
        Fixture f;
        Check(f.Start(0, 2) == 0 && f.Start(1, 2) == 1, "two allowed inputs before host fence");
        auto request = f.RequestFor(2, 2);
        request.weight = .75f;
        const auto before = Eval(*f.actor, 0).GetPhysicalInputsForAnalysis();
        Check(!f.actor->StartForAnalysis(request, f.actions, &f.error) && !f.error.empty() &&
                  f.actions.empty(),
              "third overlapping input rejected before publishing actions");
        Check(request.weight == .75f && !f.actor->GetPlaybackForAnalysis(2)->animation &&
                  f.actor->GetPlaybackForAnalysis(0)->bindingUseCount == 2 &&
                  f.actor->GetPlaybackForAnalysis(1)->bindingUseCount == 2,
              "host atomic failure preserves request and all playback states");
        const auto after = Eval(*f.actor, 0).GetPhysicalInputsForAnalysis();
        Check(after[0].playback == before[0].playback && after[1].playback == before[1].playback &&
                  after[0].cache.position == before[0].cache.position &&
                  after[1].cache.position == before[1].cache.position,
              "host third-input failure preserves published pointers/caches");
        // Exclusive request can safely replace two inputs; this is not a blanket
        // limit of two simultaneous actor state slots.
        request.fadeMode = 0;
        f.animations[2]->SetPriorityGroupForAnalysis(4);
        Check(f.actor->StartForAnalysis(request, f.actions, &f.error) == 2 &&
                  Eval(*f.actor, 0).GetInputCountForAnalysis() == 1,
              "safe high-priority exclusive replacement remains possible");
    }
    void DuplicateNodesAndUnmatchedStart()
    {
        Fixture f;
        auto duplicate = std::make_shared<spNode>();
        duplicate->SetName("Head");
        Check(f.actor->DiscoverNodeForAnalysis(duplicate) &&
                  f.actor->GetOwnedControllerCountForAnalysis() == 3,
              "duplicate name still owns controller");
        Check(f.Start() == 0 && Eval(*f.actor, 0).GetInputCountForAnalysis() == 0 &&
                  Eval(*f.actor, 2).GetInputCountForAnalysis() == 1 &&
                  f.actor->GetPlaybackForAnalysis(0)->bindingUseCount == 2,
              "last duplicate name wins map without independent slot");
        spActor empty;
        auto request = f.RequestFor();
        Check(empty.StartForAnalysis(request, f.actions, &f.error) == 0 &&
                  !empty.GetPlaybackForAnalysis(0)->bindingUseCount && f.actions.size() == 2 &&
                  f.actions[0].eventCode == 2,
              "unmatched start returns selected unused slot");
        spActor second;
        Check(second.BindDescendantsForAnalysis(*f.root) &&
                  second.GetOwnedControllerCountForAnalysis() == 0,
              "second actor cannot rediscover nodes owned by first");
        f.actor.reset();
        Check(second.BindDescendantsForAnalysis(*f.root) &&
                  second.GetOwnedControllerCountForAnalysis() == 2,
              "released marks permit later actor binding");
    }
    void ControlOwnershipGuards()
    {
        struct Restore{std::size_t value=spActor::GetDefaultPlaybackCapacityForAnalysis();~Restore(){(void)spActor::SetDefaultPlaybackCapacityForAnalysis(value);}} restore;
        Fixture f;
        Check(spActor::SetDefaultPlaybackCapacityForAnalysis(2),"configured factory capacity");
        spActor empty;Check(empty.GetPlaybackCapacityForAnalysis()==2,"new actor consumes current default");
        auto clone=f.actor->Clone();Check(dynamic_cast<spActor*>(clone.get())->GetPlaybackCapacityForAnalysis()==2,"clone uses current factory default, not source extent");
        Check(!spActor::SetDefaultPlaybackCapacityForAnalysis(41)&&spActor::GetDefaultPlaybackCapacityForAnalysis()==2,"oversized default rejected without mutation");
        Check(!f.actor->ResetPlaybackCapacityForAnalysis(19),"bound actor state views cannot be invalidated by host reset");
        Check(empty.ResetPlaybackCapacityForAnalysis(19)&&empty.GetPlaybackForAnalysis(18)->slotIndex==18,"bounded empty reset initializes indices");
        empty.GetPlaybackForAnalysis(0)->running=true;
        Check(!empty.ResetPlaybackCapacityForAnalysis(0)&&empty.GetPlaybackCapacityForAnalysis()==19,"active reset rejected before replacement");
        empty.GetPlaybackForAnalysis(0)->running=false;
        Check(empty.ResetPlaybackCapacityForAnalysis(0)&&!empty.GetPlaybackForAnalysis(0),"empty zero capacity supported");
        Check(!f.actor->FadeOutAndStopForAnalysis(nullptr,1,0),"host null fade resource guard");
        Check(!f.actor->FadeOutAndStopForAnalysis(f.animations[0].get(),std::numeric_limits<float>::quiet_NaN(),0),"host finite fade guard");
        Check(f.actor->GetPlaybackForAnalysis(0)->fadeMode==0,"invalid fade requests leave states unchanged");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--control")
        {std::cout<<sparkplug::tests::ActorControlScenario(argv[2])<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--control-pipeline")
        {std::cout<<sparkplug::tests::ActorControlPipeline(argv[2],argv[3])<<'\n';return 0;}
        if (argc == 4 && std::string(argv[1]) == "--scenario")
            return sparkplug::tests::ActorScenario(argv[2], argv[3]);
        if (argc != 1)
            throw std::runtime_error("Expected --scenario <bounded SAN> <name>");
        DiscoveryAndFrame();
        RestartAndStop();
        ThirdInputHostFence();
        DuplicateNodesAndUnmatchedStart();
        ControlOwnershipGuards();
        std::cout << "PASS " << checks << '/' << checks
                  << ": owned actor binding/start/stop checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
