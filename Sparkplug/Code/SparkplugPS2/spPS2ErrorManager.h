#pragma once

// Inferred header/module path.  The class name is literal PS2 RTTI evidence;
// no original source path was retained in the executable.

#include "../SparkBase/spErrorManager.h"

#include <memory>

namespace sparkplug::reconstruction
{
    class spPS2ErrorManager final : public spErrorManager
    {
    public:
        static constexpr spClassID ClassID = 0x226A416D;

        spPS2ErrorManager();
        ~spPS2ErrorManager() override = default;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] AnalysisHandlerBinding
            ResolveHandlerForAnalysis() const noexcept override;

    private:
        static void NativeNoOpHandler(
            const AnalysisDispatch& dispatch,
            void* context) noexcept;
    };
}
