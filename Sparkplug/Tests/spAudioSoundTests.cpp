#include "Code/Sparkplug/spAudioSound.h"
#include "Analysis/PC/spAudioSoundAbi.h"
#include "Analysis/PS2/spAudioSoundAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* text)
    { ++checks; if (!value) throw std::runtime_error(text); }
    float Float(std::uint32_t bits)
    { float value; std::memcpy(&value, &bits, 4); return value; }
    std::uint32_t Bits(float value)
    { std::uint32_t bits; std::memcpy(&bits, &value, 4); return bits; }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--mutating-batch")
        {
            std::uint32_t initial, group, firstGroup, firstOwn, secondGroup, secondOwn, changedGroup;
            unsigned count = 0;
            while (std::cin >> initial >> group >> firstGroup >> firstOwn >> secondGroup >> secondOwn >> changedGroup)
            {
                if (++count > 1024 || !group) throw std::runtime_error("bounded nonzero mutation batch");
                spAudioSound sound; auto state = sound.GetStateForAnalysis();
                state.words[5] = initial; state.words[6] = group; sound.SetStateForAnalysis(state);
                sparkplug::host::spAudioSoundHost host; unsigned reads = 0;
                host.readGroupScalar = [&](std::uint32_t index, float& output) {
                    Check(index == group && reads < 2, "original cached group index on both reads");
                    auto changed = sound.GetStateForAnalysis();
                    changed.words[5] = ++reads == 1 ? firstOwn : secondOwn;
                    changed.words[6] = changedGroup; sound.SetStateForAnalysis(changed);
                    output = Float(reads == 1 ? firstGroup : secondGroup); return true; };
                float output;
                Check(sound.GetCombinedScalarForAnalysis(host, output), "mutating native read order");
                std::cout << '[' << Bits(output) << ',' << sound.GetStateForAnalysis().words[5] << ','
                    << sound.GetStateForAnalysis().words[6] << ',' << reads << "]\n";
            }
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--batch")
        {
            std::uint32_t scalar, group, cached; unsigned count = 0;
            while (std::cin >> scalar >> group >> cached)
            {
                if (++count > 1024) throw std::runtime_error("bounded audio scalar batch");
                spAudioSound sound; auto state = sound.GetStateForAnalysis();
                state.words[5] = scalar; state.words[6] = group; sound.SetStateForAnalysis(state);
                sparkplug::host::spAudioSoundHost host;
                unsigned reads = 0;
                host.readGroupScalar = [&](std::uint32_t index, float& output) {
                    Check(index == group, "exact group index"); ++reads; output = Float(cached); return true; };
                float output;
                Check(sound.GetCombinedScalarForAnalysis(host, output), "batch grouped scalar");
                std::cout << '[' << Bits(sound.GetScalarForAnalysis()) << ',' << Bits(output) << ',' << reads << "]\n";
            }
            return 0;
        }
        spAudioSound sound;
        Check(sound.IsExactly(spAudioSound::ClassID) && sound.IsKindOf(spNode::ClassID) &&
            sound.IsKindOf(spNamedObject::ClassID), "native RTTI chain");
        Check(spAudioSound::StaticRTTI().factory() != nullptr, "factory");
        Check(sound.GetStateForAnalysis().words == std::array<std::uint32_t,17>{0,0,0,0x447A0000,
            0x3F800000,0,0,0,0x3F800000,0,0,0,0,0,0,0,0}, "all known tail defaults");
        float output = 42;
        Check(sound.GetCombinedScalarForAnalysis({}, output) && output == 0, "group zero bypasses manager");
        auto state = sound.GetStateForAnalysis(); state.words[5] = 0x7FC01234;
        sound.SetStateForAnalysis(state);
        Check(sound.GetCombinedScalarForAnalysis({}, output) && Bits(output) == 0x7FC01234,
            "group zero preserves raw NaN payload");
        state.words[5] = Bits(0.25f); state.words[6] = 7; sound.SetStateForAnalysis(state);
        output = 42; std::string error;
        Check(!sound.GetCombinedScalarForAnalysis({}, output, &error) && output == 42 && !error.empty(),
            "required manager data guard leaves output alone");
        sparkplug::host::spAudioSoundHost host;
        unsigned reads = 0;
        host.readGroupScalar = [&](std::uint32_t index, float& value) {
            Check(index == 7, "selected group"); ++reads; value = 0.5f; return true; };
        Check(sound.GetCombinedScalarForAnalysis(host, output) && output == 0.625f && reads == 2,
            "original group + scalar - product arithmetic");
        host.readGroupScalar = [](std::uint32_t, float&) { return false; };
        Check(!sound.GetCombinedScalarForAnalysis(host, output) && output == 0.625f,
            "failed provider leaves previous result");
        host.readGroupScalar = [](std::uint32_t, float& value) {
            value = std::numeric_limits<float>::infinity(); return true; };
        Check(!sound.GetCombinedScalarForAnalysis(host, output) && output == 0.625f,
            "declared grouped finite guard");
        reads = 0;
        host.readGroupScalar = [&](std::uint32_t index, float& value) {
            Check(index == 7 && reads < 2, "group index is cached before mutations");
            auto changed = sound.GetStateForAnalysis();
            changed.words[5] = Bits(++reads == 1 ? 0.25f : 0.5f);
            changed.words[6] = 0; sound.SetStateForAnalysis(changed);
            value = reads == 1 ? 0.5f : 0.75f; return true; };
        Check(sound.GetCombinedScalarForAnalysis(host, output) && output == 0.375f && reads == 2,
            "first group plus first own minus second group times second own");
        state.words[6] = 7; sound.SetStateForAnalysis(state); reads = 0; output = 42;
        host.readGroupScalar = [&](std::uint32_t, float& value) {
            if (++reads == 2) return false;
            value = 0.5f; return true; };
        Check(!sound.GetCombinedScalarForAnalysis(host, output) && output == 42 && reads == 2,
            "second provider failure leaves output alone");
        sound.SetName("sound"); sound.SetPositionForAnalysis({10,20,30});
        state.words[0] = 0xDEADBEEF; state.words[16] = 0xABCD1234; sound.SetStateForAnalysis(state);
        auto clone = sound.Clone(); const auto* copy = dynamic_cast<spAudioSound*>(clone.get());
        Check(copy && std::string(copy->GetName()) == "sound" &&
            copy->GetPositionForAnalysis() == spNode::Vector3{10,20,30}, "Node prefix is copied");
        Check(copy->GetStateForAnalysis().words == spAudioSound{}.GetStateForAnalysis().words,
            "clone leaves whole audio tail fresh");
        spCloneManager manager; const auto before = sound.GetStateForAnalysis().words;
        Check(sound.vfunc_14(sound, manager) && sound.GetStateForAnalysis().words == before,
            "self copy leaves own tail intact");
        std::cout << "PASS " << checks << "/" << checks << ": AudioSound state and grouped scalar\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
