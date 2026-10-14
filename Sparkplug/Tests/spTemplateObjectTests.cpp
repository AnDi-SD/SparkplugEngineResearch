#include "Code/Sparkplug/spTemplateObject.h"
#include "Code/Sparkplug/spTemplate.h"
#include "Code/Sparkplug/spNode.h"

#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;

namespace
{
    int checks = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    template <std::size_t N>
    bool SameBits(const std::array<float, N>& first, const std::array<float, N>& second)
    {
        return std::memcmp(first.data(), second.data(), sizeof(float) * N) == 0;
    }
    constexpr spTemplateObject::Vector3 Zero{0.0F, 0.0F, 0.0F};
    constexpr spTemplateObject::Vector3 Unit{1.0F, 1.0F, 1.0F};
    constexpr spTemplateObject::Matrix3 Identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
    const spTemplateObject::Vector3 Position{2.5F, -0.0F, -7.0F};
    const spTemplateObject::Vector3 Scale{-2.0F, 0.25F, 3.0F};
    const spTemplateObject::Matrix3 Orientation{0, -1, 0, 1, 0, -0.0F, 0, 0, 1};

    void DefaultPlacement(const spTemplateObject& descriptor)
    {
        Check(SameBits(descriptor.GetPositionForAnalysis(), Zero), "PC startup descriptor position is zero");
        Check(SameBits(descriptor.GetOrientationForAnalysis(), Identity), "PC startup descriptor orientation is identity");
        Check(SameBits(descriptor.GetScaleForAnalysis(), Unit), "PC startup descriptor scale is one");
    }

    void ConstructorCloneAndExistingDestination()
    {
        spTemplateObject source;
        DefaultPlacement(source);
        Check(!source.HasSerializedDescriptorForAnalysis()
            && !source.GetNativeStateForAnalysis().has_value()
            && source.GetResourcePathForAnalysis() == nullptr,
            "native unwritten state/path stay explicitly unknown");
        Check(source.GetField12CForAnalysis() == 0 && source.GetField130ForAnalysis() == -1,
            "constructor descriptor identifier/parent defaults");
        source.SetName("source-name");
        Check(source.SetSerializedDescriptorForAnalysis(0, "descriptor-node"), "set native-valid state/path");
        source.SetPlacementForAnalysis(Position, Orientation, Scale);
        auto loaded = std::make_shared<spNode>();
        source.SetLoadedObjectForAnalysis(loaded);
        spCloneManager manager;
        auto copied = manager.Clone(source);
        auto* descriptorClone = dynamic_cast<spTemplateObject*>(copied.get());
        Check(descriptorClone && manager.FindClone(source) == nullptr,
            "real root clone succeeds and clears temporary clone mappings");
        DefaultPlacement(*descriptorClone);
        Check(descriptorClone->GetName() == nullptr
            && descriptorClone->GetLoadedObjectForAnalysis() == loaded.get()
            && *descriptorClone->GetNativeStateForAnalysis() == 0
            && std::strcmp(descriptorClone->GetResourcePathForAnalysis(), "descriptor-node") == 0,
            "selective copy shares loaded object/state/path while retaining fresh Named state");

        spTemplateObject destination;
        destination.SetName("destination-name");
        const spTemplateObject::Vector3 oldPosition{-0.0F, 17.0F, 23.0F};
        const spTemplateObject::Vector3 oldScale{0.0F, -3.0F, 5.0F};
        const spTemplateObject::Matrix3 oldOrientation{9, 8, 7, 6, 5, 4, 3, 2, 1};
        destination.SetPlacementForAnalysis(oldPosition, oldOrientation, oldScale);
        Check(source.vfunc_14(destination, manager), "selective copy into existing descriptor succeeds");
        Check(SameBits(destination.GetPositionForAnalysis(), oldPosition)
            && SameBits(destination.GetOrientationForAnalysis(), oldOrientation)
            && SameBits(destination.GetScaleForAnalysis(), oldScale),
            "existing descriptor placement is untouched by native copy");
        Check(std::strcmp(destination.GetName(), "destination-name") == 0,
            "copy does not invoke Named base");
        Check(source.vfunc_14(source, manager)
            && SameBits(source.GetPositionForAnalysis(), Position)
            && SameBits(source.GetOrientationForAnalysis(), Orientation)
            && SameBits(source.GetScaleForAnalysis(), Scale),
            "self-copy preserves P/R/S bits");
        auto created = spRTTIManager::Instance().Create(spTemplateObject::ClassID);
        auto* factoryDescriptor = dynamic_cast<spTemplateObject*>(created.get());
        Check(factoryDescriptor != nullptr, "RTTI factory creates real descriptor");
        DefaultPlacement(*factoryDescriptor);
    }

    void PlacementAndGraph()
    {
        auto sourceNode = std::make_shared<spNode>();
        sourceNode->SetName("loaded-node");
        sourceNode->SetPositionForAnalysis({11, 12, 13});
        sourceNode->SetScaleForAnalysis({4, 5, 6});
        sourceNode->SetOrientationForAnalysis(Orientation);
        sourceNode->MarkLocalTransformDirtyForAnalysis();

        // The real Template copy invokes each descriptor's real clone. Its
        // loaded Node remains shared; later Node graph cloning is separate.
        spTemplate sourceTemplate;
        auto descriptor = std::make_shared<spTemplateObject>();
        Check(descriptor->SetSerializedDescriptorForAnalysis(0, "loaded-node"), "graph descriptor state0 setup");
        descriptor->SetLoadedObjectForAnalysis(sourceNode);
        descriptor->SetPlacementForAnalysis(Position, Orientation, Scale);
        Check(sourceTemplate.AddObjectForAnalysis(descriptor), "template owns source descriptor");
        auto templateCloneBase = sourceTemplate.Clone();
        auto* templateClone = dynamic_cast<spTemplate*>(templateCloneBase.get());
        Check(templateClone && templateClone->GetObjectsForAnalysis().size() == 1,
            "template clone contains actual descriptor clone");
        const auto& clonedDescriptor = *templateClone->GetObjectsForAnalysis()[0];
        DefaultPlacement(clonedDescriptor);
        Check(clonedDescriptor.GetLoadedObjectForAnalysis() == sourceNode.get(),
            "descriptor clone preserves loaded source identity");
        auto nodeCloneBase = sourceNode->Clone();
        auto* clonedNode = dynamic_cast<spNode*>(nodeCloneBase.get());
        Check(clonedNode != nullptr, "separate real Node clone for placement");
        std::shared_ptr<spNode> child(static_cast<spNode*>(nodeCloneBase.release()));
        clonedDescriptor.ApplyNodePlacementForAnalysis(*child);
        Check(SameBits(child->GetPositionForAnalysis(), Zero)
            && SameBits(child->GetOrientationForAnalysis(), Identity)
            && SameBits(child->GetScaleForAnalysis(), Unit),
            "constructor-fresh descriptor placement overwrites cloned Node local P/R/S");
        Check((child->GetFlagsForAnalysis() & 1U) != 0,
            "placement marks Node local transform dirty");
        Check(SameBits(sourceNode->GetPositionForAnalysis(), spNode::Vector3{11, 12, 13})
            && SameBits(sourceNode->GetScaleForAnalysis(), spNode::Vector3{4, 5, 6}),
            "placement does not mutate shared loaded Node");

        // HOST graph orchestration using recovered Node operations. Selection
        // of descriptors and native Instance graph parent lookup are tested by
        // their own implementation; this checks the common placement consumer.
        spNode graphRoot;
        graphRoot.SetPositionForAnalysis({10, -20, 30});
        graphRoot.SetScaleForAnalysis({2, 3, 4});
        graphRoot.MarkLocalTransformDirtyForAnalysis();
        Check(graphRoot.AttachChildForAnalysis(child), "placement Node attaches to real graph");
        Check(graphRoot.UpdateWorldForAnalysis(), "whole graph transform update succeeds");
        Check(SameBits(child->GetWorldPositionForAnalysis(), spNode::Vector3{10, -20, 30})
            && SameBits(child->GetWorldScaleForAnalysis(), spNode::Vector3{2, 3, 4})
            && SameBits(child->GetWorldOrientationForAnalysis(), Identity),
            "default descriptor no longer collapses placed graph scale or orientation");

        descriptor->ApplyNodePlacementForAnalysis(*child);
        Check(SameBits(child->GetPositionForAnalysis(), Position)
            && SameBits(child->GetOrientationForAnalysis(), Orientation)
            && SameBits(child->GetScaleForAnalysis(), Scale),
            "nondefault descriptor placement transfers all bits including signed zero/scale");
        Check(graphRoot.UpdateWorldForAnalysis(), "graph updates again after nondefault placement");
        Check(SameBits(child->GetWorldScaleForAnalysis(), spNode::Vector3{-4, 0.75F, 12}),
            "signed placement scale participates in real inherited transform");
        Check(child->GetParentForAnalysis() == &graphRoot,
            "placement preserves graph parent identity");
    }

    template <std::size_t N>
    void EmitWords(const std::array<float, N>& values)
    {
        std::cout << '[';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            std::uint32_t word;
            std::memcpy(&word, &values[i], sizeof(word));
            if (i) std::cout << ',';
            std::cout << word;
        }
        std::cout << ']';
    }
    void EmitPlacementTail(const spTemplateObject& descriptor)
    {
        std::array<float, 15> tail{};
        const auto& position = descriptor.GetPositionForAnalysis();
        const auto& orientation = descriptor.GetOrientationForAnalysis();
        const auto& scale = descriptor.GetScaleForAnalysis();
        std::memcpy(tail.data(), position.data(), 3 * sizeof(float));
        std::memcpy(tail.data() + 3, orientation.data(), 9 * sizeof(float));
        std::memcpy(tail.data() + 12, scale.data(), 3 * sizeof(float));
        EmitWords(tail);
    }
    void EmitLifecycle()
    {
        spTemplateObject source, target;
        std::cout << "{\"ctorTail\":";
        EmitPlacementTail(source);
        Check(source.SetSerializedDescriptorForAnalysis(0, "descriptor-node"), "native compare descriptor setup");
        source.SetPlacementForAnalysis(Position, Orientation, Scale);
        target.SetPlacementForAnalysis({-0.0F, 17, 23}, {9, 8, 7, 6, 5, 4, 3, 2, 1}, {0, -3, 5});
        spCloneManager manager;
        Check(source.vfunc_14(target, manager), "native compare existing descriptor copy");
        std::cout << ",\"existingDestinationTail\":";
        EmitPlacementTail(target);
        Check(source.vfunc_14(source, manager), "native compare self-copy");
        std::cout << ",\"selfCopyTail\":";
        EmitPlacementTail(source);
        auto cloneBase = source.Clone();
        auto* clone = dynamic_cast<spTemplateObject*>(cloneBase.get());
        Check(clone != nullptr, "native compare real descriptor clone");
        std::cout << ",\"cloneTail\":";
        EmitPlacementTail(*clone);
        std::cout << "}\n";
    }
    void EmitPlacement(bool custom)
    {
        spTemplateObject descriptor;
        if (custom) descriptor.SetPlacementForAnalysis(Position, Orientation, Scale);
        spNode node;
        descriptor.ApplyNodePlacementForAnalysis(node);
        std::cout << "{\"placement\":";
        EmitPlacementTail(descriptor);
        std::cout << ",\"nodePosition\":";
        EmitWords(node.GetPositionForAnalysis());
        std::cout << ",\"nodeOrientation\":";
        EmitWords(node.GetOrientationForAnalysis());
        std::cout << ",\"nodeScale\":";
        EmitWords(node.GetScaleForAnalysis());
        std::cout << ",\"nodeDirtyFlags\":" << node.GetFlagsForAnalysis() << "}\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--native-lifecycle")
        { EmitLifecycle(); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--native-placement")
        {
            const std::string mode(argv[2]);
            Check(mode == "default" || mode == "custom", "bounded native placement mode");
            EmitPlacement(mode == "custom"); return 0;
        }
        Check(argc == 1, "known descriptor test invocation");
        ConstructorCloneAndExistingDestination();
        PlacementAndGraph();
        std::cout << "PASS " << checks << '/' << checks
            << ": PC template descriptor defaults, selective clone and Node graph placement\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
