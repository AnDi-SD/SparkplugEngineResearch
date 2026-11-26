#include "spMaterialMovieTexture.h"
#include "../../Analysis/PC/spMaterialMovieTextureAbi.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace Abi = sparkplug::evidence::pc;

        static_assert(Abi::MaterialMovieTextureResourceLink + 4
            == spMaterialMovieTexture::CapturedPcMaterialForAnalysis{}.size());
        static_assert(Abi::MaterialMovieTextureResourceWord + 4
            == spMaterialMovieTexture::OpaqueResourcePrefixForAnalysis{}.size());

        std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
        {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
    }

    bool spMaterialMovieTexture::ReadPcResourceWordForAnalysis(const CapturedPcMaterialForAnalysis& material,
        const BorrowedResourceForAnalysis& resource, std::uint32_t& output) noexcept
    {
        if (ReadWord(material.data()) != Abi::MaterialMovieTexturePrimary || resource.capturedAddress == 0
            || ReadWord(material.data() + Abi::MaterialMovieTextureResourceLink) != resource.capturedAddress)
            return false;

        output = ReadWord(resource.prefix.data() + Abi::MaterialMovieTextureResourceWord);
        return true;
    }
} // namespace sparkplug::reconstruction
