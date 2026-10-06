#pragma once

// Original identity and lifetime; source path/API names are analytical.
// Native primary interface is separate from the Named/CrossPlatform view+4.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <functional>
#include <utility>

namespace sparkplug::reconstruction
{
    class spVideoStream : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x27DE5AF5;
        using BufferOwnerForAnalysis = std::unique_ptr<std::uint8_t[], std::function<void(std::uint8_t*)>>;
        struct StateForAnalysis final { std::uint8_t byte18 = 0, byte19 = 0; std::uint32_t word20 = 0; };
        spVideoStream();
        ~spVideoStream() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        // CrossPlatform retains original null clone, and base registration has
        // no allocating factory. No abstract native interface method is faked.
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        void SetStateForAnalysis(const StateForAnalysis& state) noexcept { state_ = state; }
        [[nodiscard]] const std::uint8_t* GetOwnedBufferForAnalysis() const noexcept { return buffer_.get(); }
        // Host-only ownership injection. The original setter/allocation policy
        // is unknown; only destructor releasing +1C is established here.
        void AdoptBufferForAnalysis(BufferOwnerForAnalysis buffer) { buffer_ = std::move(buffer); }

    private:
        StateForAnalysis state_;
        BufferOwnerForAnalysis buffer_;
    };
}
