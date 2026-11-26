#include "Analysis/PC/SparkplugAbi.h"
#include "Code/Sparkplug/spOctreeNode.h"

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
    using Node = sparkplug::reconstruction::spOctreeNode;
    using Layout = sparkplug::evidence::pc::spOctreeNodeLayout;
    using Owner = std::array<std::uint8_t, sizeof(Layout)>;
    using Ray = std::array<std::uint8_t, 24>;

    constexpr std::uint32_t Primary = 0x006E4420u;
    constexpr std::size_t PivotOffset = offsetof(Layout, pivot);
    constexpr std::size_t ScratchOffset = offsetof(Layout, rayCandidates);
    static_assert(sizeof(Layout) == 0xC8 && PivotOffset == 0x84 && ScratchOffset == 0x90);

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

    template <std::size_t Size>
    std::array<std::uint8_t, Size> ParseHex(std::string_view hex)
    {
        Require(hex.size() == Size * 2, "unexpected raw buffer length");
        std::array<std::uint8_t, Size> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            unsigned value = 0;
            const char* first = hex.data() + i * 2;
            const auto parsed = std::from_chars(first, first + 2, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != first + 2)
                throw std::runtime_error("expected a hexadecimal buffer");
            bytes[i] = static_cast<std::uint8_t>(value);
        }
        return bytes;
    }

    struct Result final
    {
        std::uint32_t count = 0;
        std::uint32_t outputOffset = 0xFFFFFFFFu;
    };

    Result UpdatePrefix(Owner& raw, const Ray& ray, bool requestOutput)
    {
        if (GetWord(raw, 0) != Primary)
            return {};
        Node::RayCandidateCacheForAnalysis cache;
        Node::RayInputForAnalysis input;
        for (std::size_t i = 0; i < 3; ++i)
        {
            cache.pivotBits[i] = GetWord(raw, PivotOffset + 4 * i);
            input.originBits[i] = GetWord(ray, 4 * i);
            input.directionBits[i] = GetWord(ray, 12 + 4 * i);
        }
        for (std::size_t i = 0; i < cache.scratch.size(); ++i)
            cache.scratch[i] = {GetWord(raw, ScratchOffset + 8 * i), GetWord(raw, ScratchOffset + 8 * i + 4)};

        const Node::RayCandidateForAnalysis* borrowed = nullptr;
        const auto count = Node::UpdateRayCandidatesForAnalysis(cache, input, requestOutput ? &borrowed : nullptr);
        Require(borrowed == (count != 0 && requestOutput ? cache.scratch.data() : nullptr),
            "optional result must borrow the four-pair cache");
        if (count == 0)
            return {};
        for (std::size_t i = 0; i < cache.scratch.size(); ++i)
        {
            PutWord(raw, ScratchOffset + 8 * i, cache.scratch[i].octant);
            PutWord(raw, ScratchOffset + 8 * i + 4, cache.scratch[i].timeBits);
        }
        return {count, borrowed != nullptr ? static_cast<std::uint32_t>(ScratchOffset) : 0xFFFFFFFFu};
    }

    template <std::size_t Size>
    void WriteHex(const std::array<std::uint8_t, Size>& raw)
    {
        for (const auto byte : raw)
            std::cout << std::setw(2) << static_cast<unsigned>(byte);
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string flag, ownerText, rayText, extra;
            Require(bool(fields >> flag >> ownerText >> rayText) && !(fields >> extra), "expected output flag and two raw buffers");
            Require(flag == "out" || flag == "null", "expected out or null");
            auto owner = ParseHex<sizeof(Layout)>(ownerText);
            const auto ray = ParseHex<24>(rayText);
            const auto result = UpdatePrefix(owner, ray, flag == "out");
            Require(result.count != 0, "qualified replay was refused");
            std::cout << std::hex << std::setfill('0') << std::setw(8) << result.count << ' ';
            WriteHex(owner);
            std::cout << ' ';
            WriteHex(ray);
            std::cout << ' ' << std::setw(8) << result.outputOffset << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay stream failed");
        return 0;
    }

    struct Witness final
    {
        std::array<std::uint32_t, 3> pivot;
        std::array<std::uint32_t, 6> ray;
        std::array<std::uint32_t, 8> expectedScratch;
        std::uint32_t count;
        bool output;
    };

    constexpr std::array<Witness, 12> Witnesses{{
        // archived distinct
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xC0000000u, 0xC0400000u, 0xC0800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u},
         {0x00000000u, 0x00000000u, 0x00000001u, 0x40000000u, 0x00000003u, 0x40400000u, 0x00000007u, 0x40800000u}, 4u, true},
        // archived diagonal tail
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xC0000000u, 0xC0000000u, 0xC0000000u, 0x3F800000u, 0x3F800000u, 0x3F800000u},
         {0x00000000u, 0x00000000u, 0x00000007u, 0x40000000u, 0x00000007u, 0x7FC12345u, 0xBAD00106u, 0x80000000u}, 2u, true},
        // archived away
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0x40000000u, 0x40400000u, 0x40800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u},
         {0x00000007u, 0x00000000u, 0xBAD00202u, 0xBAD00203u, 0xBAD00204u, 0x7FC12345u, 0xBAD00206u, 0x80000000u}, 1u, true},
        // archived zero direction
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xC0000000u, 0xC0400000u, 0xC0800000u, 0x00000000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0x00000000u, 0xBAD00302u, 0xBAD00303u, 0xBAD00304u, 0x7FC12345u, 0xBAD00306u, 0x80000000u}, 1u, true},
        // descending crossings null out
        {{0x41200000u, 0xC0800000u, 0x40000000u},
         {0x40C00000u, 0xC0E00000u, 0x00000000u, 0x3F800000u, 0x3F800000u, 0x3F800000u},
         {0x00000000u, 0x00000000u, 0x00000004u, 0x40000000u, 0x00000006u, 0x40400000u, 0x00000007u, 0x40800000u}, 4u, false},
        // negative direction distinct
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0x40000000u, 0x40800000u, 0x40400000u, 0xBF800000u, 0xBF800000u, 0xBF800000u},
         {0x00000007u, 0x00000000u, 0x00000006u, 0x40000000u, 0x00000002u, 0x40400000u, 0x00000000u, 0x40800000u}, 4u, true},
        // negative zero live time and duplicate tail
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0x00000000u, 0xBA03126Fu, 0x3F800000u, 0xBF800000u, 0x3F800000u, 0x00000000u},
         {0x00000004u, 0x00000000u, 0x00000006u, 0x80000000u, 0x00000006u, 0x7FC12345u, 0xBAD00606u, 0x80000000u}, 2u, true},
        // below at and above parallel threshold
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF800000u, 0xBF800000u, 0xBF800000u, 0x3727C5ABu, 0x3727C5ACu, 0x3727C5ADu},
         {0x00000000u, 0x00000000u, 0x00000007u, 0x47C34FFFu, 0xBAD00704u, 0x7FC12345u, 0xBAD00706u, 0x80000000u}, 2u, true},
        // yz product spill changes octant
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF800000u, 0xBF336113u, 0xBE99C0ECu, 0x3F800000u, 0x3F333333u, 0x3E99999Au},
         {0x00000000u, 0x00000000u, 0x00000001u, 0x3F800000u, 0x00000007u, 0x3F8020C5u, 0x00000007u, 0x80000000u}, 3u, true},
        // x unspilled product changes octant
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF336113u, 0xBF800000u, 0x3F800000u, 0x3F333333u, 0x3F800000u, 0x00000000u},
         {0x00000004u, 0x00000000u, 0x00000007u, 0x3F8020C5u, 0x00000007u, 0x7FC12345u, 0xBAD00906u, 0x80000000u}, 2u, true},
        // equal stored time keeps x before y
        {{0x00000000u, 0x00000000u, 0x00000000u},
         {0xBF800000u, 0xC0000000u, 0xBF801893u, 0x3F800000u, 0x40000000u, 0x3F800000u},
         {0x00000000u, 0x00000000u, 0x00000007u, 0x3F800000u, 0x00000003u, 0x3F800000u, 0x00000007u, 0x80000000u}, 3u, true},
        // finite magnitude endpoints
        {{0x33800000u, 0xB3800000u, 0x00000000u},
         {0xB3800000u, 0x33800000u, 0x3F800000u, 0x33800000u, 0xB3800000u, 0xC5800000u},
         {0x00000006u, 0x00000000u, 0x00000002u, 0x39800000u, 0xBAD00B04u, 0x7FC12345u, 0xBAD00B06u, 0x80000000u}, 2u, true},
    }};

    Owner StagedOwner(std::size_t index, const Witness& witness)
    {
        Owner owner{};
        for (std::size_t offset = 0; offset < owner.size(); ++offset)
            owner[offset] = static_cast<std::uint8_t>(23 * index + 47 + 39 * offset);
        PutWord(owner, 0, Primary);
        for (std::size_t i = 0; i < witness.pivot.size(); ++i)
            PutWord(owner, PivotOffset + 4 * i, witness.pivot[i]);
        for (std::size_t i = 0; i < witness.expectedScratch.size(); ++i)
            PutWord(owner, ScratchOffset + 4 * i, 0xBAD00000u + static_cast<std::uint32_t>(index * 0x100 + i));
        PutWord(owner, ScratchOffset + 20, 0x7FC12345u);
        PutWord(owner, ScratchOffset + 28, 0x80000000u);
        return owner;
    }

    void CheckTransitions()
    {
        for (std::size_t i = 0; i < Witnesses.size(); ++i)
        {
            const auto& witness = Witnesses[i];
            auto owner = StagedOwner(i, witness);
            auto expected = owner;
            Ray ray{};
            for (std::size_t word = 0; word < witness.ray.size(); ++word)
                PutWord(ray, word * 4, witness.ray[word]);
            for (std::size_t word = 0; word < witness.expectedScratch.size(); ++word)
                PutWord(expected, ScratchOffset + word * 4, witness.expectedScratch[word]);
            const auto result = UpdatePrefix(owner, ray, witness.output);
            Require(result.count == witness.count, "active ray-candidate count differs");
            Require(owner == expected, "full owner differs from the observed scratch effects");
            Require(result.outputOffset == (witness.output ? ScratchOffset : 0xFFFFFFFFu), "optional output relation differs");
        }
    }

    void CheckRefusal()
    {
        const Node::RayCandidateForAnalysis originalBorrow{7, 0x80000000u};
        for (const auto invalid : {1u, 0x80000001u, 0x337FFFFFu, 0x45800001u, 0x7F800000u, 0x7FC12345u})
        {
            Node::RayCandidateCacheForAnalysis cache;
            cache.scratch[0] = {7, 0x80000000u};
            Node::RayInputForAnalysis ray;
            ray.directionBits[1] = invalid;
            const auto* borrowed = &originalBorrow;
            Require(Node::UpdateRayCandidatesForAnalysis(cache, ray, &borrowed) == 0, "out-of-profile raw input was admitted");
            Require(borrowed == &originalBorrow && cache.scratch[0].octant == 7 && cache.scratch[0].timeBits == 0x80000000u,
                "refused query changed scratch or borrowed result");
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
        CheckTransitions();
        CheckRefusal();
        std::cout << "PASS " << checks << " octree ray candidate checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
