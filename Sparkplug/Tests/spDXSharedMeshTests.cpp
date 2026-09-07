#include "../Code/SparkBase/spMemoryStream.h"
#include "../Code/SparkplugDX/spDXCombinedVB.h"
#include "../Code/SparkplugDX/spDXSharedMeshData.h"
#include "../Code/SparkplugDX/spDXSharedMeshDataSerializer.h"
#include "../Code/SparkplugDX/spDXIndexBuffer.h"
#include "../Code/SparkplugDX/spDXVertexBuffer.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace {
void Require(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
std::string Hex(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::string result; result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result += "0123456789abcdef"[bytes[i] >> 4];
        result += "0123456789abcdef"[bytes[i] & 15];
    }
    return result;
}
}
int main(int argc, char** argv) {
    try {
        const std::uint16_t values[] = {0, 1, 2};
        std::vector<std::byte> indices(sizeof(values)), vertices(24 * sizeof(float));
        std::memcpy(indices.data(), values, sizeof(values));
        for (int i = 0; i < 24; ++i) {
            const float value = static_cast<float>(i);
            std::memcpy(vertices.data() + i * sizeof(value), &value, sizeof(value));
        }
        spDXCombinedVB combined; spDXSharedMeshDataSerializer serializer;
        spMemoryStream stream; spDXSharedMeshData sequential, contiguous;
        Require(combined.InitializePayloadForAnalysis(indices, vertices)
            && stream.Open("shared") && serializer.WritePayloadForAnalysis(stream, combined)
            && stream.Seek(spStream::SeekSource::essStart, 0), "write payload");
        std::uint32_t size = 0, sequentialPosition = 0, contiguousPosition = 0;
        Require(stream.GetSize(&size) && serializer.ReadPayloadForAnalysis(stream, sequential)
            && stream.GetCurrentPosition(sequentialPosition) && sequentialPosition == size,
            "sequential convenience consumes the whole payload");
        Require(stream.Seek(spStream::SeekSource::essStart, 0)
            && serializer.ReadContiguousPayloadForAnalysis(stream, contiguous)
            && stream.GetCurrentPosition(contiguousPosition) && contiguousPosition == 8,
            "native contiguous success leaves cursor after sizes");
        for (const auto* object : {&sequential, &contiguous}) {
            Require(object->GetIndexBufferForAnalysis()->GetDataForAnalysis() == indices
                && object->GetVertexBufferForAnalysis()->GetDataForAnalysis() == vertices,
                "both readers materialize every byte");
        }
        const auto oldIndex = contiguous.GetIndexBufferForAnalysis();
        for (auto declared : {16u, 0xFFFFFFFFu}) {
            spMemoryStream bad;
            Require(bad.Open("bad") && bad.Write(declared) && bad.Write(declared), "bad fixture");
            for (bool view : {false, true}) {
                Require(bad.Seek(spStream::SeekSource::essStart, 0), "bad rewind");
                Require(!(view ? serializer.ReadContiguousPayloadForAnalysis(bad, contiguous)
                    : serializer.ReadPayloadForAnalysis(bad, contiguous)), "reject absent/overflow payload before allocation");
                Require(contiguous.GetIndexBufferForAnalysis() == oldIndex,
                    "invalid input preserves an existing target");
            }
        }
        spMemoryStream closed;
        Require(!serializer.ReadContiguousPayloadForAnalysis(closed, contiguous), "no raw buffer");
        spMemoryStream prefixed; const std::uint32_t prefix = 0xDEADBEEF;
        Require(prefixed.Open("origin") && prefixed.Write(prefix)
            && serializer.WritePayloadForAnalysis(prefixed, combined), "origin fixture");
        prefixed.SetLogicalOriginForAnalysis(4);
        Require(prefixed.Seek(spStream::SeekSource::essStart, 0)
            && !serializer.ReadContiguousPayloadForAnalysis(prefixed, contiguous)
            && prefixed.GetCurrentPosition(contiguousPosition) && contiguousPosition == 0,
            "view explicitly rejects unsupported origin before input");
        Require(serializer.ReadPayloadForAnalysis(prefixed, contiguous)
            && contiguous.GetIndexBufferForAnalysis()->GetDataForAnalysis() == indices,
            "sequential reader bounds use physical size and logical origin");
        if (argc == 2 && std::string(argv[1]) == "--capture") {
            std::cout << "{\"wire\":\"" << Hex(stream.GetBuffer(), size)
                << "\",\"indices\":\"" << Hex(indices.data(), indices.size())
                << "\",\"vertices\":\"" << Hex(vertices.data(), vertices.size())
                << "\",\"sequentialCursor\":" << sequentialPosition
                << ",\"contiguousCursor\":8}\n";
        } else std::cout << "PASS DX shared payload bounds, bytes, cursor and ownership\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
