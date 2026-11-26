#include "Analysis/PC/wxSpinningStatuesPuzzleAbi.h"
#include "Code/wxSpinningStatuesPuzzle.h"

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
    using Puzzle = winx::reconstruction::wxSpinningStatuesPuzzle;
    using Prefix = Puzzle::CapturedPrefixForAnalysis;
    using Payload = std::array<std::uint8_t, 42>;

    constexpr std::array<std::uint8_t, 28> PcPrimary{
        0xB0, 0x92, 0x57, 0x00, 0xD0, 0x97, 0x57, 0x00, 0xE0, 0xD4, 0x40, 0x00,
        0x80, 0x8D, 0x57, 0x00, 0x70, 0x8D, 0x57, 0x00, 0x50, 0x83, 0x40, 0x00,
        0x70, 0x83, 0x40, 0x00,
    };

    constexpr std::uint32_t ReadWord(const std::uint8_t* bytes) noexcept
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    static_assert(ReadWord(PcPrimary.data() + Abi::wxSpinningStatuesPuzzleCopySlotOffset)
        == Abi::wxSpinningStatuesPuzzleCopyMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x10) == Abi::wxSpinningStatuesPuzzleRttiMethod);
    static_assert(ReadWord(PcPrimary.data() + 0x14) == 0x00408350);

    constexpr std::array<std::size_t, 10> WordOffsets{
        0x124, 0x138, 0x128, 0x12C, 0x130, 0x134, 0x13C, 0x140, 0x14C, 0x150,
    };

    struct Witness
    {
        std::string_view sourcePayload;
        std::string_view destinationPayload;
    };

    // Raw lane bytes in the archived PC instruction order.
    constexpr Witness Witnesses[]{
        {"0000103f0101103f0202103f0303103f0404103f0505103f0606103f0707103f0808103f0909103fa5a5",
         "000010400101104002021040030310400404104005051040060610400707104008081040090910405a5a"}, // archive-lanes
        {"7ca1c6eb6085aacf10355a7fa4c9ee13385d82a7ccf1163bf4193e6388add2f744698eb3d8fd22476c91",
         "7ca1c6eb6085aacf10355a7fa4c9ee13385d82a7ccf1163bf4193e6388add2f744698eb3d8fd22476c91"}, // all-idempotent
        {"00000000000000804523a17f5634c2ff0000807f000080ff04030201c3d2e1f0ffffffffefcdab897ffe",
         "f124578aed205386bdf0235689bcef225588bbee215487bab9ec1f5285b8eb1ee91c4f82b5e81b4e81b4"}, // raw-dwords
        {"9ec3e80d82a7ccf132577ca1c6eb10355a7fa4c9ee13385d163b6085aacff419668bb0d5fa1f44138001",
         "0e4174a70a3d70a3da0d4073a6d90c3f72a5d80b3e71a4d7d6093c6fa2d5083b06396c9fd20538ca1180"}, // byte-distinction
        {"afd4f91e93b8dd0243688db2d7fc21466b90b5daff24496e274c7196bbe0052a779cc1e60b30557a9fc4",
         "502b06e16c4722fdbc97724d2803deb9946f4a2500dbb691d8b38e69441ffad588633e19f4cfaa85603b"}, // opposed-bits
        {"c0e50a2fa4c9ee1354799ec3e80d32577ca1c6eb10355a7f385d82a7ccf1163b88add2f71c41668bb0d5",
         "487baee1a4c9ee1314477aade80d3257acdf124510355a7f104376a9ccf1163b4073a6d91c41668bd8d5"}, // partial-equality
        {"00000000000000000000000000000000000000000000000000000000000000000000000000000000ff00",
         "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"}, // zero-ones
        {"01020304111213142122232431323334414243445152535461626364717273748182838491929394a1b1",
         "82b5e81b7eb1e4174e81b4e71a4d80b3e6194c7fb2e5184b4a7db0e316497caf7aade0134679acdf1245"}, // raw-offset-order
    };

    template <std::size_t Size>
    std::array<std::uint8_t, Size> ParseHex(std::string_view hex)
    {
        if (hex.size() != 2 * Size || hex.find_first_not_of("0123456789abcdef") != std::string_view::npos)
            throw std::runtime_error("invalid SpinningStatuesPuzzle hex capture");
        std::array<std::uint8_t, Size> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            unsigned value{};
            const auto begin = hex.data() + 2 * i;
            const auto parsed = std::from_chars(begin, begin + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
                throw std::runtime_error("invalid SpinningStatuesPuzzle hex byte");
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
            capture[i] = static_cast<std::uint8_t>(Abi::wxSpinningStatuesPuzzleVTable >> (8 * i));
        for (std::size_t word = 0; word < WordOffsets.size(); ++word)
            std::copy_n(payload.begin() + 4 * word, 4, capture.begin() + WordOffsets[word]);
        capture[0x154] = payload[40];
        capture[0x155] = payload[41];
        return capture;
    }

    Payload ExtractPayload(const Prefix& capture) noexcept
    {
        Payload payload{};
        for (std::size_t word = 0; word < WordOffsets.size(); ++word)
            std::copy_n(capture.begin() + WordOffsets[word], 4, payload.begin() + 4 * word);
        payload[40] = capture[0x154];
        payload[41] = capture[0x155];
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
                throw std::runtime_error("expected two SpinningStatuesPuzzle captures");

            const auto source = archive ? MakeCapture(0, 0, ParseHex<42>(first)) : ParseHex<348>(first);
            auto destination = archive ? MakeCapture(0, 0, ParseHex<42>(second)) : ParseHex<348>(second);
            if (!Puzzle::TryCopyPcPayloadForAnalysis(source, destination))
                throw std::runtime_error("SpinningStatuesPuzzle capture pair was refused");
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
            auto source = MakeCapture(17 * index + 0x37, 37, ParseHex<42>(fixture.sourcePayload));
            auto destination = MakeCapture(29 * index + 0x8B, 51, ParseHex<42>(fixture.destinationPayload));
            if (index == 3)
            {
                source[0x156] = 0xCD;
                destination[0x156] = 0x78;
            }
            const auto sourceBefore = source;
            auto expected = destination;
            std::copy_n(source.begin() + 0x124, 0x20, expected.begin() + 0x124);
            std::copy_n(source.begin() + 0x14C, 0x0A, expected.begin() + 0x14C);
            check(Puzzle::TryCopyPcPayloadForAnalysis(source, destination), "valid puzzle captures were refused");
            check(source == sourceBefore, "SpinningStatuesPuzzle source changed");
            check(destination == expected, "SpinningStatuesPuzzle destination bytes differ");
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

        std::cout << "SpinningStatuesPuzzleCopyPayload: " << checks << " contracts passed\n";
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
