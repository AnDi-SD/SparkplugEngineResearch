#include "spSerializerManager.h"
#include "spResourceFATSerializer.h"
#include "spResourceManager.h"
#include "spSerializerHook.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSerializerManager()
        {
            return std::make_unique<spSerializerManager>();
        }

        const spRTTIRecord SerializerManagerRecord{
            spSerializerManager::ClassID,
            spBaseObject::ClassID,
            "spSerializerManager",
            &spBaseObject::StaticRTTI(),
            &CreateSerializerManager,
            nullptr,
        };

        const bool SerializerManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SerializerManagerRecord);
    }

    spSerializerManager* spSerializerManager::instance_ = nullptr;

    spSerializerManager::spSerializerManager() noexcept
        : fat_(std::make_unique<spResourceFATHelperForAnalysis>())
    {
        // PC and PS2 both initialize +0x10/+0x14/+0x18 to 0/1/2 and publish
        // the complete object through a process-global singleton pointer.
        instance_ = this;
    }

    spSerializerManager::~spSerializerManager()
    {
        // Native teardown first deletes the owned FAT, releases every
        // serializer stored in the registration list, then clears the global.
        fat_.reset();
        ClearRegistrationsInNativeOrder();
        instance_ = nullptr;
    }

    const spRTTIRecord& spSerializerManager::StaticRTTI() noexcept
    {
        (void)SerializerManagerRegistered;
        return SerializerManagerRecord;
    }

    spSerializerManager* spSerializerManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spSerializerManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSerializerManager>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spSerializerManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Both native clone paths construct a fresh manager and route the copy
        // slot to the storage-free spBaseObject implementation. Runtime
        // dispatch state and registrations are intentionally not copied.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spSerializerManager::vfunc_18() const noexcept
    {
        return SerializerManagerRecord;
    }

    bool spSerializerManager::RegisterForAnalysis(
        const spClassID targetClassID,
        std::shared_ptr<spSerializer> serializer,
        const std::uint32_t platformMask,
        const std::uint32_t operationMask)
    {
        if (!serializer)
        {
            return false;
        }

        registrations_.push_back(Registration{
            targetClassID,
            platformMask,
            operationMask,
            std::move(serializer),
        });
        return true;
    }

    spSerializer* spSerializerManager::FindForAnalysis(
        const spClassID targetClassID) const noexcept
    {
        for (const auto& registration : registrations_)
        {
            if (registration.targetClassID == targetClassID
                && (registration.platformMask & platformMask_) != 0
                && (registration.operationMask & operationMask_) != 0)
            {
                return registration.serializer.get();
            }
        }
        return nullptr;
    }

    spSerializer* spSerializerManager::FindForAnalysis(
        const spBaseObject& object) const noexcept
    {
        return FindForAnalysis(object.vfunc_18().classID);
    }

    void spSerializerManager::SetDispatchContextForAnalysis(
        const std::uint32_t platformMask,
        const std::uint32_t operationMask) noexcept
    {
        platformMask_ = platformMask;
        operationMask_ = operationMask;
    }

    std::uint32_t spSerializerManager::GetPlatformMaskForAnalysis() const noexcept
    {
        return platformMask_;
    }

    std::uint32_t spSerializerManager::GetOperationMaskForAnalysis() const noexcept
    {
        return operationMask_;
    }

    std::uint32_t
    spSerializerManager::GetSerializationPolicyForAnalysis() const noexcept
    {
        return serializationPolicy_;
    }

    std::size_t spSerializerManager::GetRegistrationCountForAnalysis() const noexcept
    {
        return registrations_.size();
    }

    bool spSerializerManager::SetSerializationPolicyForAnalysis(std::uint32_t policy) noexcept
    {if(policy>2)return false;serializationPolicy_=policy;return true;}

    spResourceFATHelperForAnalysis*
    spSerializerManager::GetFATForAnalysis() noexcept
    {
        return fat_.get();
    }

    const spResourceFATHelperForAnalysis*
    spSerializerManager::GetFATForAnalysis() const noexcept
    {
        return fat_.get();
    }

    void spSerializerManager::ClearRegistrationsInNativeOrder() noexcept
    {
        // sub_00182EC0 repeatedly takes the first serializer pointer, destroys
        // that one instance, erases every aliasing node, and then starts over.
        // Holding one temporary shared owner lets us reproduce the same group
        // order without the native raw-pointer/double-delete hazard.
        while (!registrations_.empty())
        {
            auto serializer = registrations_.front().serializer;
            const auto* const pointer = serializer.get();
            registrations_.remove_if(
                [pointer](const Registration& registration)
                {
                    return registration.serializer.get() == pointer;
                });
            serializer.reset();
        }
    }

    spSerializerFileHeaderStatus
    spSerializerManager::ValidateFileHeaderForAnalysis(
        const spSerializerFileHeader& header,
        const std::uint32_t streamSize,
        const std::uint32_t nativePlatformMask) noexcept
    {
        if (header.signature != FileSignature)
        {
            return spSerializerFileHeaderStatus::WrongFileType;
        }
        if (header.version != FileVersion)
        {
            return spSerializerFileHeaderStatus::WrongVersion;
        }

        const auto acceptedPlatforms = PlatformCommon | nativePlatformMask;
        if ((header.platformMask & acceptedPlatforms) == 0)
        {
            return spSerializerFileHeaderStatus::UnsupportedPlatform;
        }

        // This strict comparison mirrors both executables. Neither native
        // validator checks declaredFileSize or dataSize in this function.
        if (header.dataOffset >= streamSize)
        {
            return spSerializerFileHeaderStatus::DataOffsetBeyondEnd;
        }
        return spSerializerFileHeaderStatus::Valid;
    }

    spSerializerFileHeaderStatus
    spSerializerManager::ReadAndValidateHeaderForAnalysis(
        spStream& source,
        const std::uint32_t nativePlatformMask,
        spSerializerFileHeader* const header)
    {
        spSerializerFileHeader localHeader{};
        if (!source.ReadData(&localHeader, sizeof(localHeader)))
        {
            return spSerializerFileHeaderStatus::HeaderReadFailed;
        }

        // The native validator does not ask the stream for its size until the
        // signature and version have passed. Preserve that observable order:
        // a malformed header must not be hidden by a failing GetSize call.
        if (localHeader.signature != FileSignature
            || localHeader.version != FileVersion)
        {
            if (header != nullptr)
            {
                *header = localHeader;
            }
            return localHeader.signature != FileSignature
                ? spSerializerFileHeaderStatus::WrongFileType
                : spSerializerFileHeaderStatus::WrongVersion;
        }

        std::uint32_t streamSize = 0;
        if (!source.GetSize(&streamSize))
        {
            return spSerializerFileHeaderStatus::StreamSizeUnavailable;
        }

        const auto status = ValidateFileHeaderForAnalysis(
            localHeader, streamSize, nativePlatformMask);
        if (header != nullptr)
        {
            *header = localHeader;
        }
        if (status == spSerializerFileHeaderStatus::Valid)
        {
            SetDispatchContextForAnalysis(
                localHeader.platformMask, OperationLoad);
        }
        return status;
    }

    spBaseObject* spSerializerManager::MaterializeResourcesForAnalysis(
        spStream& source, spSerializerReadContextForAnalysis& context, std::string* error)
    {
        if (error) error->clear();
        const auto fail = [&](const char* message) -> spBaseObject* {
            context.failed = true;
            if (error) *error = message;
            return nullptr;
        };
        if (&context.manager != this || context.failed || context.depth)
            return fail("Invalid or failed materialization context");
        spBaseObject* root = nullptr;
        bool first = true;
        for (auto* entry = fat_->FirstForAnalysis(); entry; entry = fat_->NextForAnalysis())
        {
            if (entry->object) continue;
            if (entry->fileID)
                return fail("External file-ID materialization is not reconstructed");
            entry->object = context.resources.FindForAnalysis(entry->classID, entry->GetNameForAnalysis());
            if (!entry->object)
            {
                std::uint32_t physicalSize = 0;
                const auto origin = source.GetLogicalOriginForAnalysis();
                if (!source.GetSize(&physicalSize) || origin > physicalSize ||
                    entry->offset > physicalSize - origin || entry->size < 8 ||
                    entry->size > physicalSize - origin - entry->offset ||
                    entry->offset > std::uint32_t(std::numeric_limits<std::int32_t>::max()) ||
                    !source.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(entry->offset)))
                    return fail("Invalid FAT object extent or seek failure");
                auto* serializer = FindForAnalysis(entry->classID);
                if (!serializer) return fail("No serializer for FAT resource");
                if (context.createdObjects.size() >= 4096) return fail("Resource object limit exceeded");
                auto object = serializer->ReadObjectHeaderAndCreateForAnalysis(source);
                if (!object) return fail("FAT resource header or factory failed");
                auto* objectPointer = object.get();
                context.createdObjects.push_back(std::move(object));
                entry->object = objectPointer;
                bool loaded = false;
                {
                    struct DepthGuard final
                    {
                        std::uint32_t& depth;
                        explicit DepthGuard(std::uint32_t& value) : depth(value) { ++depth; }
                        ~DepthGuard() { --depth; }
                    } guard(context.depth);
                    loaded = serializer->ReadPayloadForAnalysis(context, source, entry->size - 8, *entry->object, error);
                }
                if (!loaded)
                {
                    context.failed = true;
                    if (error && error->empty()) *error = "FAT resource payload failed";
                    return nullptr;
                }
                std::uint32_t end = 0;
                if (context.failed || !source.GetCurrentPosition(end) || end != entry->offset + entry->size)
                    return fail("FAT resource reader did not consume its bounded extent");
                // Unlike ReadReference, the original outer loop attempts
                // cache registration BEFORE applying the directory name.
                if (entry->object->IsKindOf(spNamedObject::ClassID))
                    if (auto* resource = dynamic_cast<spResource*>(entry->object))
                        (void)context.resources.RegisterForAnalysis(*resource);
            }
            if (first) { root = entry->object; first = false; }
            if (entry->object && entry->object->IsKindOf(spNamedObject::ClassID))
                if (auto* named = dynamic_cast<spNamedObject*>(entry->object)) named->SetName(entry->GetNameForAnalysis());
        }
        return root;
    }

    spBaseObject* spSerializerManager::LoadResourcesForAnalysis(
        spStream& source, spSerializerReadContextForAnalysis& context, std::string* error)
    {
        if (error) error->clear();
        const auto fail = [&](const char* message) -> spBaseObject* {
            context.failed = true;
            if (error) *error = message;
            return nullptr;
        };
        if (&context.manager != this || context.failed || context.depth)
            return fail("Invalid or failed file-load context");
        fat_->ClearResourceEntriesForAnalysis(); fat_->ClearFileEntriesForAnalysis();
        struct ClearFAT final
        {
            spResourceFATHelperForAnalysis& fat;
            ~ClearFAT() { fat.ClearResourceEntriesForAnalysis(); fat.ClearFileEntriesForAnalysis(); }
        } cleanup{*fat_};
        std::uint32_t position = 0;
        if (!source.GetCurrentPosition(position) || position != 0)
            return fail("File loader requires the start of an explicitly opened logical stream");
        spSerializerFileHeader header;
        if (ReadAndValidateHeaderForAnalysis(source, PlatformPC, &header) != spSerializerFileHeaderStatus::Valid)
            return fail("Invalid PC FFPS header");
        if (!fat_->LoadIndexForAnalysis(source)) return fail("Cannot read resource FAT index");
        if (!fat_->ReadDiscardedFileIndexForAnalysis(source)) return fail("Cannot read compatibility file index");
        if (!source.GetCurrentPosition(position) || position != header.dataOffset)
            return fail("FAT/file indices do not end at FFPS data offset");
        const auto origin = source.GetLogicalOriginForAnalysis();
        std::uint32_t physicalSize = 0;
        if (!source.GetSize(&physicalSize) || origin > physicalSize || position > physicalSize - origin)
            return fail("Invalid physical FFPS data origin");
        source.SetLogicalOriginForAnalysis(origin + position);
        spDXSerializerHook hook;
        if (!hook.PrepareForAnalysis(header.platformMask, &context.resources, fat_.get(), source))
            return fail("PC mesh preparation failed");
        if (!hook.MaterializePreparedForAnalysis(source,context,error))
            return nullptr; // hook already poisoned context and recorded the cause
        auto* root = MaterializeResourcesForAnalysis(source, context, error);
        if (!root && !context.failed) return fail("No newly materialized root resource");
        return root;
    }
}
