#include "Analysis/PC/spPolygonClip.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace clip = sparkplug::evidence::pc::polygon_clip;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    float FromBits(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        return bits;
    }
    float ReadFloat()
    {
        std::uint32_t word;
        if (!(std::cin >> word))
            throw std::runtime_error("truncated input");
        return FromBits(word);
    }
    void Batch()
    {
        unsigned count, keep, alias;
        while (std::cin >> count >> keep >> alias)
        {
            if (count > 127)
                throw std::runtime_error("bounded input");
            const float epsilon = ReadFloat();
            clip::Plane plane{};
            for (auto& v : plane)
                v = ReadFloat();
            std::vector<clip::Vector3> points(count), output;
            for (auto& point : points)
                for (auto& v : point)
                    v = ReadFloat();
            auto& destination = alias ? points : output;
            const bool result =
                clip::ClipForAnalysis(points, plane, keep != 0, destination, epsilon);
            std::cout << result << ',' << destination.size();
            for (const auto& point : destination)
                for (float v : point)
                    std::cout << ',' << Bits(v);
            std::cout << '\n';
        }
    }
    void Units()
    {
        const clip::Plane plane{1, 0, 0, 0};
        const std::vector<clip::Vector3> quad{{-2, -2, 0}, {2, -2, 0}, {2, 2, 0}, {-2, 2, 0}};
        std::vector<clip::Vector3> output;
        Check(clip::ClipForAnalysis(quad, plane, false, output), "cross-plane accepted");
        Check(output == std::vector<clip::Vector3>{{0, -2, 0}, {2, -2, 0}, {2, 2, 0}, {0, 2, 0}},
              "ordered edge intersections");
        auto alias = quad;
        Check(clip::ClipForAnalysis(alias, plane, false, alias) && alias == output,
              "in-place geometry equivalent");
        const std::vector<clip::Vector3> on{{0, 0, 0}, {0, 1, 0}, {0, 1, 1}};
        Check(!clip::ClipForAnalysis(on, plane, false, output) && output.empty(),
              "coplanar discarded by flag0");
        Check(clip::ClipForAnalysis(on, plane, true, output) && output == on,
              "coplanar retained by flag1");
        Check(!clip::ClipForAnalysis({}, plane, false, output) && output.empty(), "empty false");
        Check(clip::ClipForAnalysis({}, plane, true, output) && output.empty(),
              "empty true with keep flag");
        const std::vector<clip::Vector3> edge{
            {-.001F, 0, 0}, {-2, 1, 0}, {2, 2, 0}, {-.001F, 3, 0}};
        Check(clip::ClipForAnalysis(edge, plane, false, output) && output.front()[0] == 0 &&
                  output.back()[0] == -.001F,
              "native repeated-first correction, final on-point not snapped");
        const auto previous = output;
        Check(!clip::ClipForAnalysis(std::vector<clip::Vector3>(128), plane, false, output) &&
                  output == previous,
              "host stack-derived bound preserves output");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        Check(!clip::ClipForAnalysis(quad, {nan, 0, 0, 0}, false, output) && output.empty(),
              "unordered is coplanar, flag0 discard");
        Check(clip::ClipForAnalysis(quad, {nan, 0, 0, 0}, true, output) && output == quad,
              "unordered retained with flag1");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            Batch();
        else
        {
            Units();
            std::cout << "PASS " << checks << '/' << checks << ": Polygon clip source\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
