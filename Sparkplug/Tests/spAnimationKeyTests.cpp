#include "Analysis/PC/spAnimationKeySampling.h"
#include "Code/Sparkplug/spNodeController.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

namespace keys = sparkplug::evidence::pc::animation_keys;
namespace math = sparkplug::evidence::pc::animation_math;
using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Require(bool value, const char* label)
    {
        ++checks;
        if (!value)
        {
            std::cerr << "FAIL " << label << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
    template <std::size_t N> bool Near(const std::array<float, N>& a, const std::array<float, N>& b)
    {
        for (std::size_t i = 0; i < N; ++i)
            if (std::fabs(a[i] - b[i]) > .0001F)
                return false;
        return true;
    }
    keys::TrackDataForAnalysis Demo(unsigned rep)
    {
        keys::TrackDataForAnalysis data;
        const std::array<std::array<float, 3>, 3> positions{
            {{1, 2, 3}, {11, 22, 33}, {21, 42, 63}}};
        const std::array<std::array<float, 3>, 3> scales{{{2, 3, 4}, {4, 6, 8}, {6, 9, 12}}};
        const std::array<std::array<float, 3>, 3> angles{
            {{.1F, .2F, .3F}, {.4F, .5F, .6F}, {.7F, .8F, .9F}}};
        const std::array<std::array<float, 3>, 3> coefficients{{{2, 3, 4}, {3, 5, 7}, {5, 7, 9}}};
        // Same serialized float fixtures as the PC probe, not a separately
        // rounded run-time Euler construction that can cross acos' domain.
        const std::array<math::Quaternion,3> rotations{{
            {.03427079692482948F,.10602051019668579F,.14357218146324158F,.9833474159240723F},
            {.11224028468132019F,.28852832317352295F,.23366929590702057F,.9217115640640259F},
            {.12527373433113098F,.46676668524742126F,.25610336661338806F,.8371657133102417F}}};
        for (std::size_t role = 0; role < 3; ++role)
        {
            const auto& rows = role == 0 ? positions : role == 1 ? angles : scales;
            for (std::size_t axis = 0; axis < (rep >= 3 ? 3U : 1U); ++axis)
            {
                keys::KeyDataForAnalysis key{rep, {0, 10, 20}, {}};
                for (std::size_t i = 0; i < 3; ++i)
                {
                    if (rep >= 3)
                    {
                        key.values.push_back(rows[i][axis]);
                        if (rep == 4)
                        {
                            key.values.push_back(99 + static_cast<float>(axis));
                            key.values.insert(key.values.end(), coefficients[axis].begin(),
                                              coefficients[axis].end());
                        }
                    }
                    else if (role == 1)
                    {
                        const auto q = rotations[i];
                        key.values.insert(key.values.end(), q.begin(), q.end());
                        if (rep == 2)
                            key.values.insert(key.values.end(), {91, 92, 93, 94});
                    }
                    else
                    {
                        key.values.insert(key.values.end(), rows[i].begin(), rows[i].end());
                        if (rep == 2)
                        {
                            key.values.insert(key.values.end(), {97, 98, 99});
                            for (std::size_t component = 0; component < 3; ++component)
                                for (std::size_t a = 0; a < 3; ++a)
                                    key.values.push_back(coefficients[a][component]);
                        }
                    }
                }
                data[role][axis] = std::move(key);
            }
        }
        return data;
    }
    void Emit(unsigned rep, float time, const keys::Sample& s, const keys::KeyCache& cache)
    {
        std::cout << '[' << rep << ',' << time;
        for (auto value : s.position)
            std::cout << ',' << value;
        for (auto value : s.rotation)
            std::cout << ',' << value;
        for (auto value : s.scale)
            std::cout << ',' << value;
        for (auto value : cache.position)
            std::cout << ',' << value;
        for (auto value : cache.rotation)
            std::cout << ',' << value;
        for (auto value : cache.scale)
            std::cout << ',' << value;
        std::cout << "]\n";
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--emit-cubic-bits")
    {
        std::uint32_t seed = 0x47A12C39U;
        const unsigned counts[] = {1, 2, 3, 5, 6, 9};
        for (unsigned fixture = 0; fixture < 96; ++fixture)
        {
            const unsigned dimensions = fixture % 2 ? 3 : 1;
            const unsigned count = counts[(fixture / 2) % 6];
            keys::TrackDataForAnalysis data;
            keys::KeyDataForAnalysis key;
            key.representation = dimensions == 1 ? 4 : 2;
            for (unsigned i = 0; i < count; ++i) key.times.push_back(float(i));
            for (unsigned i = 0; i < count * dimensions * 5; ++i)
            {
                seed = seed * 1664525U + 1013904223U;
                const auto signedValue = static_cast<std::int32_t>(seed & 0xFFFFFFU) - 0x800000;
                key.values.push_back(std::ldexp(float(signedValue) / 123457.F, int(i % 15) - 7));
            }
            data[0][0] = key;
            if (dimensions == 1) data[0][1] = data[0][2] = key;
            const auto prepared = keys::PreparedTrackForAnalysis::Create(std::move(data));
            Require(bool(prepared), "cubic bits fixture preparation");
            std::cout << '[' << dimensions << ',' << count;
            for (const auto* values : std::array<const std::vector<float>*, 2>{&key.values, &prepared->Data()[0][0]->values})
            {
                std::cout << ",[";
                bool first = true;
                for (float value : *values)
                {
                    std::uint32_t bits = 0;
                    std::memcpy(&bits, &value, 4);
                    std::cout << (first ? "" : ",") << bits;
                    first = false;
                }
                std::cout << ']';
            }
            std::cout << "]\n";
        }
        return EXIT_SUCCESS;
    }
    const bool emit = argc == 2 && std::string_view(argv[1]) == "--emit-fixtures";
    std::cout << std::setprecision(9);
    for (unsigned rep = 1; rep <= 4; ++rep)
    {
        auto prepared = keys::PreparedTrackForAnalysis::Create(Demo(rep));
        if (!prepared) std::cerr<<"representation "<<rep<<'\n';
        Require(prepared.has_value(), "prepare full PRS representation");
        keys::KeyCache cache;
        for (float time : {-5.F, 0.F, 2.5F, 5.F, 10.F, 17.5F, 20.F, 25.F, 12.5F, 2.5F})
        {
            const auto sample = prepared->Evaluate(time, cache);
            Require(sample.hasPosition && sample.hasRotation && sample.hasScale,
                    "three valid channels");
            if (emit)
                Emit(rep, time, sample, cache);
        }
        Require(cache.position == std::array<int, 3>{} && cache.rotation == std::array<int, 3>{} &&
                    cache.scale == std::array<int, 3>{},
                "backward caches reset");
    }
    if (emit)
        return EXIT_SUCCESS;

    keys::TrackDataForAnalysis single;
    single[0][0] = keys::KeyDataForAnalysis{1, {0}, {1, 2, 3}};
    single[1][0] = keys::KeyDataForAnalysis{1, {0}, {0, 0, 0, 1}};
    single[2][0] = keys::KeyDataForAnalysis{1, {}, {}};
    auto prepared = keys::PreparedTrackForAnalysis::Create(single);
    Require(prepared.has_value(), "single packed keys and empty linear scale supported");
    keys::KeyCache cache;
    cache.position[0] = -1;
    auto value = prepared->Evaluate(100, cache);
    Require(value.position == spNode::Vector3{1, 2, 3} &&
                value.rotation == math::Quaternion{0, 0, 0, 1} && !value.hasScale,
            "bounded single-key finite endpoint and empty validity");
    Require(cache.position[0] == 0, "host-only negative cache reset");
    single[0][0] = keys::KeyDataForAnalysis{1, {0, 10}, {1, 2, 3, 11, 22, 33}};
    prepared = keys::PreparedTrackForAnalysis::Create(single);
    Require(prepared->Evaluate(10, cache).position == spNode::Vector3{1, 2, 3},
            "two-key endpoint quirk retained");
    Require(Near(prepared->Evaluate(5, cache).position, spNode::Vector3{6, 12, 18}),
            "linear midpoint");
    single[0][0]->times = {1, 0};
    Require(!keys::PreparedTrackForAnalysis::Create(single), "host rejects unsorted times");
    single[0][0]->times = {0, 10};
    single[0][0]->values[0] = std::numeric_limits<float>::quiet_NaN();
    Require(!keys::PreparedTrackForAnalysis::Create(single), "host rejects nonfinite keys");
    auto invalid = Demo(3);
    invalid[0][1].reset();
    Require(!keys::PreparedTrackForAnalysis::Create(invalid),
            "host rejects incomplete scalar triple");
    invalid = Demo(2);
    invalid[0][0]->times.clear();
    invalid[0][0]->values.clear();
    Require(!keys::PreparedTrackForAnalysis::Create(invalid),
            "host rejects unsafe empty cubic precompute");

    auto owned = std::make_shared<keys::PreparedTrackForAnalysis>(
        *keys::PreparedTrackForAnalysis::Create(Demo(1)));
    auto sampler = keys::MakeSampler(owned);
    auto evaluator = std::make_unique<spTransformTrackEval>();
    spTransformTrackEval::PlaybackForAnalysis state{1, 5};
    Require(evaluator->SetInputsForAnalysis({{&state, &sampler}}), "real prepared track adapter");
    auto node = std::make_shared<spNode>();
    spNodeController controller;
    controller.SetNodeForAnalysis(node);
    controller.SetEvaluatorForAnalysis(std::move(evaluator));
    owned.reset();
    controller.ApplyForAnalysis(999);
    Require(node->UpdateWorldForAnalysis() &&
                Near(node->GetWorldPositionForAnalysis(), spNode::Vector3{6, 12, 18}),
            "prepared keys -> evaluator -> controller -> node world; state time wins");
    Require(Near(node->GetWorldScaleForAnalysis(), spNode::Vector3{3, 4.5F, 6}),
            "prepared scale reaches node world");
    std::cout << "PASS " << checks << '/' << checks << " animation key checks\n";
}
