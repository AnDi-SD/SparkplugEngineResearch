#include "Analysis/PC/spMaterialMovieTextureAbi.h"
#include "Code/Sparkplug/spMaterialMovieTexture.h"
#include "Code/Sparkplug/spMaterialTexture.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using sparkplug::reconstruction::spMaterialMovieTexture;
    using sparkplug::reconstruction::spMaterialTexture;
    namespace Abi = sparkplug::evidence::pc;
    using Material = spMaterialMovieTexture::CapturedPcMaterialForAnalysis;
    using Resource = spMaterialMovieTexture::OpaqueResourcePrefixForAnalysis;

    static_assert(spMaterialMovieTexture::ClassID == Abi::MaterialMovieTextureClassId);
    static_assert(spMaterialTexture::ClassID == Abi::MaterialMovieTextureRegisteredBase);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    void PutWord(std::uint8_t* bytes, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(value >> (8 * i));
    }

    Material MakeMaterial(std::uint32_t address, std::uint8_t seed)
    {
        Material material{};
        for (std::size_t i = 0; i < material.size(); ++i)
            material[i] = std::uint8_t(seed + i * 37);
        PutWord(material.data(), 0x006EA94C);
        PutWord(material.data() + 0x68, address);
        return material;
    }

    Resource MakeResource(std::uint32_t word, std::uint8_t seed)
    {
        Resource resource{};
        for (std::size_t i = 0; i < resource.size(); ++i)
            resource[i] = std::uint8_t(seed + i * 13);
        PutWord(resource.data() + 0x18, word);
        return resource;
    }

    std::uint32_t ParseWord(std::string_view line)
    {
        if (line.size() != 8)
            throw std::runtime_error("expected eight raw resource word digits");
        std::uint32_t word{};
        const auto end = line.data() + line.size();
        const auto parsed = std::from_chars(line.data(), end, word, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("expected hexadecimal input");
        return word;
    }

    int Replay()
    {
        constexpr std::uint32_t capturedResourceAddress = 0x31000400;
        const auto material = MakeMaterial(capturedResourceAddress, 0xA5);
        std::string line;
        while (std::getline(std::cin, line))
        {
            const auto resource = MakeResource(ParseWord(line), 0x5A);
            std::uint32_t actual{};
            Require(spMaterialMovieTexture::ReadPcResourceWordForAnalysis(material,
                {capturedResourceAddress, resource}, actual), "qualified replay input was refused");
            std::cout << std::hex << std::setfill('0') << std::setw(8) << actual << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckQueries()
    {
        constexpr std::array<std::uint32_t, 8> words{
            0x00000000, 0x00000001, 0x7FFFFFFF, 0xFFFFFFFF,
            0x80000000, 0x7FC12345, 0x01020304, 0xA5A5A5A5,
        };
        std::uint32_t address = 0x31000400;
        for (const auto word : words)
        {
            auto material = MakeMaterial(address, std::uint8_t(address >> 10));
            auto resource = MakeResource(word, std::uint8_t(word));
            const auto materialBefore = material;
            const auto resourceBefore = resource;
            std::uint32_t actual = ~word;
            Require(spMaterialMovieTexture::ReadPcResourceWordForAnalysis(material, {address, resource}, actual)
                && actual == word, "query failed exact DWORD readback");
            Require(material == materialBefore && resource == resourceBefore, "query changed a captured prefix");
            address += 0x400;
        }
    }

    void CheckRefusals()
    {
        constexpr std::uint32_t address = 0x31000400;
        const auto resource = MakeResource(0x10203040, 0xC5);
        const auto verify = [&](const Material& material, std::uint32_t resourceAddress)
        {
            std::uint32_t output = 0xABCD0123;
            Require(!spMaterialMovieTexture::ReadPcResourceWordForAnalysis(material,
                {resourceAddress, resource}, output) && output == 0xABCD0123,
                "unqualified binding changed output");
        };

        auto foreignPrimary = MakeMaterial(address, 0xFF);
        PutWord(foreignPrimary.data(), 0x006ECB68);
        verify(foreignPrimary, address);
        verify(MakeMaterial(address, 0xFA), address + 4u);
        verify(MakeMaterial(0, 0xEC), 0);
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        if (argc != 1)
            return 2;
        CheckQueries();
        CheckRefusals();
        std::cout << "MaterialMovieTextureResource: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
