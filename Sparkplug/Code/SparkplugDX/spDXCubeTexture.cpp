#include "spDXCubeTexture.h"
#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        constexpr std::uint64_t StorageLimit = 16u * 1024u * 1024u;
        std::unique_ptr<spBaseObject> Create() { return std::make_unique<spDXCubeTexture>(); }
        const spRTTIRecord Record{spDXCubeTexture::ClassID, spCubeTexture::ClassID,
            "spDXCubeTexture", &spCubeTexture::StaticRTTI(), &Create, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXCubeTexture::StaticRTTI() noexcept
    { (void)Registered; return Record; }
    const spRTTIRecord& spDXCubeTexture::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spDXCubeTexture::vfunc_10(spCloneManager& manager) const
    {
        // PC004B9460 dispatches inherited00413120: only the Named prefix is
        // copied. Texture/surface/palette state stays in the new ctor state.
        auto clone = std::make_unique<spDXCubeTexture>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        return spNamedObject::vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    bool spDXCubeTexture::IsSupportedRuntimeFormatForAnalysis(std::uint32_t format) noexcept
    { return std::find(NativeRuntimeFormatsForAnalysis.begin(), NativeRuntimeFormatsForAnalysis.end(), format)
        != NativeRuntimeFormatsForAnalysis.end(); }
    bool spDXCubeTexture::DescribeSerializedMipForAnalysis(
        std::uint32_t dimension, std::uint32_t format, MipForAnalysis& mip) noexcept
    {
        const auto found = std::find(NativeRuntimeFormatsForAnalysis.begin(), NativeRuntimeFormatsForAnalysis.end(), format);
        if (found == NativeRuntimeFormatsForAnalysis.end()) return false;
        // HOST shares the ordinary layout algorithm after explicit raw-format
        // lookup; it does not change the raw word in the cube file/state.
        const auto layoutFormat = static_cast<std::uint32_t>(found - NativeRuntimeFormatsForAnalysis.begin());
        if (!spDXTexture::DescribeMipForAnalysis(dimension, dimension, layoutFormat, mip)) return false;
        if (format == 0x35545844) mip.rows = dimension; // Original DXT5 row-count branch.
        return std::uint64_t(mip.rowBytes) * mip.rows <= StorageLimit;
    }
    bool spDXCubeTexture::InitializeRuntimeFacesForAnalysis(
        std::uint32_t dimension, std::uint32_t format, FacesForAnalysis faces)
    {
        // HOST bounds around device-free storage. Native CreateCubeTexture
        // obtains square surfaces from width and a complete chain (levels0).
        if (!dimension || dimension > 65535 || (dimension & (dimension - 1)) || !IsSupportedRuntimeFormatForAnalysis(format)) return false;
        const auto count = spDXTexture::FullMipCountForAnalysis(dimension, dimension);
        if (count > 1 && !mipRegenerator_) return false;
        std::uint64_t nativeBytes = 0, storageBytes = 0;
        for (const auto& face : faces)
        {
            if (face.size() != count) return false;
            auto size = dimension;
            for (const auto& mip : face)
            {
                MipForAnalysis expected;
                if (!DescribeSerializedMipForAnalysis(size, format, expected)
                    || mip.width != size || mip.height != size
                    || mip.rowBytes != expected.rowBytes || mip.rows != expected.rows
                    || mip.physicalPitch < mip.rowBytes
                    || mip.packedBytes.size() != std::uint64_t(mip.rowBytes) * mip.rows) return false;
                // PC004B8874 advances by pitch; byte count uses pitch*height
                // except its DXT1/DXT3/DXT4 branch, which uses packed rows.
                nativeBytes += std::uint64_t((format == 0x31545844 || format == 0x33545844) ? mip.rowBytes : mip.physicalPitch) * mip.rows;
                storageBytes += std::uint64_t(mip.physicalPitch) * mip.rows;
                if (nativeBytes > StorageLimit || storageBytes > StorageLimit) return false;
                size = std::max(1u, size >> 1);
            }
        }
        if (count > 1)
        {
            // Native attach stores48 before regenerating mip surfaces. Keep
            // that count; the supplied backend owns filtering math and errors.
            const auto descriptors = faces;
            if (!mipRegenerator_(mipContext_, format, faces)) return false;
            for (std::size_t face = 0; face < FaceCount; ++face)
            {
                if (faces[face].size() != count) return false;
                for (std::size_t level = 0; level < count; ++level)
                {
                    const auto& before = descriptors[face][level];
                    const auto& after = faces[face][level];
                    if (before.width != after.width || before.height != after.height
                        || before.rowBytes != after.rowBytes || before.rows != after.rows
                        || before.physicalPitch != after.physicalPitch
                        || before.packedBytes.size() != after.packedBytes.size()) return false;
                    if (level == 0 && before.packedBytes != after.packedBytes) return false;
                }
            }
        }
        ApplyRuntimeAttachmentStateForAnalysis(dimension, dimension);
        runtimeFormat_ = format; formatInitialized_ = true;
        byteCount_ = static_cast<std::uint32_t>(nativeBytes); faces_ = std::move(faces);
        return true;
    }
    void spDXCubeTexture::AdoptPaletteForAnalysis(std::unique_ptr<spPalette> palette) noexcept
    { palette_ = std::move(palette); }
}
