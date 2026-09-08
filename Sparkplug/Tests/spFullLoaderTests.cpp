#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    int checks = 0;
    void Check(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
    template<class T> void Add(Bytes& bytes, T value)
    { const auto* p = reinterpret_cast<const std::uint8_t*>(&value); bytes.insert(bytes.end(), p, p + sizeof(value)); }
    void SetWord(Bytes& bytes, unsigned at, std::uint32_t value)
    { Check(at + 4 <= bytes.size(), "bounded patch"); std::memcpy(bytes.data() + at, &value, 4); }
    void Load(spMemoryStream& stream, const Bytes& bytes)
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())), "bounded input");
        if (!bytes.empty()) std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
        stream.SetLogicalOriginForAnalysis(0);
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size = 0; Check(stream.GetSize(&size), "size");
        const auto* p = static_cast<const std::uint8_t*>(stream.GetBuffer()); return Bytes(p, p + size);
    }
    Bytes File(unsigned count = 2, std::uint32_t platform = 1)
    {
        spAnimation animation;
        auto* track = animation.AppendTrackForAnalysis(); Check(track != nullptr, "track"); track->SetName("Head");
        spAnimTrack::TrackDataForAnalysis keys;
        for (auto& role : keys) role[0] = sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis{1, {}, {}};
        Check(track->SetKeysForAnalysis(std::move(keys)), "empty packed descriptors");
        spMemoryStream fields; Check(fields.Open(nullptr), "writer");
        Check(spAnimationSerializer{}.WriteFieldsForAnalysis(fields, animation), "canonical known writer");
        const auto body = Data(fields);
        const auto objectSize = static_cast<std::uint32_t>(body.size() + 8);
        const std::uint32_t origin = 36 + 18 * count;
        Bytes bytes;
        // Evidence-based TEST envelope builder, not a recovered whole Save API.
        for (auto word : {0x53504646U, 0x26U, 0U, origin + objectSize * count, platform, origin, objectSize * count}) Add(bytes, word);
        Add(bytes, count);
        for (unsigned i = 0; i < count; ++i)
        {
            Add(bytes, i + 1); Add(bytes, std::uint16_t(0)); Add(bytes, spAnimation::ClassID);
            Add(bytes, i * objectSize); Add(bytes, objectSize);
        }
        Add(bytes, 0U);
        for (unsigned i = 0; i < count; ++i)
        {
            Add(bytes, spAnimation::ClassID); Add(bytes, 0x4F4F4253U);
            bytes.insert(bytes.end(), body.begin(), body.end());
        }
        return bytes;
    }
    void Register(spSerializerManager& manager)
    { Check(manager.RegisterForAnalysis(spAnimation::ClassID, std::make_shared<spAnimationSerializer>(), 0xFF, 3), "register concrete reader"); }
    void Reuse()
    {
        spSerializerManager manager; spResourceManager resources; spAnimationManager names; Register(manager);
        const auto file = File(); spMemoryStream firstInput, secondInput; Load(firstInput, file); Load(secondInput, file);
        auto first = std::make_unique<spSerializerReadContextForAnalysis>(manager, resources, &names);
        auto second = std::make_unique<spSerializerReadContextForAnalysis>(manager, resources, &names);
        first->captureFileObjectIDsForAnalysis = true;
        std::string error;
        auto* a = dynamic_cast<spAnimation*>(manager.LoadResourcesForAnalysis(firstInput, *first, &error));
        Check(a && !first->failed && error.empty() && first->createdObjects.size() == 2, "whole multi-object FFPS load");
        Check(a == first->createdObjects.front().get() && a->GetTrackCountForAnalysis() == 1, "first new object is root");
        const auto& identities = first->fileObjectsForAnalysis;
        Check(identities.size() == 2 && identities[0].id == 1 && identities[0].object == a
            && identities[1].id == 2 && identities[1].object == first->createdObjects[1].get()
            && identities[0].object != identities[1].object,
            "completed FAT IDs distinguish two objects with the same runtime class after FAT clear");
        Check(names.FindNameForAnalysis("Head")->references == 2, "two objects own two name leases");
        Check(manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0 && firstInput.GetLogicalOriginForAnalysis() == 72,
              "FAT cleared while objects and advanced stream origin survive");
        std::uint32_t position = 0, physical = 0;
        Check(firstInput.GetCurrentPosition(position) && firstInput.GetSize(&physical) && physical == file.size() && position == physical - 72,
              "physical size vs logical cursor contract");
        auto* b = manager.LoadResourcesForAnalysis(secondInput, *second, &error);
        Check(b && b != a && !second->failed && names.FindNameForAnalysis("Head")->references == 4, "second full load while first remains alive");
        Check(second->fileObjectsForAnalysis.empty(), "optional identity observation stays disabled by default");
        first.reset(); Check(names.FindNameForAnalysis("Head")->references == 2, "release only first file bindings");
        second.reset(); Check(names.GetNameCountForAnalysis() == 0, "last file releases last leases");
        for (auto platform : {2U, 3U})
        {
            spMemoryStream input; Load(input, File(1, platform)); spSerializerReadContextForAnalysis context(manager, resources, &names);
            Check(manager.LoadResourcesForAnalysis(input, context, &error) && !context.failed, "PC bit hook no-mesh path executes without GPU");
        }
    }
    void Failures()
    {
        for (unsigned mode = 0; mode < 9; ++mode)
        {
            spSerializerManager manager; spResourceManager resources; spAnimationManager names;
            if (mode != 8) Register(manager);
            auto file = File();
            if (mode == 0) SetWord(file, 0, 0);
            if (mode == 1) SetWord(file, 4, 0);
            if (mode == 2) SetWord(file, 16, 8);
            if (mode == 3) SetWord(file, 20, 73);
            if (mode == 4) SetWord(file, 42, 0x7FFFFFFF);
            if (mode == 5) SetWord(file, 46, 1);
            if (mode == 6) SetWord(file, 72, 0);
            if (mode == 7) file.pop_back();
            spMemoryStream input; Load(input, file); std::string error;
            {
                spSerializerReadContextForAnalysis context(manager, resources, &names);
                context.captureFileObjectIDsForAnalysis = true;
                Check(!manager.LoadResourcesForAnalysis(input, context, &error) && context.failed && !error.empty(), "malformed full file fails explicitly");
                Check(context.fileObjectsForAnalysis.empty(), "failed file never exposes a completed identity snapshot");
                Check(manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0, "host clears partial FAT on all exits");
                std::uint32_t before = 0, after = 0; Check(input.GetCurrentPosition(before), "failed tell");
                Check(!manager.LoadResourcesForAnalysis(input, context, &error) && input.GetCurrentPosition(after) && after == before,
                      "failed context never retries file I/O");
            }
            Check(names.GetNameCountForAnalysis() == 0, "partial full-load bindings reclaimed");
        }
    }
    void NullableNamesAndOrigin()
    {
        spMemoryStream strings; Load(strings, Bytes{0, 0, 1, 0, 0});
        std::string name = "before"; bool isNull = false;
        Check(strings.ReadString(name, &isNull) && isNull && name.empty(), "length0 retains nullptr distinction");
        Check(strings.ReadString(name, &isNull) && !isNull && name.empty(), "length1/NUL retains nonnull empty name");
        spResourceFATEntryForAnalysis entry;
        Check(entry.GetNameForAnalysis() == nullptr, "default null FAT name");
        entry.nameIsNullForAnalysis = false; Check(entry.GetNameForAnalysis() && !*entry.GetNameForAnalysis(), "explicit empty FAT name");
        entry.nameIsNullForAnalysis = true; entry.name = "changed";
        Check(std::strcmp(entry.GetNameForAnalysis(), "changed") == 0, "manual nonempty name remains usable");
        spSerializerManager manager; spResourceManager resources; Register(manager);
        auto file = File(1); SetWord(file, 12, 0); SetWord(file, 24, 0); // native ignores both declarations
        spMemoryStream input; Load(input, file); spSerializerReadContextForAnalysis context(manager, resources);
        Check(manager.LoadResourcesForAnalysis(input, context) != nullptr, "declared size/dataSize are not invented native validators");
    }

    void IndexInspection()
    {
        spMemoryStream stream;Check(stream.Open(nullptr)&&stream.Write(3u),"index inspection setup");
        std::uint32_t expectedSecondEnd=0;
        for(std::uint32_t id=1;id<=3;++id)
        {
            const char* name=id==1?nullptr:id==2?"":"later";
            Check(stream.Write(id)&&stream.Write(name)&&stream.Write(id==2?0xDEADBEEFu:spAnimation::ClassID)
                &&stream.Write(100u*id)&&stream.Write(9u),"test index entries");
            if(id==2)Check(stream.GetCurrentPosition(expectedSecondEnd),"unknown entry extent");
        }
        Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind index");
        spResourceFATHelperForAnalysis runtime;std::uint32_t stopped=0;
        Check(!runtime.LoadIndexForAnalysis(stream)&&runtime.GetResourceCountForAnalysis()==1
            &&stream.GetCurrentPosition(stopped)&&stopped==expectedSecondEnd,
            "runtime FAT preserves early unknown-RTTI rejection and preceding entry");
        Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind raw observation");
        std::vector<spResourceFATEntryLocationForAnalysis> locations;std::vector<std::uint32_t> kinds;std::uint32_t count=0;
        Check(spResourceFATHelperForAnalysis::ReadIndexEntriesForAnalysis(stream,[&](auto entry,const auto& location)
            {locations.push_back(location);kinds.push_back(entry->classID);return true;},true,&count),"shared raw index stream reader");
        Check(count==3&&locations.size()==3&&kinds[1]==0xDEADBEEF,"raw inspection does not invent an unknown runtime factory");
        Check(locations[0].tableOffset==4&&locations[0].nameOffset==10&&locations[0].nameBytes==0
            &&locations[1].tableOffset==22&&locations[1].nameOffset==28&&locations[1].nameBytes==1,
            "source locations preserve null vs nonnull empty names");
    }

    void FullFileProducer()
    {
        spSerializerManager manager; spResourceManager resources; spAnimationManager names; Register(manager);
        const auto expected = File(1);
        spMemoryStream input; Load(input, expected);
        spSerializerReadContextForAnalysis context(manager, resources, &names);
        auto* root = manager.LoadResourcesForAnalysis(input, context);
        Check(root != nullptr, "full producer source loaded");
        Bytes output{9, 8, 7}; std::string error;
        Check(!manager.BuildResourceFileForAnalysis(*root, output, 0, 65536, &error)
            && output == Bytes({9, 8, 7}) && !error.empty(), "wrong dispatch keeps prior output");
        manager.SetDispatchContextForAnalysis(1, 2);
        Check(manager.BuildResourceFileForAnalysis(*root, output, 0, 65536, &error)
            && error.empty() && output == expected, "whole-file producer equals independent reader-envelope fixture");
        Check(manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0
            && manager.GetFATForAnalysis()->GetNextResourceIDForAnalysis() == 1, "successful build clears transient FAT");
        const auto saved = output;
        for (const auto limit : {0U, 35U, 36U, static_cast<std::uint32_t>(saved.size() - 1), 64U * 1024 * 1024 + 1})
        {
            Check(!manager.BuildResourceFileForAnalysis(*root, output, 0, limit, &error)
                && output == saved && !error.empty(), "bounded file failure preserves previously published output");
            Check(manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0, "failed build leaves no one-shot state");
        }
        Check(manager.BuildResourceFileForAnalysis(*root, output, 0, static_cast<std::uint32_t>(saved.size()), &error)
            && output == saved, "exact output byte limit succeeds after fresh failed transaction");
        Check(manager.BuildResourceFileForAnalysis(*root, output, 0x87654321, 65536, &error), "caller-supplied opaque export tag");
        auto tagged = expected; SetWord(tagged, 8, 0x87654321);
        Check(output == tagged, "export tag is not assigned invented semantics");
        Check(spSerializer::IndexReferenceForAnalysis(manager, root), "establish separate active index");
        auto* prior = manager.GetFATForAnalysis()->FindByObjectForAnalysis(*root);
        Check(!manager.BuildResourceFileForAnalysis(*root, output, 0, 65536, &error)
            && output == tagged && manager.GetFATForAnalysis()->FindByObjectForAnalysis(*root) == prior,
            "producer rejects occupied FAT without destroying the caller context");
        manager.GetFATForAnalysis()->ClearResourceEntriesForAnalysis();
        manager.SetDispatchContextForAnalysis(8, 2);
        Check(!manager.BuildResourceFileForAnalysis(*root, output, 0, 65536, &error)
            && output == tagged, "PC producer rejects PS2-only dispatch");
        manager.SetDispatchContextForAnalysis(1, 2);
        spBaseObject unsupported;
        Check(!manager.BuildResourceFileForAnalysis(unsupported, output, 0, 65536, &error)
            && output == tagged, "unsupported root has no fabricated writer");
        spAnimation incomplete;
        Check(incomplete.AppendTrackForAnalysis() != nullptr, "track without writable descriptors");
        Check(!manager.BuildResourceFileForAnalysis(incomplete, output, 0, 65536, &error)
            && output == tagged && !error.empty() && manager.GetFATForAnalysis()->GetResourceCountForAnalysis() == 0,
            "payload preflight failure discards transient one-shot state and keeps prior output");
    }
}
int main()
{
    try { (void)spAnimation::StaticRTTI(); Reuse(); Failures(); NullableNamesAndOrigin(); IndexInspection(); FullFileProducer();
        std::cout << "PASS " << checks << '/' << checks << ": complete PC FFPS loader reconstruction\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
