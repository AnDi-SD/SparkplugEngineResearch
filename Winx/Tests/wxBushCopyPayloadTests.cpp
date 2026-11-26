#include "Analysis/PC/wxBushAbi.h"
#include "Code/wxBush.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace Abi = winx::evidence::pc;
    using Bush = winx::reconstruction::wxBush;
    using Prefix = Bush::CapturedPrefixForAnalysis;

    constexpr std::array<std::uint8_t, 28> PcPrimary{
        0x90, 0x45, 0x54, 0x00, 0xC0, 0x49, 0x54, 0x00, 0x60, 0xBE, 0x40, 0x00,
        0xA0, 0x41, 0x54, 0x00, 0xB0, 0x42, 0x54, 0x00, 0x50, 0x83, 0x40, 0x00,
        0x70, 0x83, 0x40, 0x00,
    };

    constexpr std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    static_assert(ReadWord(PcPrimary.data() + Abi::wxBushCopySlotOffset) == Abi::wxBushCopyMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x10) == 0x005442B0);
    static_assert(ReadWord(PcPrimary.data() + 0x14) == 0x00408350);

    constexpr std::array<std::size_t, 2> Offsets{0x14C, 0x16A};
    constexpr std::size_t Extent = 30;
    constexpr std::array<std::string_view, 2> Witnesses[]{
        {"54657374313233", "54657374313233"},
        {"4142434445464748494a4b4c4d4e4f505152535455565758595a414243", ""},
        {"", "e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"},
        {"ff8041", "81"},
        {"0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d",
         "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c"},
        {"", ""},
        {"4d", "6c6f6e67657254455854"},
        {"30313233343536373839414243", "6162636465666768696a6b6c6d6e6f70717273747576777879"},
    };

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    void WriteWord(std::uint8_t* bytes, std::uint32_t word) noexcept
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(word >> (i * 8));
    }

    Prefix MakeCapture(std::size_t salt, std::size_t stride) noexcept
    {
        Prefix result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<std::uint8_t>(salt + stride * i);
        WriteWord(result.data(), Abi::wxBushVTable);
        return result;
    }

    std::uint8_t ParseByte(const char* begin)
    {
        unsigned value{};
        const auto parsed = std::from_chars(begin, begin + 2, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
            throw std::runtime_error("invalid Bush byte");
        return static_cast<std::uint8_t>(value);
    }

    Prefix ParseCapture(std::string_view hex)
    {
        Prefix capture{};
        if (hex.size() != 2 * capture.size() || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("invalid Bush capture");
        for (std::size_t i = 0; i < capture.size(); ++i)
            capture[i] = ParseByte(hex.data() + i * 2);
        return capture;
    }

    void WriteTerminated(Prefix& capture, std::size_t offset, std::string_view text)
    {
        const auto count = text.size() / 2;
        for (std::size_t i = 0; i < count; ++i)
            capture[offset + i] = ParseByte(text.data() + i * 2);
        capture[offset + count] = 0;
    }

    void PrintCapture(const Prefix& capture)
    {
        for (auto byte : capture)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string sourceHex, destinationHex, trailing;
            if (!(row >> sourceHex >> destinationHex) || row >> trailing)
                throw std::runtime_error("expected two Bush captures");
            const auto source = ParseCapture(sourceHex);
            auto destination = ParseCapture(destinationHex);
            if (!Bush::TryCopyPcPayloadForAnalysis(source, destination))
                throw std::runtime_error("Bush capture pair was refused");
            PrintCapture(source);
            std::cout << ' ';
            PrintCapture(destination);
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay input or output failed");
        return 0;
    }

    void CheckCopy()
    {
        std::size_t index{};
        for (const auto& fixture : Witnesses)
        {
            auto source = MakeCapture(17 * index + 0x2B, 37);
            auto destination = MakeCapture(29 * index + 0x89, 53);
            WriteTerminated(source, Offsets[0], fixture[0]);
            WriteTerminated(source, Offsets[1], fixture[1]);
            if (index == 3)
            {
                constexpr std::array<std::uint8_t, 4> firstSuffix{0xDE, 0xAD, 0xFF, 0xCC};
                constexpr std::array<std::uint8_t, 4> secondSuffix{0xFE, 0xEF, 0xDD, 0x80};
                std::copy(firstSuffix.begin(), firstSuffix.end(), source.begin() + Offsets[0] + 4);
                std::copy(secondSuffix.begin(), secondSuffix.end(), source.begin() + Offsets[1] + 2);
            }
            if (index == 4)
                std::copy_n(source.begin() + Offsets[0], Extent, destination.begin() + Offsets[0]);
            if (index == 5)
                destination[Offsets[1]] = 0;
            if (index == 6)
            {
                destination[Offsets[0]] = source[Offsets[0]];
                destination[Offsets[1] + 1] = source[Offsets[1] + 1];
            }
            const auto sourceBefore = source;
            auto expected = destination;
            WriteTerminated(expected, Offsets[0], fixture[0]);
            WriteTerminated(expected, Offsets[1], fixture[1]);
            Require(Bush::TryCopyPcPayloadForAnalysis(source, destination), "valid Bush pair was refused");
            Require(source == sourceBefore, "Bush source changed");
            Require(destination == expected, "Bush destination bytes differ");
            ++index;
        }
    }

    void CheckIdentity()
    {
        auto source = MakeCapture(0x43, 37);
        WriteTerminated(source, Offsets[0], "ff80");
        WriteTerminated(source, Offsets[1], "a5");
        auto destination = source;
        Require(Bush::TryCopyPcPayloadForAnalysis(source, destination) && destination == source,
            "distinct byte-identical Bush captures were refused");
        Require(!Bush::TryCopyPcPayloadForAnalysis(destination, destination) && destination == source,
            "aliased Bush capture was accepted");

        auto wrongSource = source;
        wrongSource[0] ^= 1;
        wrongSource[Offsets[0]] ^= 0xFF;
        Require(!Bush::TryCopyPcPayloadForAnalysis(wrongSource, destination) && destination == source,
            "foreign Bush source type allowed a write");
        auto wrongDestination = MakeCapture(0x69, 53);
        wrongDestination[3] ^= 1;
        const auto wrongBefore = wrongDestination;
        Require(!Bush::TryCopyPcPayloadForAnalysis(source, wrongDestination) && wrongDestination == wrongBefore,
            "foreign Bush destination type allowed a write");

        for (const auto offset : Offsets)
        {
            auto unterminated = source;
            std::fill_n(unterminated.begin() + offset, Extent, std::uint8_t{0xA5});
            auto untouched = MakeCapture(0xE3, 53);
            const auto prior = untouched;
            Require(!Bush::TryCopyPcPayloadForAnalysis(unterminated, untouched) && untouched == prior,
                "unterminated Bush input allowed a write");
        }
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
        CheckCopy();
        CheckIdentity();
        std::cout << "BushCopyPayload: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
