#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    using Bytes = std::vector<std::uint8_t>;
    template <class T> void Add(Bytes& bytes, const T& value)
    {
        const auto* data = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), data, data + sizeof(value));
    }
    Bytes Name(const std::string& name)
    {
        Bytes bytes;
        Add(bytes, static_cast<std::uint16_t>(name.size() + 1));
        bytes.insert(bytes.end(), name.begin(), name.end());
        bytes.push_back(0);
        return bytes;
    }
    void Field(spMemoryStream& out, std::uint32_t id, const Bytes& bytes)
    {
        Check(spDataBlockSerializer{}.WriteFieldForAnalysis(
                  out, id, bytes.data(), static_cast<std::uint32_t>(bytes.size())),
              "generic field writer");
    }
    Bytes Fields(std::uint32_t poolCount = 3)
    {
        spMemoryStream out;
        Check(out.Open(nullptr), "open fixture stream");
        for (std::uint32_t i = 0; i < 7; ++i)
        {
            Bytes count;
            Add(count, i == 2 || i == 6 ? poolCount : 0U);
            Field(out, i + 6, count);
        }
        Bytes reserve;
        Add(reserve, 1U);
        Field(out, 64, reserve);
        Bytes time;
        Add(time, 2.f);
        Field(out, 0, time);
        Bytes keys;
        Add(keys, 1U);
        Add(keys, 3U);
        for (float value : {0.f, 1.f, 2.f, 0.f, 0.f, 0.f, 10.f, 20.f, 30.f, 20.f, 40.f, 60.f})
            Add(keys, value);
        Field(out, 2, keys);
        Field(out, 1, Name("Head"));
        auto tag = Name("event");
        Add(tag, .5f);
        Field(out, 5, tag);
        Field(out, 200, {1, 2, 3});
        Check(spDataBlockSerializer::WriteTerminatorForAnalysis(out), "generic terminator writer");
        std::uint32_t size = 0;
        Check(out.GetSize(&size), "fixture size");
        const auto* begin = static_cast<const std::uint8_t*>(out.GetBuffer());
        return Bytes(begin, begin + size);
    }
    void Load(spMemoryStream& stream, const Bytes& bytes)
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),
              "bounded memory stream");
        if (!bytes.empty())
            std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
    }
    std::uint32_t U32(const Bytes& bytes, std::size_t offset)
    {
        if (offset + 4 > bytes.size())
            throw std::runtime_error("Truncated envelope");
        std::uint32_t result;
        std::memcpy(&result, bytes.data() + offset, 4);
        return result;
    }
    void JsonString(const char* text)
    {
        std::cout << '"';
        if (text)
            for (auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
            {
                if (*p == '"' || *p == '\\')
                    std::cout << '\\' << static_cast<char>(*p);
                else if (*p < 0x20 || *p >= 0x80)
                    std::cout << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                              << unsigned(*p) << std::dec;
                else
                    std::cout << static_cast<char>(*p);
            }
        std::cout << '"';
    }
    Bytes AssetBytes(const char* path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        const auto length = input.tellg();
        if (!input || length < 32 || length > spAnimationSerializer::MaximumFieldBytesForAnalysis)
            throw std::runtime_error("Invalid bounded SAN file");
        Bytes raw(static_cast<std::size_t>(length));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(raw.data()), length))
            throw std::runtime_error("SAN read failed");
        return raw;
    }
    Bytes AssetFields(const char* path)
    {
        const auto raw = AssetBytes(path);
        const auto start = U32(raw, 20), size = U32(raw, 24);
        if (std::memcmp(raw.data(), "FFPS", 4) || U32(raw, 4) != 0x26 ||
            U32(raw, 12) != raw.size() || U32(raw, 28) != 1 || start > raw.size() ||
            size != raw.size() - start || size < 8 || U32(raw, start) != spAnimation::ClassID ||
            U32(raw, start + 4) != 0x4f4f4253)
            throw std::runtime_error("Not the single-object PC SAN test envelope");
        return Bytes(raw.begin() + start + 8, raw.end());
    }
    int RegistryLifetime(const char* path)
    {
        const auto fields = AssetFields(path);
        spAnimationManager manager;
        spAnimationSerializer serializer;
        const auto load = [&]() {
            spMemoryStream stream;
            Load(stream, fields);
            std::string error;
            auto result = serializer.ReadFieldsWithBindingsForAnalysis(
                stream, static_cast<std::uint32_t>(fields.size()), manager, nullptr, &error);
            if (!result)
                throw std::runtime_error(error);
            return result;
        };
        auto first = load();
        std::map<std::string, bool> names;
        for (std::size_t i = 0; i < first->GetTrackCountForAnalysis(); ++i)
            names.emplace(first->GetTrackForAnalysis(i)->GetName(), true);
        const auto snapshot = [&](const spAnimation* animation) {
            std::cout << "{\"slots\":[";
            if (animation)
                for (std::size_t i = 0; i < animation->GetTrackCountForAnalysis(); ++i)
                    std::cout << (i ? "," : "")
                              << animation->GetTrackForAnalysis(i)->GetBindingSlotForAnalysis();
            std::cout << "],\"registry\":[";
            bool comma = false;
            for (const auto& [name, unused] : names)
                if (const auto binding = manager.FindNameForAnalysis(name))
                {
                    std::cout << (comma ? ",[" : "[");
                    JsonString(name.c_str());
                    std::cout << ',' << binding->slot << ',' << binding->references << ']';
                    comma = true;
                }
            std::cout << "],\"next\":" << manager.GetNextSlotForAnalysis() << '}';
        };
        std::cout << '[';
        snapshot(first.get());
        auto second = load();
        std::cout << ',';
        snapshot(second.get());
        first.reset();
        std::cout << ',';
        snapshot(nullptr);
        second.reset();
        std::cout << ',';
        snapshot(nullptr);
        auto third = load();
        std::cout << ',';
        snapshot(third.get());
        third.reset();
        std::cout << ',';
        snapshot(nullptr);
        std::cout << "]\n";
        return 0;
    }
    int Rewrite(const char* path)
    {
        const auto fields = AssetFields(path);
        spMemoryStream input, output;
        Load(input, fields);
        Check(output.Open(nullptr), "rewrite output");
        spAnimationSerializer serializer;
        std::string error;
        auto animation = serializer.ReadFieldsForAnalysis(input, static_cast<std::uint32_t>(fields.size()), {}, nullptr, &error);
        if (!animation || !serializer.WriteFieldsForAnalysis(output, *animation, &error))
            throw std::runtime_error(error);
        std::uint32_t size = 0;
        Check(output.GetSize(&size), "rewrite size");
        Check(output.Seek(spStream::SeekSource::essStart, 0), "rewrite rewind");
        auto reloaded = serializer.ReadFieldsForAnalysis(output, size, {}, nullptr, &error);
        if (!reloaded) throw std::runtime_error(error);
        Check(reloaded->GetTotalTimeForAnalysis() == animation->GetTotalTimeForAnalysis() &&
              reloaded->GetTrackCountForAnalysis() == animation->GetTrackCountForAnalysis() &&
              reloaded->GetTagsForAnalysis().size() == animation->GetTagsForAnalysis().size(),
              "reread object/counts");
        const auto* data = static_cast<const std::uint8_t*>(output.GetBuffer());
        std::cout << "SAN_OUTPUT_HEX ";
        for (std::uint32_t i = 0; i < size; ++i)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(data[i]);
        std::cout << '\n' << std::dec;
        return 0;
    }
    int RewriteFile(const char* path)
    {
        const auto raw = AssetBytes(path);
        spSerializerManager manager; spResourceManager resources; spAnimationManager names;
        Check(manager.RegisterForAnalysis(spAnimation::ClassID, std::make_shared<spAnimationSerializer>(), 0xFF, 3),
            "full writer registration");
        spMemoryStream input; Load(input, raw);
        spSerializerReadContextForAnalysis context(manager, resources, &names);
        std::string error;
        auto* root = manager.LoadResourcesForAnalysis(input, context, &error);
        if (!root || !dynamic_cast<spAnimation*>(root)) throw std::runtime_error(error.empty() ? "Expected SAN root" : error);
        manager.SetDispatchContextForAnalysis(manager.GetPlatformMaskForAnalysis(), spSerializerManager::OperationSave);
        Bytes file;
        if (!manager.BuildResourceFileForAnalysis(*root, file, U32(raw, 8), 8 * 1024 * 1024, &error))
            throw std::runtime_error(error);
        std::cout << "SAN_FILE_HEX ";
        for (const auto value : file) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
        std::cout << '\n' << std::dec;
        return 0;
    }
    void WriterTests()
    {
        spAnimationSerializer serializer;
        spAnimation animation;
        spMemoryStream out;
        Check(out.Open(nullptr), "empty writer open");
        std::string error;
        Check(serializer.WriteFieldsForAnalysis(out, animation, &error) && error.empty(), "empty writer success");
        std::uint32_t size = 0;
        Check(out.GetSize(&size) && size == 47, "native empty size");
        Check(out.Seek(spStream::SeekSource::essStart, 0), "empty reread seek");
        auto empty = serializer.ReadFieldsForAnalysis(out, size);
        Check(empty && empty->GetTrackCountForAnalysis() == 0 && empty->GetTrackCapacityForAnalysis() == 1,
              "empty writer field64 implies reader reserve1");
        Check(out.Reset(), "reset before invalid model");
        auto* track = animation.AppendTrackForAnalysis();
        track->SetName("Head");
        Check(!serializer.WriteFieldsForAnalysis(out, animation, &error) && !error.empty(), "missing descriptors rejected");
        Check(out.GetSize(&size) && size == 0, "preflight rejection leaves output unchanged");
        spAnimTrack::TrackDataForAnalysis keys;
        keys[0][0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{1, {0}, {1, 2, 3}};
        keys[1][0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{1, {}, {}};
        keys[2][0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{1, {0}, {1, 1, 1}};
        Check(track->SetKeysForAnalysis(std::move(keys)), "writer test prepared descriptors");
        Check(animation.InsertTagForAnalysis({"later", 2, 0}) &&
              animation.InsertTagForAnalysis({"first", 1, 1}), "writer sorted tags");
        Check(serializer.WriteFieldsForAnalysis(out, animation, &error), "populated writer");
        Check(out.GetSize(&size) && out.Seek(spStream::SeekSource::essStart, 0), "populated rewind");
        spAnimationSerializer::ReadObservationsForAnalysis observed;
        auto loaded = serializer.ReadFieldsForAnalysis(out, size, {}, &observed, &error);
        Check(loaded && loaded->GetTrackCountForAnalysis() == 1 && error.empty(), "populated reread");
        Check(observed.declaredPools[2] == 2 && observed.declaredPools[6] == 2 &&
              observed.usedPools[2] == 2 && observed.usedPools[6] == 2, "patch-back pool counters");
        Check(loaded->GetTagsForAnalysis()[0].name == "first" && loaded->GetTagsForAnalysis()[1].name == "later",
              "writer follows runtime tag order");
        Check(loaded->GetTrackForAnalysis(0)->GetKeysForAnalysis()->at(1)[0]->times.empty(), "packed empty channel written with count zero");
    }
    int Inspect(const char* path, bool ownedBindings = false, bool throughReference = false, bool throughFile = false)
    {
        const auto fields = AssetFields(path);
        spMemoryStream stream;
        Load(stream, fields);
        spAnimationSerializer serializer;
        spAnimationSerializer::ReadObservationsForAnalysis observed;
        std::map<std::string, std::int32_t> bindings;
        const auto bind = [&bindings](std::string_view name) -> std::optional<std::int32_t> {
            auto found = bindings.find(std::string(name));
            if (found != bindings.end())
                return found->second;
            const auto value = static_cast<std::int32_t>(bindings.size());
            bindings.emplace(name, value);
            return value;
        };
        std::string error;
        // Optional native-style name ownership for comparison against the
        // complete PC SAN loader. This portable path still reads fields, not
        // FFPS/FAT; no whole-loader implementation is implied by this switch.
        std::unique_ptr<spAnimationManager> bindingManager;
        if (ownedBindings)
            bindingManager = std::make_unique<spAnimationManager>();
        spSerializerManager manager;
        spResourceManager resources;
        std::unique_ptr<spSerializerReadContextForAnalysis> referenceContext;
        std::unique_ptr<spAnimation> fieldOwner;
        spAnimation* animation = nullptr;
        if (throughFile)
        {
            (void)spAnimation::StaticRTTI();
            Check(manager.RegisterForAnalysis(spAnimation::ClassID, std::make_shared<spAnimationSerializer>(), 0xFF, 3), "file registration");
            Load(stream, AssetBytes(path));
            referenceContext = std::make_unique<spSerializerReadContextForAnalysis>(manager, resources, bindingManager.get());
            animation = dynamic_cast<spAnimation*>(manager.LoadResourcesForAnalysis(stream, *referenceContext, &error));
            Check(manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0, "whole file clears FAT");
        }
        else if (throughReference)
        {
            // Directed envelope around unchanged shipped fields, not a full
            // portable FFPS loader. Generic dispatch/factory/early publication
            // and owned bindings now execute on this side too.
            (void)spAnimation::StaticRTTI();
            const auto objectSize = static_cast<std::uint32_t>(fields.size() + 8);
            Bytes index; Add(index, 1U); Add(index, 7U); Add(index, std::uint16_t(0));
            Add(index, spAnimation::ClassID); Add(index, 0U); Add(index, objectSize);
            spMemoryStream directory; Load(directory, index);
            Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(directory), "reference directory");
            manager.SetDispatchContextForAnalysis(1, 1);
            Check(manager.RegisterForAnalysis(spAnimation::ClassID, std::make_shared<spAnimationSerializer>(), 0xFF, 3), "reference registration");
            Bytes bytes; Add(bytes, 7U); Add(bytes, objectSize); Add(bytes, spAnimation::ClassID); Add(bytes, 0x4F4F4253U);
            bytes.insert(bytes.end(), fields.begin(), fields.end()); Load(stream, bytes);
            referenceContext = std::make_unique<spSerializerReadContextForAnalysis>(manager, resources, bindingManager.get());
            animation = dynamic_cast<spAnimation*>(spSerializer::ReadReferenceForAnalysis(
                *referenceContext, spAnimation::ClassID, stream, stream, &error));
        }
        else
        {
            fieldOwner = ownedBindings ? serializer.ReadFieldsWithBindingsForAnalysis(
                  stream, static_cast<std::uint32_t>(fields.size()), *bindingManager,
                  &observed, &error)
                : serializer.ReadFieldsForAnalysis(
                  stream, static_cast<std::uint32_t>(fields.size()), bind, &observed, &error);
            animation = fieldOwner.get();
        }
        if (!animation)
            throw std::runtime_error(error);
        std::cout << std::setprecision(9) << "{\"time\":" << animation->GetTotalTimeForAnalysis()
                  << ",\"capacity\":" << animation->GetTrackCapacityForAnalysis()
                  << ",\"tracks\":[";
        for (std::size_t i = 0; i < animation->GetTrackCountForAnalysis(); ++i)
        {
            if (i)
                std::cout << ',';
            const auto* track = animation->GetTrackForAnalysis(i);
            std::cout << "{\"name\":";
            JsonString(track->GetName());
            std::cout << ",\"slot\":" << track->GetBindingSlotForAnalysis()
                      << ",\"duration\":" << track->GetDurationForAnalysis() << ",\"samples\":[";
            auto sampler = track->GetSamplerForAnalysis();
            spTransformTrackEval::KeyCacheForAnalysis cache{};
            for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex)
            {
                if (sampleIndex)
                    std::cout << ',';
                const float time = animation->GetTotalTimeForAnalysis() * (.5f * sampleIndex);
                const auto sample = sampler(time, cache);
                std::cout << '[' << time << ',' << sample.hasPosition << ',' << sample.hasRotation
                          << ',' << sample.hasScale;
                for (float value : sample.position)
                    std::cout << ',' << value;
                for (float value : sample.rotation)
                    std::cout << ',' << value;
                for (float value : sample.scale)
                    std::cout << ',' << value;
                std::cout << ']';
            }
            std::cout << "]}";
        }
        std::cout << "],\"tags\":[";
        for (std::size_t i = 0; i < animation->GetTagsForAnalysis().size(); ++i)
        {
            if (i)
                std::cout << ',';
            const auto& tag = animation->GetTagsForAnalysis()[i];
            std::cout << '[';
            JsonString(tag.name.c_str());
            std::cout << ',' << tag.time << ',' << tag.wireOrdinal << ']';
        }
        if (throughReference || throughFile)
        {
            std::cout << "],\"usedPools\":null}\n";
            return 0; // observations not requested, never represent them as zero
        }
        std::cout << "],\"usedPools\":[";
        for (std::size_t i = 0; i < 7; ++i)
        {
            if (i)
                std::cout << ',';
            std::cout << observed.usedPools[i];
        }
        std::cout << "]}\n";
        return 0;
    }
    void OwnedBindingTests()
    {
        spAnimationManager manager;
        spAnimationSerializer serializer;
        const auto bytes = Fields();
        const auto read = [&]() {
            spMemoryStream stream;
            Load(stream, bytes);
            return serializer.ReadFieldsWithBindingsForAnalysis(
                stream, static_cast<std::uint32_t>(bytes.size()), manager);
        };
        auto first = read();
        auto second = read();
        Check(first && second && manager.FindNameForAnalysis("Head")->references == 2,
              "reader acquires one owned reference per live track");
        const auto oldSlot = first->GetTrackForAnalysis(0)->GetBindingSlotForAnalysis();
        Check(second->GetTrackForAnalysis(0)->GetBindingSlotForAnalysis() == oldSlot,
              "simultaneous resources share same slot");
        first.reset();
        Check(manager.FindNameForAnalysis("Head")->references == 1,
              "one resource release preserves other");
        second.reset();
        Check(manager.GetNameCountForAnalysis() == 0,
              "last resource destruction releases registration");
        auto reloaded = read();
        Check(reloaded && reloaded->GetTrackForAnalysis(0)->GetBindingSlotForAnalysis() > oldSlot,
              "reload uses new monotonic slot");
        reloaded.reset();
        spMemoryStream bad;
        auto truncated = bytes;
        truncated.pop_back();
        Load(bad, truncated);
        const auto next = manager.GetNextSlotForAnalysis();
        Check(!serializer.ReadFieldsWithBindingsForAnalysis(
                  bad, static_cast<std::uint32_t>(truncated.size()), manager) &&
                  manager.GetNameCountForAnalysis() == 0 &&
                  manager.GetNextSlotForAnalysis() == next,
              "field validation failure acquires no references or IDs");
        spMemoryStream names;
        Check(names.Open(nullptr), "open name-only fixture");
        Field(names, 1, Name("valid-before-failure"));
        Field(names, 1, Name(std::string(4096, 'x')));
        Check(spDataBlockSerializer::WriteTerminatorForAnalysis(names), "name-only terminator");
        std::uint32_t size = 0;
        Check(names.GetSize(&size) && names.Seek(spStream::SeekSource::essStart, 0),
              "rewind name-only fixture");
        std::string error;
        spAnimationSerializer::ReadObservationsForAnalysis observed;
        observed.unknownFields.push_back(99);
        Check(!serializer.ReadFieldsWithBindingsForAnalysis(names, size, manager, &observed,
                                                            &error) &&
                  !error.empty() && observed.unknownFields.empty(),
              "late lease failure returns no partial animation/observations");
        Check(manager.GetNameCountForAnalysis() == 0 &&
                  manager.GetNextSlotForAnalysis() == next + 1,
              "failed acquisition releases prior lease but does not recycle consumed ID");
    }
    void Tests()
    {
        spAnimationSerializer serializer;
        Check(serializer.IsKindOf(spSerializer::ClassID), "serializer native base identity");
        Check(serializer.GetTargetClassIDForAnalysis() == spAnimation::ClassID,
              "serializer target");
        Check(serializer.Clone()->IsExactly(spAnimationSerializer::ClassID), "serializer clone");
        auto bytes = Fields();
        spMemoryStream stream;
        Load(stream, bytes);
        spAnimationSerializer::ReadObservationsForAnalysis observed;
        std::string error;
        int lookups = 0;
        const auto resolve = [&lookups](std::string_view name) -> std::optional<std::int32_t> {
            ++lookups;
            return name == "Head" ? std::optional<std::int32_t>(7) : std::nullopt;
        };
        auto animation = serializer.ReadFieldsForAnalysis(
            stream, static_cast<std::uint32_t>(bytes.size()), resolve, &observed, &error);
        Check(animation && error.empty(), "valid full fields");
        Check(animation->GetTrackCountForAnalysis() == 1 &&
                  animation->GetTrackCapacityForAnalysis() == 2,
              "reserve hint plus one");
        Check(lookups == 1 && animation->GetTrackForAnalysis(0)->GetBindingSlotForAnalysis() == 7,
              "deferred resolver");
        Check(observed.usedPools[2] == 3 && observed.usedPools[6] == 3, "shared pool accounting");
        Check(observed.unknownFields == std::vector<std::uint32_t>{200},
              "generic unknown field skip");
        Check(animation->GetTagsForAnalysis()[0].name == "event", "tag name");
        spTransformTrackEval::KeyCacheForAnalysis cache{};
        Check(animation->GetTrackForAnalysis(0)->GetSamplerForAnalysis()(1, cache).position[1] ==
                  20,
              "reader-to-sampler chain");
        bytes.pop_back();
        Load(stream, bytes);
        lookups = 0;
        Check(!serializer.ReadFieldsForAnalysis(stream, static_cast<std::uint32_t>(bytes.size()),
                                                resolve, nullptr, &error),
              "strict missing terminator rejection");
        Check(lookups == 0 && !error.empty(), "validation before binding lookups");
        bytes = Fields(2);
        Load(stream, bytes);
        Check(!serializer.ReadFieldsForAnalysis(stream, static_cast<std::uint32_t>(bytes.size()),
                                                {}, nullptr, &error),
              "shared array overrun rejected");
        bytes = Fields();
        bytes.push_back(0);
        Load(stream, bytes);
        Check(!serializer.ReadFieldsForAnalysis(stream, static_cast<std::uint32_t>(bytes.size())),
              "trailing bytes rejected");
        Load(stream, Fields());
        Check(!serializer.ReadFieldsForAnalysis(stream, 0), "zero extent rejected");
        OwnedBindingTests();
        WriterTests();
        std::cout << "PASS " << checks << '/' << checks << ": SAN field reader tests\n";
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--inspect")
            return Inspect(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--inspect-owned")
            return Inspect(argv[2], true);
        if (argc == 3 && std::string(argv[1]) == "--inspect-reference")
            return Inspect(argv[2], true, true);
        if (argc == 3 && std::string(argv[1]) == "--inspect-file")
            return Inspect(argv[2], true, false, true);
        if (argc == 3 && std::string(argv[1]) == "--registry-lifetime")
            return RegistryLifetime(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--rewrite-fields")
            return Rewrite(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--rewrite-file")
            return RewriteFile(argv[2]);
        if (argc != 1)
            throw std::runtime_error("Expected no args or --inspect <bounded SAN>");
        Tests();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
