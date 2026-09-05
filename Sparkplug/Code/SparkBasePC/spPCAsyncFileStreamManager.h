#pragma once

// Inferred header path.  The class name and platform module are executable-
// backed; unlike spPCFileStream, no exact source path for this leaf survived.

#include "../SparkBase/spAsyncFileStreamManager.h"

namespace sparkplug::reconstruction
{
    class spPCAsyncFileStreamManager final : public spAsyncFileStreamManager
    {
    public:
        static constexpr spClassID ClassID = 0x15B533A4;

        spPCAsyncFileStreamManager() noexcept = default;
        ~spPCAsyncFileStreamManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] bool vfunc_Request(
            const char* streamName,
            spMemoryStream* destination,
            CompletionCallback completion,
            void* context) override;
    };
}
