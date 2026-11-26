#include "spDXSceneGraphOptimizer.h"

#include "../../Analysis/PC/SparkplugAbi.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace Abi = sparkplug::evidence::pc;
        using OwnerLayout = Abi::spDXSceneGraphOptimizerObservedPrefixLayout;
        using CommonLayout = Abi::spSceneGraphOptimizerObservedPrefixLayout;

        constexpr auto CallbackOffset = offsetof(OwnerLayout, base) + offsetof(CommonLayout, callbackVTable);
        constexpr auto GroupOffset = offsetof(OwnerLayout, groupMapSentinel);
        constexpr auto BatchOffset = offsetof(OwnerLayout, batchListSentinel);
        static_assert(CallbackOffset == 0x18 && GroupOffset == 0x4C && BatchOffset == 0x40);
        static_assert(sizeof(OwnerLayout) == spDXSceneGraphOptimizer::CapturedOwnerForAnalysis{}.size());
        static_assert(Abi::spDXSceneGraphOptimizerClassID == spDXSceneGraphOptimizer::ClassID);

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }

        bool MatchesEmptyHead(std::uint32_t ownerLink, std::uint32_t capturedAddress,
            const std::uint8_t* head) noexcept
        {
            return ownerLink != 0 && ownerLink == capturedAddress && ReadWord(head) == ownerLink;
        }
    }

    bool spDXSceneGraphOptimizer::TryReadPcEmptyOnNodeForAnalysis(const CapturedOwnerForAnalysis& owner,
        const CapturedGroupSentinelForAnalysis& group, const CapturedBatchSentinelForAnalysis& batch,
        std::uint8_t& output) noexcept
    {
        if (ReadWord(owner.data()) != Abi::spDXSceneGraphOptimizerVTable
            || ReadWord(owner.data() + CallbackOffset) != Abi::spDXSceneGraphOptimizerCallbackVTable)
            return false;

        const auto groupLink = ReadWord(owner.data() + GroupOffset);
        if (!MatchesEmptyHead(groupLink, group.capturedAddress, group.bytes.data()))
            return false;
        const auto batchLink = ReadWord(owner.data() + BatchOffset);
        if (batchLink == groupLink || !MatchesEmptyHead(batchLink, batch.capturedAddress, batch.bytes.data()))
            return false;

        output = 1;
        return true;
    }
}
