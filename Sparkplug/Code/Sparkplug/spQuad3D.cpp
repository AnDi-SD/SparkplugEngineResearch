#include "spQuad3D.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    namespace
    {
        bool Fail(std::string* error, const char* text)
        { if (error) *error = text; return false; }
        void Normalize(spQuad3D::Vector3& vector)
        {
            // PC41D2D0 retains the squared sum, sqrt and reciprocal on x87.
            const double length = std::sqrt((double(vector[0]) * vector[0] +
                double(vector[1]) * vector[1]) + double(vector[2]) * vector[2]);
            if (length > double(0.001f))
                for (auto& value : vector) value = float(double(value) * (1.0 / length));
            else vector = {0, 0, 0};
        }
        spQuad3D::Vector3 Cross(const spQuad3D::Vector3& a, const spQuad3D::Vector3& b)
        {
            return {float(double(a[1]) * b[2] - double(a[2]) * b[1]),
                float(double(a[2]) * b[0] - double(a[0]) * b[2]),
                float(double(a[0]) * b[1] - double(a[1]) * b[0])};
        }
    }
    spQuad3D::~spQuad3D() = default;
    const spRTTIRecord& spQuad3D::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spBaseObject::ClassID, "spQuad3D",
            &spBaseObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spQuad3D>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spQuad3D::vfunc_18() const noexcept { return StaticRTTI(); }
    std::unique_ptr<spBaseObject> spQuad3D::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spQuad3D>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spQuad3D::DrawForAnalysis(const CameraOrientation& camera, host::spQuad3DHost& host, std::string* error)
    {
        if (!host.acquireVertices || !host.acquireIndices || !host.stateOverride ||
            !host.selectVertices || !host.applyMaterial || !host.draw)
            return Fail(error, "Quad3D renderer callbacks are missing.");
        auto* vertices = host.acquireVertices(0x900);
        if (!vertices) return false;
        auto* indices = host.acquireIndices();
        if (!indices) return false;
        // Refusal of nonfinite geometry is host preflight after native buffer
        // acquisition. The original has no corresponding finite-input guard.
        for (const auto value : camera)
            if (!std::isfinite(value)) return Fail(error, "Quad3D camera must be finite.");
        for (const auto value : state_.position)
            if (!std::isfinite(value)) return Fail(error, "Quad3D position must be finite.");
        if (!std::isfinite(state_.width) || !std::isfinite(state_.height))
            return Fail(error, "Quad3D size must be finite.");
        Vector3 forward{-camera[6], -camera[7], -camera[8]};
        Normalize(forward);
        Vector3 up{0, 1, 0};
        if (std::abs(std::abs(double(forward[1])) - 1.0) <= double(0.001f))
        { up = {camera[3], camera[4], camera[5]}; Normalize(up); }
        auto right = Cross(up, forward);
        Normalize(right);
        up = Cross(forward, right);
        Normalize(up);
        for (auto& value : right) value = float(double(value) * (double(state_.width) * 0.5));
        for (auto& value : up) value = float(double(value) * (double(state_.height) * 0.5));
        std::array<host::spQuad3DVertex, 4> result{};
        constexpr int signs[4][2]{{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
        for (unsigned v = 0; v < 4; ++v)
        {
            result[v].color = state_.color;
            result[v].uv = {v < 2 ? 0.f : 1.f, (v & 1) ? 1.f : 0.f};
            for (unsigned c = 0; c < 3; ++c)
            {
                double first = double(state_.position[c]) + signs[v][0] * double(right[c]);
                if (c) first = float(first); // original Y/Z spill before second term
                result[v].position[c] = float(first + signs[v][1] * double(up[c]));
                if (!std::isfinite(result[v].position[c]))
                    return Fail(error, "Quad3D finite geometry overflowed.");
            }
        }
        for (unsigned v = 0; v < 4; ++v) vertices[v] = result[v];
        constexpr std::uint16_t order[6]{0, 2, 1, 1, 2, 3};
        for (unsigned i = 0; i < 6; ++i) indices[i] = order[i];
        if (!host.stateOverride())
            host.selectVertices(vertices_ ? vertices_.get() : host.defaultVertexBuffer, host.worldMatrixToken);
        host.applyMaterial(material_.get());
        return host.draw(2, 2, 4) != 0;
    }
}
