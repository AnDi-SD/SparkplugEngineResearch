#include "Code/SparkplugDX/spDXSceneGraphOptimizer.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using Optimizer = sparkplug::reconstruction::spDXSceneGraphOptimizer;
    using Owner = Optimizer::CapturedOwnerForAnalysis;
    using Group = Optimizer::CapturedGroupSentinelForAnalysis;
    using Batch = Optimizer::CapturedBatchSentinelForAnalysis;

    struct Capture
    {
        Owner owner{};
        Group group{};
        Batch batch{};
    };

    unsigned checks{};

    void Require(bool result, const char* message)
    {
        if (!result)
            throw std::runtime_error(message);
        ++checks;
    }

    void StoreWord(std::uint8_t* bytes, std::uint32_t word)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = std::uint8_t(word >> (i * 8));
    }

    std::uint32_t WordAt(const std::uint8_t* bytes)
    {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
            | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }

    template<std::size_t Size>
    std::array<std::uint8_t, Size> ParseBytes(std::string_view text)
    {
        std::array<std::uint8_t, Size> bytes{};
        if (text.size() != Size * 2)
            throw std::runtime_error("wrong captured slice length");
        for (std::size_t i = 0; i < Size; ++i)
        {
            const auto start = text.data() + 2 * i;
            unsigned word{};
            const auto result = std::from_chars(start, start + 2, word, 16);
            if (result.ec != std::errc{} || result.ptr != start + 2)
                throw std::runtime_error("expected a hexadecimal byte");
            bytes[i] = std::uint8_t(word);
        }
        return bytes;
    }

    template<std::size_t Size>
    void PrintBytes(const std::array<std::uint8_t, Size>& bytes)
    {
        for (const auto byte : bytes)
            std::cout << std::setw(2) << static_cast<unsigned>(byte);
    }

    Capture MakeCapture(std::uint32_t groupAddress, std::uint32_t batchAddress, std::uint8_t marker)
    {
        Capture result;
        StoreWord(result.owner.data(), 0x006F1E2C);
        StoreWord(result.owner.data() + 0x14, 0x006F1E28);
        StoreWord(result.owner.data() + 0x18, 0x006F1E14);
        StoreWord(result.owner.data() + 0x40, batchAddress);
        StoreWord(result.owner.data() + 0x4C, groupAddress);
        result.owner[0x0A] = marker;
        result.group.capturedAddress = groupAddress;
        for (const auto offset : {0u, 4u, 8u})
            StoreWord(result.group.bytes.data() + offset, groupAddress);
        result.group.bytes[0x1D] = 1;
        result.batch.capturedAddress = batchAddress;
        StoreWord(result.batch.bytes.data(), batchAddress);
        StoreWord(result.batch.bytes.data() + 4, batchAddress);
        return result;
    }

    void CheckEmpty()
    {
        struct Fixture
        {
            std::uint32_t groupAddress, batchAddress;
            std::uint8_t marker;
        };
        constexpr Fixture fixtures[]{
            {0x31000700, 0x31000760, 0x11},
            {0x31000760, 0x31000700, 0xA5},
            {0x31000810, 0x310008E0, 0x7F},
            {0x310009A0, 0x31000A40, 0xEA},
        };
        for (const auto& fixture : fixtures)
        {
            const auto capture = MakeCapture(fixture.groupAddress, fixture.batchAddress, fixture.marker);
            const auto before = capture;
            std::uint8_t output = 0xA5;
            Require(Optimizer::TryReadPcEmptyOnNodeForAnalysis(capture.owner, capture.group, capture.batch, output)
                && output == 1 && capture.owner == before.owner
                && capture.group.bytes == before.group.bytes && capture.batch.bytes == before.batch.bytes,
                "empty typed sentinels changed or returned the wrong AL");
        }
    }

    void CheckRefusals()
    {
        const auto valid = MakeCapture(0x31000700, 0x31000760, 0x11);
        const auto refuse = [](const Capture& capture)
        {
            std::uint8_t output = 0xA5;
            Require(!Optimizer::TryReadPcEmptyOnNodeForAnalysis(capture.owner, capture.group, capture.batch, output)
                && output == 0xA5, "unqualified capture changed the result");
        };
        auto modified = valid;
        modified.owner[0] ^= 1;
        refuse(modified);
        modified = valid;
        modified.owner[0x18] ^= 1;
        refuse(modified);
        modified = valid;
        StoreWord(modified.owner.data() + 0x4C, 0);
        refuse(modified);
        modified = valid;
        StoreWord(modified.owner.data() + 0x40, 0);
        refuse(modified);
        modified = valid;
        modified.group.capturedAddress += 4;
        refuse(modified);
        modified = valid;
        modified.batch.capturedAddress += 4;
        refuse(modified);
        modified = valid;
        modified.group.bytes[0] ^= 1;
        refuse(modified);
        modified = valid;
        modified.batch.bytes[0] ^= 1;
        refuse(modified);
        refuse(MakeCapture(0x31000700, 0x31000700, 0x11));
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string ownerHex, groupHex, batchHex, extra;
            if (!(fields >> ownerHex >> groupHex >> batchHex) || fields >> extra)
                throw std::runtime_error("expected three captured slices");
            const auto owner = ParseBytes<Owner{}.size()>(ownerHex);
            const Group group{WordAt(owner.data() + 0x4C), ParseBytes<0x20>(groupHex)};
            const Batch batch{WordAt(owner.data() + 0x40), ParseBytes<0x0C>(batchHex)};
            std::uint8_t al{};
            Require(Optimizer::TryReadPcEmptyOnNodeForAnalysis(owner, group, batch, al),
                "qualified empty replay was refused");
            std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(al) << ' ';
            PrintBytes(owner);
            std::cout << ' ';
            PrintBytes(group.bytes);
            std::cout << ' ';
            PrintBytes(batch.bytes);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
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
        CheckEmpty();
        CheckRefusals();
        std::cout << "DXSceneGraphOptimizerEmptyOnNode: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
