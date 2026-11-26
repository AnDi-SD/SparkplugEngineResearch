#include "Analysis/PC/spDXEnvironmentMapLayerAbi.h"
#include "Code/SparkplugDX/spDXEnvironmentMapLayer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = sparkplug::evidence::pc;
    using Layer = sparkplug::reconstruction::spDXEnvironmentMapLayer;
    using Prefix = Layer::CapturedPcPrefixForAnalysis;
    using Word = std::array<std::uint8_t, Abi::DXEnvironmentMapLayerOwnWordBytes>;

    constexpr std::string_view ArchiveSource{"780b6f00000000000000cccc000000008000003100000000"};
    constexpr std::string_view ArchiveDestination{"780b6f00000000000000cccc00000000a0010031eac07e55"};
    constexpr auto Offset = Abi::DXEnvironmentMapLayerOwnWordOffset;

    struct CaptureWitness
    {
        std::string_view source;
        std::string_view before;
        std::string_view after;
    };

    constexpr CaptureWitness FreshWitnesses[]{
        {"780b6f005366798c9fb2c5d8ebfe11248000003104000000", "780b6f00859cb3cae1f80f263d546b82a0010031ffffffff",
         "780b6f00859cb3cae1f80f263d546b82a001003104000000"},
        {"780b6f00708396a9bccfe2f5081b2e418000003100000000", "780b6f00aac1d8ef061d344b627990a7a001003100000000",
         "780b6f00aac1d8ef061d344b627990a7a001003100000000"},
        {"780b6f008da0b3c6d9ecff1225384b5e8000003100000080", "780b6f00cfe6fd142b425970879eb5cca001003100000080",
         "780b6f00cfe6fd142b425970879eb5cca001003100000080"},
        {"780b6f00aabdd0e3f6091c2f4255687b80000031ffffffff", "780b6f00f40b223950677e95acc3daf1a001003100ffffff",
         "780b6f00f40b223950677e95acc3daf1a0010031ffffffff"},
        {"780b6f00c7daed001326394c5f7285988000003167452301", "780b6f001930475e758ca3bad1e8ff16a0010031efcdab89",
         "780b6f001930475e758ca3bad1e8ff16a001003167452301"},
        {"780b6f00e4f70a1d304356697c8fa2b58000003189abcdef", "780b6f003e556c839ab1c8dff60d243ba001003167452301",
         "780b6f003e556c839ab1c8dff60d243ba001003189abcdef"},
        {"780b6f000114273a4d60738699acbfd28000003100000100", "780b6f00637a91a8bfd6ed041b324960a001003100000000",
         "780b6f00637a91a8bfd6ed041b324960a001003100000100"},
        {"780b6f001e3144576a7d90a3b6c9dcef800000315a5aa5a5", "780b6f00889fb6cde4fb122940576e85a0010031a5a55a5a",
         "780b6f00889fb6cde4fb122940576e85a00100315a5aa5a5"},
    };

    struct ArchiveWitness
    {
        std::string_view sourceWord;
        std::string_view cloneWord;
    };

    constexpr ArchiveWitness ArchivedWitnesses[]{
        {"04000000", "04000000"},
        {"00000000", "00000000"},
        {"00000080", "00000080"},
        {"ffffffff", "ffffffff"},
    };

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    unsigned HexNibble(char digit)
    {
        if (digit >= '0' && digit <= '9')
            return static_cast<unsigned>(digit - '0');
        if (digit >= 'a' && digit <= 'f')
            return static_cast<unsigned>(digit - 'a' + 10);
        throw std::runtime_error("expected lowercase hex");
    }

    template <std::size_t Size>
    std::array<std::uint8_t, Size> Decode(std::string_view text)
    {
        Require(text.size() == 2 * Size, "unexpected hex width");
        std::array<std::uint8_t, Size> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<std::uint8_t>((HexNibble(text[2 * i]) << 4) | HexNibble(text[2 * i + 1]));
        return bytes;
    }

    template <std::size_t Size>
    std::string Encode(const std::array<std::uint8_t, Size>& bytes)
    {
        constexpr char Digits[] = "0123456789abcdef";
        std::string text(2 * Size, '0');
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            text[2 * i] = Digits[bytes[i] >> 4];
            text[2 * i + 1] = Digits[bytes[i] & 0x0F];
        }
        return text;
    }

    void SetWord(Prefix& capture, const Word& word)
    {
        std::copy(word.begin(), word.end(), capture.begin() + Offset);
    }

    int Replay(bool archived)
    {
        for (std::string line; std::getline(std::cin, line);)
        {
            std::istringstream row(line);
            std::string sourceText, destinationText, extra;
            Require(static_cast<bool>(row >> sourceText), "missing source bytes");
            if (archived)
            {
                Require(!(row >> extra), "unexpected archive input field");
                auto source = Decode<Prefix{}.size()>(ArchiveSource);
                auto destination = Decode<Prefix{}.size()>(ArchiveDestination);
                SetWord(source, Decode<Word{}.size()>(sourceText));
                Require(Layer::TryCopyPcPayloadForAnalysis(source, destination), "archive row refused");
                Word copied{};
                std::copy_n(destination.begin() + Offset, copied.size(), copied.begin());
                std::cout << Encode(copied) << '\n';
            }
            else
            {
                Require(static_cast<bool>(row >> destinationText) && !(row >> extra), "expected two PC prefixes");
                auto source = Decode<Prefix{}.size()>(sourceText);
                auto destination = Decode<Prefix{}.size()>(destinationText);
                Require(Layer::TryCopyPcPayloadForAnalysis(source, destination), "fresh row refused");
                std::cout << Encode(source) << ' ' << Encode(destination) << '\n';
            }
        }
        return std::cin.eof() ? 0 : 1;
    }

    int RunChecks()
    {
        unsigned checks = 0;
        const auto check = [&](bool condition, const char* message)
        {
            Require(condition, message);
            ++checks;
        };

        for (const auto& fixture : FreshWitnesses)
        {
            auto source = Decode<Prefix{}.size()>(fixture.source);
            auto destination = Decode<Prefix{}.size()>(fixture.before);
            check(Layer::TryCopyPcPayloadForAnalysis(source, destination), "fresh fixture refused");
            check(Encode(source) == fixture.source, "source prefix changed");
            check(Encode(destination) == fixture.after, "destination prefix differs");
        }

        for (const auto& fixture : ArchivedWitnesses)
        {
            auto source = Decode<Prefix{}.size()>(ArchiveSource);
            auto destination = Decode<Prefix{}.size()>(ArchiveDestination);
            auto expected = destination;
            SetWord(source, Decode<Word{}.size()>(fixture.sourceWord));
            SetWord(expected, Decode<Word{}.size()>(fixture.cloneWord));
            check(Layer::TryCopyPcPayloadForAnalysis(source, destination), "archived fixture refused");
            check(destination == expected, "archive word or surrounding bytes differ");
        }

        const auto valid = Decode<Prefix{}.size()>(FreshWitnesses[0].source);
        auto wrongSource = valid;
        wrongSource[0] ^= 1;
        auto destination = Decode<Prefix{}.size()>(FreshWitnesses[0].before);
        const auto untouched = destination;
        check(!Layer::TryCopyPcPayloadForAnalysis(wrongSource, destination), "foreign source primary admitted");
        check(destination == untouched, "bad source changed destination");
        destination[3] ^= 1;
        const auto wrongDestination = destination;
        check(!Layer::TryCopyPcPayloadForAnalysis(valid, destination), "foreign destination primary admitted");
        check(destination == wrongDestination, "bad destination changed");
        auto aliased = valid;
        check(!Layer::TryCopyPcPayloadForAnalysis(aliased, aliased), "aliased captures admitted");
        check(aliased == valid, "refused alias changed");
        std::cout << "DXEnvironmentMapLayerCopyPayload: " << checks << " checks passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunChecks();

        Require(argc == 2, "expected --replay or --replay-archive");
        const std::string_view mode(argv[1]);
        Require(mode == "--replay" || mode == "--replay-archive", "unexpected mode");
        return Replay(mode == "--replay-archive");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
