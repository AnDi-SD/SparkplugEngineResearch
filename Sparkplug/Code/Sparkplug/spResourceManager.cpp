#include "spResourceManager.h"

#include "spMesh.h"
#include "spTexture.h"

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateResourceManager()
        {
            return std::make_unique<spResourceManager>();
        }

        const spRTTIRecord ResourceManagerRecord{
            spResourceManager::ClassID,
            spBaseObject::ClassID,
            "spResourceManager",
            &spBaseObject::StaticRTTI(),
            &CreateResourceManager,
            nullptr,
        };

        const bool ResourceManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(ResourceManagerRecord);
    }

    spResourceManager* spResourceManager::instance_ = nullptr;

    spResourceManager::spResourceManager() noexcept
    {
        // PS2 factory writes false/0/-1 to +0x15/+0x18/+0x1C, constructs the
        // platform vector at +0x20 and then publishes the singleton.
        instance_ = this;
    }

    spResourceManager::~spResourceManager()
    {
        // Cache entries are non-owning. Native teardown releases only vector
        // storage and unconditionally clears the singleton-support global.
        resources_.clear();
        instance_ = nullptr;
    }

    const spRTTIRecord& spResourceManager::StaticRTTI() noexcept
    {
        (void)ResourceManagerRegistered;
        return ResourceManagerRecord;
    }

    spResourceManager* spResourceManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spResourceManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spResourceManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spResourceManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // PC 0x00458D60 / PS2 0x0017DC40 construct a new manager and invoke
        // only the storage-free spBaseObject copy slot. Cache/config state is
        // intentionally absent from the clone.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spResourceManager::vfunc_18() const noexcept
    {
        return ResourceManagerRecord;
    }

    spResourceManager::ResourceKindForAnalysis
    spResourceManager::ClassifyForAnalysis(const spClassID classID) noexcept
    {
        // Force the two boundary records to exist even in a statically linked
        // host that dead-strips unrelated construction paths.
        (void)spTexture::StaticRTTI();
        (void)spMesh::StaticRTTI();
        const auto* const record = spRTTIManager::Instance().Find(classID);
        if (record != nullptr && record->IsKindOf(spTexture::ClassID))
        {
            return ResourceKindForAnalysis::Texture;
        }
        if (record != nullptr && record->IsKindOf(spMesh::ClassID))
        {
            return ResourceKindForAnalysis::Mesh;
        }
        return ResourceKindForAnalysis::Unsupported;
    }

    bool spResourceManager::RegisterForAnalysis(spResource& resource)
    {
        if (resource.GetName() == nullptr)
        {
            return false;
        }

        const auto kind = ClassifyForAnalysis(resource.vfunc_18().classID);
        if (kind != ResourceKindForAnalysis::Texture
            && kind != ResourceKindForAnalysis::Mesh)
        {
            return false;
        }

        resources_.push_back(Entry{kind, &resource});
        return true;
    }

    bool spResourceManager::UnregisterForAnalysis(spResource& resource) noexcept
    {
        for (auto iterator = resources_.begin(); iterator != resources_.end();
             ++iterator)
        {
            if (iterator->resource == &resource)
            {
                resources_.erase(iterator);
                return true;
            }
        }
        return false;
    }

    spResource* spResourceManager::FindForAnalysis(
        const spClassID requestedClassID,
        const char* const name) const noexcept
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        const auto requestedKind = ClassifyForAnalysis(requestedClassID);
        if (requestedKind != ResourceKindForAnalysis::Texture
            && requestedKind != ResourceKindForAnalysis::Mesh)
        {
            return nullptr;
        }

        for (const auto& entry : resources_)
        {
            const char* const candidateName = entry.resource == nullptr
                ? nullptr
                : entry.resource->GetName();
            if (entry.kind == requestedKind && candidateName != nullptr
                && std::strcmp(candidateName, name) == 0)
            {
                return entry.resource;
            }
        }
        return nullptr;
    }

    bool spResourceManager::ConfigureReserveForAnalysis(
        const bool reserveEnabled,
        const std::uint32_t resourceCount) noexcept
    {
        reserveEnabled_ = reserveEnabled;
        reserveCount_ = resourceCount;
        if (!reserveEnabled)
        {
            return true;
        }

        try
        {
            resources_.reserve(resourceCount);
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        catch (const std::length_error&)
        {
            return false;
        }
    }

    std::size_t spResourceManager::GetResourceCountForAnalysis() const noexcept
    {
        return resources_.size();
    }

    bool spResourceManager::IsReserveEnabledForAnalysis() const noexcept
    {
        return reserveEnabled_;
    }

    std::uint32_t spResourceManager::GetReserveCountForAnalysis() const noexcept
    {
        return reserveCount_;
    }

    std::int32_t spResourceManager::GetField1CForAnalysis() const noexcept
    {
        return field1C_;
    }
}
