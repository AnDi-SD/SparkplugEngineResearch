#include "BufferInspection.h"
#include "BorrowedInput.h"
#include <cstring>
#include <stdexcept>

namespace spvhost {
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void CheckInput(const std::uint8_t* bytes, std::uint32_t size) {
    Require(bytes && size >= 12 && size <= MaximumBufferInspectionBytes,
        "CPU buffer inspection requires 12 bytes..16 MiB");
}
}
IndexBufferInspection::IndexBufferInspection(const std::uint8_t* bytes, std::uint32_t size) {
    CheckInput(bytes, size);
    BorrowedInput input(bytes, size);
    // The shared reader checks the expanded count against this exact byte
    // budget before allocating. No header or topology is decoded by this host.
    Require(buffer_.ReadForAnalysis(input, size), "Cannot read bounded IndexBuffer");
    Require(input.GetCurrentPosition(finalPosition_) && finalPosition_ == size,
        "Trailing IndexBuffer inspection bytes");
}
IndexBufferInfo IndexBufferInspection::Info() const noexcept {
    return {static_cast<std::uint32_t>(buffer_.GetTypeForAnalysis()),
        buffer_.GetPrimitiveCountForAnalysis(), buffer_.GetIndexCountForAnalysis(),
        buffer_.GetFormatFlagsForAnalysis(), static_cast<std::uint32_t>(buffer_.GetIndexElementSizeForAnalysis()),
        finalPosition_};
}
void IndexBufferInspection::CopyIndices(std::uint32_t* output, std::uint32_t count) const {
    Require(count == buffer_.GetIndexCountForAnalysis() && (output || !count),
        "IndexBuffer output size mismatch");
    for (std::uint32_t i = 0; i < count; ++i)
        output[i] = *buffer_.GetIndexForAnalysis(i);
}
VertexBufferInspection::VertexBufferInspection(const std::uint8_t* bytes, std::uint32_t size) {
    CheckInput(bytes, size);
    BorrowedInput input(bytes, size);
    // Layout and allocation preflight remain in spVertexBuffer. The input is
    // borrowed only during this call; the actual buffer owns the resulting data.
    Require(buffer_.ReadForAnalysis(input, size), "Cannot read bounded VertexBuffer");
    Require(input.GetCurrentPosition(finalPosition_) && finalPosition_ == size,
        "Trailing VertexBuffer inspection bytes");
}
VertexBufferInfo VertexBufferInspection::Info() const noexcept {
    return {buffer_.GetComponentFlagsForAnalysis(), buffer_.GetVertexCountForAnalysis(),
        buffer_.GetFlagsForAnalysis(), buffer_.GetVertexStrideForAnalysis(),
        buffer_.GetComponentCountForAnalysis(), buffer_.GetVertexSizeForAnalysis(), finalPosition_};
}
void VertexBufferInspection::CopyPositions(float* output, std::uint32_t floats) const {
    const auto count = buffer_.GetVertexCountForAnalysis();
    Require(std::uint64_t(count) * 3 == floats && (output || !floats),
        "VertexBuffer position output size mismatch");
    const auto stride = buffer_.GetVertexStrideForAnalysis();
    const auto positionOffset = std::uint32_t(buffer_.GetComponentOffsetsForAnalysis()[0]) * sizeof(float);
    const auto& data = buffer_.GetDataForAnalysis();
    Require(positionOffset + 3 * sizeof(float) <= stride && data.size() == std::uint64_t(count) * stride,
        "VertexBuffer position projection exceeds shared layout");
    for (std::uint32_t i = 0; i < count; ++i)
        std::memcpy(output + std::size_t(i) * 3,
            data.data() + std::size_t(i) * stride + positionOffset, 3 * sizeof(float));
}
}
