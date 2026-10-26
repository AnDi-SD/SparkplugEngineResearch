#pragma once

// Inferred header/path. PC factory/RTTI establish spBaseObject ancestry and a
// secondary singleton interface; the portable class does not imitate its ABI.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxTargetManagerHost.h"
#include <array>

namespace winx::reconstruction
{
    enum class wxTargetNotificationResultForAnalysis
    {
        NotRun, Ignored, UnqualifiedActivation, UnqualifiedDeactivation, InvalidInput
    };

    class wxTargetManager final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3582501B;
        explicit wxTargetManager(wxTargetManagerHost&);
        ~wxTargetManager() override;
        wxTargetManager(const wxTargetManager&) = delete;
        wxTargetManager& operator=(const wxTargetManager&) = delete;
        static void SetFactoryHostForAnalysis(wxTargetManagerHost*) noexcept;
        [[nodiscard]] static wxTargetManager* GetInstanceForAnalysis() noexcept;
        // Analytical binding for explicit singleton stores in recovered callers
        // after a successful factory return; not an attributed native method.
        static void PublishInstanceForAnalysis(wxTargetManager& value) noexcept { instance_ = &value; }
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // PC table uses the literal root Copy: no own state or destination-kind
        // operation. Clone constructs/loads a fresh node before this no-op.
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        // PC59CCC0. Resource path, ref replacement, scale/dirty/disable and
        // graph attach are implemented here; none is a successful host stub.
        // The accepted domain is successful plain-Node loads with stable
        // service/binding storage, valid catalog and a null-Scene graph.
        void InitializeResourceForAnalysis();
        // PC59D030/59CF30 restricted to constructor/default ownership: optional
        // 38/40/44/4C objects remain absent. Active target/effect paths are open.
        void ResetDefaultLifetimeForAnalysis() noexcept;
        [[nodiscard]] const std::shared_ptr<wxTargetNodeBindingForAnalysis>& ResourceForAnalysis() const noexcept
        { return resource_; }
        [[nodiscard]] wxTargetNotificationResultForAnalysis LastNotificationResultForAnalysis() const noexcept
        { return lastNotification_; }
        [[nodiscard]] const std::array<std::uintptr_t, 3>& CurrentWordsForAnalysis() const noexcept { return current_; }
        [[nodiscard]] const std::array<std::uint8_t, 3>& DefaultFlagsForAnalysis() const noexcept { return flags_; }
        [[nodiscard]] const std::array<std::uintptr_t, 6>& OptionalDefaultWordsForAnalysis() const noexcept { return optional_; }

    private:
        static void ReleaseNodeForAnalysis(std::shared_ptr<wxTargetNodeBindingForAnalysis>&) noexcept;
        static wxTargetManager* instance_;
        wxTargetManagerHost& host_;
        // Original constructor initializes all these represented words/bytes
        // to zero. Padding and allocator state belong only to the ABI record.
        std::array<std::uintptr_t, 3> current_{};
        std::array<std::uint8_t, 3> flags_{};
        std::array<std::uintptr_t, 6> optional_{}; // native38/3C/40/44/48/4C, absent in this slice
        std::shared_ptr<wxTargetNodeBindingForAnalysis> resource_;
        wxTargetNotificationResultForAnalysis lastNotification_ = wxTargetNotificationResultForAnalysis::NotRun;
    };
}
