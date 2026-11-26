#include "Code/Sparkplug/spAnimationScalarCubicPc.h"

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using sparkplug::reconstruction::AnimationScalarCubicKeyPcForAnalysis;
    using sparkplug::reconstruction::TryPrepareAnimationScalarCubicPcForAnalysis;

    using Key = AnimationScalarCubicKeyPcForAnalysis;
    constexpr auto Poison = 0x7FC12345u;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    std::array<std::uint32_t, 5> Words(const Key& key)
    {
        return {key.valueBits, key.incomingBits, key.outgoingBits, key.quadraticBits, key.cubicBits};
    }

    void CheckDiscriminators()
    {
        std::array<Key, 2> keys{{
            {0xA1800000u, Poison, 0x3FC00000u, Poison, Poison},
            {0x3F800000u, 0, Poison, Poison, Poison},
        }};
        auto original = keys;
        Check(TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), keys.size()), "Quadratic case was rejected");
        Check(keys.front().quadraticBits == 0x22400000u, "Quadratic x87 cancellation lost the low bits");
        Check(Words(keys.back()) == Words(original.back()), "Final key changed");

        keys = original;
        keys.front().outgoingBits = 0x40000000u;
        Check(TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), keys.size()), "Cubic case was rejected");
        Check(keys.front().cubicBits == 0xA2000000u, "Cubic x87 cancellation lost the low bits");

        keys = {{{0, Poison, 0, Poison, Poison}, {0x80000000u, 0, Poison, Poison, Poison}}};
        Check(TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), keys.size()), "Signed zero case was rejected");
        Check(keys.front().quadraticBits == 0x80000000u, "Quadratic negative zero was lost");
        Check(keys.front().cubicBits == 0, "Cubic positive zero was lost");
    }

    template <std::size_t Count>
    void CheckCount()
    {
        std::array<Key, Count + 2> storage{};
        storage.front() = {Poison, Poison, Poison, Poison, Poison};
        storage.back() = storage.front();
        for (std::size_t i = 1; i <= Count; ++i)
            storage[i] = {0x3F800000u, 0, 0, Poison, Poison};

        const auto original = storage;
        Check(TryPrepareAnimationScalarCubicPcForAnalysis(storage.data() + 1, Count), "Count case was rejected");
        Check(Words(storage.front()) == Words(original.front()), "Words before the array changed");
        Check(Words(storage.back()) == Words(original.back()), "Words after the array changed");
        Check(Words(storage[Count]) == Words(original[Count]), "Final key changed");
        for (std::size_t i = 1; i < Count; ++i)
        {
            const auto& key = storage[i];
            Check(Words(key) == Words(Key{0x3F800000u, 0, 0, 0, 0}), "Segment wrote unexpected words");
        }
    }

    void CheckRefusal()
    {
        std::array<Key, 3> keys{{
            {0, Poison, 0, Poison, Poison},
            {0, 0, 0, Poison, Poison},
            {0, Poison, Poison, Poison, Poison},
        }};
        const auto original = keys;
        Check(!TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), 0), "An empty array was accepted");
        Check(!TryPrepareAnimationScalarCubicPcForAnalysis(nullptr, 1), "A null array was accepted");
        Check(!TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), keys.size()), "A consumed NaN was accepted");
        for (std::size_t i = 0; i < keys.size(); ++i)
            Check(Words(keys[i]) == Words(original[i]), "Rejected preparation changed the array");

        auto single = Key{Poison, Poison, Poison, Poison, Poison};
        Check(TryPrepareAnimationScalarCubicPcForAnalysis(&single, 1), "Single unread key was rejected");
        Check(Words(single) == Words(Key{Poison, Poison, Poison, Poison, Poison}), "Single key changed");
    }

    int Replay()
    {
        std::string row;
        while (std::getline(std::cin, row))
        {
            std::istringstream input(row);
            std::size_t count = 0;
            Check(bool(input >> count) && count > 0 && count <= 1024, "Invalid replay key count");
            input >> std::hex;
            std::vector<Key> keys(count);
            for (auto& key : keys)
                Check(bool(input >> key.valueBits >> key.incomingBits >> key.outgoingBits >>
                    key.quadraticBits >> key.cubicBits), "Incomplete replay key");
            input >> std::ws;
            Check(input.eof(), "Unexpected trailing replay words");
            Check(TryPrepareAnimationScalarCubicPcForAnalysis(keys.data(), keys.size()), "Replay input was rejected");

            std::cout << count << std::hex << std::setfill('0');
            for (const auto& key : keys)
                for (const auto word : Words(key))
                    std::cout << ' ' << std::setw(8) << word;
            std::cout << std::dec << '\n';
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
        return Replay();
    Check(argc == 1, "Unexpected arguments");

    CheckDiscriminators();
    CheckCount<1>();
    CheckCount<4>();
    CheckCount<5>();
    CheckCount<6>();
    CheckCount<9>();
    CheckRefusal();
    std::cout << "Scalar cubic coefficient preparation checks passed.\n";
    return 0;
}
