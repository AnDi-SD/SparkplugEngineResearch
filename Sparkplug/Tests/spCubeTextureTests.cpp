#include "Code/SparkplugDX/spDXCubeTexture.h"
#include "Code/SparkplugDX/spDXCubeTextureSerializer.h"
#include "Code/SparkplugPS2/spPS2CubeTexture.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    int checks = 0;
    void Check(bool result, const char* text)
    { ++checks; if (!result) throw std::runtime_error(text); }
    template<class T> void Add(Bytes& bytes, T value)
    { const auto* p = reinterpret_cast<const std::uint8_t*>(&value); bytes.insert(bytes.end(), p, p + sizeof(value)); }
    void Open(spMemoryStream& stream, const Bytes& bytes = {})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())), "resize stream");
        if (!bytes.empty()) std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
        stream.SetLogicalOriginForAnalysis(0);
        Check(stream.Seek(spStream::SeekSource::essStart, 0), "rewind stream");
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size = 0; Check(stream.GetSize(&size), "stream extent");
        const auto* p = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return size ? Bytes(p, p + size) : Bytes{};
    }
    Bytes Fixture(std::uint32_t format, std::uint32_t dimension, std::uint8_t paletteFlag = 0)
    {
        Bytes bytes; Add(bytes, dimension); Add(bytes, dimension); Add(bytes, format); Add(bytes, paletteFlag);
        if (paletteFlag) for (unsigned i = 0; i < 1024; ++i) bytes.push_back(static_cast<std::uint8_t>(i ^ 0x6b));
        Add(bytes, spDXTexture::FullMipCountForAnalysis(dimension, dimension));
        for (unsigned face = 0; face < 6; ++face)
        {
            auto size = dimension;
            while (true)
            {
                spDXCubeTexture::MipForAnalysis mip;
                Check(spDXCubeTexture::DescribeSerializedMipForAnalysis(size, format, mip), "fixture mip extent");
                // Distinct markers distinguish face-major from mip-major order.
                for (unsigned i = 0; i < mip.rowBytes * mip.rows; ++i)
                    bytes.push_back(static_cast<std::uint8_t>((face * 31 + size * 7 + i) & 255));
                if (size == 1) break; size >>= 1;
            }
        }
        return bytes;
    }
    bool DeclaredMipBackend(void* context, std::uint32_t format, spDXCubeTexture::FacesForAnalysis& faces)
    {
        // Test-only declared backend output. This sentinel transform proves
        // the native attach order; it is not a reconstruction of D3DX math.
        ++*static_cast<unsigned*>(context);
        Check(spDXCubeTexture::IsSupportedRuntimeFormatForAnalysis(format), "backend receives attached raw native format");
        for (auto& face : faces)
            for (std::size_t level = 1; level < face.size(); ++level)
                for (auto& byte : face[level].packedBytes) byte ^= std::byte{0x5a};
        return true;
    }
    void Codec(std::uint32_t format, std::uint32_t dimension, std::uint8_t paletteFlag)
    {
        const auto bytes = Fixture(format, dimension, paletteFlag);
        spSerializerManager manager; spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager, resources);
        context.pcTexturePitchForAnalysis = [](void*, std::uint32_t, std::uint32_t row) noexcept { return row + 4; };
        spMemoryStream input; Open(input, bytes); spDXCubeTexture cube; unsigned backendCalls = 0;
        cube.SetName("cube-native-name");
        cube.SetMipRegeneratorForAnalysis(&backendCalls, &DeclaredMipBackend);
        spDXCubeTextureSerializer serializer; std::string error;
        Check(serializer.ReadPayloadForAnalysis(context, input, static_cast<std::uint32_t>(bytes.size()), cube, &error), error.c_str());
        Check(cube.GetWidthForAnalysis() == dimension && cube.GetHeightForAnalysis() == dimension, "native attach dimensions");
        Check(cube.HasInitializedRuntimeFormatForAnalysis() && cube.GetRuntimeFormatForAnalysis() == format, "native attached format");
        Check(cube.IsInitializedForAnalysis() && !cube.GetField31ForAnalysis(), "native attach initialized flag and field31");
        Check(cube.GetField18ForAnalysis() == 0 && cube.GetField1CForAnalysis() == 0 && cube.GetTextureFlagsForAnalysis() == 0, "runtime attach leaves initialization-mode fields alone");
        Check(bool(cube.GetPaletteForAnalysis()) == bool(paletteFlag), "all nonzero palette flags consume palette bytes");
        Check(serializer.IndexRelationshipsWithContextForAnalysis(manager, cube), "cube index boundary");
        Check(!cube.HasLiveGraphicsBackendForAnalysis(), "CPU storage is explicit host resource layer");
        Check(backendCalls == (dimension > 1 ? 1u : 0u), "multi-mip attach requires backend after complete six-face staged read");
        std::uint64_t expectedCount = 0;
        for (const auto& face : cube.GetFacesForAnalysis())
            for (const auto& mip : face)
                expectedCount += std::uint64_t((format == 0x31545844 || format == 0x33545844) ? mip.rowBytes : mip.physicalPitch) * mip.rows;
        Check(cube.GetNativeByteCountForAnalysis() == expectedCount, "native pitch-sensitive byte count");
        spMemoryStream output; Open(output);
        Check(serializer.WritePayloadForAnalysis(output, cube, &error), error.c_str());
        auto canonical = bytes; if (paletteFlag) canonical[12] = 1;
        if (dimension > 1)
        {
            std::size_t offset = 17 + (paletteFlag ? 1024 : 0);
            for (const auto& face : cube.GetFacesForAnalysis())
                for (std::size_t level = 0; level < face.size(); ++level)
                {
                    const auto size = face[level].packedBytes.size();
                    if (level) for (std::size_t i = 0; i < size; ++i) canonical[offset + i] ^= 0x5a;
                    offset += size;
                }
        }
        Check(Data(output) == canonical, "writer uses regenerated backend mip bytes, canonical palette flag and tight rows");
        spDXCubeTexture decoded; unsigned decodedBackendCalls = 0;
        decoded.SetMipRegeneratorForAnalysis(&decodedBackendCalls, &DeclaredMipBackend);
        Open(input, canonical);
        Check(serializer.ReadPayloadForAnalysis(context, input, static_cast<std::uint32_t>(canonical.size()), decoded, &error), error.c_str());
        Check(decoded.GetFacesForAnalysis()[5][0].packedBytes == cube.GetFacesForAnalysis()[5][0].packedBytes, "read/write/read preserves base face and reruns native-required mip regeneration");
        Check(decoded.GetNativeByteCountForAnalysis() == cube.GetNativeByteCountForAnalysis(), "read/write/read preserves attachment size");
        spCloneManager clones; auto clone = clones.Clone(cube);
        auto* typedClone = dynamic_cast<spDXCubeTexture*>(clone.get());
        Check(typedClone && !typedClone->IsInitializedForAnalysis() && !typedClone->HasInitializedRuntimeFormatForAnalysis()
            && typedClone->GetFacesForAnalysis()[0].empty() && !typedClone->GetPaletteForAnalysis(), "native clone copies Named prefix only; cube resources stay fresh");
        Check(typedClone && std::strcmp(typedClone->GetName(), cube.GetName()) == 0, "native clone retains Named string");
    }
    std::string Hex(const Bytes& bytes)
    {
        std::ostringstream out; out << std::hex << std::setfill('0');
        for (const auto byte : bytes) out << std::setw(2) << static_cast<unsigned>(byte);
        return out.str();
    }
    void CapturedCodec(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        const Bytes bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Check(file.good() || file.eof(), "read captured cube fixture");
        spSerializerManager manager; spResourceManager resources; spSerializerReadContextForAnalysis context(manager, resources);
        context.pcTexturePitchForAnalysis = [](void*, std::uint32_t, std::uint32_t row) noexcept { return row + 4; };
        spMemoryStream input; Open(input, bytes); spDXCubeTexture cube; spDXCubeTextureSerializer serializer; std::string error;
        Check(serializer.ReadPayloadForAnalysis(context, input, static_cast<std::uint32_t>(bytes.size()), cube, &error), error.c_str());
        Check(cube.GetFacesForAnalysis()[0].size() == 1, "whole native qualification is restricted to one mip");
        spMemoryStream output; Open(output); Check(serializer.WritePayloadForAnalysis(output, cube, &error), error.c_str());
        std::cout << "{\"nativeFormat\":" << cube.GetRuntimeFormatForAnalysis()
            << ",\"nativeByteCount\":" << cube.GetNativeByteCountForAnalysis()
            << ",\"output\":\"" << Hex(Data(output)) << "\"}\n";
    }
    void CapturedRowLayout(std::uint32_t formatIndex, std::uint32_t size)
    {
        Check(formatIndex < spDXCubeTexture::NativeRuntimeFormatsForAnalysis.size(), "explicit native row format");
        const auto format = spDXCubeTexture::NativeRuntimeFormatsForAnalysis[formatIndex];
        spDXCubeTexture::MipForAnalysis mip;
        Check(spDXCubeTexture::DescribeSerializedMipForAnalysis(size, format, mip), "native row descriptor");
        const auto count = std::uint64_t(formatIndex < 2 ? mip.rowBytes : mip.rowBytes + 4) * mip.rows;
        std::cout << "{\"rowBytes\":" << mip.rowBytes << ",\"rows\":" << mip.rows
            << ",\"nativeByteCount\":" << count << "}\n";
    }
    void FullReferenceRoundTrip()
    {
        spSerializerManager manager; spResourceManager resources;
        Check(manager.RegisterForAnalysis(spDXCubeTexture::ClassID, std::make_shared<spDXCubeTextureSerializer>(), 1, 3), "cube serializer registration");
        spSerializerReadContextForAnalysis directContext(manager, resources);
        spDXCubeTexture source; source.SetName("cube-file-name");
        spMemoryStream stream; const auto payload = Fixture(0x15, 1); Open(stream, payload); std::string error;
        Check(spDXCubeTextureSerializer{}.ReadPayloadForAnalysis(directContext, stream, static_cast<std::uint32_t>(payload.size()), source, &error), error.c_str());
        manager.SetDispatchContextForAnalysis(1, spSerializerManager::OperationSave); Bytes file;
        Check(manager.BuildResourceFileForAnalysis(source, file, 0, 4096, &error), error.c_str());
        Open(stream, file); spSerializerReadContextForAnalysis context(manager, resources);
        auto* loaded = dynamic_cast<spDXCubeTexture*>(manager.LoadResourcesForAnalysis(stream, context, &error));
        Check(loaded && !context.failed && context.createdObjects.size() == 1, "whole FFPS loader creates the registered cube leaf");
        Check(std::strcmp(loaded->GetName(), source.GetName()) == 0 && loaded->GetRuntimeFormatForAnalysis() == 0x15,
            "FAT name and raw surface format survive whole file load");
        Check(loaded->GetFacesForAnalysis()[5][0].packedBytes == source.GetFacesForAnalysis()[5][0].packedBytes,
            "whole loader retains sixth face ownership");
        manager.SetDispatchContextForAnalysis(1, spSerializerManager::OperationSave); Bytes saved;
        Check(manager.BuildResourceFileForAnalysis(*loaded, saved, 0, 4096, &error) && saved == file,
            "whole file save repeats canonical header/FAT/reference/payload bytes");
        spSerializerManager secondManager; spResourceManager secondResources;
        Check(secondManager.RegisterForAnalysis(spDXCubeTexture::ClassID, std::make_shared<spDXCubeTextureSerializer>(), 1, 3), "fresh reread registration");
        Open(stream, saved); spSerializerReadContextForAnalysis secondContext(secondManager, secondResources);
        auto* reread = dynamic_cast<spDXCubeTexture*>(secondManager.LoadResourcesForAnalysis(stream, secondContext, &error));
        Check(reread && !secondContext.failed && reread->GetFacesForAnalysis()[5][0].packedBytes == source.GetFacesForAnalysis()[5][0].packedBytes,
            "whole read/save/read completes with independently owned cube faces");
        auto retained = std::dynamic_pointer_cast<spDXCubeTexture>(secondContext.ShareObjectForAnalysis(reread));
        Check(retained && retained->GetFacesForAnalysis()[0][0].packedBytes == source.GetFacesForAnalysis()[0][0].packedBytes,
            "context exposes shared root ownership");
    }
    void RejectMalformed()
    {
        const auto valid = Fixture(0x15, 2);
        // Every byte-prefix is incomplete, including final-face/final-mip cuts.
        for (std::size_t length = 0; length < valid.size(); ++length)
        {
            spSerializerManager manager; spResourceManager resources; spSerializerReadContextForAnalysis context(manager, resources);
            spMemoryStream stream; Open(stream, Bytes(valid.begin(), valid.begin() + length));
            spDXCubeTexture cube; std::string error;
            Check(!spDXCubeTextureSerializer{}.ReadPayloadForAnalysis(context, stream, static_cast<std::uint32_t>(length), cube, &error)
                && context.failed && !cube.IsInitializedForAnalysis() && !error.empty(), "truncated whole cube transaction rejected without fabricated partial object");
        }
        for (unsigned mode = 0; mode < 6; ++mode)
        {
            auto bytes = valid;
            if (mode == 0) bytes[4] = 1;       // Non-square header.
            if (mode == 1) bytes[8] = 8;       // Unknown runtime format.
            if (mode == 2) bytes[13] = 1;      // Partial chain.
            if (mode == 3) bytes.push_back(0); // Trailing payload.
            if (mode == 4) bytes[0] = 3;       // Unsupported non-power-of-two.
            spSerializerManager manager; spResourceManager resources; spSerializerReadContextForAnalysis context(manager, resources);
            if (mode == 5) context.pcTexturePitchForAnalysis = [](void*, std::uint32_t, std::uint32_t row) noexcept { return row - 1; };
            spMemoryStream stream; Open(stream, bytes); spDXCubeTexture cube; std::string error;
            Check(!spDXCubeTextureSerializer{}.ReadPayloadForAnalysis(context, stream, static_cast<std::uint32_t>(bytes.size()), cube, &error)
                && context.failed && !cube.IsInitializedForAnalysis(), "host unsupported/malformed cube extent rejected");
        }
        spDXCubeTexture empty; spMemoryStream stream; Open(stream); std::string error;
        Check(!spDXCubeTextureSerializer{}.WritePayloadForAnalysis(stream, empty, &error) && Data(stream).empty(), "fresh cube has no invented serialized resource");
        spSerializerManager manager; spResourceManager resources; spSerializerReadContextForAnalysis context(manager, resources);
        Open(stream, valid); spDXCubeTexture unsupported;
        Check(!spDXCubeTextureSerializer{}.ReadPayloadForAnalysis(context, stream, static_cast<std::uint32_t>(valid.size()), unsupported, &error)
            && context.failed && !unsupported.IsInitializedForAnalysis(), "multi-mip success requires supplied backend; no filtering default fabricated");
    }
    void PS2State()
    {
        auto created = spRTTIManager::Instance().Create(spPS2CubeTexture::ClassID);
        auto* cube = dynamic_cast<spPS2CubeTexture*>(created.get());
        Check(cube && cube->IsKindOf(spCubeTexture::ClassID), "PS2 cube concrete factory/base");
        Check(!cube->NativeHook35B0ForAnalysis() && !cube->NativeHook35C0ForAnalysis()
            && cube->NativeHook35E0ForAnalysis(), "original PS2 backend false/false/true stubs");
        cube->NativeHook35D0ForAnalysis();
        Check(cube->InitCubeBufferStateForAnalysis(3, 5, 7, 0x9a, true), "PS2 primary48 original success stub");
        Check(cube->GetWidthForAnalysis() == 4 && cube->GetHeightForAnalysis() == 8
            && cube->GetField18ForAnalysis() == 7 && cube->GetTextureFlagsForAnalysis() == 0x9a
            && cube->GetField1CForAnalysis() == 0 && !cube->WereDimensionsUnchangedForAnalysis()
            && cube->IsInitializedForAnalysis() && !cube->HasBackendCubeForAnalysis(), "PS2 shared state initialization succeeds without backend storage");
        Check(cube->InitCubeBufferStateForAnalysis(4, 8, 0xab, 0, true)
            && cube->WereDimensionsUnchangedForAnalysis(), "PS2 normalization equality byte");
        Check(cube->InitCubeBufferStateForAnalysis(9, 11, 0xc, 0x72, false)
            && cube->GetWidthForAnalysis() == 9 && cube->GetHeightForAnalysis() == 11
            && cube->WereDimensionsUnchangedForAnalysis(), "disabled normalization keeps dimensions and earlier equality byte");
        cube->SetName("ps2-cube-native-name"); spCloneManager clones; auto clone = clones.Clone(*cube);
        const auto* typed = dynamic_cast<spPS2CubeTexture*>(clone.get());
        Check(typed && std::strcmp(typed->GetName(), cube->GetName()) == 0
            && !typed->IsInitializedForAnalysis() && typed->GetField18ForAnalysis() == 0,
            "PS2 Named clone copies string and constructor-fresh texture state");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--native-codec") { CapturedCodec(argv[2]); return 0; }
        if (argc == 4 && std::string(argv[1]) == "--native-rows")
        { CapturedRowLayout(static_cast<std::uint32_t>(std::stoul(argv[2])), static_cast<std::uint32_t>(std::stoul(argv[3]))); return 0; }
        Check(spCubeTexture::StaticRTTI().baseClassID == spTexture::ClassID
            && !spCubeTexture::StaticRTTI().factory, "abstract cube RTTI parent and null factory");
        Check(spDXCubeTextureSerializer::StaticRTTI().baseClassID == spDXTextureSerializer::ClassID, "cube serializer keeps physical/registration DX serializer parent");
        auto cube = spRTTIManager::Instance().Create(spDXCubeTexture::ClassID);
        Check(cube && cube->IsKindOf(spCubeTexture::ClassID), "concrete cube factory resolves base chain");
        Check(!spRTTIManager::Instance().Create(spCubeTexture::ClassID), "abstract cube factory stays absent");
        auto serializer = spRTTIManager::Instance().Create(spDXCubeTextureSerializer::ClassID);
        Check(serializer && serializer->IsKindOf(spDXTextureSerializer::ClassID), "cube serializer factory resolves DX serializer parent");
        for (const auto format : spDXCubeTexture::NativeRuntimeFormatsForAnalysis)
            for (const auto dimension : {1u, 2u, 4u}) Codec(format, dimension, 0);
        for (const auto flag : {1u, 2u, 255u}) Codec(0x29, 2, static_cast<std::uint8_t>(flag));
        spDXCubeTexture::MipForAnalysis dxt3, dxt5;
        Check(spDXCubeTexture::DescribeSerializedMipForAnalysis(4, 0x33545844, dxt3)
            && spDXCubeTexture::DescribeSerializedMipForAnalysis(4, 0x35545844, dxt5)
            && dxt3.rows == 1 && dxt5.rows == 4 && dxt5.rowBytes == 16, "original DXT5 row-count omission is preserved");
        FullReferenceRoundTrip();
        RejectMalformed();
        PS2State();
        std::cout << "PASS " << checks << "/" << checks << ": cube texture runtime codec, six-face ownership, native clone and bounds\n";
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL after " << checks << ": " << error.what() << '\n'; return 1; }
}
