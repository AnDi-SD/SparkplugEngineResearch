#include "Analysis/PC/wxColourfulFlowerAbi.h"
#include "Code/wxColourfulFlower.h"

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
    using Flower = winx::reconstruction::wxColourfulFlower;
    using Prefix = Flower::CapturedPrefixForAnalysis;

    constexpr std::array<std::uint8_t, 28> PcPrimary{
        0x50, 0x77, 0x54, 0x00, 0xD0, 0x71, 0x54, 0x00, 0x40, 0xC0, 0x40, 0x00,
        0xD0, 0x70, 0x54, 0x00, 0x10, 0x73, 0x54, 0x00, 0x50, 0x83, 0x40, 0x00,
        0x70, 0x83, 0x40, 0x00,
    };

    constexpr std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    static_assert(ReadWord(PcPrimary.data() + Abi::wxColourfulFlowerCopySlotOffset) == Abi::wxColourfulFlowerCopyMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x10) == 0x00547310);
    static_assert(ReadWord(PcPrimary.data() + 0x14) == 0x00408350);

    struct RawFields
    {
        std::uint8_t first;
        std::uint8_t second;
        std::uint32_t word;
    };

    struct Witness
    {
        RawFields source;
        RawFields destination;
    };

    constexpr Witness Witnesses[]{
        {{0xA5, 0xA5, 0x3F100202}, {0x5A, 0x5A, 0x40100202}},
        {{0x00, 0x00, 0x00000000}, {0x00, 0x00, 0x00000000}},
        {{0xFF, 0x80, 0x80000000}, {0x01, 0x7F, 0x00000000}},
        {{0x01, 0x7F, 0x7FC01234}, {0x01, 0x80, 0x7FC01234}},
        {{0xFE, 0xFF, 0xFFFFFFFF}, {0x00, 0xFF, 0x7FFFFFFF}},
        {{0x80, 0x01, 0x00000001}, {0x80, 0xFF, 0x80000001}},
        {{0x5A, 0xA5, 0x12345678}, {0xA5, 0x5A, 0x78563412}},
        {{0x00, 0xFF, 0xDEADBEEF}, {0xFF, 0x00, 0xDEADBEEF}},
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

    void WriteFields(Prefix& capture, const RawFields& fields) noexcept
    {
        capture[0x138] = fields.first;
        capture[0x139] = fields.second;
        WriteWord(capture.data() + 0x14C, fields.word);
    }

    Prefix MakeCapture(std::size_t salt, std::size_t stride, const RawFields& fields) noexcept
    {
        Prefix result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<std::uint8_t>(salt + stride * i);
        WriteWord(result.data(), Abi::wxColourfulFlowerVTable);
        WriteFields(result, fields);
        return result;
    }

    Prefix ParseCapture(std::string_view hex)
    {
        Prefix capture{};
        if (hex.size() != 2 * capture.size() || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("invalid ColourfulFlower capture");
        for (std::size_t i = 0; i < capture.size(); ++i)
        {
            unsigned value{};
            const auto begin = hex.data() + i * 2;
            const auto parsed = std::from_chars(begin, begin + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
                throw std::runtime_error("invalid ColourfulFlower byte");
            capture[i] = static_cast<std::uint8_t>(value);
        }
        return capture;
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
                throw std::runtime_error("expected two ColourfulFlower captures");
            const auto source = ParseCapture(sourceHex);
            auto destination = ParseCapture(destinationHex);
            if (!Flower::TryCopyPcPayloadForAnalysis(source, destination))
                throw std::runtime_error("ColourfulFlower capture pair was refused");
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
            const auto source = MakeCapture(17 * index + 0x2B, 37, fixture.source);
            auto destination = MakeCapture(29 * index + 0x89, 53, fixture.destination);
            const auto sourceBefore = source;
            auto expected = destination;
            WriteFields(expected, fixture.source);
            Require(Flower::TryCopyPcPayloadForAnalysis(source, destination), "valid ColourfulFlower pair was refused");
            Require(source == sourceBefore, "ColourfulFlower source changed");
            Require(destination == expected, "ColourfulFlower destination bytes differ");
            ++index;
        }
    }

    void CheckIdentity()
    {
        const auto source = MakeCapture(0x43, 37, {1, 2, 0x01234567});
        auto destination = source;
        Require(Flower::TryCopyPcPayloadForAnalysis(source, destination) && destination == source,
            "distinct byte-identical ColourfulFlower captures were refused");
        Require(!Flower::TryCopyPcPayloadForAnalysis(destination, destination) && destination == source,
            "aliased ColourfulFlower capture was accepted");

        auto wrongSource = source;
        wrongSource[0] ^= 1;
        wrongSource[0x138] ^= 0xFF;
        Require(!Flower::TryCopyPcPayloadForAnalysis(wrongSource, destination) && destination == source,
            "foreign ColourfulFlower source type allowed a write");
        auto wrongDestination = MakeCapture(0x69, 53, {4, 5, 0x89ABCDEF});
        wrongDestination[3] ^= 1;
        const auto wrongBefore = wrongDestination;
        Require(!Flower::TryCopyPcPayloadForAnalysis(source, wrongDestination) && wrongDestination == wrongBefore,
            "foreign ColourfulFlower destination type allowed a write");
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
        std::cout << "ColourfulFlowerCopyPayload: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
