#include "spWindowsError.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateWindowsError()
        {
            return std::make_unique<spWindowsError>();
        }
        const spRTTIRecord Record{
            spWindowsError::ClassID, spError::ClassID, "spWindowsError",
            &spError::StaticRTTI(), &CreateWindowsError, nullptr};
    }

    spWindowsError::~spWindowsError() = default;
    const spRTTIRecord& spWindowsError::StaticRTTI() noexcept { return Record; }
    const spRTTIRecord& spWindowsError::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spWindowsError::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spWindowsError>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    const char* spWindowsError::ErrorNameForAnalysis() const noexcept
    {
        return "windows error";
    }
    std::string spWindowsError::DescribeForAnalysis() const
    {
        // This own method never reads error code/severity or calls Win32.
        // Its message was already supplied by the caller/error manager.
        const auto* message = GetMessageForAnalysis();
        if (message == nullptr) return {};
        std::string result{"("};
        result += message;
        result += ')';
        if (result.size() > 255) result.resize(255);
        return result;
    }
}
