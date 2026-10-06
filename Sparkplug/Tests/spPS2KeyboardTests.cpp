#include "Code/SparkplugPS2/spPS2Keyboard.h"
#include "Analysis/PS2/spPS2KeyboardAbi.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* label)
    { ++checks; if (!value) throw std::runtime_error(label); }
    std::uint32_t Bits(float value)
    { std::uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }
    int Batch()
    {
        unsigned op, flag, count = 0;
        std::uint32_t physical;
        while (std::cin >> op >> physical >> flag)
        {
            if (op > 7 || flag > 255 || ++count > 512) throw std::runtime_error("bounded keyboard batch");
            spPS2Keyboard keyboard;
            keyboard.SetField44ForAnalysis(static_cast<std::uint8_t>(flag));
            std::uint32_t result = 0;
            if (op == 0) result = keyboard.ReadyForAnalysis();
            if (op == 1) result = keyboard.PhysicalSlot1ForAnalysis(physical);
            if (op == 2) result = keyboard.PhysicalSlot2ForAnalysis(physical);
            if (op == 3) result = keyboard.PhysicalSlot3ForAnalysis(physical);
            if (op == 4) result = static_cast<std::uint32_t>(keyboard.PhysicalSlot4ForAnalysis(physical));
            if (op == 5) keyboard.PhysicalSlot5ForAnalysis(physical, 0xffffffffu, 17, 0x12345678u);
            if (op == 6) result = Bits(keyboard.PhysicalSlot6ForAnalysis(physical));
            if (op == 7) result = keyboard.InitializeForAnalysis();
            std::cout << '[' << result << ',' << unsigned(keyboard.GetField44ForAnalysis()) << "]\n";
        }
        return 0;
    }
    void Tests()
    {
        spPS2Keyboard keyboard;
        Check(keyboard.IsExactly(spPS2Keyboard::ClassID) && keyboard.IsKindOf(spInputDevice::ClassID), "keyboard identity chain");
        const auto factory = spPS2Keyboard::StaticRTTI().factory;
        Check(factory && factory()->IsExactly(spPS2Keyboard::ClassID), "native non-null keyboard factory");
        Check(keyboard.GetField44ForAnalysis() == 0 && keyboard.ReadyForAnalysis(), "ready before initialize");
        keyboard.SetName("keyboard source");
        keyboard.AppendBindingForAnalysis(100, 9);
        keyboard.AppendBindingForAnalysis(101, 9);
        const auto queries = keyboard.GetQueriesForAnalysis();
        Check(keyboard.QuerySlot1ForAnalysis(9, queries).value() == false && keyboard.QuerySlot2ForAnalysis(9, queries).value() == false,
            "logical Boolean routing through native constant leaves");
        Check(keyboard.QuerySlot3ForAnalysis(9, queries).value() == 0 && keyboard.QuerySlot4ForAnalysis(9, queries).value() == 0,
            "logical scalar routing through native zero leaves");
        Check(keyboard.CommandSlot5ForAnalysis(9, 1, 2, 3, queries) && Bits(keyboard.QuerySlot6ForAnalysis(9, queries).value()) == 0,
            "native command no-op and float positive zero");
        Check(keyboard.GetScratchForAnalysis()[0].value() == 100 && keyboard.GetScratchForAnalysis()[1].value() == 101,
            "physical providers preserve common scratch collection");
        Check(keyboard.InitializeForAnalysis() && keyboard.GetField44ForAnalysis() == 1, "initialize sets common flag");
        auto clone = keyboard.Clone();
        auto* copy = dynamic_cast<spPS2Keyboard*>(clone.get());
        Check(copy && copy->GetName() && std::string(copy->GetName()) == "keyboard source", "keyboard clone preserves physical name");
        Check(copy->GetField44ForAnalysis() == 0 && copy->GetBindingsForAnalysis().empty(), "clone reconstructs flag and map instead of copying them");
        for (const auto& cell : copy->GetScratchForAnalysis()) Check(!cell, "clone scratch remains unwritten");
        keyboard.SetName("changed source");
        Check(std::string(copy->GetName()) == "keyboard source", "clone retains original name ownership");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch") return Batch();
        Tests(); std::cout << "PASS " << checks << '/' << checks << ": PS2 keyboard checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
