#include "spSerializer.h"
#include "spSerializerManager.h"
#include "spResourceFATSerializer.h"
#include "spResourceManager.h"
#include "spDataBlockSerializer.h"

#include "../SparkBase/spStream.h"
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord SerializerRecord{
            spSerializer::ClassID,
            spBaseObject::ClassID,
            "spSerializer",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool SerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SerializerRecord);
    }

    spSerializer::spSerializer() noexcept = default;

    spSerializer::~spSerializer() = default;

    const spRTTIRecord& spSerializer::StaticRTTI() noexcept
    {
        (void)SerializerRegistered;
        return SerializerRecord;
    }

    std::unique_ptr<spBaseObject> spSerializer::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    bool spSerializer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spSerializer::vfunc_18() const noexcept
    {
        return SerializerRecord;
    }

    spClassID spSerializer::ResolveClassIDForAnalysis(
        const spClassID serializedClassID) const noexcept
    {
        return serializedClassID;
    }

    std::unique_ptr<spBaseObject>
    spSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream& source,
        spSerializerObjectHeaderForAnalysis* const observedHeader) const
    {
        spSerializerObjectHeaderForAnalysis header{};
        if (!ReadObjectHeaderForAnalysis(source,header))
        {
            if (observedHeader != nullptr)
            {
                *observedHeader = header;
            }
            return nullptr;
        }

        if (observedHeader != nullptr)
        {
            *observedHeader = header;
        }

        // Deliberately no marker comparison: neither shipped body reads the
        // second word after ReadData. Strict tools can call the helper below.
        const auto resolvedClassID = ResolveClassIDForAnalysis(header.classID);
        return spRTTIManager::Instance().Create(resolvedClassID);
    }

    bool spSerializer::HasCanonicalObjectMarkerForAnalysis(
        const spSerializerObjectHeaderForAnalysis& header) noexcept
    {
        return header.marker == 0x4F4F4253;
    }

    bool spSerializer::ReadObjectHeaderForAnalysis(spStream& source,
        spSerializerObjectHeaderForAnalysis& header)
    {return source.ReadData(&header,sizeof(header));}

    bool spSerializer::WriteObjectHeaderForAnalysis(spStream& destination, const spBaseObject& object)
    {
        const spSerializerObjectHeaderForAnalysis header{object.vfunc_18().classID, 0x4F4F4253};
        return destination.WriteData(&header, sizeof(header));
    }

    bool spSerializer::IndexRelationshipsForAnalysis(spBaseObject&) const
    {
        return false; // explicitly unsupported host slice, not native success
    }

    bool spSerializer::WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string* error) const
    {
        if (error) *error = "Payload writer is not reconstructed for this serializer";
        return false;
    }

    bool spSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&, spStream&,
        std::uint32_t, spBaseObject&, std::string* error) const
    {
        if (error) *error = "Payload reader is not reconstructed for this serializer";
        return false;
    }

    bool spSerializer::IndexRelationshipsWithContextForAnalysis(
        spSerializerManager&, spBaseObject& object) const
    {
        return IndexRelationshipsForAnalysis(object);
    }

    bool spSerializer::WritePayloadWithContextForAnalysis(spSerializerManager&,
        spStream& destination, const spBaseObject& object, std::string* error) const
    {
        return WritePayloadForAnalysis(destination, object, error);
    }

    bool spSerializer::IndexResourceForAnalysis(spSerializerManager& manager, spBaseObject& object) const
    {
        auto* fat = manager.GetFATForAnalysis();
        if (!fat) return false;
        if (fat->FindByObjectForAnalysis(object)) return true;
        if (!fat->IndexObjectForAnalysis(object.vfunc_18().classID, object)) return false;
        return IndexRelationshipsWithContextForAnalysis(manager, object);
    }

    bool spSerializer::IndexReferenceForAnalysis(spSerializerManager& manager, spBaseObject* object)
    {
        if (!object) return true;
        auto* serializer = manager.FindForAnalysis(*object);
        return serializer && serializer->IndexResourceForAnalysis(manager, *object);
    }

    bool spSerializer::WriteReferenceForAnalysis(
        spSerializerManager& manager, spStream& destination,
        const spBaseObject* object, std::string* error)
    {
        if (error) error->clear();
        const auto fail = [error](const char* message) {
            if (error) *error = message;
            return false;
        };
        const std::uint32_t zero = 0;
        if (!object)
            return destination.Write(zero) || fail("Cannot write null resource reference");
        auto* fat = manager.GetFATForAnalysis();
        auto* entry = fat ? fat->FindByObjectForAnalysis(*object) : nullptr;
        if (!entry) return fail("Referenced resource was not indexed");
        auto* serializer = manager.FindForAnalysis(*object);
        if (!entry->payloadWritten && !serializer)
            return fail("No serializer for referenced resource");
        if (!destination.Write(entry->id)) return fail("Cannot write resource ID");
        std::uint32_t sizePosition = 0;
        if (!destination.GetCurrentPosition(sizePosition) ||
            sizePosition > std::uint32_t(std::numeric_limits<std::int32_t>::max()) ||
            !destination.Write(zero))
            return fail("Cannot reserve resource size");
        if (entry->payloadWritten) return true;
        entry->payloadWritten = true; // before header/payload; native one-shot guard
        if (!destination.GetCurrentPosition(entry->offset) ||
            !WriteObjectHeaderForAnalysis(destination, *object))
            return fail("Cannot write resource object header; discard failed save context");
        if (!serializer->WritePayloadWithContextForAnalysis(manager, destination, *object, error)) return false;
        std::uint32_t end = 0;
        if (!destination.GetCurrentPosition(end) || end < entry->offset ||
            end > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
            return fail("Invalid written resource extent");
        entry->size = end - entry->offset;
        if (!destination.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(sizePosition)) ||
            !destination.Write(entry->size) ||
            !destination.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(end)))
            return fail("Resource size patch failed; discard failed save context");
        return true;
    }

    spSerializerReadContextForAnalysis::spSerializerReadContextForAnalysis(
        spSerializerManager& manager, spResourceManager& resources, spAnimationManager* bindings) noexcept
        : manager(manager), resources(resources), animationBindings(bindings) {}

    spSerializerReadContextForAnalysis::~spSerializerReadContextForAnalysis()
    {
        // Remove only pointers owned here. Native FAT does not own objects;
        // leaving aliases dangling after safe host teardown is not required.
        auto* fat = manager.GetFATForAnalysis();
        if (fat)
            for (auto* entry = fat->FirstForAnalysis(); entry; entry = fat->NextForAnalysis())
            {
                for (const auto& owned : createdObjects)
                    if (entry->object == owned.get()) { entry->object = nullptr; break; }
                for (const auto& identity : directObjectIdentitiesForAnalysis_)
                    if (entry->object == identity.object) { entry->object = nullptr; break; }
            }
    }

    std::size_t spSerializerReadContextForAnalysis::GetCreatedObjectCountForAnalysis() const noexcept
    {
        return createdObjects.size() + directObjectIdentitiesForAnalysis_.size();
    }

    spBaseObject* spSerializerReadContextForAnalysis::PublishObjectForAnalysis(
        std::unique_ptr<spBaseObject> object)
    {
        if (!object || failed) return nullptr;
        auto* pointer = object.get();
        for (const auto identity : directOwnedClassIDsForAnalysis)
            if (object->IsKindOf(identity))
            {
                pendingDirectObjectsForAnalysis.push_back(std::move(object));
                directObjectIdentitiesForAnalysis_.push_back({pointer, nullptr});
                return pointer;
            }
        createdObjects.push_back(std::move(object));
        return pointer;
    }

    std::unique_ptr<spBaseObject> spSerializerReadContextForAnalysis::TakeDirectOwnerForAnalysis(
        spBaseObject* object, const spBaseObject* owner, std::string* error)
    {
        const auto fail = [&](const char* message) -> std::unique_ptr<spBaseObject> {
            failed = true; if (error) *error = message; return nullptr;
        };
        if (failed || !object || !owner) return fail("Invalid direct ownership transfer");
        // A malformed file must not create a unique_ptr cycle. Ownership is a
        // host lifetime concern; native borrowed FAT publication stays intact.
        for (const auto* ancestor = owner; ancestor;)
        {
            if (ancestor == object) return fail("Cyclic direct resource ownership");
            const spBaseObject* next = nullptr;
            for (const auto& identity : directObjectIdentitiesForAnalysis_)
                if (identity.object == ancestor) { next = identity.owner; break; }
            ancestor = next;
        }
        for (std::size_t i = 0; i < directObjectIdentitiesForAnalysis_.size(); ++i)
            if (directObjectIdentitiesForAnalysis_[i].object == object)
            {
                if (!pendingDirectObjectsForAnalysis[i])
                    return fail("Resource already has a direct owner");
                directObjectIdentitiesForAnalysis_[i].owner = owner;
                return std::move(pendingDirectObjectsForAnalysis[i]);
            }
        return fail("Resource lacks an explicit transferable direct owner");
    }

    std::shared_ptr<spBaseObject> spSerializerReadContextForAnalysis::ShareObjectForAnalysis(
        spBaseObject* object) const noexcept
    {
        if (!object) return nullptr;
        for (const auto& owned : createdObjects) if (owned.get() == object) return owned;
        for (const auto& owned : externalOwners) if (owned.get() == object) return owned;
        return nullptr;
    }

    spBaseObject* spSerializer::ReadFieldReferenceForAnalysis(
        spSerializerReadContextForAnalysis& context, spClassID expectedClassID,
        spStream& source, const spDataBlockHeaderForAnalysis& field, std::string* error)
    {
        const auto fail = [&](const char* message) -> spBaseObject* {
            context.failed = true; if (error) *error = message; return nullptr;
        };
        std::uint32_t position = 0;
        ReferencePrefixForAnalysis prefix;
        if (context.failed || !source.GetCurrentPosition(position)
            || position != field.dataStreamPosition || field.payloadSize < 4
            || !ReadReferencePrefixForAnalysis(source,source,prefix,error,field.payloadSize)) return fail("Cannot inspect bounded reference field");
        if (prefix.id)
        {
            if (field.payloadSize < 8 || prefix.inlineSize != field.payloadSize - 8)
                return fail("Inline reference extent differs from enclosing field");
        }
        else if (field.payloadSize != 4) return fail("Null reference field has trailing bytes");
        if (position > std::uint32_t(std::numeric_limits<std::int32_t>::max())
            || !source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(position)))
            return fail("Cannot restore inspected reference field");
        return ReadReferenceForAnalysis(context,expectedClassID,source,source,error);
    }

    spBaseObject* spSerializer::ReadSequenceReferenceForAnalysis(
        spSerializerReadContextForAnalysis& context,spClassID expectedClassID,
        spStream& source,std::uint32_t sequenceEnd,std::string* error)
    {
        const auto fail=[&](const char* message)->spBaseObject*{context.failed=true;if(error)*error=message;return nullptr;};
        std::uint32_t position=0,extent=4;
        ReferencePrefixForAnalysis prefix;
        if(context.failed||!source.GetCurrentPosition(position)||position>sequenceEnd||sequenceEnd-position<4
            ||!ReadReferencePrefixForAnalysis(source,source,prefix,error,sequenceEnd-position))
            return fail("Cannot inspect reference sequence entry");
        if(prefix.id)
        {
            if(sequenceEnd-position<8||prefix.inlineSize>sequenceEnd-position-8)
                return fail("Reference sequence entry exceeds field");
            extent=prefix.inlineSize+8;
        }
        if(position>std::uint32_t(std::numeric_limits<std::int32_t>::max())
            ||!source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(position)))return fail("Cannot restore reference sequence cursor");
        spDataBlockHeaderForAnalysis entry;entry.dataStreamPosition=position;entry.payloadSize=extent;
        return ReadFieldReferenceForAnalysis(context,expectedClassID,source,entry,error);
    }

    bool spSerializer::ReadReferencePrefixForAnalysis(spStream& idSource,spStream& payloadSource,
        ReferencePrefixForAnalysis& prefix,std::string* error,std::uint32_t availableBytes)
    {
        prefix={};
        if(availableBytes<4||!idSource.Read(prefix.id)){if(error)*error="Cannot read reference ID";return false;}
        if(prefix.id&&(availableBytes<8||!payloadSource.Read(prefix.inlineSize))){if(error)*error="Cannot read reference inline size";return false;}
        return true;
    }

    spBaseObject* spSerializer::ReadReferenceForAnalysis(
        spSerializerReadContextForAnalysis& context, spClassID expectedClassID,
        spStream& idSource, spStream& payloadSource, std::string* error)
    {
        (void)expectedClassID; // original argument has no consumer
        if (error) error->clear();
        const auto fail = [&](const char* message) -> spBaseObject* {
            context.failed = true;
            if (error) *error = message;
            return nullptr;
        };
        if (context.failed) return fail("Discard failed reference-read context before retry");
        ReferencePrefixForAnalysis prefix;
        if(!ReadReferencePrefixForAnalysis(idSource,payloadSource,prefix,error)){context.failed=true;return nullptr;}
        const auto id=prefix.id,inlineSize=prefix.inlineSize;
        if (!id) return nullptr;
        auto* fat = context.manager.GetFATForAnalysis();
        auto* entry = fat ? fat->FindByIDForAnalysis(id) : nullptr;
        if (!entry) return fail("Reference ID is absent from FAT");
        const auto skip = [&]() {
            if (!inlineSize) return true;
            std::uint32_t position = 0, size = 0;
            const auto origin = payloadSource.GetLogicalOriginForAnalysis();
            return payloadSource.GetCurrentPosition(position) && payloadSource.GetSize(&size) && origin <= size &&
                position <= size - origin && inlineSize <= size - origin - position &&
                inlineSize <= std::uint32_t(std::numeric_limits<std::int32_t>::max()) &&
                payloadSource.Seek(spStream::SeekSource::essCurrent, static_cast<std::int32_t>(inlineSize));
        };
        if (entry->object) return skip() ? entry->object : fail("Cannot skip existing reference payload");
        // Native resolves serializer BEFORE the cache; a cache hit still works
        // if that lookup returned null, because it never dereferences it.
        auto* serializer = context.manager.FindForAnalysis(entry->classID);
        entry->object = context.resources.FindForAnalysis(entry->classID, entry->GetNameForAnalysis());
        if (entry->object) return skip() ? entry->object : fail("Cannot skip cached reference payload");
        if (!serializer) return fail("No serializer for inline resource");
        std::uint32_t position = 0, size = 0;
        const auto origin = payloadSource.GetLogicalOriginForAnalysis();
        if (inlineSize < 8 || !payloadSource.GetCurrentPosition(position) || !payloadSource.GetSize(&size) ||
            origin > size || position > size - origin || inlineSize > size - origin - position)
            return fail("Invalid inline object extent");
        if (context.depth >= 64) return fail("Reference graph exceeds host depth bound (64)");
        if (context.GetCreatedObjectCountForAnalysis() >= context.maximumCreatedObjectsForAnalysis)
            return fail("Reference graph exceeds configured host object bound");
        auto object = serializer->ReadObjectHeaderAndCreateForAnalysis(payloadSource);
        if (!object) return fail("Inline object header or factory failed");
        auto* result = context.PublishObjectForAnalysis(std::move(object));
        entry->object = result; // before any recursive payload callback
        struct DepthGuard final
        {
            std::uint32_t& depth;
            explicit DepthGuard(std::uint32_t& value) : depth(value) { ++depth; }
            ~DepthGuard() { --depth; }
        } guard(context.depth);
        if (!serializer->ReadPayloadForAnalysis(context, payloadSource, inlineSize - 8, *result, error))
        {
            context.failed = true;
            if (error && error->empty()) *error = "Inline resource payload failed";
            if (error) *error += " [inline ID " + std::to_string(prefix.id) + ", class " + std::to_string(result->vfunc_18().classID) + "]";
            return nullptr;
        }
        std::uint32_t end = 0;
        if (context.failed || !payloadSource.GetCurrentPosition(end) || end != position + inlineSize)
            return fail("Inline payload did not consume its bounded extent");
        if (result->vfunc_18().IsKindOf(spNamedObject::ClassID))
        {
            if (auto* named = dynamic_cast<spNamedObject*>(result)) named->SetName(entry->GetNameForAnalysis());
            if (auto* resource = dynamic_cast<spResource*>(result))
                (void)context.resources.RegisterForAnalysis(*resource);
        }
        return result;
    }
}
