#include "Code/Sparkplug/spTemplate.h"
#include "Code/Sparkplug/spTemplateInstance.h"
#include "Analysis/PC/spTemplateAbi.h"
#include "Analysis/PS2/spTemplateAbi.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
    std::shared_ptr<spTemplateObject> Object(std::uint32_t state, const spTemplate* nested = nullptr)
    {
        auto descriptor = std::make_shared<spTemplateObject>();
        Check(descriptor->SetSerializedDescriptorForAnalysis(state, "descriptor-path"), "descriptor state setup");
        descriptor->SetName("descriptor-native-name");
        if (nested)
        {
            auto instance = std::make_shared<spTemplateInstance>();
            instance->SetTemplateOwnerForAnalysis(nested); descriptor->SetLoadedObjectForAnalysis(instance);
        }
        return descriptor;
    }
    struct Graph
    {
        spTemplate source, target, nested, other;
        Graph()
        {
            source.SetName("source-name"); target.SetName("target-name");
            source.SetResourcePathForAnalysis("source"); target.SetResourcePathForAnalysis("target");
            nested.SetResourcePathForAnalysis("nested"); other.SetResourcePathForAnalysis("other");
            source.SetField20ForAnalysis(0x12345678); target.SetField20ForAnalysis(0xabcdef01);
        }
        void Add(spTemplate& owner, std::uint32_t state, const spTemplate* dependency = nullptr)
        { Check(owner.AddObjectForAnalysis(Object(state, dependency)), "graph descriptor attach"); }
    };
    void Case(std::uint32_t id, bool emit)
    {
        Check(id < 20, "bounded template case"); Graph g;
        switch (id)
        {
        case 0: break;
        case 1: g.Add(g.source, 0, &g.target); break;
        case 2: g.Add(g.source, 1, &g.target); break;
        case 3: g.Add(g.source, 2, &g.target); break;
        case 4: g.Add(g.source, 3, &g.target); break;
        case 5: g.nested.SetResourcePathForAnalysis("target"); g.Add(g.source, 3, &g.nested); break;
        case 6: g.Add(g.source, 3, &g.nested); break;
        case 7: g.Add(g.source, 3, &g.nested); g.Add(g.nested, 3, &g.target); break;
        case 8: g.Add(g.source, 0); g.Add(g.source, 3, &g.target); break;
        case 9: g.Add(g.source, 3, &g.nested); g.Add(g.nested, 3, &g.other); g.other.SetResourcePathForAnalysis("target"); break;
        case 10: g.target.SetResourcePathForAnalysis("abc"); g.nested.SetResourcePathForAnalysis("abcd"); g.Add(g.source, 3, &g.nested); break;
        case 11: g.nested.SetResourcePathForAnalysis("Target"); g.Add(g.source, 3, &g.nested); break;
        case 12: g.nested.SetResourcePathForAnalysis(std::string("a\0b", 3)); g.Add(g.source, 3, &g.nested); break;
        case 13: break;
        case 14: g.Add(g.source, 0); break;
        case 15: g.Add(g.source, 1); break;
        case 16: g.Add(g.source, 0); g.Add(g.source, 2); break;
        case 17: g.Add(g.source, 0); g.Add(g.source, 1); g.Add(g.source, 2); break;
        case 18: g.Add(g.source, 3, &g.target); g.Add(g.target, 0); break;
        case 19: g.Add(g.source, 3, &g.nested); g.Add(g.target, 0); break;
        }
        const auto dependency = g.source.DependsOnTemplateForAnalysis(g.target);
        Check(dependency.has_value(), "native-valid graph has known dependency result");
        std::ostringstream out; out << "{\"case\":" << id << ",\"dependency\":" << (*dependency ? "true" : "false");
        if (id >= 13)
        {
            spCloneManager clones;
            Check(g.source.vfunc_14(g.target, clones) == !*dependency, "copy rejects dependency before destination clear");
            Check(std::strcmp(g.target.GetName(), "target-name") == 0 && g.target.GetResourcePathForAnalysis() == "target",
                "native copy preserves destination Named and path fields");
            out << ",\"copy\":" << (!*dependency ? "true" : "false") << ",\"field20\":" << g.target.GetField20ForAnalysis() << ",\"states\":[";
            bool first = true;
            for (const auto& descriptor : g.target.GetObjectsForAnalysis())
            { if (!first) out << ','; first = false; out << *descriptor->GetNativeStateForAnalysis(); }
            out << ']';
        }
        out << '}'; if (emit) std::cout << out.str() << '\n';
    }
    void OwnershipAndUnknowns()
    {
        Check(spTemplate::StaticRTTI().baseClassID == spNamedObject::ClassID, "registered template Named parent");
        auto factory = spRTTIManager::Instance().Create(spTemplate::ClassID);
        auto* fresh = dynamic_cast<spTemplate*>(factory.get());
        Check(fresh && fresh->GetObjectsForAnalysis().empty() && fresh->GetField20ForAnalysis() == 0
            && fresh->GetResourcePathForAnalysis().empty(), "factory has constructor-fresh semantic state");
        Graph g; auto descriptor = Object(2); auto weak = std::weak_ptr<spTemplateObject>(descriptor);
        Check(g.source.AddObjectForAnalysis(descriptor) && !g.source.AddObjectForAnalysis(descriptor), "host rejects duplicate intrusive descriptor membership");
        descriptor.reset(); Check(!weak.expired(), "template owns attached descriptor");
        spCloneManager clones; auto clone = clones.Clone(g.source); auto* typed = dynamic_cast<spTemplate*>(clone.get());
        Check(typed && typed->GetName() == nullptr && typed->GetResourcePathForAnalysis().empty()
            && typed->GetField20ForAnalysis() == 0x12345678 && typed->GetObjectsForAnalysis().size() == 1,
            "native clone copies word and descriptor list with fresh name/path");
        Check(typed->GetObjectsForAnalysis()[0].get() != weak.lock().get()
            && typed->GetObjectsForAnalysis()[0]->GetName() == nullptr, "real TemplateObject selective clone runs for each descriptor");
        g.source.ClearObjectsForAnalysis(); Check(weak.expired(), "clear releases source descriptor while clone remains independent");
        g.Add(g.source, 3); g.Add(g.target, 0);
        Check(!g.source.DependsOnTemplateForAnalysis(g.target).has_value(), "unresolved state3 owner is not a fabricated negative dependency");
        Check(!g.source.vfunc_14(g.target, clones) && g.target.GetObjectsForAnalysis().size() == 1
            && g.target.GetField20ForAnalysis() == 0xabcdef01, "unknown foreign graph rejects copy before mutation");
        g.source.ClearObjectsForAnalysis(); g.Add(g.source, 3, &g.nested); g.Add(g.nested, 3, &g.source);
        Check(!g.source.DependsOnTemplateForAnalysis(g.target).has_value(), "host recursion guard does not invent a native traversal result");
        spTemplateInstance instance; instance.SetTemplateOwnerForAnalysis(&g.source);
        auto instanceClone = instance.Clone();
        const auto* clonedInstance = dynamic_cast<spTemplateInstance*>(instanceClone.get());
        Check(clonedInstance && clonedInstance->GetTemplateOwnerForAnalysis() == nullptr,
            "instance Named-only clone keeps owner pointer constructor-fresh");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--native-case")
        { Case(static_cast<std::uint32_t>(std::stoul(argv[2])), true); return 0; }
        for (std::uint32_t id = 0; id < 20; ++id) Case(id, false);
        OwnershipAndUnknowns(); std::cout << "PASS " << checks << "/" << checks << ": template descriptor copy, dependency graph and ownership\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL after " << checks << ": " << error.what() << '\n'; return 1; }
}
