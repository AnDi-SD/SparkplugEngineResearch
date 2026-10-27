#pragma once

// Inferred path/API. Native PC has a primary FX interface and its BaseObject
// interface at +4. This logical portable hierarchy does not reproduce that ABI.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/spProjectionFxHost.h"

namespace sparkplug::reconstruction
{
    enum class spProjectionFxInitializationResultForAnalysis
    {
        Completed,
        UnqualifiedMissingOwner,
        UnqualifiedMaterialInitialization,
    };
    class spProjectionFX : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0xF147D2EE;
        spProjectionFX() noexcept = default;
        ~spProjectionFX() override = default;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // The two native primary-interface slots remain distinct from the
        // secondary object interface. The unqualified draw slot is not exposed
        // as a successful portable operation.
        virtual spProjectionFxInitializationResultForAnalysis InitializeForAnalysis(const spProjectionFxHost&) = 0;
        // Literal owner18/byte1C data bindings. No native setter name is claimed.
        void BindOwnerForAnalysis(const void* value) noexcept { owner_ = value; }
        const void* BorrowedOwnerForAnalysis() const noexcept { return owner_; }
        std::uint8_t InitializedByteForAnalysis() const noexcept { return initialized_; }
        void SetInitializedByteForFixture(std::uint8_t value) noexcept { initialized_ = value; }
    protected:
        const void* owner_ = nullptr;
        std::uint8_t initialized_ = 0;
    };
}
