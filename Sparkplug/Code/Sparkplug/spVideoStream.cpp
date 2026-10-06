#include "spVideoStream.h"

namespace sparkplug::reconstruction
{
    spVideoStream::spVideoStream() : buffer_(nullptr, [](std::uint8_t* buffer) { delete[] buffer; }) {}
    spVideoStream::~spVideoStream() = default;
    const spRTTIRecord& spVideoStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spCrossPlatform::ClassID, "spVideoStream",
            &spCrossPlatform::StaticRTTI(), nullptr, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered; return record;
    }
    const spRTTIRecord& spVideoStream::vfunc_18() const noexcept { return StaticRTTI(); }
}
