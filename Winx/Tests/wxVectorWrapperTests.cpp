#include "Code/wxVectorWrapper.h"
#include "Analysis/PC/wxVectorWrapperAbi.h"
#include "Analysis/PS2/wxVectorWrapperAbi.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Check(const bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void PrintWords(const wxVectorWrapper::WordsForAnalysis& words)
    {
        for (const auto word : words) std::cout << ' ' << std::hex << word;
    }

    void EmitCases()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream input(line);
            std::string profile;
            wxVectorWrapper::WordsForAnalysis words{};
            if (!(input >> profile >> std::hex >> words[0] >> words[1] >> words[2]))
                throw std::invalid_argument("Expected PC/PS2 and three hex words");
            if (profile != "PC" && profile != "PS2")
                throw std::invalid_argument("Unknown vector Copy platform");
            const auto platform = profile == "PC"
                ? wxVectorWrapper::CopyPlatformForAnalysis::PC
                : wxVectorWrapper::CopyPlatformForAnalysis::PS2;
            wxVectorWrapper source(platform), target(platform);
            source.SetWordsForAnalysis(words);
            target.SetWordsForAnalysis({0xDEADBEEF,0xBAADF00D,0x12345678});
            spCloneManager manager;
            const bool copied = source.vfunc_14(target, manager);
            auto cloned = manager.Clone(source);
            auto* typed = dynamic_cast<wxVectorWrapper*>(cloned.get());
            Check(source.GetWordsForAnalysis() == words, "Copy and Clone preserve source");
            Check(manager.FindClone(source) == nullptr, "Root Clone clears mapping");
            std::cout << profile << ' ' << copied;
            PrintWords(target.GetWordsForAnalysis());
            std::cout << ' ' << static_cast<bool>(typed);
            PrintWords(typed ? typed->GetWordsForAnalysis() : wxVectorWrapper::WordsForAnalysis{});
            std::cout << '\n';
        }
    }

    void TestLifecycleAndCopy()
    {
        wxVectorWrapper object;
        Check(object.GetWordsForAnalysis() == wxVectorWrapper::WordsForAnalysis{}, "Factory payload is zero");
        Check(object.IsExactly(wxVectorWrapper::ClassID), "Exact vector RTTI");
        Check(object.IsKindOf(spBaseObject::ClassID), "Physical and registered base object");
        Check(!object.IsKindOf(spNamedObject::ClassID), "No inferred named-object inheritance");
        auto created = spRTTIManager::Instance().Create(wxVectorWrapper::ClassID);
        auto* factoryObject = dynamic_cast<wxVectorWrapper*>(created.get());
        Check(factoryObject != nullptr && factoryObject->GetWordsForAnalysis() == object.GetWordsForAnalysis(),
            "Registered factory constructs the vector payload");

        spCloneManager manager;
        const wxVectorWrapper::WordsForAnalysis payload{0x80000000,0x00000001,0xFF800000};
        object.SetWordsForAnalysis(payload);
        wxVectorWrapper target(wxVectorWrapper::CopyPlatformForAnalysis::PS2);
        target.SetWordsForAnalysis({1,2,3});
        Check(object.vfunc_14(target, manager) && target.GetWordsForAnalysis() == payload,
            "Copy preserves signed zero, subnormal and infinity words");
        Check(target.GetCopyPlatformForAnalysis() == wxVectorWrapper::CopyPlatformForAnalysis::PS2,
            "Copy leaves the destination analytical profile alone");
        Check(object.vfunc_14(object, manager) && object.GetWordsForAnalysis() == payload, "Self Copy");
        spBaseObject wrongType;
        Check(!object.vfunc_14(wrongType, manager) && object.GetWordsForAnalysis() == payload,
            "Portable type guard leaves the source alone");
        auto cloned = manager.Clone(object);
        auto* vectorClone = dynamic_cast<wxVectorWrapper*>(cloned.get());
        Check(vectorClone && vectorClone != &object && vectorClone->GetWordsForAnalysis() == payload,
            "Clone creates an independent vector");
        Check(manager.FindClone(object) == nullptr, "Root Clone transaction releases mappings");
        object.SetWordsForAnalysis({5,6,7});
        Check(vectorClone->GetWordsForAnalysis() == payload, "Clone payload has no source alias");
    }

    void TestPlatformBoundary()
    {
        spCloneManager manager;
        wxVectorWrapper source, target;
        const auto original = target.GetWordsForAnalysis();
        source.SetWordsForAnalysis({0x7F812345,0x3F800000,0x40000000});
        Check(source.vfunc_14(target, manager) && target.GetWordsForAnalysis() == source.GetWordsForAnalysis(),
            "PC integer X transfer accepts signaling NaN bits");
        const auto before = target.GetWordsForAnalysis();
        source.SetWordsForAnalysis({1,0x7F812345,0xFF812345});
        Check(!source.HasSupportedCopyInputForAnalysis() && !source.vfunc_14(target, manager),
            "PC signaling Y/Z domain is explicitly unqualified");
        Check(target.GetWordsForAnalysis() == before, "Portable preflight refusal leaves destination untouched");
        Check(source.vfunc_10(manager) == nullptr && manager.FindClone(source) == nullptr,
            "Direct unsupported Clone leaves no temporary mapping");
        Check(manager.Clone(source) == nullptr && manager.FindClone(source) == nullptr,
            "Unsupported PC clone returns no object and clears transaction");
        wxVectorWrapper ps2(wxVectorWrapper::CopyPlatformForAnalysis::PS2);
        ps2.SetWordsForAnalysis(source.GetWordsForAnalysis());
        target.SetWordsForAnalysis(original);
        Check(ps2.HasSupportedCopyInputForAnalysis() && ps2.vfunc_14(target, manager)
            && target.GetWordsForAnalysis() == ps2.GetWordsForAnalysis(), "PS2 raw transfer accepts all patterns");
        auto clone = manager.Clone(ps2);
        auto* typed = dynamic_cast<wxVectorWrapper*>(clone.get());
        Check(typed && typed->GetCopyPlatformForAnalysis() == wxVectorWrapper::CopyPlatformForAnalysis::PS2
            && typed->GetWordsForAnalysis() == ps2.GetWordsForAnalysis(), "PS2 clone preserves selected profile");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--emit")
        {
            EmitCases();
            return 0;
        }
        TestLifecycleAndCopy();
        TestPlatformBoundary();
        std::cout << "PASS wxVectorWrapper lifecycle, Copy, Clone and platform boundary\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
