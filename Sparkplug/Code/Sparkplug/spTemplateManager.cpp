#include "spTemplateManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTemplateManager()
        {
            return std::make_unique<spTemplateManager>();
        }

        const spRTTIRecord TemplateManagerRecord{
            spTemplateManager::ClassID,
            spBaseObject::ClassID,
            "spTemplateManager",
            &spBaseObject::StaticRTTI(),
            &CreateTemplateManager,
            nullptr,
        };

        const bool TemplateManagerRegistered =
            spRTTIManager::Instance().Register(TemplateManagerRecord);
    }

    spTemplateManager* spTemplateManager::instance_ = nullptr;

    spTemplateManager::spTemplateManager() noexcept
    {
        instance_ = this;
    }

    spTemplateManager::~spTemplateManager()
    {
        ClearForAnalysis();
        instance_ = nullptr;
    }

    const spRTTIRecord& spTemplateManager::StaticRTTI() noexcept
    {
        (void)TemplateManagerRegistered;
        return TemplateManagerRecord;
    }

    spTemplateManager* spTemplateManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spTemplateManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplateManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spTemplateManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spTemplateManager::vfunc_18() const noexcept
    {
        return TemplateManagerRecord;
    }

    bool spTemplateManager::AddForAnalysis(std::shared_ptr<spNamedObject> instance)
    {
        if (!instance)
        {
            return false;
        }
        templates_.push_back(std::move(instance));
        return true;
    }

    spNamedObject* spTemplateManager::FindForAnalysis(
        const std::string_view name) const noexcept
    {
        for (const auto& instance : templates_)
        {
            if (instance && instance->GetName() != nullptr
                && name == instance->GetName())
            {
                return instance.get();
            }
        }
        return nullptr;
    }

    void spTemplateManager::ClearForAnalysis() noexcept
    {
        templates_.clear();
    }

    std::size_t spTemplateManager::GetTemplateCountForAnalysis() const noexcept
    {
        return templates_.size();
    }
}
