#include "Code/Sparkplug/spQuad3D.h"
#include "Analysis/PC/spQuad3DAbi.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* description)
    { ++checks; if (!value) throw std::runtime_error(description); }
    std::uint32_t Bits(float value)
    { std::uint32_t bits; std::memcpy(&bits, &value, 4); return bits; }
    struct Fixture
    {
        std::array<sparkplug::host::spQuad3DVertex, 4> vertices{};
        std::array<std::uint16_t, 6> indices{};
        sparkplug::host::spQuad3DHost host;
        std::vector<unsigned> calls;
        spBaseObject fallback;
        spBaseObject* selected = nullptr;
        spBaseObject* material = nullptr;
        bool overrideState = false;
        std::uint8_t drawResult = 1;
        Fixture()
        {
            host.acquireVertices = [&](std::uint32_t format) {
                Check(format == 0x900, "original buffer format"); calls.push_back(1); return vertices.data(); };
            host.acquireIndices = [&] { calls.push_back(2); return indices.data(); };
            host.stateOverride = [&] { calls.push_back(3); return overrideState; };
            host.defaultVertexBuffer = &fallback;
            host.worldMatrixToken = 0x1234;
            host.selectVertices = [&](spBaseObject* buffer, std::uint32_t matrix) {
                Check(matrix == 0x1234, "matrix identity boundary"); selected = buffer; calls.push_back(4); };
            host.applyMaterial = [&](spBaseObject* value) { material = value; calls.push_back(5); };
            host.draw = [&](std::uint32_t kind, std::uint32_t primitives, std::uint32_t count) {
                Check(kind == 2 && primitives == 2 && count == 4, "original draw tuple");
                calls.push_back(6); return drawResult; };
        }
    };
    int Batch()
    {
        unsigned count = 0;
        while (std::cin.peek() != std::char_traits<char>::eof())
        {
            spQuad3D::StateForAnalysis state;
            spQuad3D::CameraOrientation camera;
            auto read = []() { std::uint32_t bits; std::cin >> bits; float value; std::memcpy(&value, &bits, 4); return value; };
            std::cin >> std::ws;
            if (std::cin.peek() == std::char_traits<char>::eof()) break;
            if (++count > 256) throw std::runtime_error("bounded quad batch");
            for (auto& value : state.position) value = read();
            state.width = read(); state.height = read(); std::cin >> state.color;
            for (auto& value : camera) value = read();
            if (!std::cin) throw std::runtime_error("malformed quad batch");
            spQuad3D quad; quad.SetStateForAnalysis(state); Fixture fixture;
            Check(quad.DrawForAnalysis(camera, fixture.host), "batch draw");
            std::cout << '[';
            bool comma = false;
            for (const auto& vertex : fixture.vertices)
            {
                for (const auto value : vertex.position)
                { if (comma) std::cout << ','; comma = true; std::cout << Bits(value); }
                std::cout << ',' << vertex.color << ',' << Bits(vertex.uv[0]) << ',' << Bits(vertex.uv[1]);
            }
            std::cout << "]\n";
        }
        return 0;
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch") return Batch();
        constexpr spQuad3D::CameraOrientation identity{1,0,0,0,1,0,0,0,1};
        spQuad3D quad;
        Check(quad.IsExactly(spQuad3D::ClassID) && quad.IsKindOf(spBaseObject::ClassID), "RTTI chain");
        Check(spQuad3D::StaticRTTI().factory() != nullptr, "factory");
        Check(quad.GetStateForAnalysis().width == 0 && quad.GetStateForAnalysis().height == 0 &&
            quad.GetStateForAnalysis().color == 0xFFFFFFFFu, "factory defaults");
        Fixture fixture;
        quad.SetStateForAnalysis({{10,20,30}, 4, 2, 0x12345678});
        Check(quad.DrawForAnalysis(identity, fixture.host), "identity camera draw");
        Check(fixture.calls == std::vector<unsigned>{1,2,3,4,5,6} && fixture.selected == &fixture.fallback,
            "submission ordering and fallback vertex buffer");
        Check(fixture.vertices[0].position == spQuad3D::Vector3{12,19,30} &&
            fixture.vertices[1].position == spQuad3D::Vector3{12,21,30} &&
            fixture.vertices[2].position == spQuad3D::Vector3{8,19,30} &&
            fixture.vertices[3].position == spQuad3D::Vector3{8,21,30}, "camera facing corners");
        Check(fixture.indices == std::array<std::uint16_t,6>{0,2,1,1,2,3}, "original winding");
        Check(fixture.vertices[0].uv == std::array<float,2>{0,0} &&
            fixture.vertices[1].uv == std::array<float,2>{0,1} &&
            fixture.vertices[2].uv == std::array<float,2>{1,0} &&
            fixture.vertices[3].uv == std::array<float,2>{1,1} &&
            fixture.vertices[3].color == 0x12345678, "UV/color records");
        auto buffer = std::make_shared<spBaseObject>(), material = std::make_shared<spBaseObject>();
        quad.SetVertexBufferForAnalysis(buffer); quad.SetMaterialForAnalysis(material);
        fixture.calls.clear(); fixture.overrideState = true; fixture.drawResult = 255;
        Check(quad.DrawForAnalysis(identity, fixture.host) && fixture.material == material.get() &&
            fixture.calls == std::vector<unsigned>{1,2,3,5,6}, "override skips binding but applies material");
        fixture.overrideState = false; fixture.drawResult = 0;
        Check(!quad.DrawForAnalysis(identity, fixture.host) && fixture.selected == buffer.get(),
            "explicit buffer and failed draw return");
        auto clone = quad.Clone(); auto* fresh = dynamic_cast<spQuad3D*>(clone.get());
        Check(fresh && fresh->GetStateForAnalysis().width == 0 && !fresh->GetMaterialForAnalysis() &&
            !fresh->GetVertexBufferForAnalysis(), "clone resets complete derived payload");
        Fixture nullVertices;
        nullVertices.host.acquireVertices = [&](std::uint32_t) { nullVertices.calls.push_back(1);
            return static_cast<sparkplug::host::spQuad3DVertex*>(nullptr); };
        Check(!quad.DrawForAnalysis(identity, nullVertices.host) && nullVertices.calls == std::vector<unsigned>{1},
            "null vertices stops before index request");
        Fixture nullIndices;
        nullIndices.host.acquireIndices = [&]() -> std::uint16_t* { nullIndices.calls.push_back(2); return nullptr; };
        Check(!quad.DrawForAnalysis(identity, nullIndices.host) && nullIndices.calls == std::vector<unsigned>{1,2},
            "null indices stops before geometry");
        Fixture invalid;
        auto camera = identity; camera[0] = std::numeric_limits<float>::infinity();
        Check(!quad.DrawForAnalysis(camera, invalid.host) && invalid.calls == std::vector<unsigned>{1,2},
            "declared finite guard stops before storage mutation/submission");
        Fixture degenerate; camera.fill(0);
        Check(quad.DrawForAnalysis(camera, degenerate.host) &&
            degenerate.vertices[0].position == quad.GetStateForAnalysis().position,
            "native zero-vector normalization collapses quad");
        std::vector<unsigned> releases;
        {
            spQuad3D owned;
            owned.SetVertexBufferForAnalysis(std::shared_ptr<spBaseObject>(new spBaseObject,
                [&](spBaseObject* value) { releases.push_back(1); delete value; }));
            owned.SetMaterialForAnalysis(std::shared_ptr<spBaseObject>(new spBaseObject,
                [&](spBaseObject* value) { releases.push_back(2); delete value; }));
        }
        Check(releases == std::vector<unsigned>{2,1}, "material release precedes vertex resource");
        std::cout << "PASS " << checks << "/" << checks << ": Quad3D geometry and submission\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
