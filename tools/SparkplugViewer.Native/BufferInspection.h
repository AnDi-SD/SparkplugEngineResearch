#pragma once
// Host ownership and projections of the shared CPU buffer readers. This does
// not initialize an occluder, a mesh, a renderer, or any GPU resource.
#include "Code/Sparkplug/spIndexBuffer.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include <cstdint>

namespace spvhost {
inline constexpr std::uint32_t MaximumBufferInspectionBytes = 16u * 1024u * 1024u;
struct IndexBufferInfo {
    std::uint32_t primitiveType, primitiveCount, indexCount, formatFlags, elementSize, finalPosition;
};
struct VertexBufferInfo {
    std::uint32_t componentFlags, vertexCount, flags, stride, componentCount, byteCount, finalPosition;
};
static_assert(sizeof(IndexBufferInfo) == 24 && sizeof(VertexBufferInfo) == 28);

class IndexBufferInspection final {
public:
    IndexBufferInspection(const std::uint8_t*, std::uint32_t);
    [[nodiscard]] IndexBufferInfo Info() const noexcept;
    void CopyIndices(std::uint32_t*, std::uint32_t) const;
private:
    sparkplug::reconstruction::spIndexBuffer buffer_;
    std::uint32_t finalPosition_ = 0;
};
class VertexBufferInspection final {
public:
    VertexBufferInspection(const std::uint8_t*, std::uint32_t);
    [[nodiscard]] VertexBufferInfo Info() const noexcept;
    void CopyPositions(float*, std::uint32_t floats) const;
private:
    sparkplug::reconstruction::spVertexBuffer buffer_;
    std::uint32_t finalPosition_ = 0;
};
}
