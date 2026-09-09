// Metadata cursor from original PS2 176A10, scalar helpers114D40/114E30,
// palette branches176B1C/176C38 and mip reads176B48..176BD0. Synthetic
// guards are explicit host restrictions, not a C# parser or GPU oracle.
#include "Code/Sparkplug/spPS2TextureDataSerializer.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace sparkplug::reconstruction;
namespace
{
    using Serializer = spPS2TextureDataSerializer;
    using Observation = Serializer::NativeSectionInspectionForAnalysis;
    using Bytes = std::vector<std::uint8_t>;
    using Fields = std::initializer_list<std::pair<std::uint32_t, Bytes>>;
    unsigned checks = 0;
    void Check(bool value, const char* message)
    { ++checks; if (!value) throw std::runtime_error(message); }

    void Open(spMemoryStream& stream, const Bytes& bytes = {})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())), "Memory fixture extent");
        if (!bytes.empty()) std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart, 0), "Memory fixture rewind");
    }
    void AppendWord(Bytes& bytes, std::uint32_t word)
    {
        for (unsigned shift = 0; shift != 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>(word >> shift));
    }
    void PutWord(Bytes& bytes, std::size_t offset, std::uint32_t word)
    {
        Check(offset + 4 <= bytes.size(), "Fixture scalar offset");
        for (unsigned i = 0; i != 4; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
    }
    Bytes Raw(std::uint8_t flag, std::uint32_t format, std::uint32_t width,
        std::uint32_t height, const std::vector<Bytes>& pixels = {})
    {
        Bytes bytes{flag};
        for (auto word : {format, width, height, 0x12345678u, static_cast<std::uint32_t>(pixels.size())})
            AppendWord(bytes, word);
        // Independent original176B1C/176C38 palette branches, not Describe helper.
        bytes.resize(bytes.size() + (format == 0 ? 64 : format == 1 ? 1024 : 0), 0xa5);
        for (const auto& data : pixels)
        {
            for (auto word : {0xfedcba98u, 0x87654321u, 0x10203040u, static_cast<std::uint32_t>(data.size())})
                AppendWord(bytes, word);
            bytes.insert(bytes.end(), data.begin(), data.end());
        }
        return bytes;
    }
    Bytes Section(Fields fields)
    {
        spMemoryStream stream; Open(stream); spDataBlockSerializer blocks;
        for (const auto& field : fields)
            Check(blocks.WriteFieldForAnalysis(stream, field.first, field.second.data(),
                static_cast<std::uint32_t>(field.second.size())), "Common field fixture writer");
        Check(blocks.WriteTerminatorForAnalysis(stream), "Common terminator fixture writer");
        std::uint32_t size = 0; Check(stream.GetSize(&size), "Field fixture size");
        const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return {bytes, bytes + size};
    }
    bool Inspect(const Bytes& bytes, Observation& result, std::uint32_t prefix = 0)
    {
        Bytes wrapped(prefix, 0xcc);
        wrapped.insert(wrapped.end(), bytes.begin(), bytes.end());
        // Suffix verifies supplied section limits independently of physical EOF.
        wrapped.insert(wrapped.end(), {0xaa, 0xbb, 0xcc});
        spMemoryStream stream; Open(stream, wrapped);
        Check(stream.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(prefix)), "Positioned input");
        std::string error = "stale";
        const bool ok = Serializer::InspectNativeSectionForAnalysis(stream,
            static_cast<std::uint32_t>(bytes.size()), result, &error);
        std::uint32_t position = 0; Check(stream.GetCurrentPosition(position), "Observed source cursor");
        Check(ok == error.empty() && result.complete == ok, "Completion and diagnostic agree");
        Check(result.inputOffset == prefix && result.inputSize == bytes.size()
            && result.finalPosition == position && position <= prefix + bytes.size(), "Bounded source-coordinate cursor");
        if (ok) Check(position == prefix + bytes.size(), "Completed section consumes exact supplied extent");
        return ok;
    }

    class ExtentOnlyStream final : public spStream
    {
    public:
        std::uint32_t position = 0, size = 0, reads = 0;
        bool Open(const char*) override { return false; }
        bool Open(std::uint32_t, const char*) override { return false; }
        bool Close() override { return false; }
        bool Seek(SeekSource, std::int32_t) override { return false; }
        bool GetCurrentPosition(std::uint32_t& output) const override { output = position; return true; }
        bool GetSize(std::uint32_t* output) const override { if (!output) return false; *output = size; return true; }
        bool ReadData(void*, std::uint32_t) override { ++reads; return false; }
        bool WriteData(const void*, std::uint32_t) override { return false; }
        bool vfunc_WriteFromStream(spStream*, std::uint32_t) override { return false; }
    };

    void Guards()
    {
        Observation result;
        Check(Inspect({0}, result) && result.images.empty() && result.fields.size() == 1
            && result.fields[0].terminator, "Empty native section is metadata, not a fake texture");
        const auto zero = Raw(0, 0x76543210, 0, 0xffffffffu);
        Check(Inspect(Section({{0, zero}}), result) && result.images[0].complete
            && result.images[0].nativeFlag == 0 && result.images[0].pixelFormat == 0x76543210
            && result.images[0].width == 0 && result.images[0].height == 0xffffffffu
            && result.images[0].mipCount == 0 && result.images[0].paletteByteCount == 0,
            "Original byte is not presence gate; raw format/dimensions and zero mips retained");
        const auto indexed = Raw(2, 1, 1, 1, {{1, 2, 3}, {}});
        Check(Inspect(Section({{0, indexed}}), result) && result.images[0].nativeFlag == 2
            && result.images[0].paletteByteCount == 1024 && result.images[0].mips.size() == 2
            && result.images[0].mips[0].dataSize == 3 && result.images[0].mips[1].dataSize == 0
            && result.images[0].mips[0].descriptor1 == 0x87654321u,
            "Palette branch and wire mip extents, without width*bpp inference");
        const auto mixed = Section({{250, {0, 0xff}}, {0, zero}, {17, {0, 0, 0}},
            {0, Raw(1, 0, 16, 16, {{0x80}})}});
        Check(Inspect(mixed, result, 11) && result.images.size() == 2 && result.fields.size() == 5
            && result.images[0].fieldIndex == 1 && result.images[1].fieldIndex == 3
            && result.fields[1].imageIndex == 0 && result.fields[3].imageIndex == 1
            && result.fields[0].fieldID == 250 && result.fields[0].imageIndex == Serializer::NoImageForAnalysis
            && result.fields[2].complete && result.images[1].paletteByteCount == 64,
            "Unknown and repeated fields retain original order and independent images");
        Check(result.images[0].paletteOffset == result.fields[1].payloadOffset + 21
            && result.images[1].mips[0].descriptorOffset == result.fields[3].payloadOffset + 85
            && result.images[1].mips[0].dataOffset == result.fields[3].payloadOffset + 101,
            "All palette/mip offsets preserve positioned source coordinates");
        Check(Inspect({0}, result) && result.images.empty() && result.fields.size() == 1,
            "Reused observation clears previous fields and images");

        auto shortHeader = zero; shortHeader.resize(20);
        auto shortPalette = Raw(1, 0, 16, 16); shortPalette.pop_back();
        auto shortDescriptor = zero; PutWord(shortDescriptor, 17, 1); shortDescriptor.resize(36);
        auto hugeCount = zero; PutWord(hugeCount, 17, 0xffffffffu);
        auto hugeData = Raw(1, 3, 1, 1, {{1}}); PutWord(hugeData, 33, 0xffffffffu);
        auto extra = zero; extra.push_back(0x80);
        for (const auto& raw : {shortHeader, shortPalette, shortDescriptor, hugeCount, hugeData, extra})
            Check(!Inspect(Section({{0, raw}}), result), "Incomplete or overflowing PS2 native record rejected by host guard");
        Check(!Inspect({}, result) && !Inspect({0, 0}, result), "Missing and trailing terminators rejected");
        auto noTerminator = Section({{0, zero}}); noTerminator.pop_back();
        Check(!Inspect(noTerminator, result) && result.images.size() == 1 && result.images[0].complete
            && !result.complete, "Completed image retained when outer section lacks terminator");
        Check(!Inspect({0xe0}, result) && result.finalPosition == 1,
            "Truncated common UInt32-size header cannot read physical suffix outside section");
        Check(!Inspect({0xff, 250, 0xff, 0xff, 0xff, 0xff, 0}, result)
            && result.fields.size() == 1 && !result.fields[0].complete,
            "Unknown field UInt32 extent overflow rejected before seek");

        ExtentOnlyStream stream; stream.position = 0xfffffff8u; stream.size = 0xffffffffu;
        std::string error;
        Check(!Serializer::InspectNativeSectionForAnalysis(stream, 8, result, &error)
            && !error.empty() && stream.reads == 0, "Input offset+size UInt32 overflow rejected before reads");
        stream.position = 0;
        Check(!Serializer::InspectNativeSectionForAnalysis(stream, Serializer::MaximumInspectionBytes + 1,
            result, &error) && stream.reads == 0, "Explicit16MiB input cap rejected before reads");
        stream.size = 10; stream.position = 9;
        Check(!Serializer::InspectNativeSectionForAnalysis(stream, 2, result, &error)
            && stream.reads == 0, "Input extent exceeds physical source before reads");

        // Each 0x31 is original fixed-size1 field17, then one payload byte.
        Bytes manyFields;
        for (std::uint32_t i = 0; i < Serializer::MaximumInspectionFields - 1; ++i)
            manyFields.insert(manyFields.end(), {0x31, 0xa5});
        manyFields.push_back(0);
        Check(Inspect(manyFields, result), "Exact host field-count limit includes terminator");
        manyFields.insert(manyFields.end() - 1, {0x31, 0xa5});
        Check(!Inspect(manyFields, result) && result.fields.size() == Serializer::MaximumInspectionFields,
            "Field-count overflow is an explicit bounded observation failure");
        const auto one = Section({{0, zero}});
        Bytes manyImages;
        for (std::uint32_t i = 0; i < Serializer::MaximumInspectionImages + 1; ++i)
            manyImages.insert(manyImages.end(), one.begin(), one.end() - 1);
        manyImages.push_back(0);
        Check(!Inspect(manyImages, result) && result.images.size() == Serializer::MaximumInspectionImages,
            "Image-count host cap bounds repeated native records");
    }

    void CaptureFixture(const char* inputPath, const char* outputPath)
    {
        std::ifstream input(inputPath, std::ios::binary | std::ios::ate);
        Check(bool(input), "Original PS2 fixture input");
        const auto count = input.tellg();
        Check(count == 235, "Expected exact DB noisesm native-section extent235");
        Bytes bytes(235); input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Check(bool(input), "Original PS2 fixture read");
        Observation observed;
        Check(Inspect(bytes, observed) && observed.fields.size() == 2 && observed.images.size() == 1,
            "Original noisesm section structure");
        const auto& image = observed.images[0];
        Check(image.nativeFlag == 1 && image.pixelFormat == 0 && image.width == 16 && image.height == 16
            && image.auxiliaryValue == 99590 && image.mipCount == 1 && image.paletteOffset == 26
            && image.paletteByteCount == 64 && image.mips.size() == 1, "Original noisesm header/palette facts");
        const auto& mip = image.mips[0];
        Check(mip.descriptor0 == 0 && mip.descriptor1 == 16 && mip.descriptor2 == 16
            && mip.dataSize == 128 && mip.descriptorOffset == 90 && mip.dataOffset == 106
            && observed.finalPosition == 235, "Original noisesm MIPS wire cursor facts");
        if (!outputPath) return;
        std::ofstream output(outputPath);
        output << "{\"metadataOnly\":true,\"runtimeOrGpuCompleted\":false,\"complete\":true,"
            << "\"inputSize\":" << observed.inputSize << ",\"finalPosition\":" << observed.finalPosition
            << ",\"fieldCount\":" << observed.fields.size() << ",\"imageCount\":" << observed.images.size()
            << ",\"nativeFlag\":" << unsigned(image.nativeFlag) << ",\"pixelFormat\":" << image.pixelFormat
            << ",\"width\":" << image.width << ",\"height\":" << image.height << ",\"auxiliaryValue\":" << image.auxiliaryValue
            << ",\"paletteOffset\":" << image.paletteOffset << ",\"paletteByteCount\":" << image.paletteByteCount
            << ",\"mipCount\":" << image.mipCount << ",\"descriptor\":[" << mip.descriptor0 << ','
            << mip.descriptor1 << ',' << mip.descriptor2 << ',' << mip.dataSize << "],\"descriptorOffset\":"
            << mip.descriptorOffset << ",\"dataOffset\":" << mip.dataOffset << "}\n";
        Check(bool(output), "PS2 metadata fixture capture output");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 || argc == 4)
        {
            Check(std::string(argv[1]) == "--fixture", "Expected --fixture INPUT [OUTPUT]");
            CaptureFixture(argv[2], argc == 4 ? argv[3] : nullptr);
        }
        else
        { Check(argc == 1, "Expected no arguments or --fixture INPUT [OUTPUT]"); Guards(); }
        std::cout << "PS2 texture metadata inspection: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << error.what() << '\n'; return 1; }
}
