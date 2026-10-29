#pragma once

// Inferred declaration path. PC468820 and the registered concrete serializer
// establish this bounded reader; no original header/TU path is recovered.
#include "spRenderableSerializer.h"

namespace sparkplug::reconstruction
{
    class spProjection;
    struct spProjectionNullIndexContextForAnalysis;
    struct spProjectionPreindexedIndexContextForAnalysis;
    struct spProjectionFogOnlyIndexContextForAnalysis;
    struct spProjectionEmptyMaterialIndexContextForAnalysis;
    class spProjectionSerializer : public spRenderableSerializer
    {
    public:
        static constexpr spClassID ClassID=0x3344790A;
        static constexpr spClassID TargetClassID=0x5CB4145D;
        spProjectionSerializer() noexcept=default;
        ~spProjectionSerializer() override;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        // Stable raw scalar sections; relationships are null or externally
        // retained DXMaterial/Fog/RenderNode already written in supplied FAT.
        // Material/Fog readback needs explicit shared owners;84 stays borrowed.
        // Inline relationships and implicit index are unqualified.
        bool IndexRelationshipsForAnalysis(spBaseObject&) const override;
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;
        // Separate explicit PC profile: supplied warm empty ErrorManager,
        // empty save FAT and stable null Material/Fog/RenderNode relationships.
        // Existing index entry points retain their no-context refusal.
        virtual bool IndexNullRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionNullIndexContextForAnalysis&,spBaseObject&) const;
        // Separate exact host profile: at least one typed relationship already
        // indexed at supplied IDs7/9/11. No relation recursion or new indexing.
        virtual bool IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionPreindexedIndexContextForAnalysis&,spBaseObject&) const;
        // Separate first-insert/repeat profile for one typed Fog, other links null.
        virtual bool IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionFogOnlyIndexContextForAnalysis&,spBaseObject&) const;
        // Separate empty Material first-insert/repeat profile; own dispatch
        // uses an explicit stack-local host adapter for the nested error gate.
        virtual bool IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(
            spProjectionEmptyMaterialIndexContextForAnalysis&,spBaseObject&) const;
        bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
    protected:
        // Sequential Renderable + Projection sections. Derived serializers
        // can consume their own section after a non-exact parent terminator.
        bool ReadProjectionFieldsForAnalysis(spSerializerReadContextForAnalysis&,
            spStream&,std::uint32_t,spProjection&,bool requireEnd,std::string*) const;
        bool WriteProjectionFieldsForAnalysis(spSerializerManager*,spStream&,
            const spProjection&,std::string*) const;
        bool IndexProjectionNullFieldsForAnalysis(
            spProjectionNullIndexContextForAnalysis&,spProjection&) const;
        bool IndexProjectionPreindexedFieldsForAnalysis(
            spProjectionPreindexedIndexContextForAnalysis&,spProjection&) const;
        bool IndexProjectionFogOnlyFieldsForAnalysis(
            spProjectionFogOnlyIndexContextForAnalysis&,spProjection&) const;
        bool IndexProjectionEmptyMaterialFieldsForAnalysis(
            spProjectionEmptyMaterialIndexContextForAnalysis&,spProjection&) const;
    };
}
