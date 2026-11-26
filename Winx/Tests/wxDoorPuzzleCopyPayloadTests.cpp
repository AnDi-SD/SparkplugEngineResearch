#include "Analysis/PC/wxDoorPuzzleAbi.h"
#include "Code/wxDoorPuzzle.h"

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
    using Puzzle = winx::reconstruction::wxDoorPuzzle;
    using Prefix = Puzzle::CapturedPrefixForAnalysis;
    using Payload = std::array<std::uint8_t, 54>;

    constexpr std::array<std::uint8_t, 28> PcPrimary{
        0x90, 0x89, 0x57, 0x00, 0x10, 0x88, 0x57, 0x00, 0x90, 0xD4, 0x40, 0x00,
        0x70, 0x82, 0x57, 0x00, 0x60, 0x82, 0x57, 0x00, 0x50, 0x83, 0x40, 0x00,
        0x70, 0x83, 0x40, 0x00,
    };

    constexpr std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    static_assert(ReadWord(PcPrimary.data() + Abi::wxDoorPuzzleCopySlotOffset)
        == Abi::wxDoorPuzzleCopyMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x10) == Abi::wxDoorPuzzleRttiMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x14) == 0x00408350);

    struct Field
    {
        std::size_t offset;
        std::size_t width;
    };

    constexpr Field PayloadOrder[]{
        {0x130, 4}, {0x134, 4}, {0x138, 4}, {0x13C, 4}, {0x140, 4},
        {0x144, 4}, {0x15C, 4}, {0x160, 4}, {0x164, 4}, {0x168, 1},
        {0x169, 1}, {0x16C, 4}, {0x124, 4}, {0x128, 4}, {0x12C, 4},
    };

    struct Witness
    {
        std::string_view sourcePayload;
        std::string_view destinationPayload;
    };

    // Raw lane bytes in the archived PC instruction order.
    constexpr Witness Witnesses[]{
        {"0000103f0101103f0202103f0303103f0404103f0505103f0606103f0707103f0808103fa5a50b0b103f0c0c103f0d0d103f0e0e103f",
         "0000104001011040020210400303104004041040050510400606104007071040080810405a5a0b0b10400c0c10400d0d10400e0e1040"}, // case-00
        {"082d52779cc1e60b30557a9fc4e90e33587da2c7ec11365b6489aed3f81d42678cb1d6fb2045b4d9fe234c7196bbe0052a4f7499bee3",
         "082d52779cc1e60b30557a9fc4e90e33587da2c7ec11365b6489aed3f81d42678cb1d6fb2045b4d9fe234c7196bbe0052a4f7499bee3"}, // case-01
        {"193e6388add2f71c41668bb0d5fa1f44698eb3d8fd22476c759abfe4092e53789dc2e70c3156c5ea0f345d82a7ccf1163b6085aacff4",
         "0f4c89c603407dba41668bb0eb2865a2df1c5996d3104d8a759abfe47fbcf93673b0ed2a67565b98d5123370adea2764a1de85aacff4"}, // case-02
        {"2a4f7499bee3082d52779cc1e60b30557a9fc4e90e33587d86abd0f51a3f6489aed3f81d4267d6fb20456e93b8dd02274c7196bbe005",
         "2c69a6e3205d9ad714518ecb084582bffc3976b3f02d6aa7a8e5225f9cd9165390cd0a4784c178b5f22f508dca074481befb3875b2ef"}, // case-03
        {"3b6085aacff4193e6388add2f71c41668bb0d5fa1f44698e97bce1062b50759abfe4092e5378e70c31567fa4c9ee13385d82a7ccf116",
         "4986c3003d7ab7f4316eabe825629fdc195693d00d4a87c4c5023f7cb9f63370adea2764a17895d20f4c6daae724619edb185592cf0c"}, // case-04
        {"4c7196bbe0052a4f7499bee3082d52779cc1e60b30557a9fa8cdf2173c6186abd0f51a3f6489f81d426790b5daff24496e93b8dd0227",
         "66a3e01d5a97d4114e8bc805427fbcf93673b0ed2a67a4e1e21f5c99d613508dca074481befbb2ef2c698ac704417ebbf83572afec29"}, // case-05
        {"5d82a7ccf1163b6085aacff4193e6388add2f71c41668bb0b9de03284d7297bce1062b50759a092e5378a1c6eb10355a7fa4c9ee1338",
         "83c0fd3a77b4f12e6ba8e5225f9cd9165390cd0a4784c1feff3c79b6f3306daae724619edb18cf0c4986a7e4215e9bd815528fcc0946"}, // case-06
        {"6e93b8dd02274c7196bbe0052a4f7499bee3082d52779cc1caef14395e83a8cdf2173c6186ab1a3f6489b2d7fc21466b90b5daff2449",
         "a0dd1a5794d10e4b88c5023f7cb9f63370adea2764a1de1b1c5996d3104d8ac704417ebbf835ec2966a3c4013e7bb8f5326face92663"}, // case-07
    };

    template <std::size_t Size>
    std::array<std::uint8_t, Size> ParseHex(std::string_view hex)
    {
        if (hex.size() != 2 * Size || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("invalid DoorPuzzle hex capture");
        std::array<std::uint8_t, Size> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            unsigned value{};
            const auto begin = hex.data() + 2 * i;
            const auto parsed = std::from_chars(begin, begin + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
                throw std::runtime_error("invalid DoorPuzzle hex byte");
            bytes[i] = static_cast<std::uint8_t>(value);
        }
        return bytes;
    }

    template <std::size_t Size>
    void PrintHex(const std::array<std::uint8_t, Size>& bytes)
    {
        for (const auto value : bytes)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }

    Prefix MakeCapture(std::size_t salt, std::size_t stride, const Payload& payload) noexcept
    {
        Prefix capture{};
        for (std::size_t i = 0; i < capture.size(); ++i)
            capture[i] = static_cast<std::uint8_t>(salt + stride * i);
        for (unsigned i = 0; i < 4; ++i)
            capture[i] = static_cast<std::uint8_t>(Abi::wxDoorPuzzleVTable >> (8 * i));
        std::size_t consumed{};
        for (const auto& field : PayloadOrder)
        {
            std::copy_n(payload.begin() + consumed, field.width, capture.begin() + field.offset);
            consumed += field.width;
        }
        return capture;
    }

    Payload ExtractPayload(const Prefix& capture) noexcept
    {
        Payload payload{};
        std::size_t produced{};
        for (const auto& field : PayloadOrder)
        {
            std::copy_n(capture.begin() + field.offset, field.width, payload.begin() + produced);
            produced += field.width;
        }
        return payload;
    }

    int Replay(bool archive)
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::string first, second, trailing;
            if (!(row >> first >> second) || row >> trailing)
                throw std::runtime_error("expected two DoorPuzzle captures");

            const auto source = archive ? MakeCapture(0, 0, ParseHex<54>(first)) : ParseHex<392>(first);
            auto destination = archive ? MakeCapture(0, 0, ParseHex<54>(second)) : ParseHex<392>(second);
            if (!Puzzle::TryCopyPcPayloadForAnalysis(source, destination))
                throw std::runtime_error("DoorPuzzle capture pair was refused");
            if (archive)
                PrintHex(ExtractPayload(destination));
            else
            {
                PrintHex(source);
                std::cout << ' ';
                PrintHex(destination);
            }
            std::cout << '\n';
        }
        if (!std::cin.eof() || !std::cout)
            throw std::runtime_error("replay stream failure");
        return 0;
    }

    int RunTests()
    {
        unsigned checks{};
        const auto check = [&](bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
            ++checks;
        };
        std::size_t index{};
        for (const auto& fixture : Witnesses)
        {
            const auto source = MakeCapture(17 * index + 7, 37, ParseHex<54>(fixture.sourcePayload));
            auto destination = MakeCapture(29 * index + 101, 61, ParseHex<54>(fixture.destinationPayload));
            const auto sourceBefore = source;
            auto expected = destination;
            std::copy_n(source.begin() + 0x124, 0x24, expected.begin() + 0x124);
            std::copy_n(source.begin() + 0x15C, 0x0E, expected.begin() + 0x15C);
            std::copy_n(source.begin() + 0x16C, 0x04, expected.begin() + 0x16C);
            check(Puzzle::TryCopyPcPayloadForAnalysis(source, destination), "valid puzzle captures were refused");
            check(source == sourceBefore, "DoorPuzzle source changed");
            check(destination == expected, "DoorPuzzle destination bytes differ");
            ++index;
        }

        const auto source = MakeCapture(0x43, 37, {});
        auto destination = source;
        check(Puzzle::TryCopyPcPayloadForAnalysis(source, destination) && destination == source,
            "distinct byte-identical puzzle captures were refused");
        check(!Puzzle::TryCopyPcPayloadForAnalysis(destination, destination) && destination == source,
            "aliased puzzle capture was accepted");
        auto wrongSource = source;
        wrongSource[0] ^= 1;
        wrongSource[0x124] ^= 0xFF;
        check(!Puzzle::TryCopyPcPayloadForAnalysis(wrongSource, destination) && destination == source,
            "foreign puzzle source allowed a write");
        auto wrongDestination = MakeCapture(0x69, 53, {});
        wrongDestination[3] ^= 1;
        const auto wrongBefore = wrongDestination;
        check(!Puzzle::TryCopyPcPayloadForAnalysis(source, wrongDestination) && wrongDestination == wrongBefore,
            "foreign puzzle destination allowed a write");

        std::cout << "DoorPuzzleCopyPayload: " << checks << " contracts passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunTests();
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay(false);
        if (argc == 2 && std::string_view(argv[1]) == "--replay-archive")
            return Replay(true);
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
