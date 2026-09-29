#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxCharacterHost.h"

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // The native parent is wxEntity. The character host still mediates its
    // constructor argument and lifecycle, which are not bound to the partial
    // portable wxEntity implementation.
    class wxCharacter : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x0003CC73;
        // Index zero represents native PC +124 (PS2 +130). Opaque bytes have
        // no inferred names or ownership semantics.
        using OwnBytesForAnalysis = std::array<std::uint8_t, 0x3C>;

        explicit wxCharacter(wxCharacterHost& host);
        ~wxCharacter() override;
        wxCharacter(const wxCharacter&) = delete;
        wxCharacter& operator=(const wxCharacter&) = delete;

        static void SetFactoryHostForAnalysis(wxCharacterHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;

        // Native slot 10 delegates reference assignment to wxEntity, then
        // clears external flag 10 only when the selector is zero.
        void vfunc_28_AssignReferenceForAnalysis(void* reference);
        // Native slot 11 reads flags from the object reached through +24.
        // The caller supplies flags from the external object reached via +24.
        [[nodiscard]] bool vfunc_2C_FlagsAllowForAnalysis(std::uint32_t flags) const noexcept;
        [[nodiscard]] bool vfunc_30() const noexcept { return true; }
        // Notification 2749: publish this character for a matching kind when
        // its enable byte is set. The remaining notification cases are open.
        void Handle2749ForAnalysis(std::uint32_t kind, wxCharacter*& result) noexcept;
        [[nodiscard]] OwnBytesForAnalysis& GetOwnBytesForAnalysis() noexcept { return ownBytes_; }
        [[nodiscard]] const OwnBytesForAnalysis& GetOwnBytesForAnalysis() const noexcept { return ownBytes_; }

    private:
        wxCharacterHost& host_;
        OwnBytesForAnalysis ownBytes_{};
    };
}
