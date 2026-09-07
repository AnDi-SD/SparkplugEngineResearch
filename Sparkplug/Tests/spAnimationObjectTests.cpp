#include "Code/Sparkplug/spAnimation.h"
#include "Code/Sparkplug/spNodeController.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool ok, const char* label)
    {
        ++checks;
        if (!ok)
            throw std::runtime_error(label);
    }
    spAnimTrack::TrackDataForAnalysis Keys()
    {
        spAnimTrack::TrackDataForAnalysis data{};
        data[0][0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{
            1, {0, 5, 10}, {0, 2, 4, 10, 12, 14, 20, 22, 24}};
        return data;
    }
} // namespace
int main()
{
    try
    {
        spTrack base;
        Check(base.IsExactly(0x60C839C5), "spTrack identity");
        Check(base.IsKindOf(spNamedObject::ClassID), "track named ancestry");
        Check(base.GetDurationForAnalysis() == 0, "base duration");
        base.ReleaseKeysForAnalysis();
        base.SetName("track-base");
        auto baseClone = base.Clone();
        Check(baseClone && baseClone->IsExactly(spTrack::ClassID), "base track concrete clone");
        Check(std::string(dynamic_cast<spTrack*>(baseClone.get())->GetName()) == "track-base",
              "base name copied");

        spAnimTrack detached;
        Check(detached.IsKindOf(spTrack::ClassID), "anim track base");
        Check(detached.GetOwnerForAnalysis() == nullptr, "host detached owner guard");
        Check(detached.GetBindingSlotForAnalysis() == -1, "binding default");
        Check(!detached.SetKeysForAnalysis(Keys()), "detached attach denied");

        spAnimation animation;
        animation.SetName("animation-with-name");
        Check(animation.IsExactly(0x56EE563A), "animation identity");
        Check(animation.IsKindOf(0x4FAD24F1), "animation engine controller relation");
        Check(!animation.IsKindOf(spNamedObject::ClassID),
              "engine RTTI is not physical named base");
        Check(animation.GetTotalTimeForAnalysis() == 0, "animation time default");
        Check(animation.GetPriorityGroupForAnalysis() == 0, "priority group default");
        animation.SetPriorityGroupForAnalysis(0xffffff03);
        Check(animation.GetPriorityGroupForAnalysis() == 0xffffff03,
              "priority field preserves full uint32, start uses low eight bits");
        Check(animation.GetTrackCountForAnalysis() == 0 &&
                  animation.GetTrackCapacityForAnalysis() == 0,
              "empty track storage");
        Check(!animation.GetDebugCycleValueForAnalysis(), "debug dependency not invented");
        animation.SetDebugCycleValueForAnalysis(0xff00ff00);
        Check(animation.GetDebugCycleValueForAnalysis() == 0xff00ff00,
              "explicit debug cycle input");
        Check(animation.SetTotalTimeForAnalysis(20), "total duration setter");
        Check(!animation.SetTotalTimeForAnalysis(std::numeric_limits<float>::infinity()),
              "host finite duration guard");
        auto* first = animation.AppendTrackForAnalysis();
        Check(first && first->GetOwnerForAnalysis() == &animation, "append binds owner");
        Check(animation.GetTrackCapacityForAnalysis() == 1, "capacity grows one");
        first->SetName("Head");
        first->SetBindingSlotForAnalysis(7);
        Check(first->SetKeysForAnalysis(Keys()), "keys attached");
        Check(first->GetDurationForAnalysis() == 10, "max key time");
        Check(animation.GetTotalTimeForAnalysis() == 20,
              "total duration independent of track duration");
        auto sampler = first->GetSamplerForAnalysis();
        spTransformTrackEval::KeyCacheForAnalysis cache{};
        auto sample = sampler(5, cache);
        Check(sample.hasPosition && sample.position[0] == 10, "object track samples position");
        auto bad = Keys();
        bad[0][0]->values.pop_back();
        Check(!first->SetKeysForAnalysis(std::move(bad)), "invalid replacement rejected");
        Check(first->GetDurationForAnalysis() == 10,
              "host failed replacement retains previous snapshot");
        auto trackCloneBase = first->Clone();
        auto* trackClone = dynamic_cast<spAnimTrack*>(trackCloneBase.get());
        Check(trackClone && std::string(trackClone->GetName()) == "Head", "anim track name clone");
        Check(trackClone->GetOwnerForAnalysis() == nullptr &&
                  trackClone->GetBindingSlotForAnalysis() == -1,
              "clone fresh binding and owner");
        Check(trackClone->GetDurationForAnalysis() == 0, "clone does not duplicate keys");
        first->ReleaseKeysForAnalysis();
        first->ReleaseKeysForAnalysis();
        Check(first->GetDurationForAnalysis() == 0, "host idempotent release");
        Check(!first->GetSamplerForAnalysis()(5, cache).hasPosition, "released track empty");
        Check(sampler(5, cache).position[0] == 10,
              "captured host snapshot has independent lifetime");
        Check(first->SetKeysForAnalysis(Keys()), "reattach after release");
        Check(animation.ResizeTrackCapacityForAnalysis(5), "reserve track capacity");
        Check(animation.GetTrackCountForAnalysis() == 1 &&
                  animation.GetTrackCapacityForAnalysis() == 5,
              "reserve does not construct tracks");
        auto* second = animation.AppendTrackForAnalysis();
        Check(second && animation.GetTrackCountForAnalysis() == 2 &&
                  animation.GetTrackCapacityForAnalysis() == 5,
              "append uses reserve");
        Check(animation.GetTrackForAnalysis(0) == first,
              "host stable track pointers explicitly differ from native relocation");
        Check(animation.ResizeTrackCapacityForAnalysis(1), "shrink constructed tail");
        Check(animation.GetTrackCountForAnalysis() == 1 && !animation.GetTrackForAnalysis(1),
              "tail removed");
        Check(!animation.ResizeTrackCapacityForAnalysis(100001), "host track cap");
        for (auto tag :
             {spAnimation::TagForAnalysis{"later-a", 3, 0}, {"early", 1, 1}, {"later-b", 3, 2}})
            Check(animation.InsertTagForAnalysis(std::move(tag)), "tag insertion");
        const auto& tags = animation.GetTagsForAnalysis();
        Check(tags[0].name == "early" && tags[1].name == "later-a" && tags[2].name == "later-b",
              "stable time order");
        Check(tags[0].wireOrdinal == 1 && tags[1].wireOrdinal == 0,
              "tag wire ordinal not sorted index");
        Check(!animation.InsertTagForAnalysis({"bad", std::numeric_limits<float>::quiet_NaN(), 3}),
              "host finite tag guard");
        auto cloneBase = animation.Clone();
        auto* clone = dynamic_cast<spAnimation*>(cloneBase.get());
        Check(clone && std::string(clone->GetName()) == "animation-with-name",
              "animation name copied despite RTTI split");
        Check(clone->GetTotalTimeForAnalysis() == 0 && clone->GetTrackCountForAnalysis() == 0 &&
                  clone->GetTagsForAnalysis().empty(),
              "animation clone blank payload");
        Check(!clone->GetDebugCycleValueForAnalysis(), "clone does not copy debug dependency");
        Check(clone->GetPriorityGroupForAnalysis() == 0,
              "name-only clone does not copy priority group");
        Check(
            spRTTIManager::Instance().Create(spAnimation::ClassID)->IsExactly(spAnimation::ClassID),
            "registered animation factory");
        Check(animation.ResizeTrackCapacityForAnalysis(0) &&
                  animation.GetTrackCountForAnalysis() == 0,
              "clear track array");
        std::cout << "PASS " << checks << '/' << checks << ": animation object tests\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
