#include "Analysis/PC/SparkplugAbi.h"
#include "Code/Sparkplug/spCamera.h"
#include "Code/SparkplugDX/spDXCamera.h"

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
    using Camera = sparkplug::reconstruction::spCamera;
    using State = Camera::OrthographicProjectionCacheForAnalysis;
    using Layout = sparkplug::evidence::pc::spCameraObservedLayout;
    using Prefix = std::array<std::uint8_t, sizeof(Layout)>;
    constexpr std::uint32_t DXCameraTable = 0x006EF1E0;
    constexpr std::array<std::size_t, 4> StoredCells{0, 5, 10, 14};
    constexpr std::size_t MatrixOffset = offsetof(Layout, projectionMatrix);
    constexpr std::size_t DirtyOffset = offsetof(Layout, dirtyFlags);
    constexpr std::size_t BranchOffset = offsetof(Layout, projectionBranch);
    static_assert(sizeof(Layout) == 0x238 && BranchOffset == 0x231);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t GetWord(const Prefix& bytes, std::size_t offset)
    {
        std::uint32_t word{};
        for (unsigned i = 0; i < 4; ++i)
            word |= std::uint32_t(bytes[offset + i]) << (8 * i);
        return word;
    }

    void PutWord(Prefix& bytes, std::size_t offset, std::uint32_t word)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
    }

    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        Require(hex.size() == bytes.size() * 2, "expected a full 568-byte camera prefix");
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            unsigned byte{};
            const char* first = hex.data() + i * 2;
            const auto result = std::from_chars(first, first + 2, byte, 16);
            if (result.ec != std::errc{} || result.ptr != first + 2)
                throw std::runtime_error("expected camera prefix in hexadecimal");
            bytes[i] = static_cast<std::uint8_t>(byte);
        }
        return bytes;
    }

    bool UpdatePrefix(Prefix& prefix)
    {
        if (GetWord(prefix, 0) != DXCameraTable)
            return false;
        State cache;
        cache.halfExtent = GetWord(prefix, offsetof(Layout, verticalHalfExtent));
        cache.aspectExtent = GetWord(prefix, offsetof(Layout, aspectHalfExtent));
        cache.nearPlane = GetWord(prefix, offsetof(Layout, nearClipPlane));
        cache.farPlane = GetWord(prefix, offsetof(Layout, farClipPlane));
        cache.projectionBranch = prefix[BranchOffset];
        cache.dirtyFlags = GetWord(prefix, DirtyOffset);
        for (std::size_t i = 0; i < cache.matrixWords.size(); ++i)
            cache.matrixWords[i] = GetWord(prefix, MatrixOffset + i * 4);
        if (!Camera::TryUpdateOrthographicProjectionCacheForAnalysis(cache))
            return false;
        PutWord(prefix, offsetof(Layout, verticalHalfExtent), cache.halfExtent);
        PutWord(prefix, offsetof(Layout, aspectHalfExtent), cache.aspectExtent);
        PutWord(prefix, offsetof(Layout, nearClipPlane), cache.nearPlane);
        PutWord(prefix, offsetof(Layout, farClipPlane), cache.farPlane);
        prefix[BranchOffset] = cache.projectionBranch;
        PutWord(prefix, DirtyOffset, cache.dirtyFlags);
        for (std::size_t i = 0; i < cache.matrixWords.size(); ++i)
            PutWord(prefix, MatrixOffset + i * 4, cache.matrixWords[i]);
        return true;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string hex, extra;
            Require(bool(fields >> hex) && !(fields >> extra), "expected one camera prefix per line");
            auto prefix = ParsePrefix(hex);
            Require(UpdatePrefix(prefix), "qualified camera replay refused");
            std::cout << std::hex << std::setfill('0');
            for (const auto byte : prefix)
                std::cout << std::setw(2) << static_cast<unsigned>(byte);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "camera replay input or output failed");
        return 0;
    }

    struct Witness final
    {
        std::uint32_t halfExtent, aspectExtent, nearPlane, farPlane, dirty;
        std::uint8_t branch;
        std::array<std::uint32_t, 4> stored;
    };

    constexpr std::array<Witness, 8> Witnesses{{
        {0x3F13CD3A, 0x3F93CD3A, 0x3F800000, 0x461C4000, 0xDEADBEEF, 1,
         {0x405DB3D8, 0x3FDDB3D8, 0x38D1BC76, 0xB8D1BC76}},
        {0x40400000, 0x3F333333, 0x3F800000, 0x41200000, 0, 2,
         {0x3F2AAAAB, 0x4036DB6E, 0x3DE38E39, 0xBDE38E39}},
        {0x3F800000, 0x37800000, 0x3F800000, 0x3F800001, 0xFFFFFFFF, 0x80,
         {0x40000000, 0x48000000, 0x4B000000, 0xCB000000}},
        {0x37800000, 0x47800000, 0x43800000, 0x47800000, 2, 0xFF,
         {0x48000000, 0x38000000, 0x37808081, 0xBB808081}},
        {0x3FA66666, 0x40F59BA6, 0x38000000, 0x464FA930, 3, 2,
         {0x3FC4EC4F, 0x3E856A75, 0x389DCBB5, 0xB11DCBB5}},
        {0x3E0F5C29, 0x3F9374BC, 0x38800000, 0x457EBA8F, 0xDABEEF02, 0x80,
         {0x41649249, 0x3FDE38E4, 0x3980A389, 0xB280A389}},
        {0x40E00000, 0x3F400000, 0x40000000, 0x40400000, 0x80000003, 0xFF,
         {0x3E924925, 0x402AAAAB, 0x3F800000, 0xC0000000}},
        {0x38000000, 0x40000001, 0x37800000, 0x3F800001, 0xCAFEBABE, 1,
         {0x47800000, 0x3F7FFFFE, 0x3F80007F, 0xB780007F}},
    }};

    Prefix MakePrefix(const Witness& witness)
    {
        Prefix prefix{};
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = static_cast<std::uint8_t>(37 * i + 41);
        PutWord(prefix, 0, DXCameraTable);
        PutWord(prefix, offsetof(Layout, verticalHalfExtent), witness.halfExtent);
        PutWord(prefix, offsetof(Layout, aspectHalfExtent), witness.aspectExtent);
        PutWord(prefix, offsetof(Layout, nearClipPlane), witness.nearPlane);
        PutWord(prefix, offsetof(Layout, farClipPlane), witness.farPlane);
        PutWord(prefix, MatrixOffset + 11 * 4, 0x7FC12345);
        PutWord(prefix, MatrixOffset + 15 * 4, 0x80000000);
        PutWord(prefix, DirtyOffset, witness.dirty);
        prefix[BranchOffset] = witness.branch;
        return prefix;
    }

    void CheckTransitions()
    {
        for (const auto& witness : Witnesses)
        {
            auto actual = MakePrefix(witness);
            auto expected = actual;
            for (std::size_t i = 0; i < StoredCells.size(); ++i)
                PutWord(expected, MatrixOffset + StoredCells[i] * 4, witness.stored[i]);
            PutWord(expected, DirtyOffset, witness.dirty & 0xFFFFFFFDu);
            Require(UpdatePrefix(actual), "qualified camera cache refused");
            Require(actual == expected, "camera prefix differs from the four stores and dirty update");
        }
    }

    void CheckProfileRefusals()
    {
        State base;
        base.halfExtent = 0x40800000;
        base.aspectExtent = 0x41000000;
        base.nearPlane = 0x3F800000;
        base.farPlane = 0x40A00000;
        base.projectionBranch = 1;
        base.matrixWords.fill(0xA5A5A5A5);
        base.dirtyFlags = 0x33333333;
        const auto check = [&base](State state)
        {
            Require(!Camera::TryUpdateOrthographicProjectionCacheForAnalysis(state), "unqualified state accepted");
            Require(state.matrixWords == base.matrixWords && state.dirtyFlags == base.dirtyFlags,
                "refused update changed projection state");
        };
        auto sample = base;
        sample.projectionBranch = 0;
        check(sample);
        sample = base;
        sample.farPlane = sample.nearPlane;
        check(sample);
        sample.farPlane = 0x3F000000;
        check(sample);
        sample = base;
        sample.nearPlane = 0x3F800001;
        check(sample);
        sample.nearPlane = 0x44000000;
        sample.farPlane = 0x45000000;
        check(sample);
        for (const auto invalid : {0u, 1u, 0x80000000u, 0xBF800000u, 0x7F800000u,
                                   0x7FC12345u, 0x377FFFFFu, 0x47800001u})
        {
            sample = base;
            sample.halfExtent = invalid;
            check(sample);
            sample = base;
            sample.aspectExtent = invalid;
            check(sample);
            sample = base;
            sample.nearPlane = invalid;
            check(sample);
            sample = base;
            sample.farPlane = invalid;
            check(sample);
        }
    }

    void CheckFreshUtility()
    {
        const sparkplug::reconstruction::spDXCamera camera;
        Camera::Matrix4 fresh{};
        fresh.fill(9.0F);
        Require(camera.BuildProjectionMatrixForAnalysis(fresh, true), "fresh orthographic utility failed");
        Require(fresh[11] == 0.0F && fresh[15] == 1.0F, "fresh orthographic utility did not start from identity");
        Require(camera.BuildProjectionMatrixForAnalysis(fresh), "fresh perspective utility failed");
        Require(fresh[11] == 1.0F && fresh[15] == 0.0F, "fresh perspective utility changed");
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
        CheckProfileRefusals();
        CheckFreshUtility();
        std::cout << "PASS " << checks << " camera orthographic cache checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
