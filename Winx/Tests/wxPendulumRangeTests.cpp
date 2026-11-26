#include "Code/wxPendulum.h"
#include "Analysis/PC/wxPendulumAbi.h"

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
    namespace Abi = wxPendulumAbi;
    using Prefix = wxPendulum::CapturedRangePrefixForAnalysis;
    using Node = wxPendulum::RangeNodeForAnalysis;
    using Point = wxPendulum::CapturedRangePointForAnalysis;
    using Result = wxPendulum::RangeResultForAnalysis;
    using Words = std::array<std::uint32_t, 6>;

    constexpr std::uint32_t NodeAddress = 0x31000400u;

    struct Witness final
    {
        std::string_view input;
        std::string_view output;
    };

    constexpr std::array<Witness, 13> FreshWitnesses{{
        {"00000000 80000000 00000000 80000000 00000000 00000000 01", "01 01"},
        {"4479c000 447a0000 00000000 00000000 00000000 00000000 00", "01 01"},
        {"c47a0000 c47a0000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"447a0000 00000000 447a0000 00000000 00000000 00000000 01", "01 01"},
        {"447a4000 447a0000 00000000 00000000 00000000 00000000 00", "01 00"},
        {"447a0000 3f800000 447a0000 00000000 00000000 00000000 01", "01 00"},
        {"447a0000 00000000 447a0000 a6800000 00000000 00000000 ff", "01 00"},
        {"447a0000 00000000 447a0000 26800000 00000000 00000000 01", "01 01"},
        {"447a0000 447a0000 34900000 00000000 00000000 00000000 a5", "01 01"},
        {"46800000 44800000 c6800000 46706000 447fc000 c6706000 55", "01 00"},
        {"46800000 447fe000 c6800000 46706400 44800000 c6706000 7f", "01 01"},
        {"00800000 80800000 80000000 00000000 00000000 00000000 80", "01 01"},
        {"447cc000 44778000 00000000 41200000 c1200000 00000000 00", "01 00"},
    }};

    constexpr std::array<Witness, 7> RetainedWitnesses{{
        {"00000000 00000000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"40400000 40800000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"4479c000 447a0000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"447a0000 447a0000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"447a4000 447a0000 00000000 00000000 00000000 00000000 a5", "01 00"},
        {"c47a0000 c47a0000 00000000 00000000 00000000 00000000 a5", "01 01"},
        {"447cc000 44778000 00000000 41200000 c1200000 00000000 00", "01 00"},
    }};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <std::size_t Size>
    void SetWord(std::array<std::uint8_t, Size>& bytes, std::size_t offset, std::uint32_t word)
    {
        for (unsigned byte = 0; byte != 4; ++byte)
            bytes[offset + byte] = static_cast<std::uint8_t>(word >> (8 * byte));
    }

    struct Input final
    {
        Prefix owner;
        Node node;
        Point point;
    };

    Input Frame(const Words& words, std::uint8_t initial)
    {
        Input input{};
        input.owner.fill(0xA5);
        input.node.bytes.fill(0x5A);
        input.point.fill(0xC7);
        input.node.capturedAddress = NodeAddress;
        SetWord(input.owner, 0, Abi::CapturedPrimary);
        SetWord(input.owner, Abi::RangeNodeLinkOffset, NodeAddress);
        input.owner[Abi::RangeByteOffset] = initial;
        for (std::size_t lane = 0; lane != 3; ++lane)
        {
            SetWord(input.node.bytes, Abi::RangeNodePositionOffset + 4 * lane, words[lane]);
            SetWord(input.point, Abi::RangePointPositionOffset + 4 * lane, words[3 + lane]);
        }
        return input;
    }

    std::uint32_t ParseWord(std::string_view text, std::size_t digits)
    {
        Require(text.size() == digits, "incorrect raw word width");
        std::uint32_t word = 0;
        const auto end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, word, 16);
        Require(result.ec == std::errc{} && result.ptr == end, "invalid hexadecimal word");
        return word;
    }

    Input Parse(std::string_view row)
    {
        std::istringstream fields{std::string(row)};
        std::string token;
        Words words{};
        for (auto& word : words)
        {
            Require(bool(fields >> token), "missing coordinate");
            word = ParseWord(token, 8);
        }
        Require(bool(fields >> token), "missing initial range byte");
        const auto initial = ParseWord(token, 2);
        Require(!(fields >> token), "unexpected trailing field");
        return Frame(words, static_cast<std::uint8_t>(initial));
    }

    std::string Encode(const Result& output)
    {
        std::ostringstream raw;
        raw << std::hex << std::setfill('0') << std::setw(2) << unsigned(output.returnLowByte)
            << ' ' << std::setw(2) << unsigned(output.after[Abi::RangeByteOffset]);
        return raw.str();
    }

    Result Dirty()
    {
        Result value{};
        value.after.fill(0x5C);
        value.returnLowByte = 0x97;
        return value;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            const auto input = Parse(line);
            auto output = Dirty();
            Require(wxPendulum::TryUpdatePcRangeForAnalysis(input.owner, 0, input.node, input.point, output),
                "qualified range case refused");
            std::cout << Encode(output) << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failure");
        return 0;
    }

    int RunTests()
    {
        unsigned checks = 0;
        const auto check = [&](bool condition, const char* message)
        {
            Require(condition, message);
            ++checks;
        };
        const auto witness = [&](const Witness& row)
        {
            const auto input = Parse(row.input);
            const auto before = input;
            auto output = Dirty();
            check(wxPendulum::TryUpdatePcRangeForAnalysis(input.owner, 0, input.node, input.point, output),
                "original range case refused");
            check(Encode(output) == row.output, "range byte or AL differs from original");
            auto otherBytes = output.after;
            otherBytes[Abi::RangeByteOffset] = before.owner[Abi::RangeByteOffset];
            check(otherBytes == before.owner && input.owner == before.owner, "operation changed untouched owner bytes");
            check(input.node.capturedAddress == before.node.capturedAddress && input.node.bytes == before.node.bytes
                  && input.point == before.point, "operation changed a borrowed input");
        };
        for (const auto& row : FreshWitnesses) witness(row);
        for (const auto& row : RetainedWitnesses) witness(row);

        const auto valid = Parse(FreshWitnesses[0].input);
        const auto requiresRefusal = [&](const Input& input, std::uint32_t global)
        {
            const auto before = Dirty();
            auto output = before;
            check(!wxPendulum::TryUpdatePcRangeForAnalysis(input.owner, global, input.node, input.point, output),
                "unqualified range case accepted");
            check(output.after == before.after && output.returnLowByte == before.returnLowByte,
                "refused range case changed the output");
        };
        requiresRefusal(valid, 1);
        auto bad = valid;
        SetWord(bad.owner, 0, 0x00701638u);
        requiresRefusal(bad, 0);
        bad = valid;
        SetWord(bad.owner, Abi::RangeNodeLinkOffset, 0);
        bad.node.capturedAddress = 0;
        requiresRefusal(bad, 0);
        bad = valid;
        ++bad.node.capturedAddress;
        requiresRefusal(bad, 0);

        constexpr std::array<std::uint32_t, 5> Outside{0x00000001u, 0x807FFFFFu, 0x46800001u, 0xFF800000u, 0x7FA12345u};
        for (const auto word : Outside)
            for (std::size_t lane = 0; lane != 3; ++lane)
            {
                bad = valid;
                SetWord(bad.node.bytes, Abi::RangeNodePositionOffset + 4 * lane, word);
                requiresRefusal(bad, 0);
                bad = valid;
                SetWord(bad.point, Abi::RangePointPositionOffset + 4 * lane, word);
                requiresRefusal(bad, 0);
            }

        auto alias = Dirty();
        alias.after = valid.owner;
        auto expected = valid.owner;
        expected[Abi::RangeByteOffset] = 1;
        check(wxPendulum::TryUpdatePcRangeForAnalysis(alias.after, 0, valid.node, valid.point, alias),
            "in-place owner result refused");
        check(alias.after == expected && alias.returnLowByte == 1, "in-place result changed unrelated bytes");
        std::cout << "PendulumRange: " << checks << " checks passed\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return RunTests();
        Require(argc == 2 && std::string_view(argv[1]) == "--replay", "expected --replay or no arguments");
        return Replay();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
