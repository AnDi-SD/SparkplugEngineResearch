#pragma once

// Inferred portable declaration. PC452380/table6E6D08/getter451C00 establish
// the native50-byte BaseObject+singleton-support receiver. This class models
// its focus/singleton and empty-owned-view lifecycle profile, not native ABI.
#include "Code/SparkBase/spBaseObject.h"
#include <cstddef>

namespace sparkplug::reconstruction
{
    enum class spGUIFocusResultForAnalysis
    {
        Completed,
        ReentryLimit,
    };
    class spGUIManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x73634C2D;
        static constexpr std::size_t MaximumFocusDepthForAnalysis=128;
        spGUIManager() noexcept;
        ~spGUIManager() override;
        spGUIManager(const spGUIManager&)=delete;
        spGUIManager& operator=(const spGUIManager&)=delete;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native Notify and Copy are the inherited literal Base operations.
        static spGUIManager* GetInstanceForAnalysis() noexcept;
        spBaseObject* GetFocusForAnalysis() const noexcept {return focus_;}
        // PC451AD0: old17, publish desired4C, new16. One fully reset borrowed
        // frame is reused; no retain/release or filtering by receiver type.
        // Return status is analytical, not the native method's register value.
        [[nodiscard]] spGUIFocusResultForAnalysis SetFocusForAnalysis(spBaseObject*);
        // Raw resolved state input only. No notification/ownership/producer.
        void SetFocusStateForFixture(spBaseObject* value) noexcept {focus_=value;}
    private:
        spBaseObject* focus_=nullptr;
        std::size_t activeFocusCalls_=0;
        bool recursionRefused_=false;
    };
}
