#pragma once

// Inferred declaration path and analytical method names. The class is
// registered only in the shipped PC image; no PS2 identity is claimed.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/spQuad3DHost.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spQuad3D final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x42B2502F;
        using Vector3 = std::array<float, 3>;
        using CameraOrientation = std::array<float, 9>;
        struct StateForAnalysis final
        {
            Vector3 position{};
            float width = 0, height = 0;
            std::uint32_t color = 0xFFFFFFFFu;
        };
        ~spQuad3D() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native vslot0C is BaseObject: geometry/resources are not copied.
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        void SetStateForAnalysis(const StateForAnalysis& value) noexcept { state_ = value; }
        // Native fields are intrusive references. Shared ownership and the
        // broad BaseObject handles are declared host adaptations.
        void SetVertexBufferForAnalysis(std::shared_ptr<spBaseObject> value) noexcept { vertices_ = std::move(value); }
        void SetMaterialForAnalysis(std::shared_ptr<spBaseObject> value) noexcept { material_ = std::move(value); }
        [[nodiscard]] const std::shared_ptr<spBaseObject>& GetVertexBufferForAnalysis() const noexcept { return vertices_; }
        [[nodiscard]] const std::shared_ptr<spBaseObject>& GetMaterialForAnalysis() const noexcept { return material_; }
        // Camera world orientation rows supply original +98/+A4. Finite
        // geometry uses the original expressions/stores with host double
        // normalization; there is no all-input bit-exact x87 claim.
        [[nodiscard]] bool DrawForAnalysis(const CameraOrientation&, host::spQuad3DHost&, std::string* error = nullptr);
    private:
        StateForAnalysis state_{};
        std::shared_ptr<spBaseObject> vertices_;
        std::shared_ptr<spBaseObject> material_;
    };
}
