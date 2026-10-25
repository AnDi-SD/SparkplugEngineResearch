#pragma once

// Portable recovered state/command operations, not a native PS2 class layout.
#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Borrowed writable words with explicit PS2 addresses. No DMA is executed.
    // Word storage must not alias this descriptor or the manager, and must stay
    // stable during a call. base/write/tail addresses denote aligned qwords.
    struct PS2PipelineCommandBufferForAnalysis final
    {
        std::uint32_t* words = nullptr;
        std::size_t wordCount = 0;
        std::uint32_t baseAddress = 0;
        std::uint32_t writeAddress = 0; // Native buffer+08.
        std::uint32_t tailAddress = 0;  // Native buffer+0C, preceding chain tag.
    };

    class spPS2MicrocodeManager final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x0CEC4B69;
        static constexpr std::size_t NativeSizeForAnalysis = 0x1C;
        static constexpr std::size_t NativeCurrentPacketOffsetForAnalysis = 0x18;

        spPS2MicrocodeManager() noexcept;
        ~spPS2MicrocodeManager() override;
        spPS2MicrocodeManager(const spPS2MicrocodeManager&) = delete;
        spPS2MicrocodeManager& operator=(const spPS2MicrocodeManager&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] static spPS2MicrocodeManager* SingletonForAnalysis() noexcept;
        [[nodiscard]] std::uint32_t CurrentPacketForAnalysis() const noexcept;

        // Shared sequence in pipeline methods2073F0/208490; original ownership
        // remains those pipelines, not an invented native manager method.
        // Changed packet: CALL(masked31 address), END, update full32 cache.
        // Every call: patch CNT/QWC + MSCAL0 and append END, including cache hits.
        // Added host span/address guards fail before writes/cache changes; these
        // guards do not claim original bad-pointer/fault behavior.
        [[nodiscard]] bool AppendPipelineCommandsForAnalysis(
            std::uint32_t packetAddress, PS2PipelineCommandBufferForAnalysis& buffer) noexcept;

    private:
        std::uint32_t currentPacket_ = 0;
    };
}
