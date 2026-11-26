#include "Analysis/PC/SparkplugAbi.h"
#include "Code/Sparkplug/spCamera.h"

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
    using State = Camera::PerspectiveProjectionCacheForAnalysis;
    using Layout = sparkplug::evidence::pc::spCameraObservedLayout;
    using Prefix = std::array<std::uint8_t, sizeof(Layout)>;

    constexpr std::uint32_t DXCameraTable = 0x006EF1E0;
    constexpr std::size_t HalfOffset = offsetof(Layout, verticalHalfExtent);
    constexpr std::size_t AspectOffset = offsetof(Layout, aspectHalfExtent);
    constexpr std::size_t NearOffset = offsetof(Layout, nearClipPlane);
    constexpr std::size_t FarOffset = offsetof(Layout, farClipPlane);
    constexpr std::size_t MatrixOffset = offsetof(Layout, projectionMatrix);
    constexpr std::size_t DirtyOffset = offsetof(Layout, dirtyFlags);
    constexpr std::size_t BranchOffset = offsetof(Layout, projectionBranch);
    constexpr std::array<std::size_t, 6> StoredCells{0, 5, 11, 15, 10, 14};
    static_assert(sizeof(Layout) == 0x238 && MatrixOffset == 0x10C && BranchOffset == 0x231);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t GetWord(const Prefix& prefix, std::size_t offset)
    {
        std::uint32_t word{};
        for (unsigned byte = 0; byte < 4; ++byte)
            word |= std::uint32_t(prefix[offset + byte]) << (8 * byte);
        return word;
    }

    void PutWord(Prefix& prefix, std::size_t offset, std::uint32_t word)
    {
        for (unsigned byte = 0; byte < 4; ++byte)
            prefix[offset + byte] = static_cast<std::uint8_t>(word >> (8 * byte));
    }

    bool UpdatePrefix(Prefix& prefix)
    {
        if (GetWord(prefix, 0) != DXCameraTable)
            return false;

        State state;
        state.halfExtent = GetWord(prefix, HalfOffset);
        state.aspectExtent = GetWord(prefix, AspectOffset);
        state.nearPlane = GetWord(prefix, NearOffset);
        state.farPlane = GetWord(prefix, FarOffset);
        state.projectionBranch = prefix[BranchOffset];
        state.dirtyFlags = GetWord(prefix, DirtyOffset);
        for (std::size_t cell = 0; cell < state.matrixWords.size(); ++cell)
            state.matrixWords[cell] = GetWord(prefix, MatrixOffset + cell * 4);

        if (!Camera::TryUpdatePerspectiveProjectionCacheForAnalysis(state))
            return false;

        PutWord(prefix, HalfOffset, state.halfExtent);
        PutWord(prefix, AspectOffset, state.aspectExtent);
        PutWord(prefix, NearOffset, state.nearPlane);
        PutWord(prefix, FarOffset, state.farPlane);
        prefix[BranchOffset] = state.projectionBranch;
        PutWord(prefix, DirtyOffset, state.dirtyFlags);
        for (std::size_t cell = 0; cell < state.matrixWords.size(); ++cell)
            PutWord(prefix, MatrixOffset + cell * 4, state.matrixWords[cell]);
        return true;
    }

    Prefix ParsePrefix(std::string_view hex)
    {
        Prefix bytes{};
        Require(hex.size() == bytes.size() * 2, "expected one 568-byte camera prefix");
        for (std::size_t offset = 0; offset < bytes.size(); ++offset)
        {
            unsigned byte{};
            const char* first = hex.data() + offset * 2;
            const auto parse = std::from_chars(first, first + 2, byte, 16);
            if (parse.ec != std::errc{} || parse.ptr != first + 2)
                throw std::runtime_error("expected a hexadecimal camera prefix");
            bytes[offset] = static_cast<std::uint8_t>(byte);
        }
        return bytes;
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
            Require(UpdatePrefix(prefix), "qualified perspective camera replay refused");
            std::cout << std::hex << std::setfill('0');
            for (auto byte : prefix)
                std::cout << std::setw(2) << static_cast<unsigned>(byte);
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "camera replay stream failed");
        return 0;
    }

    struct Witness final
    {
        std::uint32_t half, aspect, nearPlane, farPlane, dirty;
        std::uint8_t serialized2D;
        std::array<std::uint32_t, 6> stored;
    };

    constexpr std::array<Witness, 8> Witnesses{{
        {0x3F13CD3Au, 0x3F93CD3Au, 0x3F800000u, 0x461C4000u, 0xDEADBEEFu, 0x00,
         {0x3FDDB3D8u, 0x3F5DB3D8u, 0x3F800000u, 0x00000000u, 0x3F800347u, 0xBF800347u}},
        {0x40A998FAu, 0x3F1C6021u, 0x3FC00000u, 0x41140000u, 0x00000000u, 0x01,
         {0x3E90E862u, 0x401D2901u, 0x3F800000u, 0x00000000u, 0x3F98C632u, 0xBFE5294Au}},
        {0x3F800000u, 0x37800000u, 0x437FFFFFu, 0x43800000u, 0xFFFFFFFFu, 0xFF,
         {0x437FFFFFu, 0x4B7FFFFFu, 0x3F800000u, 0x00000000u, 0x4B800000u, 0xCF7FFFFFu}},
        {0x37800000u, 0x47800000u, 0x43800000u, 0x47800000u, 0x00000002u, 0x00,
         {0x4B800000u, 0x3B800000u, 0x3F800000u, 0x00000000u, 0x3F808081u, 0xC3808081u}},
        // A direct division would store C318FA2C and C3095A56 in cell 14.
        {0x3FA66666u, 0x40F59BA6u, 0x4173C730u, 0x41875F28u, 0x00000003u, 0x80,
         {0x413B8588u, 0x3FFE17B8u, 0x3F800000u, 0x00000000u, 0x4120A595u, 0xC318FA2Bu}},
        {0x3E0F5C29u, 0x3F9374BCu, 0x41AA0088u, 0x41C91E18u, 0xDABEEF02u, 0x01,
         {0x4317C99Eu, 0x4193923Eu, 0x3F800000u, 0x00000000u, 0x40CED5B1u, 0xC3095A55u}},
        {0x40E00000u, 0x3F400000u, 0x37800000u, 0x37800001u, 0x80000003u, 0x00,
         {0x36124925u, 0x37AAAAABu, 0x3F800000u, 0x00000000u, 0x4B000001u, 0xC3000001u}},
        {0x38000000u, 0x40000001u, 0x37800000u, 0x3F800001u, 0xCAFEBABEu, 0x02,
         {0x3F000000u, 0x36FFFFFEu, 0x3F800000u, 0x00000000u, 0x3F800080u, 0xB7800080u}},
    }};

    Prefix MakePrefix(const Witness& witness)
    {
        Prefix prefix{};
        for (std::size_t offset = 0; offset < prefix.size(); ++offset)
            prefix[offset] = static_cast<std::uint8_t>(37 * offset + 41);
        PutWord(prefix, 0, DXCameraTable);
        PutWord(prefix, HalfOffset, witness.half);
        PutWord(prefix, AspectOffset, witness.aspect);
        PutWord(prefix, NearOffset, witness.nearPlane);
        PutWord(prefix, FarOffset, witness.farPlane);
        PutWord(prefix, MatrixOffset + 11 * 4, 0x7FC12345);
        PutWord(prefix, MatrixOffset + 15 * 4, 0x80000000);
        PutWord(prefix, DirtyOffset, witness.dirty);
        prefix[offsetof(Layout, twoDimensional)] = witness.serialized2D;
        prefix[BranchOffset] = 0;
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
            Require(UpdatePrefix(actual), "qualified perspective cache refused");
            Require(actual == expected, "perspective update differs from six matrix cells and dirty word");
        }
    }

    void CheckRejections()
    {
        const auto baseline = MakePrefix(Witnesses[0]);
        const auto reject = [](Prefix actual)
        {
            const auto before = actual;
            Require(!UpdatePrefix(actual), "unqualified perspective cache accepted");
            Require(actual == before, "rejected perspective cache changed the prefix");
        };

        auto wrongBranch = baseline;
        wrongBranch[BranchOffset] = 1;
        reject(wrongBranch);
        auto wrongOwner = baseline;
        PutWord(wrongOwner, 0, 0x006DEA20);
        reject(wrongOwner);
        auto wrongDepth = baseline;
        PutWord(wrongDepth, FarOffset, 0x3F800000);
        reject(wrongDepth);
        PutWord(wrongDepth, FarOffset, 0x3F000000);
        reject(wrongDepth);
        auto beyondNear = baseline;
        PutWord(beyondNear, NearOffset, 0x43800001);
        reject(beyondNear);

        for (const auto invalid : {0u, 1u, 0x80000000u, 0xBF800000u, 0x7F800000u,
                                   0x7FC12345u, 0x377FFFFFu, 0x47800001u})
        {
            for (const auto offset : {HalfOffset, AspectOffset, NearOffset, FarOffset})
            {
                auto candidate = baseline;
                PutWord(candidate, offset, invalid);
                reject(candidate);
            }
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
        CheckRejections();
        std::cout << "PASS " << checks << " camera perspective cache checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
