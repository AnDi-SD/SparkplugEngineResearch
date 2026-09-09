#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spMeshData.h"
#include "Code/Sparkplug/spSerializerHook.h"
#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spBSPNode.h"
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
    {
        auto* data = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), data, data + sizeof(value));
    }
    void Open(spMemoryStream& stream, const Bytes& bytes)
    {
        Check(stream.Close(), "release previous test buffer before reopening");
        stream.SetLogicalOriginForAnalysis(0);
        Check(stream.Open(nullptr), "open stream");
        if (!bytes.empty()) Check(stream.WriteData(bytes.data(), static_cast<std::uint32_t>(bytes.size())), "stream input");
        Check(stream.Seek(spStream::SeekSource::essStart, 0), "rewind stream");
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size = 0; Check(stream.GetSize(&size), "size");
        const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return Bytes(bytes, bytes + size);
    }
    std::uint32_t Position(spStream& stream)
    { std::uint32_t position = 0; Check(stream.GetCurrentPosition(position), "tell"); return position; }
    void Directory(spSerializerManager& manager, spClassID type = spAnimation::ClassID)
    {
        Bytes bytes; Add(bytes, 1U); Add(bytes, 7U); Add(bytes, std::uint16_t(0));
        Add(bytes, type); Add(bytes, 0U); Add(bytes, 55U);
        spMemoryStream in; Open(in, bytes);
        Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(in), "directory grammar");
        manager.SetDispatchContextForAnalysis(1, 1);
    }
    Bytes Reference()
    {
        spAnimation animation; spMemoryStream fields; Check(fields.Open(nullptr), "field output");
        Check(spAnimationSerializer{}.WriteFieldsForAnalysis(fields, animation), "native field writer");
        Bytes bytes; Add(bytes, 7U); Add(bytes, 55U); Add(bytes, spAnimation::ClassID); Add(bytes, 0x4F4F4253U);
        auto data = Data(fields); bytes.insert(bytes.end(), data.begin(), data.end());
        Add(bytes, 7U); Add(bytes, 0U); Add(bytes, 0U); return bytes;
    }
    class Observing final : public spSerializer
    {
    public:
        mutable unsigned calls = 0;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context, spStream& source,
            std::uint32_t size, spBaseObject& object, std::string* error) const override
        {
            ++calls;
            Check(context.manager.GetFATForAnalysis()->FindByIDForAnalysis(7)->object == &object,
                  "entry published before payload callback");
            Check(context.createdObjects.size() == 1 && context.depth == 1, "host owner/depth published");
            return spAnimationSerializer{}.ReadPayloadForAnalysis(context, source, size, object, error);
        }
    };
    void Success(bool split)
    {
        spSerializerManager manager; spResourceManager resources; Directory(manager);
        auto serializer = std::make_shared<Observing>();
        Check(manager.RegisterForAnalysis(spAnimation::ClassID, serializer, 0xFF, 3), "register reader");
        spMemoryStream payload, ids; Open(payload, Reference()); Open(ids, Bytes{7, 0, 0, 0});
        if (split) Check(payload.Seek(spStream::SeekSource::essStart, 4), "split skip ID in payload stream");
        auto* entry = manager.GetFATForAnalysis()->FindByIDForAnalysis(7);
        {
            spSerializerReadContextForAnalysis context(manager, resources); std::string error;
            auto* object = spSerializer::ReadReferenceForAnalysis(context, 0xDEADBEEF, split ? ids : payload, payload, &error);
            auto* animation = dynamic_cast<spAnimation*>(object);
            Check(animation && animation->GetTotalTimeForAnalysis() == 0 && animation->GetTrackCountForAnalysis() == 0,
                  "actual header type wins over unused expected class");
            Check(!context.failed && error.empty() && Position(payload) == 63, "first inline extent consumed");
            Check(!split || Position(ids) == 4, "separate ID cursor");
            Check(spSerializer::ReadReferenceForAnalysis(context, spAnimation::ClassID, payload, payload) == object &&
                  serializer->calls == 1 && Position(payload) == 71, "repeat reuses object and skips reader");
            Check(!spSerializer::ReadReferenceForAnalysis(context, spAnimation::ClassID, payload, payload) &&
                  !context.failed && Position(payload) == 75, "null success is distinct from failure");
        }
        Check(!entry->object, "host teardown clears owned FAT alias");
    }
    void CacheAndSkip()
    {
        spSerializerManager manager; spResourceManager resources; spMeshData mesh;
        (void)spMeshData::StaticRTTI(); Directory(manager, spMeshData::ClassID);
        mesh.SetName("cache"); Check(resources.RegisterForAnalysis(mesh), "cache borrowed mesh");
        auto* entry = manager.GetFATForAnalysis()->FindByIDForAnalysis(7); entry->name = "cache";
        Bytes bytes; Add(bytes, 7U); Add(bytes, 4U); Add(bytes, 123U);
        spMemoryStream stream; Open(stream, bytes);
        {
            spSerializerReadContextForAnalysis context(manager, resources);
            Check(spSerializer::ReadReferenceForAnalysis(context, 0, stream, stream) == &mesh &&
                  context.createdObjects.empty() && Position(stream) == 12, "cache hit works without serializer and skips inline bytes");
        }
        Check(entry->object == &mesh && resources.GetResourceCountForAnalysis() == 1, "context never destroys borrowed cache object");
        bytes.clear(); Add(bytes, 7U); Add(bytes, 1000U); Open(stream, bytes);
        spSerializerReadContextForAnalysis failed(manager, resources); std::string error;
        Check(!spSerializer::ReadReferenceForAnalysis(failed, 0, stream, stream, &error) && failed.failed && !error.empty(),
              "host reports invalid skip even though native returns object");
    }
    void Failures()
    {
        for (unsigned mode = 0; mode < 6; ++mode)
        {
            spSerializerManager manager; spResourceManager resources; Directory(manager);
            if (mode != 3) Check(manager.RegisterForAnalysis(spAnimation::ClassID,
                std::make_shared<spAnimationSerializer>(), 0xFF, 3), "failure reader");
            Bytes bytes = Reference();
            if (mode == 0) bytes.clear();
            if (mode == 1) bytes.resize(4);
            if (mode == 2) bytes[0] = 99;
            if (mode == 4) bytes[62] = 0x60; // missing terminator, leaves published object
            if (mode == 5) bytes[4] = 54; // strict bounded extent rejects trailing/malformed fields
            spMemoryStream stream; Open(stream, bytes); std::string error;
            auto* entry = manager.GetFATForAnalysis()->FindByIDForAnalysis(7);
            {
                spSerializerReadContextForAnalysis context(manager, resources);
                Check(!spSerializer::ReadReferenceForAnalysis(context, 0, stream, stream, &error) && context.failed && !error.empty(),
                      "ID/size/directory/serializer/payload/extent errors explicit");
                Check(context.createdObjects.size() == (mode >= 4 ? 1U : 0U), "partial payload ownership retained until teardown");
                const auto position = Position(stream);
                Check(!spSerializer::ReadReferenceForAnalysis(context, 0, stream, stream, &error) && Position(stream) == position,
                      "poisoned context never retries or consumes more input");
            }
            Check(!entry->object, "failure teardown leaves no dangling owned entry");
        }
    }
    void DXHookCacheRules()
    {
        for (unsigned index = 0; index < 6; ++index)
        {
            const std::uint32_t masks[]{1, 4, 2, 3, 6, 2};
            spSerializerManager manager; spResourceManager resources; spMeshData mesh;
            const auto type = index == 5 ? spMesh::ClassID : spMeshData::ClassID;
            Directory(manager, type); manager.SetDispatchContextForAnalysis(masks[index], 1);
            mesh.SetName("cache"); Check(resources.RegisterForAnalysis(mesh), "DX hook cached resource");
            auto* entry = manager.GetFATForAnalysis()->FindByIDForAnalysis(7); entry->name = "cache";
            Check(resources.FindForAnalysis(type, "cache") == &mesh, "cache can match both exact and base Mesh categories");
            spMemoryStream stream; Open(stream, {}); spDXSerializerHook hook;
            hook.vfunc_24(manager.GetFATForAnalysis(), stream);
            const bool active = (masks[index] & 2) && index != 5;
            Check(entry->object == (active ? &mesh : nullptr), "PC bit2 and exact serialized MeshData are both required");
            Check(hook.GetLastBatchPlanForAnalysis().empty() && Position(stream) == 0,
                  "excluded/cached entries never parse metadata or request GPU batches");
        }
    }
    void DirectOwnership()
    {
        // Destructor observation is a host test fixture, not a new game class.
        class CountedPartition final : public spPartitionNode
        {
        public:
            explicit CountedPartition(unsigned& count) : deleted(count) {}
            ~CountedPartition() override { ++deleted; }
            unsigned& deleted;
        };
        unsigned deleted = 0;
        spSerializerManager manager; spResourceManager resources;
        (void)spPartitionNode::StaticRTTI(); (void)spBSPNode::StaticRTTI();
        Directory(manager, spPartitionNode::ClassID);
        auto* entry = manager.GetFATForAnalysis()->FindByIDForAnalysis(7);
        {
            spSerializerReadContextForAnalysis context(manager, resources);
            context.directOwnedClassIDsForAnalysis.push_back(spPartitionNode::ClassID);
            auto* child = context.PublishObjectForAnalysis(std::make_unique<CountedPartition>(deleted));
            auto* root = dynamic_cast<spBSPNode*>(context.PublishObjectForAnalysis(std::make_unique<spBSPNode>()));
            entry->object = child;
            Check(root && !context.ShareObjectForAnalysis(child) && context.createdObjects.empty(),
                  "direct family and derived BSP are never assigned competing shared owners");
            Bytes reference; Add(reference, 7U); Add(reference, 0U);
            spMemoryStream stream; Open(stream, reference);
            Check(spSerializer::ReadReferenceForAnalysis(context, 0, stream, stream) == child,
                  "borrowed FAT hit preserves identity before direct owning edge is read");
            auto owned = context.TakeDirectOwnerForAnalysis(child, root);
            Check(owned.get() == child && root->SetChildForAnalysis(0,
                  std::unique_ptr<spPartitionNode>(static_cast<spPartitionNode*>(owned.release()))),
                  "previously borrowed resource transfers once to actual owned child slot");
            Check(context.GetCreatedObjectCountForAnalysis() == 2 && deleted == 0,
                  "transferred descendants remain counted and alive");
            std::string error;
            Check(!context.TakeDirectOwnerForAnalysis(root, child, &error) && context.failed && !error.empty(),
                  "host rejects unique ownership cycle before moving the root");
        }
        Check(deleted == 1 && !entry->object, "parent deletes child exactly once and borrowed FAT alias clears");
        {
            spSerializerReadContextForAnalysis context(manager, resources);
            context.directOwnedClassIDsForAnalysis.push_back(spPartitionNode::ClassID);
            auto* child = context.PublishObjectForAnalysis(std::make_unique<CountedPartition>(deleted));
            spBSPNode first, second;
            auto owned = context.TakeDirectOwnerForAnalysis(child, &first);
            Check(first.SetChildForAnalysis(0, std::unique_ptr<spPartitionNode>(static_cast<spPartitionNode*>(owned.release()))),
                  "first direct owner accepts child");
            Check(!context.TakeDirectOwnerForAnalysis(child, &second) && context.failed,
                  "second direct parent cannot duplicate lifetime ownership");
        }
        Check(deleted == 2, "rejected second owner cannot double-delete the child");
    }
    void ConfiguredObjectBound()
    {
        spSerializerManager manager;spResourceManager resources;Directory(manager);
        Check(manager.RegisterForAnalysis(spAnimation::ClassID,std::make_shared<spAnimationSerializer>(),0xFF,3),"bounded reader registered");
        spMemoryStream input;Open(input,Reference());
        spSerializerReadContextForAnalysis context(manager,resources);
        context.maximumCreatedObjectsForAnalysis=0;
        Check(!spSerializer::ReadReferenceForAnalysis(context,0,input,input)&&context.failed
              &&context.GetCreatedObjectCountForAnalysis()==0,"configured host count limit rejects before factory allocation");
    }
}
int main()
{
    try
    {
        (void)spAnimation::StaticRTTI(); Success(false); Success(true); CacheAndSkip(); Failures(); DXHookCacheRules(); DirectOwnership(); ConfiguredObjectBound();
        std::cout << "PASS " << checks << '/' << checks << ": PC reference-reader reconstruction\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
