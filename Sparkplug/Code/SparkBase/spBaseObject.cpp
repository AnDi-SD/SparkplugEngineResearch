// Exact original source path recovered from the PC executable:
//   Z:\Sparkplug\Code\SparkBase\spBaseObject.cpp
//
// The class names and the behavior implemented here are executable-backed.
// The header path, C++ namespace, method names and portable ownership types are
// reconstruction choices and are marked as such in spBaseObject.h.

#include "spBaseObject.h"
#include "spApp.h"
#include "spAsyncFileStreamManager.h"
#include "spFileStream.h"
#include "spErrorManager.h"
#include "spMemoryStream.h"
#include "spPCKManager.h"
#include "spStream.h"
#include "spSubscriptionManager.h"
#if defined(_WIN32)
#include "../SparkBasePC/spPCAsyncFileStreamManager.h"
#include "../SparkBasePC/spPCFileStream.h"
#include "../SparkplugPC/spPCApp.h"
#include "../SparkplugPC/spPCErrorManager.h"
#endif
#include "../SparkplugPS2/spPS2ErrorManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        bool RegisterBaseProperties()
        {
            // Both native builds invoke a post-registration callback for
            // spBaseObject.  Its complete body only returns true.
            return true;
        }

        std::unique_ptr<spBaseObject> CreateNamedObject()
        {
            return std::make_unique<spNamedObject>();
        }

        const spRTTIRecord BaseRecord{
            spBaseObject::ClassID,
            0,
            "spBaseObject",
            nullptr,
            nullptr,
            &RegisterBaseProperties,
        };

        const spRTTIRecord NamedRecord{
            spNamedObject::ClassID,
            spBaseObject::ClassID,
            "spNamedObject",
            &BaseRecord,
            &CreateNamedObject,
            nullptr,
        };

        const spRTTIRecord CrossPlatformRecord{
            spCrossPlatform::ClassID,
            spNamedObject::ClassID,
            "spCrossPlatform",
            &NamedRecord,
            nullptr,
            nullptr,
        };

        bool RegisterSparkBaseTypes(spRTTIManager& manager)
        {
            return manager.Register(BaseRecord)
                && manager.Register(NamedRecord)
                && manager.Register(CrossPlatformRecord)
                && manager.Register(spApp::StaticRTTI())
                && manager.Register(spAsyncFileStreamManager::StaticRTTI())
                && manager.Register(spStream::StaticRTTI())
                && manager.Register(spFileStream::StaticRTTI())
                && manager.Register(spMemoryStream::StaticRTTI())
                && manager.Register(spError::StaticRTTI())
                && manager.Register(spErrorManager::StaticRTTI())
                && manager.Register(spPCKManager::StaticRTTI())
                && manager.Register(spSubscriptionManager::StaticRTTI())
                && manager.Register(spPS2ErrorManager::StaticRTTI())
#if defined(_WIN32)
                && manager.Register(spPCAsyncFileStreamManager::StaticRTTI())
                && manager.Register(spPCFileStream::StaticRTTI())
                && manager.Register(spPCApp::StaticRTTI())
                && manager.Register(spPCErrorManager::StaticRTTI())
#endif
                ;
        }
    }

    bool spRTTIRecord::IsExactly(const spClassID candidate) const noexcept
    {
        return classID == candidate;
    }

    bool spRTTIRecord::IsKindOf(const spClassID candidate) const noexcept
    {
        for (auto current = this; current != nullptr; current = current->base)
        {
            if (current->classID == candidate)
            {
                return true;
            }
        }

        return false;
    }

    spRTTIManager& spRTTIManager::Instance()
    {
        static spRTTIManager instance;
        static const bool registered = RegisterSparkBaseTypes(instance);
        (void)registered;
        return instance;
    }

    bool spRTTIManager::Register(const spRTTIRecord& record)
    {
        if (record.classID == 0 || record.className == nullptr)
        {
            return false;
        }

        if ((record.base == nullptr) != (record.baseClassID == 0))
        {
            return false;
        }

        if (record.base != nullptr && record.base->classID != record.baseClassID)
        {
            return false;
        }

        const auto [iterator, inserted] = records_.emplace(record.classID, &record);
        if (!inserted)
        {
            return iterator->second == &record;
        }

        if (record.propertyRegistrar != nullptr && !record.propertyRegistrar())
        {
            records_.erase(iterator);
            return false;
        }

        return true;
    }

    const spRTTIRecord* spRTTIManager::Find(const spClassID classID) const noexcept
    {
        const auto iterator = records_.find(classID);
        return iterator == records_.end() ? nullptr : iterator->second;
    }

    std::unique_ptr<spBaseObject> spRTTIManager::Create(const spClassID classID) const
    {
        const auto* record = Find(classID);
        if (record == nullptr || record->factory == nullptr)
        {
            return nullptr;
        }

        return record->factory();
    }

    std::size_t spRTTIManager::GetRegistrationCount() const noexcept
    {
        return records_.size();
    }

    std::unique_ptr<spBaseObject> spCloneManager::Clone(const spBaseObject& source)
    {
        const bool isRoot = depth_ == 0;
        ++depth_;
        auto result = source.vfunc_10(*this);
        --depth_;

        if (isRoot)
        {
            clones_.clear();
        }

        return result;
    }

    spBaseObject* spCloneManager::FindClone(const spBaseObject& source) const noexcept
    {
        const auto iterator = clones_.find(&source);
        return iterator == clones_.end() ? nullptr : iterator->second;
    }

    void spCloneManager::RegisterClone(const spBaseObject& source, spBaseObject& clone)
    {
        clones_[&source] = &clone;
    }

    spBaseObject::~spBaseObject() = default;

    const spRTTIRecord& spBaseObject::StaticRTTI() noexcept
    {
        return BaseRecord;
    }

    void spBaseObject::vfunc_0C(const void*) noexcept
    {
        // PS2 sub_00100810 and PC 0x005B7A00 are immediate returns.  Both
        // calling conventions nevertheless expose one context argument.
    }

    std::unique_ptr<spBaseObject> spBaseObject::vfunc_10(spCloneManager&) const
    {
        // sub_00100370 returns null: the root object is not directly creatable.
        return nullptr;
    }

    bool spBaseObject::vfunc_14(spBaseObject&, spCloneManager&) const
    {
        // sub_00100320 performs no base-field copy and returns true.  The call
        // through spCloneManager present in the binary is also a no-op here.
        return true;
    }

    const spRTTIRecord& spBaseObject::vfunc_18() const noexcept
    {
        return BaseRecord;
    }

    bool spBaseObject::vfunc_1C(const spClassID candidate) const noexcept
    {
        return vfunc_18().IsExactly(candidate);
    }

    bool spBaseObject::vfunc_20(const spClassID candidate) const noexcept
    {
        return vfunc_18().IsKindOf(candidate);
    }

    std::unique_ptr<spBaseObject> spBaseObject::Clone() const
    {
        spCloneManager manager;
        return manager.Clone(*this);
    }

    bool spBaseObject::IsExactly(const spClassID candidate) const noexcept
    {
        return vfunc_1C(candidate);
    }

    bool spBaseObject::IsKindOf(const spClassID candidate) const noexcept
    {
        return vfunc_20(candidate);
    }

    spNamedObject::spNamedObject(const char* name)
    {
        SetName(name);
    }

    spNamedObject::~spNamedObject() = default;

    const spRTTIRecord& spNamedObject::StaticRTTI() noexcept
    {
        return NamedRecord;
    }

    const char* spNamedObject::GetName() const noexcept
    {
        return name_ == nullptr ? nullptr : name_->c_str();
    }

    void spNamedObject::SetName(const char* name)
    {
        if (name == nullptr)
        {
            name_.reset();
            return;
        }

        SetName(std::string_view{name});
    }

    void spNamedObject::SetName(const std::string_view name)
    {
        name_ = std::make_shared<const std::string>(name);
    }

    std::unique_ptr<spBaseObject> spNamedObject::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spNamedObject>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }

        return clone;
    }

    bool spNamedObject::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID)
            || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }

        auto& namedDestination = static_cast<spNamedObject&>(destination);
        namedDestination.name_ = name_;
        return true;
    }

    const spRTTIRecord& spNamedObject::vfunc_18() const noexcept
    {
        return NamedRecord;
    }

    spCrossPlatform::~spCrossPlatform() = default;

    const spRTTIRecord& spCrossPlatform::StaticRTTI() noexcept
    {
        return CrossPlatformRecord;
    }

    std::unique_ptr<spBaseObject> spCrossPlatform::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spCrossPlatform::vfunc_18() const noexcept
    {
        return CrossPlatformRecord;
    }
}
