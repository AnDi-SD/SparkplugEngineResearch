#pragma once

// Exact original source path recovered from the PC executable:
// Z:\Sparkplug\Code\Sparkplug\spSerializer.cpp

#include "../SparkBase/spBaseObject.h"
#include <string>
#include <vector>
#include <array>

namespace sparkplug::reconstruction
{
    class spStream;
    class spSerializerManager;
    class spResourceManager;
    class spAnimationManager;
    class spDXRenderer;
    class spDXMeshCombiner;
    struct spSerializerReadContextForAnalysis;
    struct spDataBlockHeaderForAnalysis;

    struct spSerializerObjectHeaderForAnalysis final
    {
        std::uint32_t classID = 0;
        std::uint32_t marker = 0x4F4F4253; // bytes "SBOO"
    };

    static_assert(sizeof(spSerializerObjectHeaderForAnalysis) == 0x08);

    class spSerializer : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x42429877;

        ~spSerializer() override;

        spSerializer(const spSerializer&) = delete;
        spSerializer& operator=(const spSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // The native base registration has no factory and its clone slot
        // returns null. Concrete per-object serializers provide allocation.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x004671E0 and PS2 0x001814D0 both return their class-ID
        // argument unchanged. The virtual hook lets specialised serializers
        // remap legacy IDs, although no such override is claimed here.
        [[nodiscard]] virtual spClassID ResolveClassIDForAnalysis(
            spClassID serializedClassID) const noexcept;

        // Native PC 0x00467550 / PS2 0x001815B0 read the full 8-byte
        // [classID, "SBOO"] record, but only classID participates in control
        // flow. The marker is exposed to strict callers without pretending
        // that the shipped loader validated it here.
        [[nodiscard]] virtual std::unique_ptr<spBaseObject>
            ReadObjectHeaderAndCreateForAnalysis(
                spStream& source,
                spSerializerObjectHeaderForAnalysis* observedHeader = nullptr) const;

        [[nodiscard]] static bool HasCanonicalObjectMarkerForAnalysis(
            const spSerializerObjectHeaderForAnalysis& header) noexcept;

        // Original eight-byte read, shared with metadata-only inspection.
        // Factory/RTTI dispatch remains in ReadObjectHeaderAndCreate above.
        [[nodiscard]] static bool ReadObjectHeaderForAnalysis(spStream& source,
            spSerializerObjectHeaderForAnalysis& header);

        // PC467260 writes actual object's RTTI class ID plus literal SBOO.
        [[nodiscard]] static bool WriteObjectHeaderForAnalysis(
            spStream& destination, const spBaseObject& object);

        // Portable interfaces for the native secondary serializer table.
        // Base defaults explicitly reject unsupported reconstruction slices;
        // they are NOT claims about a native default implementation.
        [[nodiscard]] virtual bool IndexRelationshipsForAnalysis(spBaseObject& object) const;
        [[nodiscard]] virtual bool WritePayloadForAnalysis(
            spStream& destination, const spBaseObject& object, std::string* error) const;
        [[nodiscard]] virtual bool ReadPayloadForAnalysis(
            spSerializerReadContextForAnalysis& context, spStream& source,
            std::uint32_t byteCount, spBaseObject& object, std::string* error) const;
        // Explicit host dependency for concrete graph serializers; native
        // methods use the SerializerManager singleton. Existing scalar
        // overrides remain the default, so there is only one dispatch core.
        [[nodiscard]] virtual bool IndexRelationshipsWithContextForAnalysis(
            spSerializerManager& manager, spBaseObject& object) const;
        [[nodiscard]] virtual bool WritePayloadWithContextForAnalysis(
            spSerializerManager& manager, spStream& destination,
            const spBaseObject& object, std::string* error) const;

        // PC4672C0: register object before recursively indexing relationships;
        // an already indexed object succeeds without revisiting its children.
        [[nodiscard]] bool IndexResourceForAnalysis(
            spSerializerManager& manager, spBaseObject& object) const;
        // PC467300 selects the object's serializer; null is an empty success.
        [[nodiscard]] static bool IndexReferenceForAnalysis(
            spSerializerManager& manager, spBaseObject* object);

        // PC467350: null -> u32 zero; first use -> ID,size,header,payload;
        // later uses -> ID,zero. Explicit manager is a host dependency seam,
        // native uses singleton. ErrorManager UI/dispatch is not forwarded.
        // Host checks missing registry/index and final patch failures. Failure
        // after payloadWritten becomes true requires discarding this save
        // context/output: native has no rollback and retry is not safe.
        [[nodiscard]] static bool WriteReferenceForAnalysis(
            spSerializerManager& manager, spStream& destination,
            const spBaseObject* object, std::string* error = nullptr);

        // PC4678B0/467670: expectedClassID is unused by the original. ID and
        // inline-size/payload may come from different streams. Cache and FAT
        // hits return borrowed pointers, without retain. New objects are
        // published before payload reading. Host validates all extents/I/O;
        // failure poisons context, never silently retries partially read data.
        [[nodiscard]] static spBaseObject* ReadReferenceForAnalysis(
            spSerializerReadContextForAnalysis& context, spClassID expectedClassID,
            spStream& idSource, spStream& payloadSource, std::string* error = nullptr);
        struct ReferencePrefixForAnalysis { std::uint32_t id=0, inlineSize=0; };
        // The original resolver's two stream reads. A null ID consumes no
        // size word. This observation does not resolve FAT/cache or own objects.
        [[nodiscard]] static bool ReadReferencePrefixForAnalysis(spStream& idSource,
            spStream& payloadSource,ReferencePrefixForAnalysis& prefix,std::string* error=nullptr,
            std::uint32_t availableBytes=0xFFFFFFFFu);
        // Host guard around the SAME resolver: validate ID/inline extent inside
        // the enclosing field before dispatch/allocation, then restore cursor.
        [[nodiscard]] static spBaseObject* ReadFieldReferenceForAnalysis(
            spSerializerReadContextForAnalysis& context, spClassID expectedClassID,
            spStream& source, const spDataBlockHeaderForAnalysis& field,
            std::string* error = nullptr);
        // One reference inside a packed sequence (e.g. AnimTex frame array).
        // The containing field is NOT one reference; inspect only this entry's
        // ID/size, bound it by sequenceEnd, then use the same reference core.
        [[nodiscard]] static spBaseObject* ReadSequenceReferenceForAnalysis(
            spSerializerReadContextForAnalysis& context,spClassID expectedClassID,
            spStream& source,std::uint32_t sequenceEnd,std::string* error=nullptr);

    protected:
        spSerializer() noexcept;
    };

    // Explicit host lifetime/limits, not a recovered native class. Manager,
    // cache and optional animation name registry must outlive this context;
    // cached/pre-materialized objects require explicit shared owners before an
    // owning graph edge can retain them. No no-op deleter or guessed ownership.
    // Arbitrary cyclic SMO ownership remains outside this safe host slice.
    struct spSerializerReadContextForAnalysis final
    {
        spSerializerManager& manager;
        spResourceManager& resources;
        spAnimationManager* animationBindings = nullptr;
        spDXRenderer* pcRenderer = nullptr; // explicit CPU-only renderer/cache dependency
        spDXMeshCombiner* activeMeshCombiner = nullptr; // scoped batch, never owns
        using PCTexturePitchForAnalysis=std::uint32_t (*)(void*,std::uint32_t level,std::uint32_t packedRowBytes) noexcept;
        PCTexturePitchForAnalysis pcTexturePitchForAnalysis=nullptr; // explicit CPU shadow storage policy; default tightly packed, NOT device pitch
        void* pcTexturePitchContext=nullptr;
        // PC42EA50 constructs6BD580 for external texture source field4.
        // Host supplies the owned stream backend explicitly; no implicit I/O.
        using TextureSourceStreamFactoryForAnalysis=std::unique_ptr<spStream> (*)(void*);
        TextureSourceStreamFactoryForAnalysis textureSourceStreamFactoryForAnalysis=nullptr;
        void* textureSourceStreamContext=nullptr;
        const std::array<float,9>* cameraOrientation = nullptr; // explicit CPU billboard input
        bool failed = false;
        std::uint32_t depth = 0;
        std::size_t maximumCreatedObjectsForAnalysis = 4096; // configurable HOST bound, not game format
        std::vector<std::shared_ptr<spBaseObject>> createdObjects;
        std::vector<std::shared_ptr<spBaseObject>> externalOwners;
        // Explicit host ownership policy for native direct-delete families.
        // Publish before reading, then transfer the unique owner when the real
        // owning edge is read. Borrowed references never consume this owner.
        // Null slots remain after transfer so the object limit still counts
        // every allocation, including descendants owned by a resource.
        std::vector<spClassID> directOwnedClassIDsForAnalysis;
        std::vector<std::unique_ptr<spBaseObject>> pendingDirectObjectsForAnalysis;
        [[nodiscard]] std::size_t GetCreatedObjectCountForAnalysis() const noexcept;
        [[nodiscard]] spBaseObject* PublishObjectForAnalysis(std::unique_ptr<spBaseObject> object);
        [[nodiscard]] std::unique_ptr<spBaseObject> TakeDirectOwnerForAnalysis(
            spBaseObject* object, const spBaseObject* owner, std::string* error = nullptr);
        // Optional host observation of the completed file's FAT identities.
        // Borrowed pointers retain no additional ownership and live only as
        // long as the context/external owners. Native FAT cleanup is unchanged.
        struct FileObjectForAnalysis
        {
            std::uint32_t id; spBaseObject* object;
            // Optional host snapshot of the FAT that the original loader clears.
            // These are wire identities/extents, not the runtime factory's class.
            std::uint32_t wireClassID = 0, offset = 0, size = 0;
            std::string name;
        };
        bool captureFileObjectIDsForAnalysis = false;
        std::vector<FileObjectForAnalysis> fileObjectsForAnalysis;
        [[nodiscard]] std::shared_ptr<spBaseObject> ShareObjectForAnalysis(
            spBaseObject* object) const noexcept;
        spSerializerReadContextForAnalysis(spSerializerManager& manager,
            spResourceManager& resources, spAnimationManager* bindings = nullptr) noexcept;
        ~spSerializerReadContextForAnalysis();
        spSerializerReadContextForAnalysis(const spSerializerReadContextForAnalysis&) = delete;
        spSerializerReadContextForAnalysis& operator=(const spSerializerReadContextForAnalysis&) = delete;
    private:
        struct DirectObjectIdentityForAnalysis
        {
            spBaseObject* object;
            const spBaseObject* owner;
        };
        std::vector<DirectObjectIdentityForAnalysis> directObjectIdentitiesForAnalysis_;
    };
}
