#include "spStaticRenderObjectSerializer.h"
#include "spStaticRenderObject.h"
#include "Analysis/PC/spSectionCursor.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spStaticRenderObjectSerializer>(); }
        const spRTTIRecord Record{spStaticRenderObjectSerializer::ClassID, spSerializer::ClassID,
            "spStaticRenderObjectSerializer", &spSerializer::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spStaticRenderObjectSerializer::StaticRTTI() noexcept { (void)Registered; return Record; }
    const spRTTIRecord& spStaticRenderObjectSerializer::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spStaticRenderObjectSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spStaticRenderObjectSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spStaticRenderObjectSerializer::ReadMatrixFieldForAnalysis(std::uint32_t field,
        spStream& source, spStaticRenderObject& object)
    {
        if (field != 1 && field != 2) return false;
        spStaticRenderObject::Matrix4 matrix{};
        if (!source.ReadData(matrix.data(), sizeof(matrix))) return false;
        if (field == 1) object.SetWorldMatrixForAnalysis(matrix);
        else object.SetWorldInverseMatrixForAnalysis(matrix);
        return true;
    }
    bool spStaticRenderObjectSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source, std::uint32_t size, spBaseObject& base, std::string* error) const
    {
        if (error) error->clear();
        evidence::pc::serialization::SectionCursor cursor(context, source, size, true, error);
        auto* object = dynamic_cast<spStaticRenderObject*>(&base);
        if (!object) return cursor.Fail("StaticRenderObject target mismatch");
        while (const auto* header = cursor.Next())
        {
            if (header->IsTerminator()) return true;
            if (header->fieldID == 1 || header->fieldID == 2)
            {
                // Exact extent is a host envelope guard. Native416E30 requests
                // 64 bytes; it does not validate affine shape or inverse relation.
                if (header->payloadSize != 64 || !ReadMatrixFieldForAnalysis(header->fieldID, source, *object))
                    return cursor.Fail("Invalid bounded StaticRenderObject matrix field");
            }
            else if (header->fieldID == 0)
            {
                auto* raw = ReadFieldReferenceForAnalysis(context, spRenderable::ClassID, source, *header, error);
                if (context.failed) return false;
                auto renderable = std::dynamic_pointer_cast<spRenderable>(context.ShareObjectForAnalysis(raw));
                if (!object->AttachRenderableForAnalysis(std::move(renderable)))
                    return cursor.Fail("StaticRenderObject renderable is null, wrong type or lacks an owner");
            }
            else if (!cursor.Skip()) return cursor.Fail("Cannot skip StaticRenderObject field");
        }
        return false;
    }
    bool spStaticRenderObjectSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
        spBaseObject& base) const
    {
        auto* object = dynamic_cast<spStaticRenderObject*>(&base);
        if (!object) return false;
        for (std::size_t i = 0; i < object->GetRenderableCountForAnalysis(); ++i)
            if (!IndexReferenceForAnalysis(manager, object->GetRenderableForAnalysis(i))) return false;
        return true;
    }
    bool spStaticRenderObjectSerializer::WriteFields(spSerializerManager* manager, spStream& stream,
        const spBaseObject& base, std::string* error) const
    {
        if (error) error->clear();
        const auto fail = [&](const char* text) { if (error) *error = text; return false; };
        const auto* object = dynamic_cast<const spStaticRenderObject*>(&base);
        if (!object) return fail("StaticRenderObject write target mismatch");
        if (!manager && object->GetRenderableCountForAnalysis())
            return fail("StaticRenderObject relationships require explicit serializer manager");
        spDataBlockSerializer blocks;
        const auto& world = object->GetWorldMatrixForAnalysis();
        const auto& inverse = object->GetWorldInverseMatrixForAnalysis();
        if (!blocks.BeginObjectForAnalysis(stream, object)
            || !blocks.WriteFieldForAnalysis(stream, 1, world.data(), sizeof(world))
            || !blocks.WriteFieldForAnalysis(stream, 2, inverse.data(), sizeof(inverse))) return false;
        for (std::size_t i = 0; i < object->GetRenderableCountForAnalysis(); ++i)
            if (!blocks.WriteBeginForAnalysis(0, spDataBlockSerializer::SizeCode::UInt32)
                || !WriteReferenceForAnalysis(*manager, stream, object->GetRenderableForAnalysis(i), error)
                || !blocks.WriteEndForAnalysis(0)) return false;
        return blocks.FinalizeObjectForAnalysis();
    }
    bool spStaticRenderObjectSerializer::WritePayloadForAnalysis(spStream& stream,
        const spBaseObject& object, std::string* error) const { return WriteFields(nullptr, stream, object, error); }
    bool spStaticRenderObjectSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& stream, const spBaseObject& object, std::string* error) const
    { return WriteFields(&manager, stream, object, error); }
}
