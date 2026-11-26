#include "Analysis/PC/SparkplugAbi.h"
#include "Code/Sparkplug/spBSPNode.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Node = sparkplug::reconstruction::spBSPNode;
    using Layout = sparkplug::evidence::pc::spBSPNodeLayout;
    using Owner = std::array<std::uint8_t, sizeof(Layout)>;
    using Ray = std::array<std::uint8_t, 24>;

    constexpr std::uint32_t Primary = 0x006EBA30u;
    constexpr std::size_t PlaneOffset = offsetof(Layout, plane);
    constexpr std::size_t ScratchOffset = offsetof(Layout, rayCandidates);
    static_assert(sizeof(Layout) == 0xAC && PlaneOffset == 0x84 && ScratchOffset == 0x94);

    unsigned checks = 0;

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    template <std::size_t Size>
    std::uint32_t GetWord(const std::array<std::uint8_t, Size>& raw, std::size_t offset)
    {
        std::uint32_t word = 0;
        for (unsigned i = 0; i < 4; ++i)
            word |= std::uint32_t(raw[offset + i]) << (i * 8);
        return word;
    }

    template <std::size_t Size>
    void PutWord(std::array<std::uint8_t, Size>& raw, std::size_t offset, std::uint32_t word)
    {
        for (unsigned i = 0; i < 4; ++i)
            raw[offset + i] = static_cast<std::uint8_t>(word >> (i * 8));
    }

    struct Result final
    {
        std::uint32_t count = 0;
        std::uint32_t outputOffset = 0xFFFFFFFFu;
    };

    Result Update(Owner& owner, const Ray& ray, bool requestOutput)
    {
        if (GetWord(owner, 0) != Primary)
            return {};
        Node::RayScratchForAnalysis scratch;
        std::array<std::uint32_t, 6> rayBits{};
        for (std::size_t i = 0; i < scratch.planeBits.size(); ++i)
            scratch.planeBits[i] = GetWord(owner, PlaneOffset + 4*i);
        for (std::size_t i = 0; i < rayBits.size(); ++i)
            rayBits[i] = GetWord(ray, 4*i);
        for (std::size_t i = 0; i < scratch.scratch.size(); ++i)
            scratch.scratch[i] = {GetWord(owner, ScratchOffset + 8*i), GetWord(owner, ScratchOffset + 8*i + 4)};

        const Node::RawRayCandidateForAnalysis* borrowed = nullptr;
        const auto count = Node::UpdateRayScratchForAnalysis(scratch, rayBits, requestOutput ? &borrowed : nullptr);
        Require(borrowed == (count != 0 && requestOutput ? scratch.scratch.data() : nullptr), "borrowed result differs");
        if (count == 0)
            return {};
        for (std::size_t i = 0; i < scratch.scratch.size(); ++i)
        {
            PutWord(owner, ScratchOffset + 8*i, scratch.scratch[i].child);
            PutWord(owner, ScratchOffset + 8*i + 4, scratch.scratch[i].timeBits);
        }
        return {count, borrowed != nullptr ? static_cast<std::uint32_t>(ScratchOffset) : 0xFFFFFFFFu};
    }

    template <std::size_t Size>
    std::array<std::uint8_t, Size> ParseHex(std::string_view hex)
    {
        Require(hex.size() == 2*Size, "unexpected raw buffer length");
        std::array<std::uint8_t, Size> bytes{};
        for (std::size_t i = 0; i < Size; ++i)
        {
            unsigned value = 0;
            const char* first = hex.data() + 2*i;
            const auto parse = std::from_chars(first, first + 2, value, 16);
            if (parse.ec != std::errc{} || parse.ptr != first + 2)
                throw std::runtime_error("invalid hexadecimal buffer");
            bytes[i] = static_cast<std::uint8_t>(value);
        }
        return bytes;
    }

    template <std::size_t Size>
    void PrintHex(const std::array<std::uint8_t, Size>& bytes)
    {
        for (const auto byte : bytes)
            std::cout << std::setw(2) << static_cast<unsigned>(byte);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string flag, ownerText, rayText, extra;
            Require(bool(fields >> flag >> ownerText >> rayText) && !(fields >> extra), "expected a flag and two raw buffers");
            Require(flag == "out" || flag == "null", "expected out or null");
            auto owner = ParseHex<sizeof(Layout)>(ownerText);
            const auto ray = ParseHex<24>(rayText);
            const auto result = Update(owner, ray, flag == "out");
            Require(result.count != 0, "qualified BSP ray replay refused");
            std::cout << std::hex << std::setfill('0') << std::setw(8) << result.count << ' ';
            PrintHex(owner);
            std::cout << ' ';
            PrintHex(ray);
            std::cout << ' ' << std::setw(8) << result.outputOffset << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failed");
        return 0;
    }

    struct Witness final
    {
        std::array<std::uint32_t, 4> plane;
        std::array<std::uint32_t, 6> ray;
        std::array<std::uint32_t, 4> after;
        std::uint32_t count;
        bool output;
    };

    constexpr std::array<Witness, 12> Witnesses{{
        // archived forward
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0xC0000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0x00000000u, 0x40000000u}, 2u, true},
        // archived negative forward
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0x40000000u, 0x00000000u, 0x00000000u, 0xBF800000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0x00000001u, 0x40000000u}, 2u, true},
        // archived away
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0x40000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0xFEDD0042u, 0x7FC12345u}, 1u, true},
        // archived negative zero
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0x00000000u, 0x80000000u}, 2u, true},
        // archived parallel
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0xC0000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0xFEDD0044u, 0x7FC12345u}, 1u, true},
        // positive denominator width discriminator
        {{0x45800000u, 0x3F800000u, 0x45800000u, 0x00000000u},
         {0x00000000u, 0xBF800000u, 0x00000000u, 0xC5800000u, 0x3727C5AEu, 0x45800000u},
         {0x00000001u, 0x00000000u, 0x00000000u, 0x47C34FFEu}, 2u, true},
        // negative denominator width discriminator
        {{0x45800000u, 0x3F800000u, 0x45800000u, 0x00000000u},
         {0x00000000u, 0x3F800000u, 0x00000000u, 0x45800000u, 0xB727C5AEu, 0xC5800000u},
         {0x00000000u, 0x00000000u, 0x00000001u, 0x47C34FFEu}, 2u, true},
        // positive cutoff equality retains tail
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF800000u, 0x00000000u, 0x00000000u, 0x3727C5ACu, 0x00000000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0xFEDD0047u, 0x80000000u}, 1u, true},
        // negative cutoff equality retains tail
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0x3F800000u, 0x00000000u, 0x00000000u, 0xB727C5ACu, 0x00000000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0xFEDD0048u, 0x7FC12345u}, 1u, true},
        // null out still writes pairs
        {{0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0xBF800000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0x00000000u, 0x3F800000u}, 2u, false},
        // positive and negative profile endpoints
        {{0x33800000u, 0xB3800000u, 0x45800000u, 0x00000000u},
         {0x33800000u, 0xB3800000u, 0x3F800000u, 0xB3800000u, 0x33800000u, 0xC5800000u},
         {0x00000000u, 0x00000000u, 0x00000001u, 0x39800000u}, 2u, true},
        // negative denominator wrong side retains tail
        {{0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF800000u, 0x00000000u, 0x00000000u, 0xBF800000u, 0x00000000u, 0x00000000u},
         {0x00000001u, 0x00000000u, 0xFEDD004Bu, 0x80000000u}, 1u, true},
    }};

    void CheckWitnesses()
    {
        for (std::size_t i = 0; i < Witnesses.size(); ++i)
        {
            const auto& witness = Witnesses[i];
            Owner owner{};
            Ray ray{};
            for (std::size_t offset = 0; offset < owner.size(); ++offset)
                owner[offset] = static_cast<std::uint8_t>(31*i + 53 + 43*offset);
            PutWord(owner, 0, Primary);
            for (std::size_t j = 0; j < witness.plane.size(); ++j)
                PutWord(owner, PlaneOffset + 4*j, witness.plane[j]);
            PutWord(owner, ScratchOffset, 0xA1B20201u);
            PutWord(owner, ScratchOffset + 4, 0xFFA23456u);
            PutWord(owner, ScratchOffset + 8, 0xFEDD0040u | static_cast<std::uint32_t>(i));
            PutWord(owner, ScratchOffset + 12, i % 2 ? 0x80000000u : 0x7FC12345u);
            for (std::size_t j = 0; j < witness.ray.size(); ++j)
                PutWord(ray, 4*j, witness.ray[j]);

            auto expected = owner;
            for (std::size_t j = 0; j < witness.after.size(); ++j)
                PutWord(expected, ScratchOffset + 4*j, witness.after[j]);
            const auto result = Update(owner, ray, witness.output);
            Require(result.count == witness.count, "BSP ray count differs");
            Require(owner == expected, "full owner differs after the exact scratch stores");
            Require(result.outputOffset == (witness.output ? ScratchOffset : 0xFFFFFFFFu), "optional output differs");
        }
    }

    void CheckLegacyVector()
    {
        for (const auto i : {5u, 6u})
        {
            Node node;
            Node::Plane plane{};
            std::array<float, 6> ray{};
            std::memcpy(plane.data(), Witnesses[i].plane.data(), plane.size() * sizeof(float));
            std::memcpy(ray.data(), Witnesses[i].ray.data(), ray.size() * sizeof(float));
            node.SetPlaneForAnalysis(plane);
            const auto vector = node.RayCandidatesForAnalysis(ray);
            Require(vector.size() == 1 && vector[0].child == Witnesses[i].after[0], "legacy vector API changed near the ray cutoff");
        }
    }

    void CheckRefusals()
    {
        const Node::RawRayCandidateForAnalysis sentinel{7, 0x80000000u};
        for (const auto invalid : {1u, 0x80000001u, 0x337FFFFFu, 0x45800001u, 0x7F800000u, 0x7FC12345u})
        {
            Node::RayScratchForAnalysis state;
            state.planeBits[3] = invalid;
            state.scratch[0] = sentinel;
            const auto* borrowed = &sentinel;
            const std::array<std::uint32_t, 6> ray{};
            Require(Node::UpdateRayScratchForAnalysis(state, ray, &borrowed) == 0, "out-of-profile BSP input accepted");
            Require(borrowed == &sentinel && state.scratch[0].child == sentinel.child && state.scratch[0].timeBits == sentinel.timeBits,
                "refused BSP query changed output or scratch");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        Require(argc == 1, "expected no arguments or --replay");
        CheckWitnesses();
        CheckLegacyVector();
        CheckRefusals();
        std::cout << "PASS " << checks << " BSP ray scratch checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
