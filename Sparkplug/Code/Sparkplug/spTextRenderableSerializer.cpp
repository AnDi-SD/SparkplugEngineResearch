#include "spTextRenderableSerializer.h"
#include "spSerializerManager.h"
#include "spResourceManager.h"
#include "Analysis/PC/spSectionCursor.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextRenderableSerializer()
        { return std::make_unique<spTextRenderableSerializer>(); }

        const spRTTIRecord TextRenderableSerializerRecord{
            spTextRenderableSerializer::ClassID, spRenderableSerializer::ClassID,
            "spTextRenderableSerializer", &spRenderableSerializer::StaticRTTI(),
            &CreateTextRenderableSerializer, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(
            TextRenderableSerializerRecord);

        bool Unsupported(std::string* error)
        {
            if (error) *error = "TextRenderable supports metadata inspection only; runtime layout/loading/writing is unavailable";
            return false;
        }
    }

    const spRTTIRecord& spTextRenderableSerializer::StaticRTTI() noexcept
    { (void)Registered; return TextRenderableSerializerRecord; }

    const spRTTIRecord& spTextRenderableSerializer::vfunc_18() const noexcept
    { return TextRenderableSerializerRecord; }

    spClassID spTextRenderableSerializer::GetTargetClassIDForAnalysis() const noexcept
    { return TargetClassID; }

    std::unique_ptr<spBaseObject> spTextRenderableSerializer::vfunc_10(spCloneManager&) const
    { return nullptr; }

    bool spTextRenderableSerializer::vfunc_14(spBaseObject&, spCloneManager&) const
    { return false; }

    std::unique_ptr<spBaseObject> spTextRenderableSerializer::ReadObjectHeaderAndCreateForAnalysis(
        spStream&, spSerializerObjectHeaderForAnalysis*) const
    { return nullptr; }

    bool spTextRenderableSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream&, std::uint32_t, spBaseObject&, std::string* error) const
    { context.failed = true; return Unsupported(error); }

    bool spTextRenderableSerializer::WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string* error) const
    { return Unsupported(error); }

    bool spTextRenderableSerializer::WritePayloadWithContextForAnalysis(spSerializerManager&,
        spStream&, const spBaseObject&, std::string* error) const
    { return Unsupported(error); }

    bool spTextRenderableSerializer::IndexRelationshipsForAnalysis(spBaseObject&) const
    { return false; }

    bool spTextRenderableSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&, spBaseObject&) const
    { return false; }

    bool spTextRenderableSerializer::InspectPayloadForAnalysis(spStream& stream, std::uint32_t size,
        InspectionForAnalysis& observation, std::string* error) const
    {
        if (error) error->clear();
        observation.partial = spRenderable{};
        observation.renderable = {};
        observation.text.clear();
        // PC41A640 factory capture: text/font null, colorFFFFFFFF, wrap/alignment0.
        // See docs/research/tool-text-original-defaults-2026-09-10.md.
        observation.textWasNull = true;
        observation.textHadTrailingNull = false;
        observation.textByteCount = 0;
        observation.color = 0xFFFFFFFFu;
        observation.wrapWidth = observation.alignment = observation.fieldMask = 0;
        observation.font.reset();

        spSerializerManager manager;
        spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager, resources);
        std::uint32_t start = 0, position = 0;
        if (!stream.GetCurrentPosition(start)
            || !ReadRenderableFieldsForAnalysis(context, stream, size, observation.partial,
                false, error, &observation.renderable)
            || !stream.GetCurrentPosition(position) || position < start || position - start >= size)
        {
            if (error && error->empty()) *error = "Missing TextRenderable section";
            return false;
        }
        evidence::pc::serialization::SectionCursor cursor(context, stream, size - (position - start), true, error);
        while (const auto* header = cursor.Next())
        {
            if (header->IsTerminator()) return true;
            switch (header->fieldID)
            {
            case 0:
            {
                // Host extent preflight only. Actual byte-string decoding remains
                // spStream::ReadString (PC416DC0 called by Text reader441CB7).
                std::uint16_t count = 0;
                if (header->payloadSize < sizeof(count) || !stream.Read(count)
                    || header->payloadSize != sizeof(count) + std::uint32_t(count))
                    return cursor.Fail("Invalid TextRenderable byte-string extent");
                if (!stream.Seek(spStream::SeekSource::essCurrent, -static_cast<std::int32_t>(sizeof(count)))
                    || !stream.ReadString(observation.text, &observation.textWasNull))
                    return cursor.Fail("Cannot read TextRenderable byte string");
                observation.textByteCount = count;
                observation.textHadTrailingNull = count > observation.text.size();
                break;
            }
            case 1:
                if (!cursor.Read(observation.color)) return cursor.Fail("Invalid TextRenderable UInt32 color");
                break;
            case 2:
                if (!cursor.Read(observation.wrapWidth)) return cursor.Fail("Invalid TextRenderable UInt32 wrap width");
                break;
            case 3:
                if (!cursor.Read(observation.alignment)) return cursor.Fail("Invalid TextRenderable UInt32 alignment");
                break;
            case 4:
            {
                evidence::pc::serialization::InspectedReference reference;
                if (!evidence::pc::serialization::InspectReference(stream, header->payloadSize, true, reference, error))
                    return cursor.Fail("Cannot inspect TextRenderable font reference");
                observation.font = reference;
                break;
            }
            default:
                if (!cursor.Skip()) return cursor.Fail("Cannot skip TextRenderable field");
                continue;
            }
            // PC441C10 accepts omitted, repeated and reordered fields. This is
            // the last wire assignment, not a claim that its layout succeeded.
            observation.fieldMask |= 1u << header->fieldID;
        }
        return false;
    }
}
