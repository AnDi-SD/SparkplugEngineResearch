// Exact original translation-unit path recovered from the PC executable:
//   Z:\Sparkplug\Code\SparkplugPC\spPCApp.cpp

#include "spPCApp.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord PCAppRecord{
            spPCApp::ClassID,
            spApp::ClassID,
            "spPCApp",
            &spApp::StaticRTTI(),
            nullptr,
            nullptr,
        };
    }

    spPCApp::spPCApp() noexcept = default;

    spPCApp::~spPCApp() = default;

    const spRTTIRecord& spPCApp::StaticRTTI() noexcept
    {
        return PCAppRecord;
    }

    std::unique_ptr<spBaseObject> spPCApp::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spPCApp::vfunc_18() const noexcept
    {
        return PCAppRecord;
    }

    bool spPCApp::vfunc_20_Initialize()
    {
        // Native 0x004C3230 obtains HINSTANCE, registers the class and creates
        // the main HWND.  Performing those external effects would make an
        // isolated analysis test unsafe.  Window creation remains behind the
        // future platform backend; returning false contains the missing edge.
        return false;
    }

    bool spPCApp::vfunc_24_Update()
    {
        vfunc_30_PreUpdate();
        // Native 0x004C2D60 always returns true after foreground-window
        // throttling.  Game subclasses add the actual per-frame failure path.
        return true;
    }

    void spPCApp::vfunc_28_Shutdown()
    {
        // Native 0x004C2D10 tail-calls the spApp service shutdown cascade.
        // Those global managers are outside the current reconstruction slice.
        windowHandle_ = 0;
    }

    bool spPCApp::vfunc_2C_Run()
    {
        // 0x004C2C70 processes all queued Win32 messages.  When no message is
        // available it calls virtual Update; WM_QUIT is a successful exit,
        // while a false Update is failure.  A missing probe is contained
        // instead of creating an accidental infinite loop in host tests.
        if (messagePump_ == nullptr)
        {
            return false;
        }

        while (!GetStateFlagForAnalysis())
        {
            switch (messagePump_(messagePumpContext_))
            {
            case PumpResult::Quit:
                SetStateFlagForAnalysis(true);
                return true;
            case PumpResult::DispatchedMessage:
                continue;
            case PumpResult::NoMessage:
                if (!vfunc_24_Update())
                {
                    SetStateFlagForAnalysis(true);
                    return false;
                }
                break;
            }
        }
        return true;
    }

    void spPCApp::SetMessagePumpForAnalysis(
        const MessagePumpForAnalysis pump,
        void* const context) noexcept
    {
        messagePump_ = pump;
        messagePumpContext_ = context;
    }

    std::int32_t spPCApp::GetWindowWidthForAnalysis() const noexcept
    {
        return windowWidth_;
    }

    std::int32_t spPCApp::GetWindowHeightForAnalysis() const noexcept
    {
        return windowHeight_;
    }

    std::uint32_t spPCApp::GetWindowStyleForAnalysis() const noexcept
    {
        return windowStyle_;
    }

    void spPCApp::vfunc_30_PreUpdate() noexcept
    {
    }
}
