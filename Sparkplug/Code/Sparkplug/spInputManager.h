#pragma once

// Inferred common header/module path.  Native class identity and the
// platform interface boundary are executable-backed; original API names are
// not present.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <memory>

namespace sparkplug::reconstruction
{
    class spInputManager : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x55A1304D;

        spInputManager() noexcept;
        ~spInputManager() override;

        spInputManager(const spInputManager&) = delete;
        spInputManager& operator=(const spInputManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spInputManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetConnectedControllerCountForAnalysis()
            const noexcept;

        virtual bool InitializeForAnalysis() = 0;
        virtual void ShutdownForAnalysis() noexcept = 0;
        [[nodiscard]] virtual std::size_t
            GetMaximumControllerCountForAnalysis() const noexcept = 0;
        [[nodiscard]] virtual bool IsControllerConnectedForAnalysis(
            std::size_t index) const noexcept = 0;

    protected:
        void SetInitializedForAnalysis(bool initialized) noexcept;

    private:
        static spInputManager* instance_;
        bool initialized_ = false;
    };
}
