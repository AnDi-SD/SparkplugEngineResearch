#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxArrowTrapHost.h"

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
