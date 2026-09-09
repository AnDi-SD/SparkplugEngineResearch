#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spStaticRenderObjectSerializer.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spMesh.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    using Matrix = spStaticRenderObject::Matrix4;
    int checks = 0;
    void Check(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
    template<class T> void Add(Bytes& bytes, const T& value)
    { const auto* p = reinterpret_cast<const std::uint8_t*>(&value); bytes.insert(bytes.end(), p, p + sizeof(value)); }
    void Open(spMemoryStream& stream, const Bytes& bytes = {})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())), "fixture storage");
        if (!bytes.empty()) std::memcpy(stream.GetBuffer(), bytes.data(), bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart, 0), "fixture rewind");
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size = 0; Check(stream.GetSize(&size), "size");
        const auto* p = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return size ? Bytes(p, p + size) : Bytes{};
    }
    void Field(Bytes& bytes, unsigned id, const Bytes& value)
    {
        Check(id < 31 && !value.empty() && value.size() < 256, "tiny fixture field");
        bytes.push_back(static_cast<std::uint8_t>(0xa0 + id));
        bytes.push_back(static_cast<std::uint8_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }
    template<class T> void Field(Bytes& bytes, unsigned id, const T& value)
    { Bytes data; Add(data, value); Field(bytes, id, data); }
    bool Read(spSerializerReadContextForAnalysis& context, const Bytes& bytes, spStaticRenderObject& object)
    {
        spMemoryStream input; Open(input, bytes); std::string error;
        return spStaticRenderObjectSerializer{}.ReadPayloadForAnalysis(context, input,
            static_cast<std::uint32_t>(bytes.size()), object, &error);
    }
    Bytes Write(const spStaticRenderObject& object)
    {
        spMemoryStream output; Open(output); std::string error;
        if (!spStaticRenderObjectSerializer{}.WritePayloadForAnalysis(output, object, &error))
            throw std::runtime_error(error);
        return Data(output);
    }
    std::string Hex(const Bytes& bytes)
    {
        constexpr char digits[] = "0123456789abcdef"; std::string result;
        for (auto byte : bytes) { result += digits[byte >> 4]; result += digits[byte & 15]; }
        return result;
    }
    int RoundTrip()
    {
        std::string hex;
        while (std::getline(std::cin, hex))
        {
            Check(!hex.empty() && hex.size() <= 65536 && hex.size() % 2 == 0, "bounded hex input");
            Bytes bytes;
            for (std::size_t i = 0; i < hex.size(); i += 2)
                bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            spSerializerManager manager; spResourceManager resources;
            spSerializerReadContextForAnalysis context(manager, resources); spStaticRenderObject object;
            Check(Read(context, bytes, object), "round-trip read");
            Bytes matrices; Add(matrices, object.GetWorldMatrixForAnalysis()); Add(matrices, object.GetWorldInverseMatrixForAnalysis());
            std::cout << Hex(matrices) << ' ' << Hex(Write(object)) << '\n';
        }
        return 0;
    }
    class LiteralMesh : public spMesh
    { public: LiteralMesh() { SetBoundingSphereForAnalysis({1, 2, 3, 4}); } };
    void LifetimeAndReferences()
    {
        auto model = std::make_shared<spModel>(); model->SetBaseMeshForAnalysis(std::make_shared<LiteralMesh>());
        spStaticRenderObject object;
        const auto identity = object.GetWorldMatrixForAnalysis();
        Check(object.GetWorldInverseMatrixForAnalysis() == identity && identity[0] == 1 && identity[15] == 1, "constructor identity");
        Check(object.IsExactly(spStaticRenderObject::ClassID) && object.IsKindOf(spNamedObject::ClassID), "Static RTTI");
        auto world = identity; world[12] = 10; object.SetWorldMatrixForAnalysis(world);
        auto inverse = identity; inverse[13] = -7; object.SetWorldInverseMatrixForAnalysis(inverse);
        Check(object.AttachRenderableForAnalysis(model) && object.AttachRenderableForAnalysis(model), "duplicate original append");
        Check(object.GetRenderableCountForAnalysis() == 2 && model.use_count() == 3, "each occurrence retained");
        Check(object.GetLocalBoundingSphereForAnalysis() == spRenderable::BoundingSphere{1,2,3,4}
            && object.GetWorldBoundingSphereForAnalysis() == spRenderable::BoundingSphere{11,2,3,4}, "append uses current stored matrix");
        world[12] = 20; object.SetWorldMatrixForAnalysis(world);
        Check(object.GetWorldBoundingSphereForAnalysis()[0] == 11, "matrix assignment does not recompute support sphere");
        object.SetName("Static"); auto base = object.Clone(); auto* clone = dynamic_cast<spStaticRenderObject*>(base.get());
        Check(clone && clone->GetName() && std::string(clone->GetName()) == "Static", "Named-only clone");
        Check(clone->GetRenderableCountForAnalysis() == 0 && clone->GetWorldMatrixForAnalysis() == identity
            && clone->GetWorldInverseMatrixForAnalysis() == identity, "clone keeps fresh support and matrices");

        for (unsigned mode = 0; mode < 3; ++mode)
        {
            spSerializerManager manager; spResourceManager resources;
            auto* fat = manager.GetFATForAnalysis();
            Check(fat->IndexObjectForAnalysis(spModel::ClassID, *model), "explicit loaded model in FAT");
            auto* entry = fat->FindByObjectForAnalysis(*model); Check(entry != nullptr, "model identity");
            spSerializerReadContextForAnalysis context(manager, resources);
            if (mode != 1) context.externalOwners.push_back(model);
            Bytes reference; Add(reference, mode == 2 ? 0u : entry->id);
            if (mode != 2) Add(reference, 0u);
            Bytes bytes; Field(bytes, 0, reference); Field(bytes, 0, reference); bytes.push_back(0);
            spStaticRenderObject target; const bool ok = Read(context, bytes, target);
            if (mode == 0) Check(ok && !context.failed && target.GetRenderableCountForAnalysis() == 2
                && target.GetRenderableForAnalysis(0) == target.GetRenderableForAnalysis(1), "reader appends repeated resolved reference");
            else Check(!ok && context.failed, mode == 1 ? "host requires explicit owner" : "original rejects null renderable");
        }
    }
    void ScalarsAndEnvelope()
    {
        Matrix first{}, last{}; for (unsigned i = 0; i < 16; ++i) { first[i] = float(i - 7.0); last[i] = float(i + 3); }
        Bytes input; Field(input, 2, first); Field(input, 1, first); Field(input, 9, std::uint8_t(42)); Field(input, 1, last); input.push_back(0);
        spSerializerManager manager; spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager, resources); spStaticRenderObject object;
        Check(Read(context, input, object), "unknown/repeated/non-affine matrix fields accepted");
        Check(object.GetWorldMatrixForAnalysis() == last && object.GetWorldInverseMatrixForAnalysis() == first, "independent last fields");
        Bytes expected; Field(expected, 1, last); Field(expected, 2, first); expected.push_back(0);
        Check(Write(object) == expected, "writer always1 then2 then terminator");
        for (auto bytes : {Bytes{0xa1,63}, Bytes{0,0}, Bytes{0xa1,64,0}})
        {
            spSerializerReadContextForAnalysis invalid(manager, resources); spStaticRenderObject target;
            Check(!Read(invalid, bytes, target) && invalid.failed, "host rejects malformed bounded section");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--roundtrip") return RoundTrip();
        LifetimeAndReferences(); ScalarsAndEnvelope();
        std::cout << "PASS StaticRenderObject " << checks << " checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
