#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxArrowTrapHost.h"

#include <array>
#include <optional>

namespace winx::reconstruction
{
    struct wxArrowTrapMessageForAnalysis final { std::uint32_t code = 0; };

    // Native physical parent: wxEntity. Its unimplemented operations are
    // explicit host calls; spNamedObject supplies the portable object API.
    class wxArrowTrap : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x31AE6FA8;
        struct StateForAnalysis final
        {
            std::uint32_t entityWord84 = 1;
            std::uint8_t active = 0;
            float speed = 2000.0f;            // PC +128, PS2 +134
            std::uint32_t pauseMilliseconds = 1000; // PC +12C, PS2 +138
            float intervalSeconds = 0;       // PC +130, PS2 +13C
            void* arrows[3]{};                // +138, PS2 +144
            void* emitters[3]{};              // +144, PS2 +150
            wxArrowPosition arrowPositions[2]{}; // +150, PS2 +15C
            wxArrowPosition startPosition{}; // +168, PS2 +174
            std::uint32_t phase[3]{};         // +174, PS2 +180
            float timer[3]{};                 // +180, PS2 +18C
            void* ownedComponents[3]{};       // +18C, PS2 +198
            std::uint8_t inRange = 1;         // +198, PS2 +1A4
        };
        explicit wxArrowTrap(wxArrowTrapHost&);
        ~wxArrowTrap() override;
        wxArrowTrap(const wxArrowTrap&) = delete;
        wxArrowTrap& operator=(const wxArrowTrap&) = delete;
        static void SetFactoryHostForAnalysis(wxArrowTrapHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_2C_InRangeForAnalysis(const wxArrowPosition&) noexcept;

        // The caller supplies positions from the PC root node and argument.
        // Finite normal coordinates and signed zeros up to |16384| are
        // admitted. A refusal leaves the range byte intact. Success returns AL.
        [[nodiscard]] std::optional<std::uint8_t> TryUpdatePcRangeForAnalysis(
            const wxArrowPosition& rootPosition, const wxArrowPosition& point) noexcept;

        using CapturedPcPropertyReceiverForAnalysis = std::array<std::uint8_t, 0x19C>;
        using PcSpeedReturnForAnalysis = std::array<std::uint8_t, 10>;
        enum class PcPropertyProfileForAnalysis { Unqualified, CapturedPrimary };

        struct ComposedPcSpeedForAnalysis final
        {
            PcSpeedReturnForAnalysis before{};
            CapturedPcPropertyReceiverForAnalysis afterSet{};
            PcSpeedReturnForAnalysis after{};
        };
        struct ComposedPcPauseForAnalysis final
        {
            std::uint32_t before{};
            CapturedPcPropertyReceiverForAnalysis afterSet{};
            std::uint32_t after{};
        };

        // These operations use the complete captured PC receiver as byte data.
        // Speed getters require finite normal binary32 or signed zero.
        // The setters write the argument DWORD verbatim. Refusal preserves output.
        [[nodiscard]] static bool TryReadPcSpeedForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            PcSpeedReturnForAnalysis& output, PcPropertyProfileForAnalysis profile) noexcept;
        [[nodiscard]] static bool TryWritePcSpeedBitsForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            std::uint32_t argument, CapturedPcPropertyReceiverForAnalysis& output,
            PcPropertyProfileForAnalysis profile) noexcept;
        [[nodiscard]] static bool TryComposePcSpeedForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            std::uint32_t argument, ComposedPcSpeedForAnalysis& output, PcPropertyProfileForAnalysis profile) noexcept;

        [[nodiscard]] static bool TryReadPcPauseForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            std::uint32_t& output, PcPropertyProfileForAnalysis profile) noexcept;
        [[nodiscard]] static bool TryWritePcPauseForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            std::uint32_t argument, CapturedPcPropertyReceiverForAnalysis& output,
            PcPropertyProfileForAnalysis profile) noexcept;
        [[nodiscard]] static bool TryComposePcPauseForAnalysis(const CapturedPcPropertyReceiverForAnalysis& before,
            std::uint32_t argument, ComposedPcPauseForAnalysis& output, PcPropertyProfileForAnalysis profile) noexcept;

        void SetupForAnalysis();
        void UpdateForAnalysis() noexcept;
        static bool RegisterPropertiesForAnalysis(wxArrowTrapHost&);
        StateForAnalysis& GetStateForAnalysis() noexcept { return state_; }
        const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
    private:
        void* FindRenderableForAnalysis(void*) const;
        void EnableEmitterForAnalysis(std::size_t);
        void WaitForAnalysis(std::size_t) noexcept;
        void FlyForAnalysis(std::size_t) noexcept;
        wxArrowTrapHost& host_;
        StateForAnalysis state_;
    };
}
