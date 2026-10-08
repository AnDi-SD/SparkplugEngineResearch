#include "Code/Sparkplug/spTemplate.h"
#include "Code/Sparkplug/spTemplateInstance.h"
#include "Code/Sparkplug/spTemplateSerializer.h"
#include "Code/SparkBase/spStream.h"
#include "Analysis/PC/spTemplateAbi.h"
#include "Analysis/PS2/spTemplateAbi.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <functional>
#include <algorithm>

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

    // HOST byte-backed stream with explicitly selected partial-read boolean.
    // It exercises the whole bind -> raw read -> real Template/Named writes;
    // it does not simulate a file-system or claim native stream ABI.
    class HeaderStream final : public spStream
    {
    public:
        std::vector<std::uint8_t> bytes;
        std::size_t available = 0, copied = 0;
        std::uint32_t reads = 0, requested = 0;
        bool readResult = true;
        std::function<void()> duringRead;
        bool Open(const char*) override { return false; }
        bool Open(std::uint32_t, const char*) override { return false; }
        bool Close() override { return false; }
        bool Seek(SeekSource, std::int32_t) override { throw std::runtime_error("header unexpectedly sought stream"); }
        bool GetCurrentPosition(std::uint32_t&) const override { throw std::runtime_error("header unexpectedly asked stream position"); }
        bool ReadData(void* destination, std::uint32_t count) override
        {
            ++reads; requested = count;
            const auto* initial = static_cast<const std::uint8_t*>(destination);
            Check(std::all_of(initial, initial + count, [](std::uint8_t value) { return value == 0; }),
                "native header buffer is zeroed before foreign read");
            copied = std::min<std::size_t>(count, available);
            if (copied) std::memcpy(destination, bytes.data(), copied);
            if (duringRead) duringRead();
            return readResult;
        }
        bool WriteData(const void*, std::uint32_t) override { throw std::runtime_error("header unexpectedly wrote stream"); }
        bool vfunc_WriteFromStream(spStream*, std::uint32_t) override { return false; }
        bool GetSize(std::uint32_t*) const override { throw std::runtime_error("header unexpectedly asked stream size"); }
    };
    void PutWord(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t word)
    { for (std::uint32_t i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(word >> (8 * i)); }
    void HeaderCase(std::uint32_t id, bool emit)
    {
        Check(id < 20, "bounded binary-header case");
        spTemplate target, alternate;
        target.SetName("old-target"); target.SetField20ForAnalysis(0xABCDEF01);
        alternate.SetName("old-alternate"); alternate.SetField20ForAnalysis(0x10203040);
        spTemplateSerializer serializer; HeaderStream stream;
        stream.bytes.assign(spTemplateSerializer::BinaryHeaderSize + 8, 0x55);
        std::fill(stream.bytes.begin(), stream.bytes.begin() + spTemplateSerializer::BinaryHeaderSize, 0);
        PutWord(stream.bytes, 0, spTemplateSerializer::BinaryHeaderMagic);
        std::string name = "TemplateBinary";
        if (id == 3) name.clear();
        if (id == 4) name.assign(63, 'x');
        if (id == 5) name = std::string("a\0b", 3);
        std::memcpy(stream.bytes.data() + 4, name.data(), name.size());
        PutWord(stream.bytes, 0x44, id == 1 ? 0 : id == 2 ? 0xFFFFFFFF : 3);
        PutWord(stream.bytes, 0x48, 0x76543210);
        if (id == 6 || id == 18) PutWord(stream.bytes, 0, 0x12345678);
        stream.available = spTemplateSerializer::BinaryHeaderSize;
        if (id == 8) stream.available = 0;
        if (id == 9) stream.available = 1;
        if (id == 10) stream.available = 4;
        if (id == 11) stream.available = 0x44;
        if (id == 12) stream.available = 0x48;
        if (id == 13) stream.available = 0x49;
        if (id == 14) stream.available = 0x4A;
        if (id == 15) stream.available = 0x4B;
        if (id == 19) stream.available = stream.bytes.size();
        if (id == 7 || id == 17) stream.readResult = false;
        serializer.BindForAnalysis(&target, &stream);
        if (id >= 16 && id <= 18)
            stream.duringRead = [&] { serializer.BindForAnalysis(&alternate, &stream); };
        std::uint32_t diagnostics = 0;
        serializer.SetHeaderDiagnosticForAnalysis([](void* context)
            { ++*static_cast<std::uint32_t*>(context); }, &diagnostics);
        const auto result = serializer.ReadBinaryHeaderForAnalysis();
        Check(result.has_value(), "declared native-valid header domain has raw result");
        Check(stream.reads == 1 && stream.requested == 0x4C, "header uses one literal 0x4C-byte raw read");
        if (emit)
        {
            std::cout << "{\"case\":" << id << ",\"result\":" << *result
                << ",\"targetWord\":" << target.GetField20ForAnalysis()
                << ",\"alternateWord\":" << alternate.GetField20ForAnalysis()
                << ",\"targetName\":\"" << target.GetName()
                << "\",\"alternateName\":\"" << alternate.GetName()
                << "\",\"reads\":" << stream.reads << ",\"requested\":" << stream.requested
                << ",\"copied\":" << stream.copied << ",\"diagnostics\":" << diagnostics
                << ",\"alternateBound\":" << (serializer.GetTargetForAnalysis() == &alternate ? "true" : "false") << "}\n";
        }
        else
        {
            if (id == 1) Check(*result == 0 && target.GetField20ForAnalysis() == 0x76543210
                && std::strcmp(target.GetName(), "TemplateBinary") == 0,
                "valid zero count mutates target before outer reader fails");
            if (id == 2) Check(*result == 0xFFFFFFFF, "descriptor count retains full uint32 bits");
            if (id == 7 || id == 17) Check(*result == 0 && diagnostics == 0
                && target.GetField20ForAnalysis() == 0xABCDEF01 && alternate.GetField20ForAnalysis() == 0x10203040,
                "ReadData false leaves both templates untouched without wrong-magic diagnostic");
            if (id == 16) Check(target.GetField20ForAnalysis() == 0xABCDEF01
                && alternate.GetField20ForAnalysis() == 0x76543210,
                "target is reread after foreign raw read changes live serializer binding");
            if (id == 10) Check(*result == 0 && target.GetField20ForAnalysis() == 0
                && std::strcmp(target.GetName(), "") == 0,
                "successful four-byte short read uses zero-filled name/count/word");
            if (id == 19) Check(stream.copied == 0x4C, "header consumes no trailing descriptor bytes");
        }
    }
    void HeaderHostGuards()
    {
        spTemplate target; target.SetField20ForAnalysis(0xAABBCCDD);
        spTemplateSerializer serializer;
        Check(!serializer.ReadBinaryHeaderForAnalysis().has_value(), "missing host input remains unknown");
        HeaderStream stream; stream.bytes.assign(0x4C, 'x'); stream.available = 0x4C;
        PutWord(stream.bytes, 0, spTemplateSerializer::BinaryHeaderMagic);
        serializer.BindForAnalysis(nullptr, &stream);
        Check(!serializer.ReadBinaryHeaderForAnalysis().has_value(), "missing target after read remains unknown");
        serializer.BindForAnalysis(&target, &stream);
        Check(!serializer.ReadBinaryHeaderForAnalysis().has_value() && target.GetField20ForAnalysis() == 0xAABBCCDD,
            "host domain guard does not scan an unterminated 64-byte name or claim native atomicity");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--native-case")
        { Case(static_cast<std::uint32_t>(std::stoul(argv[2])), true); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--native-header-case")
        { HeaderCase(static_cast<std::uint32_t>(std::stoul(argv[2])), true); return 0; }
        for (std::uint32_t id = 0; id < 20; ++id) Case(id, false);
        OwnershipAndUnknowns();
        for (std::uint32_t id = 0; id < 20; ++id) HeaderCase(id, false);
        HeaderHostGuards();
        std::cout << "PASS " << checks << "/" << checks << ": template descriptor copy, dependency graph, ownership and binary header\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL after " << checks << ": " << error.what() << '\n'; return 1; }
}
