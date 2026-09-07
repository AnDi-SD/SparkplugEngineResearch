#pragma once
// Inferred path. PC class6B3E7BAA derives from original spLight72444900.
#include "../Sparkplug/spLight.h"
#include <optional>
namespace sparkplug::reconstruction
{
    class spDXLight final : public spLight
    {
    public:
        static constexpr spClassID ClassID=0x6B3E7BAA;
        using DevicePayloadForAnalysis=std::array<std::optional<std::uint32_t>,26>;
        spDXLight() noexcept=default;
        ~spDXLight() override=default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        [[nodiscard]] bool CopyIntoForAnalysis(spDXLight&) const;
        // PC4B58D0 captures dirty flags before base world/bit8 clear, then
        // refreshes enabled light payload using the newly produced world.
        [[nodiscard]] bool UpdateWorldForAnalysis(std::uint32_t inheritedFlags=0,
            const Matrix3* cameraOrientation=nullptr) noexcept override;
        void SetWorldDeviceInputsForAnalysis(const Vector3& defaultVector,std::uint32_t ambientARGB) noexcept
        {worldDefaultVector_=defaultVector;worldAmbientARGB_=ambientARGB;}
        // Actual4B53C0. Position74/directionA4 and global7600E0/73FE98 are
        // explicit already-produced inputs; no scene/world/startup implied.
        [[nodiscard]] bool RefreshDevicePayloadForAnalysis(const Vector3& worldPosition,
            const Vector3& worldDirection,const Vector3& defaultVector,std::uint32_t ambientARGB) noexcept;
        [[nodiscard]] static bool NeedsDeviceRefreshForAnalysis(std::uint32_t storedFlags,
            std::uint32_t inheritedFlags,bool enabled) noexcept;
        [[nodiscard]] const DevicePayloadForAnalysis& GetDevicePayloadForAnalysis() const noexcept {return payload_;}
        // Declared consumer cache, not factory initialization. Empty optionals
        // represent untouched native allocator bytes, never invented zeros.
        void SetDevicePayloadForAnalysis(const DevicePayloadForAnalysis& value) noexcept {payload_=value;}
    private:
        DevicePayloadForAnalysis payload_{};
        // Explicit host values for native globals7600E0/73FE98; defaults model
        // initialized zero-vector/opaque-black inputs, not renderer startup.
        Vector3 worldDefaultVector_{};
        std::uint32_t worldAmbientARGB_=0xff000000u;
    };
}
