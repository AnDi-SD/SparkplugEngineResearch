#include "Analysis/PC/spMovieTextureControllerAbi.h"
#include "Analysis/PC/spVideoStreamAbi.h"
#include "Code/Sparkplug/spMovieTextureController.h"
#include "Code/SparkplugPC/spPCVideoStream.h"

#include <array>
#include <charconv>
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
    using namespace sparkplug::reconstruction;
    namespace Abi = sparkplug::evidence::pc;

    static_assert(spMovieTextureController::ClassID == Abi::MovieTextureControllerClassId);
    static_assert(offsetof(Abi::VideoStreamLayout32, byte18) == Abi::MovieTextureVideoGate);

    unsigned checks{};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
        ++checks;
    }

    std::uint32_t ParseHex(std::string_view text, std::size_t width)
    {
        if (text.size() != width)
            throw std::runtime_error("unexpected hexadecimal width");
        std::uint32_t value{};
        const auto end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, value, 16);
        if (result.ec != std::errc{} || result.ptr != end)
            throw std::runtime_error("expected hexadecimal input");
        return value;
    }

    float DeltaFromBits(std::uint32_t bits)
    {
        float delta{};
        static_assert(sizeof(delta) == sizeof(bits));
        std::memcpy(&delta, &bits, sizeof(delta));
        return delta;
    }

    int Replay()
    {
        spPCVideoStream video;
        spMovieTextureController::BorrowedPcMaterialForAnalysis material{video};
        spMovieTextureController controller(material);
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            std::string deltaHex, gateHex, adjacentHex, wordHex, extra;
            if (!(fields >> deltaHex >> gateHex >> adjacentHex >> wordHex) || fields >> extra)
                throw std::runtime_error("expected delta bits and three video fields");
            const auto delta = DeltaFromBits(ParseHex(deltaHex, 8));
            video.SetStateForAnalysis({std::uint8_t(ParseHex(gateHex, 2)),
                                       std::uint8_t(ParseHex(adjacentHex, 2)), ParseHex(wordHex, 8)});
            const bool dispatched = controller.ApplyAndObserveForAnalysis(delta);
            const auto& state = video.GetStateForAnalysis();
            std::cout << std::hex << std::setfill('0')
                      << std::setw(2) << static_cast<unsigned>(state.byte18) << ' '
                      << std::setw(2) << static_cast<unsigned>(state.byte19) << ' '
                      << std::setw(8) << state.word20 << ' ' << std::dec << dispatched << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckController()
    {
        spPCVideoStream video;
        spMovieTextureController::BorrowedPcMaterialForAnalysis material{video};
        spMovieTextureController controller(material);
        spSubController& slot = controller;
        Require(slot.IsExactly(spMovieTextureController::ClassID) && slot.IsKindOf(spSubController::ClassID),
                "controller type metadata differs from the PC registration");
        Require(!spMovieTextureController::StaticRTTI().factory, "unqualified host factory was exposed");

        struct Case
        {
            std::uint32_t deltaBits;
            spVideoStream::StateForAnalysis state;
            bool dispatch;
        };
        constexpr std::array<Case, 6> cases{{
            {0x7FC00001, {0x00, 0xFF, 0x01020304}, true},
            {0x80000000, {0x00, 0x00, 0xA5A5A5A5}, true},
            {0x7F800000, {0x01, 0x12, 0xFFFFFFFF}, false},
            {0xFF800000, {0x02, 0x00, 0x80000000}, false},
            {0x00000001, {0x80, 0x86, 0xFFFFFFFF}, false},
            {0x3E800000, {0xFF, 0xA5, 0x00000000}, false},
        }};
        for (const auto& item : cases)
        {
            video.SetStateForAnalysis(item.state);
            const auto delta = DeltaFromBits(item.deltaBits);
            const auto matchesState = [&]
            {
                const auto& actual = video.GetStateForAnalysis();
                return actual.byte18 == item.state.byte18 && actual.byte19 == item.state.byte19
                    && actual.word20 == item.state.word20;
            };

            Require(controller.ApplyAndObserveForAnalysis(delta) == item.dispatch,
                    "dispatch disagrees with the raw video gate");
            Require(matchesState(), "dispatch changed captured video state");
            slot.ApplyForAnalysis(delta);
            Require(matchesState(), "void subcontroller slot changed captured video state");
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
        CheckController();
        std::cout << "MovieTextureController: " << checks << " contracts passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
