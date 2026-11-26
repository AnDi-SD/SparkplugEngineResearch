#include "Analysis/PC/wxTargetRegionAbi.h"
#include "Code/wxTargetRegion.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using Region = winx::reconstruction::wxTargetRegion;
    using Prefix = Region::CapturedPrefixForAnalysis;
    constexpr auto TextOffset = Abi::wxTargetRegionTextOffset;
    constexpr auto TextExtent = Abi::wxTargetRegionTextCaptureExtent;
    constexpr std::size_t PayloadExtent = Prefix{}.size() - TextOffset;

    constexpr std::string_view InitialRegion =
        "d07e6f00000000000000cccc0000000000000000000000000000000000000000010000000000"
        "000000ccccccccccccccc0010031000000000101cccc4054094bf40100000000164300cccccc"
        "000000000000000000000000fc2f7000000000000000cccc0000000000000000000000000000"
        "0000000000000000000000000000000000000000000001ccccccffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f"
        "ffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff"
        "7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7fffff7f7f556e646566696e656400cccc"
        "cccccccccccccccccccccccccccccccccccccccc0101010000cccccc000000000000cccc";

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    template<class UInt>
    UInt ParseUnsigned(std::string_view word)
    {
        UInt value{};
        const auto end = word.data() + word.size();
        const auto parsed = std::from_chars(word.data(), end, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            throw std::runtime_error("invalid hexadecimal capture");
        return value;
    }

    template<class Prefix>
    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        if (hex.size() != 2 * bytes.size() || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("unexpected hexadecimal capture width");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = std::uint8_t(ParseUnsigned<unsigned>(hex.substr(i * 2, 2)));
        return bytes;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (i * 8));
    }

    template<class Prefix>
    void PrintPrefix(const Prefix& bytes)
    {
        for (const auto value : bytes)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string sourceHex, destinationHex, trailing;
            if (!(row >> sourceHex >> destinationHex) || row >> trailing)
                throw std::runtime_error("expected two TargetRegion prefixes");
            const auto source = ParsePrefix<Prefix>(sourceHex);
            auto destination = ParsePrefix<Prefix>(destinationHex);
            if (!Region::TryCopyPcPayloadForAnalysis(source, destination))
                throw std::runtime_error("TargetRegion capture pair was refused");
            PrintPrefix(source);
            std::cout << ' ';
            PrintPrefix(destination);
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    struct Witness
    {
        std::string_view text;
        std::uint32_t word14C;
        std::string_view afterPayload;
    };

    constexpr Witness Witnesses[]{
        {"", 0x00000000u,
            "00cce90623405d7a97b4d1ee0b2845627f9cb9d6f3102d4a6784a1bedbf81532092e53789de0fd1a00000000abeae502"},
        {"ff", 0x80000000u,
            "ff00001d3a577491aecbe805223f5c7996b3d0ed0a2744617e9bb8d5f20f2c491a3f6489aef7143100000080c2fbfc19"},
        {"ff8041", 0x7fc01234u,
            "ff804100516e8ba8c5e2ff1c39567390adcae704213e5b7895b2cfec092643602b50759abf0e2b483412c07fd90c1330"},
        {"54657374313233", 0xffffffffu,
            "5465737431323300dcf91633506d8aa7c4e1fe1b3855728facc9e603203d5a773c6186abd025425ffffffffff01d2a47"},
        {"808182838485868788898a8b8c8d8e", 0xdeadbeefu,
            "808182838485868788898a8b8c8d8e00dbf815324f6c89a6c3e0fd1a3754718e4d7297bce13c5976efbeadde072e415e"},
        {"4142434445464748494a4b4c4d4e4f505152535455565758595a41424344", 0x12345678u,
            "4142434445464748494a4b4c4d4e4f505152535455565758595a4142434400a55e83a8cdf253708d785634121e3f5875"},
        {"0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", 0x00000001u,
            "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f006f94b9de036a87a40100000035506f8c"},
        {"e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", 0xffc0fe01u,
            "e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff0080a5caef14819ebb01fec0ff4c6186a3"},
    };

    void CheckWitnesses()
    {
        std::size_t index = 0;
        for (const auto& fixture : Witnesses)
        {
            auto source = ParsePrefix<Prefix>(InitialRegion);
            auto destination = source;
            for (const auto offset : {4u, 0x10u, 0x28u, 0x38u, 0x3Cu, 0x123u})
            {
                source[offset] = static_cast<std::uint8_t>(17 * index + 37 * offset + 0x17u);
                destination[offset] = static_cast<std::uint8_t>(23 * index + 29 * offset + 0x59u);
            }
            for (std::size_t offset = TextOffset; offset < source.size(); ++offset)
            {
                source[offset] = static_cast<std::uint8_t>(17 * index + 37 * offset + 0x35u);
                destination[offset] = static_cast<std::uint8_t>(23 * index + 29 * offset + 0x9Bu);
            }
            const auto length = fixture.text.size() / 2;
            for (std::size_t i = 0; i < length; ++i)
                source[TextOffset + i] = static_cast<std::uint8_t>(ParseUnsigned<unsigned>(fixture.text.substr(2 * i, 2)));
            source[TextOffset + length] = 0;
            StoreWord(source.data() + 0x14C, fixture.word14C);
            if (index == 0)
                destination[TextOffset] = 0;
            if (index == 3)
                destination[TextOffset + length / 2] = source[TextOffset + length / 2];
            if (index == 7)
            {
                std::copy_n(source.begin() + TextOffset, TextExtent, destination.begin() + TextOffset);
                StoreWord(destination.data() + 0x14C, fixture.word14C);
            }
            const auto beforeSource = source;
            auto expected = destination;
            const auto payload = ParsePrefix<std::array<std::uint8_t, PayloadExtent>>(fixture.afterPayload);
            std::copy(payload.begin(), payload.end(), expected.begin() + TextOffset);
            Require(Region::TryCopyPcPayloadForAnalysis(source, destination), "valid TargetRegion pair was refused");
            Require(source == beforeSource && destination == expected, "TargetRegion capture bytes differ");
            ++index;
        }
    }

    void CheckRejections()
    {
        const auto source = ParsePrefix<Prefix>(InitialRegion);
        auto destination = source;
        auto wrongSource = source;
        wrongSource[0] ^= 1;
        Require(!Region::TryCopyPcPayloadForAnalysis(wrongSource, destination) && destination == source,
            "foreign TargetRegion source type allowed a write");
        auto wrongDestination = destination;
        wrongDestination[0] ^= 1;
        const auto wrongBefore = wrongDestination;
        Require(!Region::TryCopyPcPayloadForAnalysis(source, wrongDestination) && wrongDestination == wrongBefore,
            "foreign TargetRegion destination type allowed a write");

        auto missingTerminator = source;
        std::fill_n(missingTerminator.begin() + TextOffset, TextExtent, std::uint8_t{0xA5});
        Require(!Region::TryCopyPcPayloadForAnalysis(missingTerminator, destination) && destination == source,
            "unterminated TargetRegion copy allowed a write");
        Require(!Region::TryCopyPcPayloadForAnalysis(destination, destination) && destination == source,
            "aliased TargetRegion capture was admitted");
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
        CheckWitnesses();
        CheckRejections();
        std::cout << "TargetRegionCopyPayload: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
