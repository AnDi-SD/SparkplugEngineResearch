#include "Code/Sparkplug/spNodeController.h"
#include "Code/Sparkplug/spTransformTrackEval.h"
#include "Code/Sparkplug/spSkin.h"
#include "Analysis/PC/SparkplugAbi.h"
#include "Analysis/PC/spAnimationMath.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Require(bool value, const char* message)
    {
        ++checks;
        if (!value)
        {
            std::cerr << "FAIL " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.00001F;
    }
    class ConstantProbe final : public spTransformEval
    {
      public:
        SampleForAnalysis sample;
        float lastTime = -1;
        SampleForAnalysis EvaluateForAnalysis(float time) override
        {
            lastTime = time;
            return sample;
        }
    };
} // namespace
int main()
{
    namespace math = sparkplug::evidence::pc::animation_math;
    using Sample = spTransformEval::SampleForAnalysis;
    using Input = spTransformTrackEval::InputForAnalysis;
    using State = spTransformTrackEval::PlaybackForAnalysis;
    using Sampler = spTransformTrackEval::TrackSamplerForAnalysis;
    spTransformTrackEval evaluator;
    Require(evaluator.IsKindOf(spTransformEval::ClassID) &&
                evaluator.IsKindOf(spEvaluator::ClassID),
            "original evaluator RTTI chain");
    Require(evaluator.GetBoundSlotForAnalysis() == -1 && evaluator.GetInputsForAnalysis().empty(),
            "evaluator default binding and inputs");
    auto empty = evaluator.EvaluateForAnalysis(9);
    Require(!empty.hasPosition && !empty.hasRotation && !empty.hasScale,
            "empty evaluator validity");
    State state{1, 7}, secondState{3, 11};
    float timeSeen = -1;
    Sample a{{10, 20, 30}, {0, 0, 0, 1}, {2, 3, 4}, true, true, true};
    Sample b{{30, 40, 50}, {0, 0, 1, 0}, {6, 7, 8}, true, true, true};
    Sampler first = [&](float time, spTransformTrackEval::KeyCacheForAnalysis& cache) {
        timeSeen = time;
        ++cache.position[0];
        ++cache.rotation[1];
        ++cache.scale[2];
        return a;
    };
    Sampler second = [&](float, spTransformTrackEval::KeyCacheForAnalysis&) { return b; };
    Require(evaluator.SetInputsForAnalysis({Input{&state, &first}}), "set single input");
    auto single = evaluator.EvaluateForAnalysis(999);
    Require(timeSeen == 7 && single.position == a.position && single.rotation == a.rotation &&
                single.scale == a.scale,
            "state time overrides outer time, single PRS output");
    Require(evaluator.GetInputsForAnalysis()[0].cache.position[0] == 1 &&
                evaluator.GetInputsForAnalysis()[0].cache.rotation[1] == 1 &&
                evaluator.GetInputsForAnalysis()[0].cache.scale[2] == 1,
            "three caches are persistent");
    Require(evaluator.SetInputsForAnalysis({Input{&state, &first}, Input{&secondState, &second}}),
            "set two weighted inputs");
    auto mixed = evaluator.EvaluateForAnalysis(0);
    Require(mixed.position == spNode::Vector3{25, 35, 45} &&
                mixed.scale == spNode::Vector3{5, 6, 7},
            "cumulative weighted position and scale");
    Require(Near(mixed.rotation[2], std::sin(0.75F * 1.57079632679F)) &&
                Near(mixed.rotation[3], std::cos(0.75F * 1.57079632679F)),
            "weighted quaternion interpolation");
    state.weight = 0;
    mixed = evaluator.EvaluateForAnalysis(0);
    Require(mixed.position == b.position && mixed.scale == b.scale && mixed.rotation == a.rotation,
            "native zero-old-weight quaternion branch preserves first rotation");
    state.weight = 1;
    a.hasPosition = false;
    a.hasRotation = false;
    b.hasScale = false;
    mixed = evaluator.EvaluateForAnalysis(0);
    Require(mixed.position == b.position && mixed.scale == a.scale && mixed.rotation == b.rotation,
            "per-channel validity chooses first valid sample");
    Require(!evaluator.SetInputsForAnalysis(std::vector<Input>(3)) &&
                evaluator.GetInputsForAnalysis().size() == 2,
            "host capacity rejection is atomic");
    Require(!evaluator.SetInputsForAnalysis({Input{nullptr, &first}}),
            "host rejects sampler without state");
    Require(evaluator.SetInputsForAnalysis({Input{}, Input{&secondState, &second}}),
            "allow null-track hole");
    Require(evaluator.EvaluateForAnalysis(0).position == b.position,
            "null track preserves second input");
    evaluator.SetBoundSlotForAnalysis(47);
    auto clonedEval = evaluator.Clone();
    const auto* trackClone = dynamic_cast<const spTransformTrackEval*>(clonedEval.get());
    Require(trackClone && trackClone->GetBoundSlotForAnalysis() == -1 &&
                trackClone->GetInputsForAnalysis().empty(),
            "native blank evaluator clone");

    spNodeController controller;
    Require(controller.IsKindOf(spSubController::ClassID) &&
                dynamic_cast<spTransformTrackEval*>(controller.GetEvaluatorForAnalysis()),
            "node controller RTTI and default evaluator");
    controller.ApplyForAnalysis(0); // host safety for unattached controller
    auto node = std::make_shared<spNode>();
    std::weak_ptr<spNode> retainedNode = node;
    controller.SetNodeForAnalysis(node);
    node.reset();
    Require(!retainedNode.expired(), "controller retains node ownership");
    node = retainedNode.lock();
    auto probe = std::make_unique<ConstantProbe>();
    auto* observed = probe.get();
    observed->sample = Sample{{10, 20, 30}, {0, 0, 1, 0}, {2, 3, 4}, true, true, true};
    controller.SetEvaluatorForAnalysis(std::move(probe));
    controller.ApplyForAnalysis(3.25F);
    Require(observed->lastTime == 3.25F &&
                node->GetPositionForAnalysis() == observed->sample.position &&
                node->GetScaleForAnalysis() == observed->sample.scale,
            "direct node PRS and time");
    Require(node->GetOrientationForAnalysis() == math::ToMatrix(observed->sample.rotation) &&
                (node->GetFlagsForAnalysis() & 1U) != 0,
            "quaternion matrix and proven local dirty bit");
    node = std::make_shared<spNode>();
    node->SetPositionForAnalysis({1, 2, 3});
    controller.SetNodeForAnalysis(node);
    observed->sample.hasRotation = false;
    controller.BlendForAnalysis(4, .25F);
    Require(node->GetPositionForAnalysis() == spNode::Vector3{3.25F, 6.5F, 9.75F} &&
                node->GetScaleForAnalysis() == spNode::Vector3{2, 3, 4},
            "blend lerps position but copies scale");
    node = std::make_shared<spNode>();
    controller.SetNodeForAnalysis(node);
    observed->sample.hasScale = false;
    observed->sample.position = {.0001F, 0, 0};
    controller.BlendForAnalysis(5, .5F);
    Require(node->GetPositionForAnalysis() == spNode::Vector3{} &&
                (node->GetFlagsForAnalysis() & 1U) == 0,
            "position epsilon skips dirty/write");
    observed->sample.position = {2, 4, 6};
    controller.BlendForAnalysis(5, 1.5F);
    Require(node->GetPositionForAnalysis() == spNode::Vector3{3, 6, 9},
            "native position transition factor is not clamped");
    observed->sample.hasPosition = false;
    observed->sample.hasRotation = true;
    controller.BlendForAnalysis(6, .5F);
    const auto rotated = node->GetOrientationForAnalysis();
    Require(Near(rotated[0], 0) && Near(rotated[1], 1) && Near(rotated[3], -1),
            "blend rotates around native positive Z convention");
    auto clonedController = controller.Clone();
    const auto* controllerClone = dynamic_cast<const spNodeController*>(clonedController.get());
    Require(controllerClone && !controllerClone->GetNodeForAnalysis() &&
                dynamic_cast<spTransformTrackEval*>(controllerClone->GetEvaluatorForAnalysis()),
            "native blank controller clone has fresh track evaluator");
    controller.SetNodeForAnalysis(nullptr);
    node.reset();
    controller.SetEvaluatorForAnalysis(nullptr);
    controller.BlendForAnalysis(0, 1);

    const math::Quaternion identity{0, 0, 0, 1}, negativeIdentity{0, 0, 0, -1};
    Require(math::Interpolate(identity, negativeIdentity, .5F) == identity,
            "shortest quaternion hemisphere");
    Require(math::Interpolate(identity, identity, 1) == identity,
            "small-angle native first-value fallback");
    for (const math::Quaternion q : {identity, math::Quaternion{1, 0, 0, 0},
                                     math::Quaternion{0, 1, 0, 0}, math::Quaternion{0, 0, 1, 0}})
    {
        const auto recovered = math::FromMatrix(math::ToMatrix(q));
        Require(recovered == q, "trace and three maximum-diagonal matrix branches");
    }
    const spSkin::Matrix4 inverseBind{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 0, 0, 1};
    const spSkin::Matrix4 boneWorld{2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1};
    const auto palette = spSkin::ComposePaletteMatrixForAnalysis(inverseBind, boneWorld);
    Require(palette[12] == 20 && palette[0] == 2 && palette[5] == 3 && palette[10] == 4,
            "palette is inverse-bind times bone-world, not reversed");
    std::cout << "PC animation reconstruction: " << checks << " checks passed\n";
}
