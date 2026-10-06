#include "spDXCubeTextureSerializer.h"
#include "spDXCubeTexture.h"
#include "../SparkBase/spStream.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spDXCubeTextureSerializer>(); }
        const spRTTIRecord Record{spDXCubeTextureSerializer::ClassID, spDXTextureSerializer::ClassID,
            "spDXCubeTextureSerializer", &spDXTextureSerializer::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spDXCubeTextureSerializer::spDXCubeTextureSerializer() noexcept
    { (void)spDXCubeTexture::StaticRTTI(); }
    const spRTTIRecord& spDXCubeTextureSerializer::StaticRTTI() noexcept
    { (void)Registered; return Record; }
    const spRTTIRecord& spDXCubeTextureSerializer::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spDXCubeTextureSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spDXCubeTextureSerializer>(); manager.RegisterCloneForAnalysis(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spDXCubeTextureSerializer::IndexRelationshipsWithContextForAnalysis(
        spSerializerManager&, spBaseObject& object) const
    { return object.IsExactly(spDXCubeTexture::ClassID); }
    bool spDXCubeTextureSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream, std::uint32_t byteCount, spBaseObject& object, std::string* error) const
    {
        if (error) error->clear();
        const auto fail = [&](const char* text) { context.failed = true; if (error) *error = text; return false; };
        auto* target = dynamic_cast<spDXCubeTexture*>(&object);
        if (context.failed || !target || !object.IsExactly(spDXCubeTexture::ClassID)
            || byteCount > 16u * 1024u * 1024u) return fail("Invalid or oversized runtime cube texture target");
        auto remaining = byteCount;
        const auto read = [&](void* destination, std::uint32_t count) {
            if (count > remaining || !stream.ReadData(destination, count)) return false;
            remaining -= count; return true;
        };
        std::uint32_t width = 0, height = 0, format = 0, count = 0; std::uint8_t hasPalette = 0;
        if (!read(&width, 4) || !read(&height, 4) || !read(&format, 4) || !read(&hasPalette, 1))
            return fail("Truncated runtime cube texture header");
        std::unique_ptr<spPalette> palette;
        // PC004B8BC3 tests nonzero, unlike the ordinary DX texture codec.
        if (hasPalette)
        {
            std::array<std::byte, spPalette::EntryByteCount> entries;
            if (!read(entries.data(), static_cast<std::uint32_t>(entries.size())))
                return fail("Truncated runtime cube palette");
            palette = std::make_unique<spPalette>();
            if (!palette->SetEntriesForAnalysis(entries.data(), entries.size())) return fail("Invalid runtime cube palette");
        }
        if (!width || width != height || width > 65535 || (width & (width - 1)) || !spDXCubeTexture::IsSupportedRuntimeFormatForAnalysis(format)
            || !read(&count, 4) || count != spDXTexture::FullMipCountForAnalysis(width, height))
            return fail("Only complete power-of-two square runtime cube chains are restored");
        spDXCubeTexture::FacesForAnalysis faces;
        for (auto& face : faces)
        {
            auto dimension = width;
            for (std::uint32_t level = 0; level < count; ++level)
            {
                spDXCubeTexture::MipForAnalysis mip;
                if (!spDXCubeTexture::DescribeSerializedMipForAnalysis(dimension, format, mip))
                    return fail("Invalid runtime cube mip layout");
                const auto size = std::uint64_t(mip.rowBytes) * mip.rows;
                if (size > remaining) return fail("Truncated runtime cube face/mip bytes");
                if (context.pcTexturePitchForAnalysis)
                    mip.physicalPitch = context.pcTexturePitchForAnalysis(context.pcTexturePitchContext, level, mip.rowBytes);
                if (mip.physicalPitch < mip.rowBytes || std::uint64_t(mip.physicalPitch) * mip.rows > 16u * 1024u * 1024u)
                    return fail("Invalid declared runtime cube pitch");
                mip.packedBytes.resize(static_cast<std::size_t>(size));
                if (!read(mip.packedBytes.data(), static_cast<std::uint32_t>(size))) return fail("Cannot read runtime cube mip bytes");
                face.push_back(std::move(mip)); dimension = std::max(1u, dimension >> 1);
            }
        }
        if (remaining || !target->InitializeRuntimeFacesForAnalysis(width, format, std::move(faces)))
            return fail("Invalid runtime cube extent/state");
        target->AdoptPaletteForAnalysis(std::move(palette)); return true;
    }
    bool spDXCubeTextureSerializer::WritePayloadForAnalysis(
        spStream& stream, const spBaseObject& object, std::string* error) const
    {
        if (error) error->clear();
        const auto fail = [&](const char* text) { if (error) *error = text; return false; };
        const auto* target = dynamic_cast<const spDXCubeTexture*>(&object);
        if (!target || !object.IsExactly(spDXCubeTexture::ClassID) || !target->IsInitializedForAnalysis()
            || !target->HasInitializedRuntimeFormatForAnalysis() || target->GetFacesForAnalysis()[0].empty())
            return fail("Uninitialized runtime cube texture");
        const auto* palette = target->GetPaletteForAnalysis();
        if (palette && !palette->HasInitializedEntriesForAnalysis()) return fail("Uninitialized runtime cube palette entries");
        const std::uint32_t header[]{target->GetWidthForAnalysis(), target->GetHeightForAnalysis(), target->GetRuntimeFormatForAnalysis()};
        const std::uint8_t hasPalette = palette ? 1 : 0;
        const auto count = static_cast<std::uint32_t>(target->GetFacesForAnalysis()[0].size());
        if (!stream.WriteData(header, sizeof(header)) || !stream.WriteData(&hasPalette, 1)) return fail("Cannot write runtime cube header");
        if (palette && !stream.WriteData(palette->GetEntriesForAnalysis().data(), spPalette::EntryByteCount)) return fail("Cannot write runtime cube palette");
        if (!stream.WriteData(&count, 4)) return fail("Cannot write runtime cube mip count");
        for (const auto& face : target->GetFacesForAnalysis())
            for (const auto& mip : face)
                if (!stream.WriteData(mip.packedBytes.data(), static_cast<std::uint32_t>(mip.packedBytes.size())))
                    return fail("Cannot write runtime cube face/mip bytes");
        return true;
    }
}
