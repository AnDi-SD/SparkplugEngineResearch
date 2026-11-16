#include "wxCylindricalDoor.h"
#include "Analysis/PC/wxCylindricalDoorAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        using Door = wxCylindricalDoor;
        using Prefix = Door::CapturedPrefixForAnalysis;
        using Property = Door::PropertyForAnalysis;
        using Profile = Door::PropertyProfileForAnalysis;
        using Result = Door::PropertyResultForAnalysis;
        namespace Abi = winx::analysis::pc::cylindrical_door;
        static_assert(sizeof(Prefix) == Abi::CapturedPrefixBytes);

        Result Admit(const Prefix& before, const Property property, const Profile profile,
            std::size_t& offset) noexcept
        {
            if (profile != Profile::PcCapturedPrimaryRawByteProperties) return Result::UnqualifiedProfile;
            const auto primary = std::uint32_t(before[0]) | (std::uint32_t(before[1]) << 8)
                | (std::uint32_t(before[2]) << 16) | (std::uint32_t(before[3]) << 24);
            if (primary != Abi::PrimaryTable) return Result::CapturedPrimaryNotAdmitted;
            switch (property)
            {
            case Property::Locked: offset = Abi::LockedOffset; break;
            case Property::OpenedByProximity: offset = Abi::OpenedByProximityOffset; break;
            default: return Result::UnknownProperty;
            }
            return Result::Completed;
        }
    }

    wxCylindricalDoor::PropertyResultForAnalysis wxCylindricalDoor::ReadPropertyRawForAnalysis(
        const CapturedPrefixForAnalysis& before, const PropertyForAnalysis property,
        std::uint8_t& output, const PropertyProfileForAnalysis profile) noexcept
    {
        std::size_t offset = 0;
        const auto admitted = Admit(before, property, profile, offset);
        if (admitted != PropertyResultForAnalysis::Completed) return admitted;
        output = before[offset];
        return PropertyResultForAnalysis::Completed;
    }

    wxCylindricalDoor::PropertyResultForAnalysis wxCylindricalDoor::WritePropertyRawForAnalysis(
        const CapturedPrefixForAnalysis& before, const PropertyForAnalysis property,
        const std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        const PropertyProfileForAnalysis profile) noexcept
    {
        std::size_t offset = 0;
        const auto admitted = Admit(before, property, profile, offset);
        if (admitted != PropertyResultForAnalysis::Completed) return admitted;
        auto staged = before;
        staged[offset] = static_cast<std::uint8_t>(rawArgument);
        output = staged;
        return PropertyResultForAnalysis::Completed;
    }
}
