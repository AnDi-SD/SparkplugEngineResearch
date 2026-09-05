// Inferred translation-unit path.  The common class is present in both native
// binaries, but no independent spFileStream.cpp source string survives.

#include "spFileStream.h"

namespace sparkplug::reconstruction
{
    spFileStream::~spFileStream() = default;

    const spRTTIRecord& spFileStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spStream::ClassID,
            "spFileStream",
            &spStream::StaticRTTI(),
            nullptr,
            nullptr,
        };
        return record;
    }

    const spRTTIRecord& spFileStream::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    bool spFileStream::Open(const char* streamName)
    {
        // The cast keeps virtual dispatch while avoiding C++ overload hiding
        // by this one-argument implementation.
        return static_cast<spStream*>(this)->Open(1, streamName);
    }
}
