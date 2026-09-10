#include "../BufferInspection.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0;
void Check(bool value, const char* message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
template<class F> void Reject(F action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, message);
}
template<class T> void Add(std::vector<std::uint8_t>& bytes, T value) {
    const auto start = bytes.size(); bytes.resize(start + sizeof(value));
    std::memcpy(bytes.data() + start, &value, sizeof(value));
}
std::vector<std::uint8_t> Header(std::uint32_t first, std::uint32_t count, std::uint32_t flags) {
    std::vector<std::uint8_t> bytes; Add(bytes, first); Add(bytes, count); Add(bytes, flags); return bytes;
}
void Indices() {
    using View = spvhost::IndexBufferInspection;
    auto bytes = Header(2, 1, 0x40);
    for (std::uint16_t value : {0, 3, 65535}) Add(bytes, value);
    const auto original = bytes;
    View view(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    const auto info = view.Info();
    Check(info.primitiveType == 2 && info.primitiveCount == 1 && info.indexCount == 3 &&
        info.formatFlags == 0x40 && info.elementSize == 2 && info.finalPosition == 18,
        "shared UInt16 reader metadata retains non-width flags");
    Check(bytes == original, "IndexBuffer read preserves immutable input");
    bytes.assign(bytes.size(), 0);
    std::array<std::uint32_t, 3> output{}; view.CopyIndices(output.data(), 3);
    Check(output == std::array<std::uint32_t, 3>{0, 3, 65535}, "IndexBuffer owns values after input mutation");
    Reject([&] { view.CopyIndices(output.data(), 2); }, "short index output rejected");
    Reject([&] { view.CopyIndices(nullptr, 3); }, "null index output rejected");
    Check(output == std::array<std::uint32_t, 3>{0, 3, 65535}, "invalid output calls preserve destination");
    bytes = Header(1, 2, 0x81); Add(bytes, 0xffffffffu); Add(bytes, 65536u);
    View wide(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    wide.CopyIndices(output.data(), 2);
    Check(wide.Info().elementSize == 4 && wide.Info().formatFlags == 0x81 &&
        output[0] == 0xffffffffu && output[1] == 65536, "generic inspector preserves UInt32 values without occluder policy");
    auto empty = Header(2, 0, 0); View zero(empty.data(), 12); zero.CopyIndices(nullptr, 0);
    Check(zero.Info().indexCount == 0 && zero.Info().finalPosition == 12, "empty native index buffer remains inspectable");
    auto huge = Header(2, 0xffffffffu, 0);
    Reject([&] { View invalid(huge.data(), 12); }, "index count overflow rejected before allocation");
    huge = Header(1, 0x40000000u, 0);
    Reject([&] { View invalid(huge.data(), 12); }, "index count above exact byte budget rejected");
    Reject([&] { View invalid(original.data(), 17); }, "truncated index payload rejected");
    auto trailing = original; trailing.push_back(0);
    Reject([&] { View invalid(trailing.data(), 19); }, "trailing index payload rejected");
    Reject([&] { View invalid(original.data(), spvhost::MaximumBufferInspectionBytes + 1); }, "input cap precedes index read");
    Reject([&] { View invalid(nullptr, 12); }, "null index input rejected");
}
void Vertices() {
    using View = spvhost::VertexBufferInspection;
    auto bytes = Header(0x840, 2, 0x1234);
    for (float value : {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f,
                       11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f, 18.f}) Add(bytes, value);
    const auto original = bytes;
    View view(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    const auto info = view.Info();
    Check(info.componentFlags == 0x840 && info.vertexCount == 2 && info.flags == 0x1234 &&
        info.stride == 32 && info.componentCount == 8 && info.byteCount == 64 && info.finalPosition == 76,
        "shared component layout and raw flags projected");
    Check(bytes == original, "VertexBuffer read preserves immutable input");
    bytes.assign(bytes.size(), 0);
    std::array<float, 6> output{}; view.CopyPositions(output.data(), 6);
    Check(output == std::array<float, 6>{1, 2, 3, 11, 12, 13}, "owned position projection follows shared stride");
    Reject([&] { view.CopyPositions(output.data(), 3); }, "short position output rejected");
    Reject([&] { view.CopyPositions(nullptr, 6); }, "null position output rejected");
    Check(output == std::array<float, 6>{1, 2, 3, 11, 12, 13}, "invalid position output preserves destination");
    auto empty = Header(0, 0, 7); View zero(empty.data(), 12); zero.CopyPositions(nullptr, 0);
    Check(zero.Info().vertexCount == 0 && zero.Info().flags == 7, "empty generic vertex buffer retains flags");
    auto huge = Header(0, 0xffffffffu, 0);
    Reject([&] { View invalid(huge.data(), 12); }, "vertex count bounded before allocation");
    Reject([&] { View invalid(original.data(), 75); }, "truncated vertex payload rejected");
    auto trailing = original; trailing.push_back(0);
    Reject([&] { View invalid(trailing.data(), 77); }, "trailing vertex payload rejected");
    Reject([&] { View invalid(original.data(), spvhost::MaximumBufferInspectionBytes + 1); }, "input cap precedes vertex read");
    Reject([&] { View invalid(nullptr, 12); }, "null vertex input rejected");
}
}
int main() {
    try { Indices(); Vertices(); std::cout << "PASS " << checks << ": owning shared CPU buffer inspection\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
