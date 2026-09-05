#pragma once

// Inferred header path.  The class name and runtime identity are exact, but
// neither shipped executable preserves an original spApp source/header path.
// Method names that describe behavior are analytical and are not claimed as
// original spellings.

#include "spBaseObject.h"

#include <memory>
#include <string>
#include <string_view>

namespace sparkplug::reconstruction
{
    // Common application lifetime boundary.  Native C++ construction uses
    // spCrossPlatform, while the engine registration deliberately names
    // spBaseObject as its direct base.  StaticRTTI preserves the latter.
    class spApp : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x391B146A;

        spApp() noexcept;
        ~spApp() override;

        spApp(const spApp&) = delete;
        spApp& operator=(const spApp&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spApp* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC emits this as the first class-local primary-table method.  PS2
        // emits the same empty-string behavior in the singleton-support table.
        // Its source-level name and exact interface owner remain unknown.
        [[nodiscard]] virtual const char* vfunc_GetEmptyString() const noexcept;

        // Four consecutive pure slots in the PC binary.  Callers and the
        // spPCApp implementations establish these signatures and lifecycle
        // order; original method spellings remain unknown.
        [[nodiscard]] virtual bool vfunc_20_Initialize() = 0;
        [[nodiscard]] virtual bool vfunc_24_Update() = 0;
        virtual void vfunc_28_Shutdown() = 0;
        [[nodiscard]] virtual bool vfunc_2C_Run() = 0;

        // Safe analytical access to the two proven native fields.  No native
        // setter names have yet been recovered, so these wrappers deliberately
        // do not pretend to be original API declarations.
        [[nodiscard]] bool GetStateFlagForAnalysis() const noexcept;
        void SetStateFlagForAnalysis(bool value) noexcept;
        [[nodiscard]] const char* GetOwnedTextForAnalysis() const noexcept;
        void SetOwnedTextForAnalysis(std::string_view value);

    private:
        static spApp* instance_;
        bool stateFlag_ = false;
        std::string ownedText_;
    };
}
