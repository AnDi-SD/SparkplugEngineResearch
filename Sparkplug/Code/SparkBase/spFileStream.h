#pragma once

// Inferred header path.  spFileStream class identity and behavior are proven
// on PC and PS2, but neither executable contains an independent original
// header or translation-unit path for this shared abstract layer.

#include "spStream.h"

namespace sparkplug::reconstruction
{
    class spFileStream : public spStream
    {
    public:
        static constexpr spClassID ClassID = 0x5E0623EC;

        spFileStream() noexcept = default;
        ~spFileStream() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // The only stream operation made concrete by this intermediate class.
        // PC 0x006BE7C0 and PS2 0x00112180 dispatch the two-argument overload
        // with exact mode bit 1 (read).
        [[nodiscard]] bool Open(const char* streamName) override;
    };
}
