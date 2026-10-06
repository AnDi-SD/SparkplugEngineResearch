#include "spStreamError.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateStreamError()
        {
            return std::make_unique<spStreamError>();
        }
        const spRTTIRecord Record{
            spStreamError::ClassID, spError::ClassID, "spStreamError",
            &spError::StaticRTTI(), &CreateStreamError, nullptr};
    }

    spStreamError::~spStreamError() = default;

    const spRTTIRecord& spStreamError::StaticRTTI() noexcept { return Record; }
    const spRTTIRecord& spStreamError::vfunc_18() const noexcept { return Record; }

    std::unique_ptr<spBaseObject> spStreamError::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spStreamError>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // Native clone dispatches the original receiver's empty copy slot;
        // it does not transfer severity, message, source or chain links.
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    const char* spStreamError::ErrorNameForAnalysis() const noexcept
    {
        return "stream error";
    }

    std::string spStreamError::DescribeForAnalysis() const
    {
        const char* label;
        switch (GetCodeForAnalysis())
        {
        case 4: label = "Can't open stream"; break;
        case 5: label = "Stream not opened"; break;
        case 6: label = "Stream reached EOF"; break;
        case 7: label = "Stream already opened"; break;
        case 8: label = "Invalid seek in stream"; break;
        default: return spError::DescribeForAnalysis();
        }
        const auto* message = GetMessageForAnalysis();
        if (GetCodeForAnalysis() == 4 && message == nullptr)
            return "Can't open stream (NULL)";
        std::string result{label};
        if (message != nullptr)
        {
            result += " (";
            result += message;
            result += ')';
        }
        // The native 256-byte global scratch buffer explicitly clears byte
        // 255 after the bounded concatenations. Portable ownership is ours.
        if (result.size() > 255) result.resize(255);
        return result;
    }
}
