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
                for (const auto& owned : createdObjects)
                    if (entry->object == owned.get()) { entry->object = nullptr; break; }
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
        std::uint32_t position = 0, id = 0, inlineSize = 0;
        if (context.failed || !source.GetCurrentPosition(position)
            || position != field.dataStreamPosition || field.payloadSize < 4
            || !source.Read(id)) return fail("Cannot inspect bounded reference field");
        if (id)
        {
            if (field.payloadSize < 8 || !source.Read(inlineSize)
                || inlineSize != field.payloadSize - 8)
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
        std::uint32_t position=0,id=0,inlineSize=0,extent=4;
        if(context.failed||!source.GetCurrentPosition(position)||position>sequenceEnd||sequenceEnd-position<4||!source.Read(id))
            return fail("Cannot inspect reference sequence entry");
        if(id)
        {
            if(sequenceEnd-position<8||!source.Read(inlineSize)||inlineSize>sequenceEnd-position-8)
                return fail("Reference sequence entry exceeds field");
            extent=inlineSize+8;
        }
        if(position>std::uint32_t(std::numeric_limits<std::int32_t>::max())
            ||!source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(position)))return fail("Cannot restore reference sequence cursor");
        spDataBlockHeaderForAnalysis entry;entry.dataStreamPosition=position;entry.payloadSize=extent;
        return ReadFieldReferenceForAnalysis(context,expectedClassID,source,entry,error);
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
        std::uint32_t id = 0, inlineSize = 0;
        if (!idSource.Read(id)) return fail("Cannot read reference ID");
        if (!id) return nullptr;
        if (!payloadSource.Read(inlineSize)) return fail("Cannot read reference inline size");
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
        if (context.depth >= 64 || context.createdObjects.size() >= 4096)
            return fail("Reference graph exceeds host depth/object bound");
        auto object = serializer->ReadObjectHeaderAndCreateForAnalysis(payloadSource);
        if (!object) return fail("Inline object header or factory failed");
        auto* result = object.get();
        context.createdObjects.push_back(std::move(object));
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
