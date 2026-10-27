#include "spGUIManager.h"
#include "Code/SparkBase/spNotification.h"
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        spGUIManager* instance=nullptr;
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spGUIManager>();}
        const spRTTIRecord record{spGUIManager::ClassID,spBaseObject::ClassID,
            "spGUIManager",&spBaseObject::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        struct ActiveCall final
        {
            std::size_t& depth;
            explicit ActiveCall(std::size_t& value):depth(value){++depth;}
            ~ActiveCall(){--depth;}
        };
    }
    spGUIManager::spGUIManager() noexcept {instance=this;}
    spGUIManager::~spGUIManager()
    {
        // Native empty-owned14/18/1C destruction clears the singleton even
        // when a different, newer manager is still alive. Focus is borrowed;
        // destruction neither notifies nor releases that receiver.
        instance=nullptr;
    }
    const spRTTIRecord& spGUIManager::StaticRTTI() noexcept {(void)registered;return record;}
    const spRTTIRecord& spGUIManager::vfunc_18() const noexcept {return record;}
    spGUIManager* spGUIManager::GetInstanceForAnalysis() noexcept {return instance;}
    std::unique_ptr<spBaseObject> spGUIManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spGUIManager>();manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spGUIFocusResultForAnalysis spGUIManager::SetFocusForAnalysis(spBaseObject* desired)
    {
        using Result=spGUIFocusResultForAnalysis;
        if(activeFocusCalls_==0)recursionRefused_=false;
        if(activeFocusCalls_>=MaximumFocusDepthForAnalysis)
        {recursionRefused_=true;return Result::ReentryLimit;}
        ActiveCall active(activeFocusCalls_);
        auto* previous=focus_;
        spNotificationForAnalysis notification;
        notification.code=0x17;notification.sender=this;
        if(previous)previous->vfunc_0C(&notification);
        if(recursionRefused_)return Result::ReentryLimit;
        // Old receiver callbacks may change the stored focus. Native then
        // overwrites it with the caller's desired pointer without reloading it.
        focus_=desired;
        notification={};notification.code=0x16;notification.sender=this;
        if(desired)desired->vfunc_0C(&notification);
        // New receiver callbacks can change focus again; retain that change.
        return recursionRefused_?Result::ReentryLimit:Result::Completed;
    }
}
