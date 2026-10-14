#pragma once

// Exact original path proven by the PC diagnostic string:
// Z:\Sparkplug\Code\Sparkplug\spTemplateObject.cpp

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sparkplug::reconstruction
{
    class spNode;
    class spTemplateObject final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x014E1394;
        static constexpr std::size_t NativePathCapacity = 0x100;
        static constexpr std::uint32_t NativeStateCount = 4;
        using Vector3 = std::array<float, 3>;
        using Matrix3 = std::array<float, 9>;

        spTemplateObject();
        ~spTemplateObject() override;

        spTemplateObject(const spTemplateObject&) = delete;
        spTemplateObject& operator=(const spTemplateObject&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // The native constructor deliberately leaves state/path uninitialized;
        // these safe analytical accessors make that condition explicit.
        [[nodiscard]] bool SetSerializedDescriptorForAnalysis(
            std::uint32_t state,
            std::string_view path);
        [[nodiscard]] bool HasSerializedDescriptorForAnalysis() const noexcept;
        [[nodiscard]] std::optional<std::uint32_t>
            GetNativeStateForAnalysis() const noexcept;
        [[nodiscard]] const char* GetResourcePathForAnalysis() const noexcept;

        void SetLoadedObjectForAnalysis(std::shared_ptr<spBaseObject> object) noexcept;
        [[nodiscard]] spBaseObject* GetLoadedObjectForAnalysis() const noexcept;

        [[nodiscard]] std::uint32_t GetField12CForAnalysis() const noexcept;
        [[nodiscard]] std::int32_t GetField130ForAnalysis() const noexcept;

        // Native descriptor placement at +174/+180/+1A4. The portable default
        // is the PC state after the original shared-matrix startup initializer;
        // pristine PS2 global bytes do not establish its runtime defaults.
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept;
        [[nodiscard]] const Matrix3& GetOrientationForAnalysis() const noexcept;
        [[nodiscard]] const Vector3& GetScaleForAnalysis() const noexcept;
        void SetPlacementForAnalysis(const Vector3& position,
            const Matrix3& orientation, const Vector3& scale) noexcept;

        // HOST entry for the repeated native Instance graph placement blocks.
        // It transfers P/R/S to a selected Node in that order and marks each
        // transfer dirty. Selecting/cloning/attaching that Node belongs to the
        // instance graph algorithm, not to this descriptor convenience API.
        void ApplyNodePlacementForAnalysis(spNode& node) const noexcept;

    private:
        // Portable representations of constructor/copy-proven roles. Native
        // compiler layouts remain in Analysis/PC and Analysis/PS2.
        std::array<std::uint32_t, 2> field14_{};
        std::shared_ptr<spBaseObject> loadedObject_;
        std::optional<std::uint32_t> state_;
        std::shared_ptr<spBaseObject> streamOrResource_;
        std::uint32_t field28_ = 0;
        std::optional<std::string> resourcePath_;
        std::uint32_t field12C_ = 0;
        std::int32_t field130_ = -1;
        std::array<std::uint8_t, 0x40> field134_{};
        Vector3 position_{0.0F, 0.0F, 0.0F};
        Matrix3 orientation_{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
        Vector3 scale_{1.0F, 1.0F, 1.0F};
    };
}
