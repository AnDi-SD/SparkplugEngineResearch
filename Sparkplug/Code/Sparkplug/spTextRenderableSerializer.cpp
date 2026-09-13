#include "spTextRenderableSerializer.h"
#include "spTextRenderable.h"
#include "spFont.h"
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
            if (error) *error = "TextRenderable writing is not reconstructed";
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
        spStream& stream, spSerializerObjectHeaderForAnalysis* header) const
    { (void)spTextRenderable::StaticRTTI();return spSerializer::ReadObjectHeaderAndCreateForAnalysis(stream,header); }

    bool spTextRenderableSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream, std::uint32_t size, spBaseObject& object, std::string* error) const
    {
        auto* text=dynamic_cast<spTextRenderable*>(&object);
        if(!text){context.failed=true;if(error)*error="TextRenderable runtime target mismatch";return false;}
        return ReadTextFieldsForAnalysis(context,stream,size,*text,nullptr,error);
    }

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
        // See docs/engine/ui/text-defaults.md.
        observation.textWasNull = true;
        observation.textHadTrailingNull = false;
        observation.textByteCount = 0;
        observation.color = 0xFFFFFFFFu;
        observation.wrapWidth = observation.alignment = observation.fieldMask = 0;
        observation.font.reset();

        spSerializerManager manager;
        spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager, resources);
        return ReadTextFieldsForAnalysis(context,stream,size,observation.partial,&observation,error);
    }

    bool spTextRenderableSerializer::ReadTextFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spRenderable& base,InspectionForAnalysis* observation,std::string* error) const
    {
        if(error)error->clear();
        auto* runtime=dynamic_cast<spTextRenderable*>(&base);
        std::uint32_t start = 0, position = 0;
        if (!stream.GetCurrentPosition(start)
            || !ReadRenderableFieldsForAnalysis(context, stream, size, base,
                false, error, observation?&observation->renderable:nullptr)
            || !stream.GetCurrentPosition(position) || position < start || position - start >= size)
        {
            if (error && error->empty()) *error = "Missing TextRenderable section";
            context.failed=true;
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
                std::string value;bool isNull=false;
                if (!stream.Seek(spStream::SeekSource::essCurrent, -static_cast<std::int32_t>(sizeof(count)))
                    || !stream.ReadString(value, &isNull))
                    return cursor.Fail("Cannot read TextRenderable byte string");
                if(observation){
                    observation->text=std::move(value);observation->textWasNull=isNull;
                    observation->textByteCount=count;observation->textHadTrailingNull=count>observation->text.size();
                }else{
                    // Native strcpy/strlen needs a terminator within this field.
                    // Keep unterminated raw bytes available to metadata only.
                    if(isNull||count==0||value.size()==count&&value.find('\0')==std::string::npos)
                        return cursor.Fail("Text runtime requires a terminated, non-NULL byte string");
                    if(!runtime->SetTextForAnalysis(value.c_str(),context.fontManager,error))
                    {context.failed=true;return false;}
                }
                break;
            }
            case 1:
            {
                std::uint32_t value=0;
                if (!cursor.Read(value)) return cursor.Fail("Invalid TextRenderable UInt32 color");
                if(observation)observation->color=value;else runtime->SetColorForAnalysis(value);
                break;
            }
            case 2:
            {
                std::uint32_t value=0;
                if (!cursor.Read(value)) return cursor.Fail("Invalid TextRenderable UInt32 wrap width");
                if(observation)observation->wrapWidth=value;
                else if(!runtime->SetWrapWidthForAnalysis(value,context.fontManager,error)){context.failed=true;return false;}
                break;
            }
            case 3:
            {
                std::uint32_t value=0;
                if (!cursor.Read(value)) return cursor.Fail("Invalid TextRenderable UInt32 alignment");
                if(observation)observation->alignment=value;
                else if(!runtime->SetAlignmentForAnalysis(value,context.fontManager,error)){context.failed=true;return false;}
                break;
            }
            case 4:
            {
                if(observation){
                    evidence::pc::serialization::InspectedReference reference;
                    if (!evidence::pc::serialization::InspectReference(stream, header->payloadSize, true, reference, error))
                        return cursor.Fail("Cannot inspect TextRenderable font reference");
                    observation->font = reference;
                }else{
                    auto* font=ReadFieldReferenceForAnalysis(context,spFont::ClassID,stream,*header,error);
                    if(context.failed)return false;
                    auto owner=std::dynamic_pointer_cast<spFont>(context.ShareObjectForAnalysis(font));
                    if(font&&!owner)return cursor.Fail("Text Font lacks canonical owner");
                    if(!runtime->SetFontForAnalysis(std::move(owner),context.fontManager,error))
                    {context.failed=true;return false;}
                }
                break;
            }
            default:
                if (!cursor.Skip()) return cursor.Fail("Cannot skip TextRenderable field");
                continue;
            }
            // PC441C10 accepts omitted, repeated and reordered fields. This is
            // the last wire assignment, not a claim that its layout succeeded.
            if(observation)observation->fieldMask |= 1u << header->fieldID;
        }
        return false;
    }
}
