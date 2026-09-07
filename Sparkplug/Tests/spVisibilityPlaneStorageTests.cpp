#include "Analysis/PC/spVisibilityPlaneStorage.h"
#include <iostream>
#include <string>

namespace ps = sparkplug::evidence::pc::visibility_storage;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
    }
    template <typename Action> void CheckRejected(Action action, const char* label)
    {
        bool rejected = false;
        try
        {
            action();
        }
        catch (const std::out_of_range&)
        {
            rejected = true;
        }
        Check(rejected, label);
    }
    void Batch()
    {
        unsigned operations;
        while (std::cin >> operations)
        {
            if (operations > 16)
                throw std::runtime_error("bounded operation count");
            ps::PlaneSetForAnalysis value;
            value.allocatorWord = 0xabcddcba;
            value.activeCount = 77;
            for (unsigned op = 0; op < operations; ++op)
            {
                unsigned kind, count, enabled{};
                ps::Plane fill;
                if (!(std::cin >> kind >> count))
                    throw std::runtime_error("incomplete plane operation");
                if (kind == 0)
                {
                    for (auto& bits : fill.equation)
                        if (!(std::cin >> bits))
                            throw std::runtime_error("incomplete plane equation");
                    if (!(std::cin >> enabled))
                        throw std::runtime_error("incomplete plane enabled byte");
                    fill.enabled = static_cast<std::uint8_t>(enabled);
                    ps::ResizeForAnalysis(value, count, fill);
                }
                else if (kind == 1)
                    ps::SetEnabledAllForAnalysis(value, count);
                else if (kind == 2)
                    ps::ReleaseVectorForAnalysis(value);
                else if (kind == 3)
                {
                    ps::PlaneSetForAnalysis copy;
                    copy.allocatorWord = 0xdddddddd;
                    copy.activeCount = 0xdddddddd;
                    ps::InitializeVectorCopyForAnalysis(copy, value);
                    value = std::move(copy);
                }
                else
                    throw std::runtime_error("unknown operation");
                std::cout << value.allocatorWord << ',' << value.logicalSize << ','
                          << value.storage.size() << ',' << value.activeCount;
                for (unsigned i = 0; i < value.logicalSize; ++i)
                {
                    for (auto bits : value.storage[i].equation)
                        std::cout << ',' << bits;
                    std::cout << ',' << static_cast<unsigned>(value.storage[i].enabled);
                }
                std::cout << '\n';
            }
        }
    }
    void UnitTests()
    {
        ps::PlaneSetForAnalysis value;
        value.allocatorWord = 19;
        value.activeCount = 77;
        ps::Plane fill{{0x7fc12345, 0x80000000, 3, 4}, 7, {11, 22, 33}};
        ps::ResizeForAnalysis(value, 3, fill);
        Check(value.logicalSize == 3 && value.storage.size() == 3, "exact growth");
        Check(value.activeCount == 77 && value.allocatorWord == 19, "outer metadata untouched");
        Check(value.storage[0].equation == fill.equation && value.storage[0].enabled == 7,
              "raw equation and enabled bits");
        Check(value.storage[0].padding == std::array<std::uint8_t, 3>{},
              "host new padding0 explicitly not native");
        value.storage[1].padding = {1, 2, 3};
        ps::ResizeForAnalysis(value, 1, fill);
        Check(value.logicalSize == 1 && value.storage.size() == 3, "shrink retains capacity");
        fill.enabled = 0;
        ps::ResizeForAnalysis(value, 3, fill);
        Check(value.storage[0].enabled == 7 && value.storage[1].enabled == 0,
              "only new suffix filled");
        Check(value.storage[1].padding == std::array<std::uint8_t, 3>{1, 2, 3},
              "reused padding preserved");
        ps::ResizeForAnalysis(value, 4, fill);
        Check(value.storage.size() == 4 &&
                  value.storage[1].padding == std::array<std::uint8_t, 3>{},
              "reallocation excludes old padding");
        ps::ResizeForAnalysis(value, 5, fill);
        Check(value.storage.size() == 6,
              "capacity grows by floor1.5 when greater than requested size");
        ps::SetEnabledAllForAnalysis(value, 255);
        Check(value.activeCount == 5 && value.storage[3].enabled == 255,
              "noncanonical byte retained");
        ps::SetEnabledAllForAnalysis(value, 256);
        Check(value.activeCount == 0 && value.storage[0].enabled == 0, "only low byte used");
        ps::SetEnabledAllForAnalysis(value, 1);
        ps::ResizeForAnalysis(value, 0, fill);
        Check(value.activeCount == 5 && value.storage.size() == 6,
              "clear keeps stale active count");
        ps::ReleaseVectorForAnalysis(value);
        Check(value.storage.empty() && value.activeCount == 5 && value.allocatorWord == 19,
              "release keeps outer metadata");
        ps::SetEnabledAllForAnalysis(value, 1);
        Check(value.activeCount == 0, "enable empty repairs count to0");
        ps::ResizeForAnalysis(value, 6, fill);
        ps::ResizeForAnalysis(value, 2, fill);
        ps::PlaneSetForAnalysis copy;
        copy.allocatorWord = 19;
        copy.activeCount = 77;
        ps::InitializeVectorCopyForAnalysis(copy, value);
        Check(copy.logicalSize == 2 && copy.storage.size() == 2,
              "copy constructor discards source spare capacity");
        Check(copy.allocatorWord == 19 && copy.activeCount == 77,
              "copy constructor preserves destination outer metadata");
        value.storage[0].equation[0] = 0;
        Check(copy.storage[0].equation[0] == fill.equation[0], "copy owns independent plane data");

        // Host guards are intentionally not claims about native failure behavior.
        CheckRejected([&] { ps::ResizeForAnalysis(copy, 4097, fill); }, "host count guard");
        Check(copy.logicalSize == 2 && copy.storage.size() == 2 && copy.activeCount == 77,
              "rejected resize preserves prior state");
        CheckRejected([&] { ps::InitializeVectorCopyForAnalysis(copy, copy); },
                      "host self-copy rejection");
        CheckRejected([&] { ps::InitializeVectorCopyForAnalysis(copy, value); },
                      "host constructed-destination rejection");
        ps::PlaneSetForAnalysis invalid;
        invalid.logicalSize = 1;
        CheckRejected([&] { ps::ResizeForAnalysis(invalid, 0, fill); },
                      "host invalid resize range");
        CheckRejected([&] { ps::SetEnabledAllForAnalysis(invalid, 1); },
                      "host invalid enable range");
        ps::PlaneSetForAnalysis fresh;
        CheckRejected([&] { ps::InitializeVectorCopyForAnalysis(fresh, invalid); },
                      "host invalid copy source range");
        Check(fresh.storage.empty() && fresh.logicalSize == 0,
              "invalid copy does not construct destination");

        ps::ResizeForAnalysis(copy, 3, copy.storage[0]);
        Check(copy.logicalSize == 3 && copy.storage[2].equation == fill.equation &&
                  copy.storage[2].enabled == fill.enabled,
              "by-value fill survives growth from aliased storage");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "batch")
            Batch();
        else
        {
            UnitTests();
            std::cout << "PASS " << checks << '/' << checks << ": PC plane storage source\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
