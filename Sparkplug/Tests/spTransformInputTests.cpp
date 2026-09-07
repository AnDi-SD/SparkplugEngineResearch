#include "Code/Sparkplug/spTransformTrackEval.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    using Eval = spTransformTrackEval;
    using Input = Eval::InputForAnalysis;
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    struct Seed
    {
        int state = -1, track = -1;
        std::uint32_t priority = 0xFFFFFFFF;
        int cache = 200;
    };
    struct Fixture
    {
        std::array<std::uint32_t, 3> uses{};
        std::array<Eval::PlaybackForAnalysis, 3> states{};
        std::array<Eval::TrackSamplerForAnalysis, 3> tracks;
        Eval evaluator;
        Fixture()
        {
            for (std::size_t index = 0; index < 3; ++index)
            {
                states[index].bindingUseCount = &uses[index];
                tracks[index] = [](float, Eval::KeyCacheForAnalysis&) {
                    return Eval::SampleForAnalysis{};
                };
            }
        }
        Input Make(Seed seed)
        {
            if (seed.state < -1 || seed.state > 2 || seed.track < -1 || seed.track > 2)
                throw std::runtime_error("Invalid input fixture identity");
            Input input;
            input.playback = seed.state < 0 ? nullptr : &states[seed.state];
            input.sampler = seed.track < 0 ? nullptr : &tracks[seed.track];
            input.priority = seed.priority;
            for (int key = 0; key < 3; ++key)
            {
                input.cache.position[key] = seed.cache + key;
                input.cache.rotation[key] = seed.cache + key + 3;
                input.cache.scale[key] = seed.cache + key + 6;
            }
            return input;
        }
        void Prepare(std::size_t count, std::array<Seed, 2> seeds,
                     std::array<std::uint32_t, 3> counters)
        {
            if (count > 2)
                throw std::runtime_error("Invalid input fixture count");
            std::vector<Input> physical{Make(seeds[0]), Make(seeds[1])};
            Check(evaluator.SetInputsForAnalysis(physical),
                  "fixture physical input initialization");
            physical.resize(count);
            Check(evaluator.SetInputsForAnalysis(physical), "fixture active prefix initialization");
            uses = counters;
        }
        int StateIndex(const Eval::PlaybackForAnalysis* value) const
        {
            if (!value)
                return -1;
            for (int index = 0; index < 3; ++index)
                if (value == &states[index])
                    return index;
            throw std::runtime_error("Unknown playback pointer");
        }
        int TrackIndex(const Eval::TrackSamplerForAnalysis* value) const
        {
            if (!value)
                return -1;
            for (int index = 0; index < 3; ++index)
                if (value == &tracks[index])
                    return index;
            throw std::runtime_error("Unknown track pointer");
        }
        std::string Snapshot() const
        {
            std::ostringstream out;
            out << "{\"count\":" << evaluator.GetInputCountForAnalysis() << ",\"slots\":[";
            bool comma = false;
            for (const auto& input : evaluator.GetPhysicalInputsForAnalysis())
            {
                if (comma)
                    out << ',';
                comma = true;
                out << '[' << StateIndex(input.playback) << ',' << TrackIndex(input.sampler) << ','
                    << input.priority << ",[";
                bool keyComma = false;
                for (const auto& channel :
                     {input.cache.position, input.cache.rotation, input.cache.scale})
                    for (int key : channel)
                    {
                        if (keyComma)
                            out << ',';
                        keyComma = true;
                        out << key;
                    }
                out << "]]";
            }
            out << "],\"uses\":[" << uses[0] << ',' << uses[1] << ',' << uses[2] << "]}";
            return out.str();
        }
    };
    void Batch()
    {
        int cases = 0;
        if (!(std::cin >> cases) || cases < 1 || cases > 256)
            throw std::runtime_error("Bounded input fixture case count");
        Fixture f;
        for (int index = 0; index < cases; ++index)
        {
            std::size_t count = 0;
            std::array<Seed, 2> slots;
            std::array<std::uint32_t, 3> uses{};
            Seed incoming;
            bool exclusive = false;
            if (!(std::cin >> count))
                throw std::runtime_error("Missing input count");
            for (auto& slot : slots)
                if (!(std::cin >> slot.state >> slot.track >> slot.priority >> slot.cache))
                    throw std::runtime_error("Missing input seed");
            if (!(std::cin >> uses[0] >> uses[1] >> uses[2] >> incoming.state >> incoming.track >>
                  incoming.priority >> exclusive))
                throw std::runtime_error("Missing insertion request");
            f.Prepare(count, slots, uses);
            Check(f.evaluator.InsertInputForAnalysis(f.Make(incoming), exclusive),
                  "safe native-compatible insertion");
            std::cout << f.Snapshot() << '\n';
        }
    }
    void Tests()
    {
        Fixture f;
        std::array<Seed, 2> both{{{0, 0, 10, 100}, {1, 1, 20, 200}}};
        f.Prepare(2, both, {1, 1, 0});
        auto before = f.Snapshot();
        Check(f.evaluator.InsertInputForAnalysis(f.Make({2, 2, 19, 999}), true) &&
                  f.Snapshot() == before,
              "exclusive lower-priority request is native no-op");
        Check(!f.evaluator.InsertInputForAnalysis(f.Make({2, 2, 30, 999}), false) &&
                  f.Snapshot() == before,
              "host third-input rejection is before counters or slots mutate");
        Check(f.evaluator.InsertInputForAnalysis(f.Make({2, 2, 20, 999}), true),
              "exclusive equal-highest priority accepted");
        Check(f.uses == std::array<std::uint32_t, 3>{1, 0, 1},
              "native asymmetric exclusive counters");
        Check(f.evaluator.GetPhysicalInputsForAnalysis()[0].cache.position[0] == 100 &&
                  f.evaluator.GetPhysicalInputsForAnalysis()[1].cache.position[0] == 200,
              "exclusive does not reset physical key caches");
        f.Prepare(2, both, {1, 1, 0});
        Check(f.evaluator.InsertInputForAnalysis(f.Make({0, 2, 30, 999}), false),
              "nonexclusive reprioritization");
        Check(f.uses == std::array<std::uint32_t, 3>{1, 1, 0} &&
                  f.StateIndex(f.evaluator.GetPhysicalInputsForAnalysis()[0].playback) == 1 &&
                  f.StateIndex(f.evaluator.GetPhysicalInputsForAnalysis()[1].playback) == 0,
              "nonexclusive reinsert stable counters and priority order");
        Check(f.evaluator.GetPhysicalInputsForAnalysis()[0].cache.position[0] == 200 &&
                  f.evaluator.GetPhysicalInputsForAnalysis()[1].cache.position[0] == 200,
              "incoming gets destination cache, retained input carries its own");
        Check(f.evaluator.ClearInputForAnalysis(0) && f.evaluator.GetInputCountForAnalysis() == 2 &&
                  f.uses == std::array<std::uint32_t, 3>{1, 1, 0},
              "pointer-only clear preserves counters/count");
        Check(!f.evaluator.ClearInputForAnalysis(2), "host clear index bound");
        Input noCounter;
        Eval::PlaybackForAnalysis view;
        noCounter.playback = &view;
        before = f.Snapshot();
        Check(!f.evaluator.InsertInputForAnalysis(noCounter, true) && f.Snapshot() == before,
              "insertion requires a live counter view; sampler-only fixtures remain supported");
        std::cout << "PASS " << checks << '/' << checks << ": transform input tests\n";
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--probe-batch")
            Batch();
        else if (argc == 1)
            Tests();
        else
            throw std::runtime_error("Unexpected input test arguments");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
