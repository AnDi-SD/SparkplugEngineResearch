// Metadata slice of PC441C10, with factory41A640 defaults and byte-string
// facts from the original probes. No test claims whole Text loading/layout.
#include "Code/Sparkplug/spTextRenderableSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    using Observation = spTextRenderableSerializer::InspectionForAnalysis;
    using Fields = std::initializer_list<std::pair<std::uint32_t, Bytes>>;
    unsigned checks = 0;
    void Check(bool ok, const char* message)
    { ++checks; if (!ok) throw std::runtime_error(message); }

    void Open(spMemoryStream& stream, const Bytes& bytes = {})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())), "Memory stream extent");
        if (!bytes.empty()) std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart, 0), "Memory stream rewind");
    }

    Bytes UInt32(std::uint32_t value)
    { Bytes bytes(4); std::memcpy(bytes.data(), &value, 4); return bytes; }

    Bytes String(const Bytes& wireBytes)
    {
        const auto count = static_cast<std::uint16_t>(wireBytes.size());
        Bytes bytes(2); std::memcpy(bytes.data(), &count, 2);
        bytes.insert(bytes.end(), wireBytes.begin(), wireBytes.end()); return bytes;
    }

    Bytes Reference(std::uint32_t id, const Bytes& inlineBytes = {})
    {
        Bytes bytes = UInt32(id);
        if (id)
        {
            const auto size = UInt32(static_cast<std::uint32_t>(inlineBytes.size()));
            bytes.insert(bytes.end(), size.begin(), size.end());
            bytes.insert(bytes.end(), inlineBytes.begin(), inlineBytes.end());
        }
        return bytes;
    }

    Bytes Section(Fields fields)
    {
        spMemoryStream stream; Open(stream); spDataBlockSerializer blocks;
        for (const auto& field : fields)
            Check(blocks.WriteFieldForAnalysis(stream, field.first, field.second.data(),
                static_cast<std::uint32_t>(field.second.size())), "Field fixture");
        Check(blocks.WriteTerminatorForAnalysis(stream), "Section terminator");
        std::uint32_t size = 0; Check(stream.GetSize(&size), "Section size");
        const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return {bytes, bytes + size};
    }

    Bytes Payload(Fields own, Fields inherited = {})
    {
        Bytes bytes = Section(inherited); const auto text = Section(own);
        bytes.insert(bytes.end(), text.begin(), text.end()); return bytes;
    }

    bool Inspect(const Bytes& bytes, Observation& observation)
    {
        spMemoryStream stream; Open(stream, bytes); std::string error = "stale error";
        const auto ok = spTextRenderableSerializer{}.InspectPayloadForAnalysis(
            stream, static_cast<std::uint32_t>(bytes.size()), observation, &error);
        Check(ok == error.empty(), "Inspection returns a diagnostic only on failure");
        if (ok)
        {
            std::uint32_t position = 0;
            Check(stream.GetCurrentPosition(position) && position == bytes.size(), "Exact payload consumed");
        }
        return ok;
    }

    void Defaults(const Observation& observed)
    {
        Check(observed.fieldMask == 0 && observed.text.empty() && observed.textWasNull
            && observed.textByteCount == 0 && !observed.textHadTrailingNull && !observed.font,
            "Omitted own fields retain null factory observations");
        Check(observed.color == 0xFFFFFFFFu && observed.wrapWidth == 0 && observed.alignment == 0,
            "Original Text factory scalar defaults");
        Check(!observed.derivedLayoutAvailable && observed.partial.IsExactly(spRenderable::ClassID),
            "Metadata has only actual base Renderable; derived layout unavailable");
    }

    void RuntimeRefusals()
    {
        spTextRenderableSerializer text; spRenderable object;
        const spSerializer& serializer = text;
        spSerializerManager manager; spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager, resources);
        spMemoryStream stream; Open(stream, {0, 0}); std::string error;
        Check(!serializer.ReadPayloadForAnalysis(context, stream, 2, object, &error)
            && context.failed && error.find("metadata inspection only") != std::string::npos,
            "Virtual runtime reader refuses even a valid base-only target");
        Check(!serializer.WritePayloadForAnalysis(stream, object, &error) && !error.empty(),
            "Virtual runtime writer refused");
        Check(!serializer.WritePayloadWithContextForAnalysis(manager, stream, object, &error) && !error.empty(),
            "Context writer refused");
        Check(!serializer.IndexRelationshipsForAnalysis(object)
            && !serializer.IndexRelationshipsWithContextForAnalysis(manager, object), "Both runtime index paths refused");
        Check(!serializer.ReadObjectHeaderAndCreateForAnalysis(stream), "Generic runtime creation refused");
        spCloneManager clones;
        Check(!serializer.vfunc_10(clones) && !serializer.vfunc_14(object, clones), "Unreconstructed clone refused");
        std::uint32_t position = 99, size = 0;
        Check(stream.GetCurrentPosition(position) && position == 0 && stream.GetSize(&size) && size == 2,
            "Runtime refusal leaves stream untouched");
        Check(text.GetTargetClassIDForAnalysis() == 0x19A745D7
            && text.IsExactly(0x15E97F32) && text.IsKindOf(spRenderableSerializer::ClassID), "Proven serializer identity");
    }

    std::string Hex(const std::string& bytes)
    {
        constexpr char digits[] = "0123456789abcdef"; std::string hex;
        for (const auto byte : bytes)
        { const auto value = static_cast<unsigned char>(byte); hex += digits[value >> 4]; hex += digits[value & 15]; }
        return hex;
    }

    void Capture(const char* inputFile, const char* outputFile)
    {
        std::ifstream input(inputFile, std::ios::binary | std::ios::ate);
        Check(bool(input), "Capture input"); const auto count = input.tellg();
        Check(count >= 0 && count <= 16 * 1024 * 1024, "Capture input bound");
        Bytes bytes(static_cast<std::size_t>(count)); input.seekg(0);
        if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Check(bool(input), "Capture read"); Observation observed;
        Check(Inspect(bytes, observed), "Captured payload metadata inspection");
        std::ofstream output(outputFile); output << std::boolalpha
            << "{\"metadataOnly\":true,\"derivedLayoutAvailable\":false,\"textHex\":\"" << Hex(observed.text)
            << "\",\"textWasNull\":" << observed.textWasNull << ",\"textHadTrailingNull\":" << observed.textHadTrailingNull
            << ",\"textByteCount\":" << observed.textByteCount << ",\"color\":" << observed.color
            << ",\"wrapWidth\":" << observed.wrapWidth << ",\"alignment\":" << observed.alignment
            << ",\"fieldMask\":" << observed.fieldMask << ",\"renderableFieldMask\":" << observed.renderable.fieldMask
            << ",\"alphaSortEnable\":" << observed.partial.IsAlphaSortEnabledForAnalysis()
            << ",\"priority\":" << observed.partial.GetPriorityForAnalysis() << ",\"font\":";
        if (!observed.font) output << "null";
        else output << "{\"id\":" << observed.font->id << ",\"inlineSize\":" << observed.font->inlineSize
            << ",\"offset\":" << observed.font->offset << ",\"size\":" << observed.font->size << '}';
        output << "}\n"; Check(bool(output), "Metadata capture output");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3)
        { Capture(argv[1], argv[2]); std::cout << "Text metadata capture passed\n"; return 0; }
        Check(argc == 1, "Expected no arguments or payload-file metadata-json-file");
        Observation observed; Defaults(observed);
        Check(Inspect({0, 0}, observed), "Both omitted sections accepted"); Defaults(observed);

        // The actual byte-count3 string AB\0, not an even-length Unicode string.
        Check(Inspect({0, 0xA0, 5, 3, 0, 'A', 'B', 0, 0}, observed)
            && observed.text == "AB" && !observed.textWasNull && observed.textHadTrailingNull
            && observed.textByteCount == 3 && observed.fieldMask == 1, "Odd byte-count original string fact");
        Check(Inspect(Payload({{0, String({'A', 0, 0x80, 0xFF, 0, 0})}}), observed)
            && observed.text == std::string("A\0\x80\xFF\0", 5) && observed.textByteCount == 6
            && observed.textHadTrailingNull, "Interior NUL, high bytes and all but one trailing NUL retained");
        Check(Inspect(Payload({{0, String({'A', 0xE9})}}), observed)
            && observed.text == std::string("A\xE9", 2) && !observed.textHadTrailingNull,
            "Common ReadString safely observes nonterminated wire bytes");
        Check(Inspect(Payload({{0, String({0})}}), observed) && observed.text.empty()
            && !observed.textWasNull && observed.textHadTrailingNull && observed.textByteCount == 1,
            "Explicit empty C string differs from null");
        Check(Inspect(Payload({{0, String({})}}), observed) && observed.text.empty()
            && observed.textWasNull && !observed.textHadTrailingNull && observed.fieldMask == 1,
            "Zero byte-count differs from omitted text by field mask");

        const auto mixed = Payload({{3, UInt32(0xFFFFFFFFu)}, {0, String({'A', 'B', 0})},
            {2, UInt32(1)}, {4, Reference(29, {0xFF, 0x80, 0})}, {17, {3, 1, 4}},
            {1, UInt32(0x10203040u)}, {2, UInt32(0xFFFFFFFFu)}, {0, String({'Z', 0})},
            {4, Reference(0)}}, {{2, UInt32(0)}, {0, Reference(7)}, {3, UInt32(0xFFFFFFFFu)}});
        Check(Inspect(mixed, observed) && observed.fieldMask == 31 && observed.text == "Z"
            && observed.color == 0x10203040u && observed.wrapWidth == 0xFFFFFFFFu && observed.alignment == 0xFFFFFFFFu,
            "Unknown, repeated and reordered own fields retain final raw assignments");
        Check(observed.font && observed.font->id == 0 && observed.font->size == 4,
            "Last explicit NULL font reference observed");
        Check(observed.renderable.fieldMask == 13 && observed.renderable.material
            && observed.renderable.material->id == 7 && !observed.partial.GetMaterialForAnalysis()
            && !observed.partial.GetFogForAnalysis() && !observed.partial.IsAlphaSortEnabledForAnalysis()
            && observed.partial.GetPriorityForAnalysis() == 0xFFFFFFFFu,
            "Inherited reader assigns base scalars and leaves references unresolved");
        Check(observed.renderable.scalarFields.size() == 2
            && observed.renderable.scalarFields[0].owner == &observed.partial
            && observed.renderable.scalarFields[1].assignmentOrder == 1, "Inherited observations retain stable owner/order");
        Check(Inspect(Payload({{4, Reference(29, {0xFF, 0x80, 0})}}), observed)
            && observed.font && observed.font->id == 29 && observed.font->inlineSize == 3,
            "Inline font body observed without fake loading");
        Check(Inspect(Payload({{4, Reference(17)}}), observed)
            && observed.font && observed.font->id == 17 && observed.font->inlineSize == 0,
            "Unresolved existing font ID observed");
        Check(Inspect(Payload({{250, {1, 2, 3}}}), observed), "Only unknown own field accepted"); Defaults(observed);
        Check(observed.renderable.fieldMask == 0 && observed.renderable.scalarFields.empty()
            && observed.partial.IsAlphaSortEnabledForAnalysis() && observed.partial.GetPriorityForAnalysis() == 0,
            "Reused observation clears previous base and own state");

        for (const auto& malformed : {Bytes{}, Bytes{0}, Bytes{0, 0, 1},
            Payload({{0, {1}}}), Payload({{0, {4, 0, 'A', 0}}}), Payload({{0, {1, 0, 'A', 0}}}),
            Payload({{2, {1, 2, 3}}}), Payload({{4, {0, 0, 0}}}), Payload({{4, {0, 0, 0, 0, 0}}}),
            Payload({{4, {1, 0, 0, 0, 9, 0, 0, 0}}})})
            Check(!Inspect(malformed, observed), "Malformed section/string/scalar/reference extent rejected");
        auto truncated = mixed; truncated.pop_back();
        Check(!Inspect(truncated, observed), "Missing own terminator rejected");
        RuntimeRefusals();
        std::cout << "Text inspection: " << checks << " checks passed\n"; return 0;
    }
    catch (const std::exception& error)
    { std::cerr << error.what() << '\n'; return 1; }
}
