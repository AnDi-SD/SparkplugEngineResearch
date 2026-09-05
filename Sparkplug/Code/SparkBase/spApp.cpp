#include "spApp.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        // Both native registrations have null factory and property callbacks.
        // The registered parent intentionally differs from C++ construction:
        // it is spBaseObject rather than spCrossPlatform.
        const spRTTIRecord AppRecord{
            spApp::ClassID,
            spBaseObject::ClassID,
            "spApp",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };
    }

    spApp* spApp::instance_ = nullptr;

    spApp::spApp() noexcept
    {
        // PC 0x004019B0 and PS2 0x00100230 both publish the complete object
        // and initialize the trailing flag/pointer fields to zero.
        instance_ = this;
    }

    spApp::~spApp()
    {
        // The native destructors release the +0x1c character allocation,
        // shut down globally owned services, and clear the singleton.  Global
        // service ownership is not reconstructed in this isolated slice.
        ownedText_.clear();
        instance_ = nullptr;
    }

    const spRTTIRecord& spApp::StaticRTTI() noexcept
    {
        return AppRecord;
    }

    spApp* spApp::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spApp::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spApp::vfunc_18() const noexcept
    {
        return AppRecord;
    }

    const char* spApp::vfunc_GetEmptyString() const noexcept
    {
        return "";
    }

    bool spApp::GetStateFlagForAnalysis() const noexcept
    {
        return stateFlag_;
    }

    void spApp::SetStateFlagForAnalysis(const bool value) noexcept
    {
        stateFlag_ = value;
    }

    const char* spApp::GetOwnedTextForAnalysis() const noexcept
    {
        return ownedText_.empty() ? nullptr : ownedText_.c_str();
    }

    void spApp::SetOwnedTextForAnalysis(const std::string_view value)
    {
        ownedText_.assign(value);
    }
}
