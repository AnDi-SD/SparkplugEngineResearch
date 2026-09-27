#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAudioEmitterHost.h"

namespace winx::reconstruction
{
    // The native physical parent is wxEntity. Its missing implementation and
    // the native audio maps are explicit host boundaries.
    class wxAudioEmitter : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0xCCABEFEA;
        struct StateForAnalysis final
        {
            void* ownedResource = nullptr; // PC +124, PS2 +130
            std::uint8_t flag5C = 0;       // PC +15C, PS2 +174
            std::uint8_t flag5D = 0;       // PC +15D, PS2 +175
            std::uint32_t words[3]{};      // PC +180..188, PS2 +190..198
        };

        explicit wxAudioEmitter(wxAudioEmitterHost&);
        ~wxAudioEmitter() override;
        wxAudioEmitter(const wxAudioEmitter&) = delete;
        wxAudioEmitter& operator=(const wxAudioEmitter&) = delete;
        static void SetFactoryHostForAnalysis(wxAudioEmitterHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        // Native v14: PC 586C60 / PS2 3C7DE0.
        void DispatchTagForAnalysis(const char* name, std::uint32_t variant);
        // Native PC comparator 581840. The third key word is ignored.
        static bool CompareKeysForAnalysis(const char* leftName, std::int32_t leftVariant,
            const char* rightName, std::int32_t rightVariant) noexcept;
        StateForAnalysis& GetStateForAnalysis() noexcept { return state_; }
        const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
    private:
        wxAudioEmitterHost& host_;
        StateForAnalysis state_;
    };
}
