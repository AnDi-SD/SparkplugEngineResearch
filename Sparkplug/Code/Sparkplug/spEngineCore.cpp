#include "spEngineCore.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateEngineCore()
        {
            return std::make_unique<spEngineCore>();
        }

        const spRTTIRecord EngineCoreRecord{
            spEngineCore::ClassID,
            spBaseObject::ClassID,
            "spEngineCore",
            &spBaseObject::StaticRTTI(),
            &CreateEngineCore,
            nullptr,
        };

        // This target is a layer above SparkBase, so it installs its own
        // registration when the translation unit is linked.
        const bool EngineCoreRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(EngineCoreRecord);
    }

    spEngineCore* spEngineCore::instance_ = nullptr;

    spEngineCore::spEngineCore() noexcept
    {
        // PS2 0x00133380 publishes the complete object at 0x0049f850 before
        // zeroing its state.  PC destruction clears 0x00755274, independently
        // identifying the equivalent singleton even though the protected PC
        // constructor body is hidden behind an .rld trampoline.
        instance_ = this;
    }

    spEngineCore::~spEngineCore()
    {
        ShutdownForAnalysis();
        // Both native destructors clear the singleton unconditionally.
        instance_ = nullptr;
    }

    const spRTTIRecord& spEngineCore::StaticRTTI() noexcept
    {
        (void)EngineCoreRegistered;
        return EngineCoreRecord;
    }

    spEngineCore* spEngineCore::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spEngineCore::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spEngineCore>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    bool spEngineCore::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Native clone functions route this slot to spBaseObject.  Runtime
        // managers, scene/camera pointers, callbacks and initialized state are
        // deliberately not copied.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spEngineCore::vfunc_18() const noexcept
    {
        return EngineCoreRecord;
    }

    bool spEngineCore::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    bool spEngineCore::InitializeForAnalysis(const AnalysisStages& stages)
    {
        for (const auto& stage : stages)
        {
            if (stage.callback == nullptr || !stage.callback(stage.context))
            {
                return false;
            }
        }
        initialized_ = true;
        return true;
    }

    void spEngineCore::ShutdownForAnalysis() noexcept
    {
        // The native shutdown releases the default scene/camera and the
        // manager graph before clearing this flag.  Those concrete manager
        // types are not yet reconstructed; this slice preserves the proven
        // terminal state without inventing placeholder ownership.
        initialized_ = false;
    }

    void spEngineCore::SetFrameCallbacksForAnalysis(
        const AnalysisCallback first,
        void* const firstContext,
        const AnalysisCallback second,
        void* const secondContext) noexcept
    {
        firstFrameCallback_ = {first, firstContext};
        secondFrameCallback_ = {second, secondContext};
    }

    bool spEngineCore::InvokeFirstFrameCallbackForAnalysis() const
    {
        return firstFrameCallback_.callback == nullptr
            || firstFrameCallback_.callback(firstFrameCallback_.context);
    }

    bool spEngineCore::InvokeSecondFrameCallbackForAnalysis() const
    {
        return secondFrameCallback_.callback == nullptr
            || secondFrameCallback_.callback(secondFrameCallback_.context);
    }
}
