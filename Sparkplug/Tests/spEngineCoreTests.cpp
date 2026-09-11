#include "Code/Sparkplug/spEngineCore.h"
#include "Code/Sparkplug/spDebugManager.h"
#include "Code/Sparkplug/spDXMeshData.h"
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spDXTextureDataSerializer.h"
#include "Code/Sparkplug/spEntityManager.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spGameLevelSerializer.h"
#include "Code/Sparkplug/spIndexBuffer.h"
#include "Code/Sparkplug/spLight.h"
#include "Code/Sparkplug/spLightData.h"
#include "Code/Sparkplug/spLightDataSerializer.h"
#include "Code/Sparkplug/spLightSerializer.h"
#include "Code/Sparkplug/spMaterialSerializer.h"
#include "Code/Sparkplug/spMaterial.h"
#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spPS2Material.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialRenderTargetTexture.h"
#include "Code/Sparkplug/spMaterialCameraViewTexture.h"
#include "Code/Sparkplug/spMaterialCubeMapTexture.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spDXMaterialDataSerializer.h"
#include "Code/Sparkplug/spPS2MaterialDataSerializer.h"
#include "Code/Sparkplug/spCameraSerializer.h"
#include "Code/Sparkplug/spCameraDataSerializer.h"
#include "Code/Sparkplug/spCamera.h"
#include "Code/Sparkplug/spCameraData.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spMatColorControllerSerializer.h"
#include "Code/Sparkplug/spLightControllerSerializer.h"
#include "Code/Sparkplug/spAnimTexControllerSerializer.h"
#include "Code/Sparkplug/spUVControllerSerializer.h"
#include "Code/Sparkplug/spTransFunctionEvalSerializer.h"
#include "Code/Sparkplug/spFunctionEvalSerializer.h"
#include "Code/Sparkplug/spColorFuncEvalSerializer.h"
#include "Code/Sparkplug/spSphereBVSerializer.h"
#include "Code/Sparkplug/spBoxBVSerializer.h"
#include "Code/Sparkplug/spOBBBVSerializer.h"
#include "Code/Sparkplug/spMesh.h"
#include "Code/Sparkplug/spMeshData.h"
#include "Code/Sparkplug/spMeshDataSerializer.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spModelSerializer.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spPlatformSpecificMeshData.h"
#include "Code/Sparkplug/spPS2MeshData.h"
#include "Code/Sparkplug/spPS2Mesh.h"
#include "Code/Sparkplug/spPS2MeshDataSerializer.h"
#include "Code/Sparkplug/spPS2TextureDataSerializer.h"
#include "Code/Sparkplug/spRenderableSerializer.h"
#include "Code/Sparkplug/spRenderMesh.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/Sparkplug/spRenderer.h"
#include "Code/Sparkplug/spRenderTarget.h"
#include "Code/Sparkplug/spCubeRenderTarget.h"
#include "Code/Sparkplug/spRenderTargetManager.h"
#include "Code/Sparkplug/spSceneGraphOptimizer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spResource.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spSerializerHook.h"
#include "Code/Sparkplug/spFontManager.h"
#include "Code/Sparkplug/spInputManager.h"
#include "Code/Sparkplug/spTemplateInstance.h"
#include "Code/Sparkplug/spTemplateManager.h"
#include "Code/Sparkplug/spTemplateObject.h"
#include "Code/Sparkplug/spTemplateSerializer.h"
#include "Code/Sparkplug/spTexture.h"
#include "Code/Sparkplug/spTextureBuffer.h"
#include "Code/Sparkplug/spTextureData.h"
#include "Code/Sparkplug/spTextureDataSerializer.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugDX/spDXCombinedVB.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugDX/spDXMeshSerializer.h"
#include "Code/SparkplugDX/spDXSharedMeshData.h"
#include "Code/SparkplugDX/spDXSharedMeshDataSerializer.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXCamera.h"
#include "Code/SparkplugDX/spDXRenderTarget.h"
#include "Code/SparkplugDX/spDXCubeRenderTarget.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/SparkplugPS2/spPS2FontManager.h"
#include "Code/SparkplugPS2/spPS2InputManager.h"
#include "Code/SparkplugPS2/spPS2Renderer.h"
#include "Code/SparkplugPS2/spPS2Camera.h"
#include "Code/SparkplugPS2/spPS2RenderTarget.h"
#include "Code/SparkplugPS2/spPS2RenderTargetManager.h"
#if defined(_WIN32)
#include "Code/SparkplugPC/spDXInputManager.h"
#include "Code/SparkplugPC/spPCFontManager.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugPC/spPCRenderTarget.h"
#include "Code/SparkplugPC/spPCRenderTargetManager.h"
#endif

#include "Analysis/PC/SparkplugAbi.h"
#include "Analysis/PS2/SparkplugAbi.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <limits>
#include <string>
#include <vector>

namespace
{
    void Require(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    struct Probe final
    {
        int calls = 0;
        bool result = true;
    };

    bool RunProbe(void* context)
    {
        auto& probe = *static_cast<Probe*>(context);
        ++probe.calls;
        return probe.result;
    }

    class EntityProbe final : public sparkplug::reconstruction::spBaseObject
    {
    public:
        void vfunc_0C(const void* notification) noexcept override
        {
            ++calls;
            lastNotification = notification;
        }

        int calls = 0;
        const void* lastNotification = nullptr;
    };

    class TextureProbe final : public sparkplug::reconstruction::spTexture
    {
    public:
        TextureProbe() noexcept = default;
    };

    class SerializerProbe final : public sparkplug::reconstruction::spSerializer
    {
    public:
        SerializerProbe() noexcept = default;
    };

    class SceneGraphOptimizerProbe final
        : public sparkplug::reconstruction::spSceneGraphOptimizer
    {
    public:
        enum class Failure
        {
            None,
            Start,
            End,
            ChildNode,
            Model,
        };

        explicit SceneGraphOptimizerProbe(
            const Failure failure = Failure::None) noexcept
            : failure_(failure)
        {
        }

        std::vector<std::string> events;

    protected:
        bool OnStartOptimize(
            sparkplug::reconstruction::spNode& root) override
        {
            events.emplace_back(std::string("start:") + root.GetName());
            return failure_ != Failure::Start;
        }

        bool OnEndOptimize() override
        {
            events.emplace_back("end");
            return failure_ != Failure::End;
        }

        bool OnNode(sparkplug::reconstruction::spNode& node) override
        {
            events.emplace_back(std::string("node:") + node.GetName());
            return failure_ != Failure::ChildNode
                || std::strcmp(node.GetName(), "Child") != 0;
        }

        bool OnModel(
            sparkplug::reconstruction::spRenderNode& renderNode,
            sparkplug::reconstruction::spModel& model) override
        {
            events.emplace_back(
                std::string("model:") + renderNode.GetName() + ":"
                + model.GetName());
            return failure_ != Failure::Model;
        }

    private:
        Failure failure_;
    };

    class LifetimeSerializerProbe final
        : public sparkplug::reconstruction::spSerializer
    {
    public:
        LifetimeSerializerProbe(
            std::vector<int>& destructionOrder,
            const int marker) noexcept
            : destructionOrder_(&destructionOrder), marker_(marker)
        {
        }

        ~LifetimeSerializerProbe() override
        {
            destructionOrder_->push_back(marker_);
        }

    private:
        std::vector<int>* destructionOrder_;
        int marker_;
    };

    class HeaderProbeStream final : public sparkplug::reconstruction::spStream
    {
    public:
        HeaderProbeStream(
            const sparkplug::reconstruction::spSerializerFileHeader& header,
            const bool readSucceeds,
            const bool sizeSucceeds) noexcept
            : header_(header),
              readSucceeds_(readSucceeds),
              sizeSucceeds_(sizeSucceeds)
        {
        }

        bool Open(const char*) override { return false; }
        bool Open(std::uint32_t, const char*) override { return false; }
        bool Close() override { return false; }
        bool Seek(SeekSource, std::int32_t) override { return false; }
        bool GetCurrentPosition(std::uint32_t&) const override { return false; }
        bool ReadData(void* destination, const std::uint32_t byteCount) override
        {
            if (!readSucceeds_ || destination == nullptr
                || byteCount != sizeof(header_))
            {
                return false;
            }
            std::memcpy(destination, &header_, sizeof(header_));
            return true;
        }
        bool WriteData(const void*, std::uint32_t) override { return false; }
        bool vfunc_WriteFromStream(spStream*, std::uint32_t) override
        {
            return false;
        }
        bool GetSize(std::uint32_t* size) const override
        {
            if (!sizeSucceeds_ || size == nullptr)
            {
                return false;
            }
            *size = 0x20;
            return true;
        }

    private:
        sparkplug::reconstruction::spSerializerFileHeader header_;
        bool readSucceeds_;
        bool sizeSucceeds_;
    };
}

int main()
{
    using namespace sparkplug::reconstruction;

    const auto& debugRecord = spDebugManager::StaticRTTI();
    Require(debugRecord.base == &spBaseObject::StaticRTTI()
            && debugRecord.factory != nullptr,
        "debug manager is a concrete direct spBaseObject class");
    {
        spDebugManager manager;
        Require(spDebugManager::GetInstance() == &manager
                && manager.GetCycleIndexForAnalysis() == 0,
            "debug manager publishes itself with a zero cycle cursor");
        Require(manager.SetFlagForAnalysis(0, true)
                && manager.SetFlagForAnalysis(11, true)
                && !manager.SetFlagForAnalysis(12, true)
                && manager.GetFlagForAnalysis(0)
                && manager.GetFlagForAnalysis(11),
            "twelve native debug flag bytes are independently bounded");
        for (std::uint32_t index = 0;
             index < spDebugManager::CycleLength; ++index)
        {
            (void)manager.AdvanceCycleIndexForAnalysis();
        }
        Require(manager.GetCycleIndexForAnalysis() == 0,
            "debug cursor wraps at the native twenty-entry boundary");
        auto cloneBase = manager.Clone();
        auto* clone = dynamic_cast<spDebugManager*>(cloneBase.get());
        Require(clone != nullptr && !clone->GetFlagForAnalysis(0)
                && clone->GetCycleIndexForAnalysis() == 0,
            "debug-manager clone starts with blank runtime state");
    }
    Require(spDebugManager::GetInstance() == nullptr,
        "debug-manager destruction clears its singleton");

    const auto& entityRecord = spEntityManager::StaticRTTI();
    Require(entityRecord.base == &spBaseObject::StaticRTTI()
            && entityRecord.factory != nullptr,
        "entity manager is a concrete direct spBaseObject class");
    {
        spEntityManager manager;
        Require(spEntityManager::GetInstance() == &manager,
            "entity-manager construction publishes the singleton");
        auto first = std::make_unique<EntityProbe>();
        auto second = std::make_unique<EntityProbe>();
        auto* const firstRaw = first.get();
        auto* const secondRaw = second.get();
        Require(manager.AddForAnalysis(std::move(first))
                && manager.AddForAnalysis(std::move(second))
                && manager.GetEntityCountForAnalysis() == 2,
            "entity-manager insertion retains two owned runtime objects");
        const int notification = 7;
        manager.DispatchForAnalysis(&notification);
        Require(firstRaw->calls == 1 && secondRaw->calls == 1
                && firstRaw->lastNotification == &notification,
            "entity dispatch visits every managed object once");
        auto removed = manager.RemoveForAnalysis(*firstRaw);
        Require(removed.get() == firstRaw
                && manager.GetEntityCountForAnalysis() == 1
                && !manager.ContainsForAnalysis(*firstRaw),
            "entity removal transfers ownership without destroying the object");
        manager.ClearForAnalysis();
        Require(manager.GetEntityCountForAnalysis() == 0,
            "entity clear destroys and removes every retained object");
        auto cloneBase = manager.Clone();
        auto* clone = dynamic_cast<spEntityManager*>(cloneBase.get());
        Require(clone != nullptr && clone->GetEntityCountForAnalysis() == 0,
            "entity-manager clone starts with an empty runtime list");
    }
    Require(spEntityManager::GetInstance() == nullptr,
        "entity-manager destruction clears its singleton");

    const auto& nodeRecord = spNode::StaticRTTI();
    Require(nodeRecord.base == &spNamedObject::StaticRTTI()
            && nodeRecord.factory != nullptr,
        "node is a concrete direct spNamedObject class");
    {
        spNode node;
        Require(node.GetPositionForAnalysis() == spNode::Vector3{0.0F, 0.0F, 0.0F}
                && node.GetScaleForAnalysis() == spNode::Vector3{1.0F, 1.0F, 1.0F}
                && node.GetOrientationForAnalysis() == spNode::Matrix3{
                    1.0F, 0.0F, 0.0F,
                    0.0F, 1.0F, 0.0F,
                    0.0F, 0.0F, 1.0F}
                && node.GetFlagsForAnalysis() == spNode::NativeDefaultFlags
                && node.IsEnabledForAnalysis()
                && node.IsAnimatedForAnalysis()
                && !node.IsStaticForAnalysis()
                && !node.IsBoneForAnalysis(),
            "node construction preserves the executable-backed local defaults");

        node.SetName("Scene Root");
        node.SetPositionForAnalysis({1.0F, 2.0F, 3.0F});
        node.SetScaleForAnalysis({2.0F, 3.0F, 4.0F});
        node.SetStaticForAnalysis(true);
        node.SetAnimatedForAnalysis(false);
        node.SetBoneForAnalysis(true);
        node.SetBillboardAxisForAnalysis(2);
        auto child = std::make_shared<spNode>();
        child->SetName("Child");
        auto* const childRaw = child.get();
        Require(node.AttachChildForAnalysis(child)
                && node.AttachChildForAnalysis(child)
                && node.GetChildCountForAnalysis() == 1
                && childRaw->GetParentForAnalysis() == &node
                && childRaw->GetRootForAnalysis() == &node,
            "node attachment retains one parent and exposes the tree root");

        node.SetEnabledForAnalysis(false, true);
        Require(!node.IsEnabledForAnalysis() && !childRaw->IsEnabledForAnalysis(),
            "node enabled mask propagates through every attached child");

        auto cloneBase = node.Clone();
        auto* clone = dynamic_cast<spNode*>(cloneBase.get());
        Require(clone != nullptr
                && std::strcmp(clone->GetName(), "Scene Root") == 0
                && clone->GetPositionForAnalysis() == spNode::Vector3{1.0F, 2.0F, 3.0F}
                && clone->GetScaleForAnalysis() == spNode::Vector3{2.0F, 3.0F, 4.0F}
                && clone->IsStaticForAnalysis()
                && !clone->IsAnimatedForAnalysis()
                && clone->IsBoneForAnalysis()
                && clone->GetBillboardAxisForAnalysis() == 2
                && clone->GetChildCountForAnalysis() == 1
                && clone->GetChildForAnalysis(0) != childRaw
                && clone->GetChildForAnalysis(0)->GetParentForAnalysis() == clone,
            "node clone copies local state and deep-clones the child hierarchy");

        auto detached = node.DetachChildForAnalysis(*childRaw);
        Require(detached.get() == childRaw
                && detached->GetParentForAnalysis() == nullptr
                && node.GetChildCountForAnalysis() == 0,
            "node detachment transfers the host reference and clears parent state");
    }

    const auto& renderNodeRecord = spRenderNode::StaticRTTI();
    Require(renderNodeRecord.base == &spNode::StaticRTTI()
            && renderNodeRecord.factory != nullptr,
        "render node is a concrete direct spNode class");
    {
        spRenderNode renderNode;
        renderNode.SetName("Renderable root");
        auto model = std::make_shared<spModel>();
        model->SetName("Model A");
        auto* const modelPointer = model.get();

        Require(!renderNode.AttachRenderableForAnalysis(nullptr)
                && renderNode.AttachRenderableForAnalysis(model)
                && renderNode.AttachRenderableForAnalysis(model)
                && renderNode.GetRenderableCountForAnalysis() == 2
                && renderNode.GetRenderableForAnalysis(0) == modelPointer
                && !renderNode.AreRenderableBoundsDirtyForAnalysis(),
            "render node owns native-order relationships and immediately rebuilds bounds");

        renderNode.MarkRenderableBoundsCleanForAnalysis();
        auto cloneBase = renderNode.Clone();
        auto* clone = dynamic_cast<spRenderNode*>(cloneBase.get());
        Require(clone != nullptr
                && clone->IsKindOf(spNode::ClassID)
                && clone->GetRenderableCountForAnalysis() == 2
                && clone->GetRenderableForAnalysis(0)
                    != renderNode.GetRenderableForAnalysis(0)
                && clone->GetRenderableForAnalysis(0)
                    != clone->GetRenderableForAnalysis(1)
                && !clone->AreRenderableBoundsDirtyForAnalysis(),
            "PC render node clones each occurrence separately through the always-clone entry");

        auto detached = renderNode.DetachRenderableForAnalysis(*modelPointer);
        Require(detached == model
                && renderNode.GetRenderableCountForAnalysis() == 1
                && !renderNode.AreRenderableBoundsDirtyForAnalysis(),
            "host render node detachment transfers first relationship and refreshes bounds");
        renderNode.ClearRenderablesForAnalysis();
        Require(renderNode.GetRenderableCountForAnalysis() == 0,
            "render node clears every owned renderable relationship");
    }

    const auto& sceneOptimizerRecord = spSceneGraphOptimizer::StaticRTTI();
    Require(sceneOptimizerRecord.base == &spCrossPlatform::StaticRTTI()
            && sceneOptimizerRecord.factory == nullptr
            && spSceneGraphOptimizer::ClassID
                == sparkplug::evidence::pc::spSceneGraphOptimizerClassID
            && spSceneGraphOptimizer::ClassID
                == sparkplug::evidence::ps2::spSceneGraphOptimizerClassID,
        "scene graph optimizer is a non-factory common cross-platform class");
    {
        spRenderNode root;
        root.SetName("Root");
        auto model = std::make_shared<spModel>();
        model->SetName("Body");
        Require(root.AttachRenderableForAnalysis(model),
            "optimizer fixture attaches a model");
        auto child = std::make_shared<spNode>();
        child->SetName("Child");
        Require(root.AttachChildForAnalysis(child),
            "optimizer fixture attaches a child");

        SceneGraphOptimizerProbe optimizer;
        Require(optimizer.OptimizeForAnalysis(root)
                && optimizer.events
                    == std::vector<std::string>{
                        "start:Root",
                        "node:Root",
                        "model:Root:Body",
                        "node:Child",
                        "end",
                    },
            "optimizer preserves native start/node/model/recursive/end order");

        SceneGraphOptimizerProbe childFailure(
            SceneGraphOptimizerProbe::Failure::ChildNode);
        Require(!childFailure.OptimizeForAnalysis(root)
                && childFailure.events.back() == "end",
            "optimizer calls OnEndOptimize after a traversal failure");

        SceneGraphOptimizerProbe startFailure(
            SceneGraphOptimizerProbe::Failure::Start);
        Require(!startFailure.OptimizeForAnalysis(root)
                && startFailure.events
                    == std::vector<std::string>{"start:Root"},
            "optimizer stops before traversal and end callback when start fails");

        SceneGraphOptimizerProbe modelFailure(
            SceneGraphOptimizerProbe::Failure::Model);
        Require(!modelFailure.OptimizeForAnalysis(root)
                && modelFailure.events
                    == std::vector<std::string>{
                        "start:Root",
                        "node:Root",
                        "model:Root:Body",
                        "end",
                    },
            "optimizer propagates model failure and still finishes the pass");

        SceneGraphOptimizerProbe endFailure(
            SceneGraphOptimizerProbe::Failure::End);
        Require(!endFailure.OptimizeForAnalysis(root)
                && endFailure.events.back() == "end",
            "optimizer gives the end callback final success veto");
    }
    {
        spNode root;
        root.SetName("Prune root");
        auto emptyRenderLeaf = std::make_shared<spRenderNode>();
        emptyRenderLeaf->SetName("Empty leaf");
        auto* emptyRenderLeafPointer = emptyRenderLeaf.get();
        Require(root.AttachChildForAnalysis(emptyRenderLeaf),
            "optimizer prune fixture attaches an empty render leaf");

        SceneGraphOptimizerProbe optimizer;
        Require(optimizer.OptimizeForAnalysis(root)
                && root.GetChildCountForAnalysis() == 0
                && emptyRenderLeafPointer->GetParentForAnalysis() == nullptr,
            "optimizer defers and detaches empty render leaves after traversal");
    }

    const auto& renderNodeSerializerRecord =
        spRenderNodeSerializer::StaticRTTI();
    Require(renderNodeSerializerRecord.base == &spNodeSerializer::StaticRTTI()
            && renderNodeSerializerRecord.factory != nullptr,
        "render-node serializer is a concrete direct node-serializer class");
    {
        spRenderNodeSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis() == spRenderNode::ClassID,
            "render-node serializer targets spRenderNode");
        auto clone = serializer.Clone();
        Require(clone != nullptr
                && clone->IsExactly(spRenderNodeSerializer::ClassID),
            "render-node serializer clone creates a blank serializer");

        spRenderNode node;
        node.SetName("Serialized render node");
        auto defaultPlan = serializer.BuildKnownWritePlanForAnalysis(node);
        Require(defaultPlan.nodeFields
                    == std::vector<spNodeSerializer::Field>{
                        spNodeSerializer::Field::IsAnimated}
                && defaultPlan.renderables.empty(),
            "render-node serializer preserves the base mandatory animated field");

        auto model = std::make_shared<spModel>();
        model->SetName("Resolved model");
        std::shared_ptr<spBaseObject> relationship = model;
        Require(serializer.AttachResolvedRenderableForAnalysis(
                    node, relationship)
                && serializer.AttachResolvedRenderableForAnalysis(
                    node, relationship),
            "render-node reader seam accepts repeated spRenderable relationships");
        auto plan = serializer.BuildKnownWritePlanForAnalysis(node);
        Require(plan.renderables.size() == 2
                && plan.renderables[0] == model.get()
                && plan.renderables[1] == model.get(),
            "render-node writer plan repeats field zero in native storage order");

        std::shared_ptr<spBaseObject> wrongRelationship =
            std::make_shared<spNode>();
        Require(!serializer.AttachResolvedRenderableForAnalysis(
                    node, nullptr)
                && !serializer.AttachResolvedRenderableForAnalysis(
                    node, wrongRelationship)
                && node.GetRenderableCountForAnalysis() == 2,
            "render-node reader seam rejects null and non-renderable relationships");
    }

    const auto& lightRecord = spLight::StaticRTTI();
    const auto& lightDataRecord = spLightData::StaticRTTI();
    Require(lightRecord.base == &spNode::StaticRTTI()
            && lightRecord.factory == nullptr
            && lightDataRecord.base == &lightRecord
            && lightDataRecord.factory != nullptr,
        "light is an abstract node base and light-data is its concrete RTTI leaf");
    {
        spLightData light;
        Require(light.GetTypeForAnalysis() == spLight::Type::Directional
                && light.GetColorForAnalysis()
                    == spLight::ColorRGBA{1.0F, 1.0F, 1.0F, 1.0F}
                && !light.ProjectsShadowVolumeForAnalysis()
                && !light.UsesAttenuationForAnalysis()
                && light.GetIntensityForAnalysis() == spLight::DefaultIntensity
                && light.GetRangeForAnalysis() == spLight::DefaultRange
                && light.GetHotspotAngleForAnalysis() == 0.0F
                && light.GetFalloffAngleForAnalysis() == 0.0F
                && light.IsLightEnabledForAnalysis()
                && light.GetOpaqueRuntimeFieldBitsForAnalysis() == 0,
            "light construction preserves native semantic defaults and safely zeros opaque host state");

        light.SetName("Point Light");
        light.SetTypeForAnalysis(spLight::Type::Point);
        light.SetColorForAnalysis({0.25F, 0.5F, 0.75F, 1.0F});
        light.SetProjectsShadowVolumeForAnalysis(true);
        light.SetUsesAttenuationForAnalysis(true);
        light.SetIntensityForAnalysis(2.5F);
        light.SetRangeForAnalysis(64.0F);
        light.SetHotspotAngleForAnalysis(0.5F);
        light.SetFalloffAngleForAnalysis(1.0F);
        light.SetLightEnabledForAnalysis(false);
        light.SetOpaqueRuntimeFieldBitsForAnalysis(0x3F000000);

        auto cloneBase = light.Clone();
        auto* clone = dynamic_cast<spLightData*>(cloneBase.get());
        Require(clone != nullptr
                && clone->IsKindOf(spLight::ClassID)
                && clone->IsKindOf(spNode::ClassID)
                && std::strcmp(clone->GetName(), "Point Light") == 0
                && clone->GetTypeForAnalysis() == spLight::Type::Point
                && clone->GetColorForAnalysis()
                    == spLight::ColorRGBA{0.25F, 0.5F, 0.75F, 1.0F}
                && clone->ProjectsShadowVolumeForAnalysis()
                && clone->UsesAttenuationForAnalysis()
                && clone->GetIntensityForAnalysis() == spLight::DefaultIntensity
                && clone->GetRangeForAnalysis() == 64.0F
                && clone->GetHotspotAngleForAnalysis() == 0.5F
                && clone->GetFalloffAngleForAnalysis() == 1.0F
                && !clone->IsLightEnabledForAnalysis()
                && clone->GetOpaqueRuntimeFieldBitsForAnalysis() == 0x3F000000,
            "light-data clone reproduces the proven native copy, including its intensity omission");
    }

    const auto& serializerRecord = spSerializer::StaticRTTI();
    Require(serializerRecord.base == &spBaseObject::StaticRTTI()
            && serializerRecord.factory == nullptr,
        "serializer is a non-creatable direct spBaseObject class");
    {
        SerializerProbe serializer;
        Require(serializer.IsKindOf(spBaseObject::ClassID)
                && serializer.ResolveClassIDForAnalysis(0x5E6402DF)
                    == 0x5E6402DF
                && serializer.Clone() == nullptr,
            "serializer preserves the native identity remap and null clone");

        spMemoryStream objectHeaderStream;
        const std::uint32_t objectMarker = 0x4F4F4253;
        Require(objectHeaderStream.Open(nullptr)
                && objectHeaderStream.Write(spNode::ClassID)
                && objectHeaderStream.Write(objectMarker)
                && objectHeaderStream.Seek(spStream::SeekSource::essStart, 0),
            "serializer object-header fixture emits class ID and SBOO bytes");
        spSerializerObjectHeaderForAnalysis observedObjectHeader{};
        auto createdObject = serializer.ReadObjectHeaderAndCreateForAnalysis(
            objectHeaderStream, &observedObjectHeader);
        Require(dynamic_cast<spNode*>(createdObject.get()) != nullptr
                && observedObjectHeader.classID == spNode::ClassID
                && spSerializer::HasCanonicalObjectMarkerForAnalysis(
                    observedObjectHeader),
            "serializer object-header reader resolves and creates a registered class");

        spMemoryStream uncheckedMarkerStream;
        const std::uint32_t nonCanonicalMarker = 0xDEADBEEF;
        Require(uncheckedMarkerStream.Open(nullptr)
                && uncheckedMarkerStream.Write(spNode::ClassID)
                && uncheckedMarkerStream.Write(nonCanonicalMarker)
                && uncheckedMarkerStream.Seek(
                    spStream::SeekSource::essStart, 0),
            "serializer non-canonical marker fixture is ready");
        createdObject = serializer.ReadObjectHeaderAndCreateForAnalysis(
            uncheckedMarkerStream, &observedObjectHeader);
        Require(dynamic_cast<spNode*>(createdObject.get()) != nullptr
                && !spSerializer::HasCanonicalObjectMarkerForAnalysis(
                    observedObjectHeader),
            "native-compatible object creation exposes but does not reject an unchecked marker");

        spMemoryStream unknownObjectHeaderStream;
        Require(unknownObjectHeaderStream.Open(nullptr)
                && unknownObjectHeaderStream.Write(std::uint32_t{0x0BADF00D})
                && unknownObjectHeaderStream.Write(objectMarker)
                && unknownObjectHeaderStream.Seek(
                    spStream::SeekSource::essStart, 0)
                && serializer.ReadObjectHeaderAndCreateForAnalysis(
                       unknownObjectHeaderStream) == nullptr,
            "serializer object-header reader rejects an unregistered class ID");
    }

    {
        spDataBlockSerializer blocks;
        spMemoryStream stream;
        const std::uint32_t fixedPayload = 0x12345678;
        const std::array<std::uint8_t, 3> variablePayload{0xA1, 0xB2, 0xC3};
        Require(stream.Open(nullptr)
                && blocks.WriteFieldForAnalysis(
                    stream, 3, &fixedPayload, sizeof(fixedPayload))
                && blocks.WriteFieldForAnalysis(
                    stream, 0x2A, variablePayload.data(),
                    static_cast<std::uint32_t>(variablePayload.size()))
                && blocks.WriteFieldForAnalysis(stream, 7, nullptr, 0)
                && spDataBlockSerializer::WriteTerminatorForAnalysis(stream)
                && stream.Seek(spStream::SeekSource::essStart, 0),
            "data-block writer emits fixed, extended, empty and terminal fields");

        const auto* header = blocks.ReadHeaderForAnalysis(stream);
        Require(header != nullptr && header->fieldID == 3
                && header->payloadSize == sizeof(fixedPayload)
                && header->headerStreamPosition == 0
                && header->dataStreamPosition == 1,
            "data-block reader decodes a fixed-size inline-ID header");
        std::uint32_t observedFixed = 0;
        Require(stream.Read(observedFixed) && observedFixed == fixedPayload,
            "data-block fixed payload remains at the native data position");

        header = blocks.ReadHeaderForAnalysis(stream);
        Require(header != nullptr && header->fieldID == 0x2A
                && header->payloadSize == variablePayload.size()
                && header->headerStreamPosition == 5
                && header->dataStreamPosition == 8
                && spDataBlockSerializer::SkipDataForAnalysis(stream, *header),
            "data-block extended ID and UInt8 size prefix decode and skip exactly");

        header = blocks.ReadHeaderForAnalysis(stream);
        Require(header != nullptr && header->fieldID == 7
                && header->payloadSize == 0 && !header->IsTerminator(),
            "zero-length data uses the variable-size form rather than the terminator");
        header = blocks.ReadHeaderForAnalysis(stream);
        Require(header != nullptr && header->IsTerminator()
                && header->payloadSize == 0,
            "size-code zero maps to the native 0xFFFFFFFF terminal field ID");

        Require(spDataBlockSerializer::SelectSizeCodeForAnalysis(1)
                    == spDataBlockSerializer::SizeCode::Fixed1
                && spDataBlockSerializer::SelectSizeCodeForAnalysis(8)
                    == spDataBlockSerializer::SizeCode::Fixed8
                && spDataBlockSerializer::SelectSizeCodeForAnalysis(0)
                    == spDataBlockSerializer::SizeCode::UInt8
                && spDataBlockSerializer::SelectSizeCodeForAnalysis(256)
                    == spDataBlockSerializer::SizeCode::UInt16
                && spDataBlockSerializer::SelectSizeCodeForAnalysis(65536)
                    == spDataBlockSerializer::SizeCode::UInt32,
            "data-block size-code selector preserves the native boundary cases");
    }

    const auto& serializerManagerRecord = spSerializerManager::StaticRTTI();
    Require(serializerManagerRecord.base == &spBaseObject::StaticRTTI()
            && serializerManagerRecord.factory != nullptr
            && spSerializerManager::ClassID
                == sparkplug::evidence::pc::spSerializerManagerClassID
            && spSerializerManager::ClassID
                == sparkplug::evidence::ps2::spSerializerManagerClassID,
        "serializer manager is a concrete direct spBaseObject class on both platforms");
    Require(sizeof(sparkplug::evidence::pc::spSerializerManagerLayout) == 0x2C
            && sizeof(sparkplug::evidence::ps2::spSerializerManagerLayout) == 0x2C
            && sizeof(sparkplug::evidence::pc::
                    spSerializerRegistrationNodeLayout) == 0x18
            && sizeof(sparkplug::evidence::ps2::
                    spSerializerRegistrationNodeLayout) == 0x18
            && sizeof(spSerializerFileHeader) == 0x1C
            && sizeof(spSerializerObjectHeaderForAnalysis) == 0x08
            && sizeof(sparkplug::evidence::pc::spSerializerObjectHeaderLayout)
                == 0x08
            && sizeof(sparkplug::evidence::ps2::spSerializerObjectHeaderLayout)
                == 0x08
            && sizeof(sparkplug::evidence::pc::spDataBlockHeaderLayout) == 0x10
            && sizeof(sparkplug::evidence::ps2::spDataBlockHeaderLayout) == 0x10
            && sizeof(sparkplug::evidence::pc::spDataBlockSerializerLayout)
                == 0x28
            && sizeof(sparkplug::evidence::ps2::spDataBlockSerializerLayout)
                == 0x28
            && sizeof(sparkplug::evidence::ps2::spResourceFATHelperLayout)
                == 0x64
            && sizeof(sparkplug::evidence::ps2::spResourceFATFileEntryLayout)
                == 0x0C
            && sizeof(sparkplug::evidence::ps2::spResourceFATEntryLayout)
                == 0x24
            && sizeof(sparkplug::evidence::ps2::spSerializerHookLayout)
                == 0x10
            && sizeof(sparkplug::evidence::ps2::spPS2SerializerHookLayout)
                == 0x10
            && sparkplug::evidence::ps2::spSerializerHookAllocationSize
                == 0x10
            && sparkplug::evidence::pc::spSerializerHookClassID
                == sparkplug::evidence::ps2::spSerializerHookClassID,
        "serializer-manager, FAT and adjacent hook evidence extents agree");
    const auto& serializerHookRecord = spSerializerHook::StaticRTTI();
    const auto& ps2SerializerHookRecord = spPS2SerializerHook::StaticRTTI();
    Require(serializerHookRecord.base == &spBaseObject::StaticRTTI()
            && serializerHookRecord.factory == nullptr
            && ps2SerializerHookRecord.base == &serializerHookRecord
            && ps2SerializerHookRecord.factory != nullptr
            && spSerializerHook::ClassID
                == sparkplug::evidence::pc::spSerializerHookClassID
            && spSerializerHook::ClassID
                == sparkplug::evidence::ps2::spSerializerHookClassID
            && spPS2SerializerHook::ClassID
                == sparkplug::evidence::ps2::spPS2SerializerHookClassID,
        "serializer-hook RTTI preserves the abstract common base and concrete PS2 leaf");
    {
        spPS2SerializerHook hook;
        auto hookCloneBase = hook.Clone();
        Require(dynamic_cast<spPS2SerializerHook*>(hookCloneBase.get()) != nullptr,
            "PS2 serializer-hook clone constructs one blank leaf instance");
    }
    {
        spSerializerManager manager;
        Require(spSerializerManager::GetInstance() == &manager
                && manager.GetPlatformMaskForAnalysis() == 0
                && manager.GetOperationMaskForAnalysis()
                    == spSerializerManager::OperationLoad
                && manager.GetSerializationPolicyForAnalysis() == 2
                && manager.GetFATForAnalysis() != nullptr,
            "serializer-manager constructor restores the native 0/1/2 state and owned FAT");
        spPS2SerializerHook hook;
        spMemoryStream ignoredHookStream;
        hook.vfunc_24(manager.GetFATForAnalysis(), ignoredHookStream);
        Require(spSerializerManager::GetInstance() == &manager,
            "PS2 serializer hook keeps the existing manager and ignores FAT/stream arguments");

        // PS2 sub_0017F570 grammar: count, then ID/name/class/offset/size.
        spMemoryStream fatIndexStream;
        const std::uint32_t fatCount = 2;
        const std::uint32_t firstID = 7;
        const std::uint32_t firstOffset = 0x120;
        const std::uint32_t firstSize = 0x40;
        const std::uint32_t secondID = 11;
        const std::uint32_t secondOffset = 0x200;
        const std::uint32_t secondSize = 0x80;
        Require(fatIndexStream.Open(nullptr)
                && fatIndexStream.Write(fatCount)
                && fatIndexStream.Write(firstID)
                && fatIndexStream.Write("Node resource")
                && fatIndexStream.Write(spNode::ClassID)
                && fatIndexStream.Write(firstOffset)
                && fatIndexStream.Write(firstSize)
                && fatIndexStream.Write(secondID)
                && fatIndexStream.Write("Model resource")
                && fatIndexStream.Write(spModel::ClassID)
                && fatIndexStream.Write(secondOffset)
                && fatIndexStream.Write(secondSize)
                && fatIndexStream.Seek(spStream::SeekSource::essStart, 0),
            "FAT test emits and rewinds the native index grammar");
        auto* const fat = manager.GetFATForAnalysis();
        Require(fat->LoadIndexForAnalysis(fatIndexStream)
                && fat->GetResourceCountForAnalysis() == 2
                && fat->GetNextResourceIDForAnalysis() == 1,
            "FAT index loader accepts registered class IDs without changing the writer counter");
        const auto* const firstLoaded = fat->FindByIDForAnalysis(firstID);
        const auto* const secondLoaded = fat->FindByIDForAnalysis(secondID);
        Require(firstLoaded != nullptr
                && firstLoaded->name == "Node resource"
                && firstLoaded->classID == spNode::ClassID
                && firstLoaded->offset == firstOffset
                && firstLoaded->size == firstSize
                && firstLoaded->fileID == 0
                && !firstLoaded->payloadWritten
                && firstLoaded->object == nullptr
                && secondLoaded != nullptr
                && secondLoaded->name == "Model resource"
                && secondLoaded->classID == spModel::ClassID,
            "FAT entries preserve the five serialized fields and safe defaults");
        Require(fat->FirstForAnalysis() == firstLoaded
                && fat->NextForAnalysis() == secondLoaded
                && fat->NextForAnalysis() == nullptr,
            "FAT cursor traverses entries in native insertion order");
        fat->ClearResourceEntriesForAnalysis();
        Require(fat->GetResourceCountForAnalysis() == 0
                && fat->GetNextResourceIDForAnalysis() == 1
                && fat->FirstForAnalysis() == nullptr,
            "FAT resource clear empties every index and resets the writer ID");

        spNode indexedNode;
        indexedNode.SetName("Indexed node");
        Require(fat->IndexObjectForAnalysis(spNode::ClassID, indexedNode),
            "FAT writer indexes a new object");
        const auto* const indexed = fat->FindByObjectForAnalysis(indexedNode);
        Require(indexed != nullptr
                && indexed == fat->FindByIDForAnalysis(1)
                && indexed->id == 1
                && indexed->classID == spNode::ClassID
                && indexed->name == "Indexed node"
                && indexed->object == &indexedNode
                && !fat->IndexObjectForAnalysis(spNode::ClassID, indexedNode)
                && fat->GetNextResourceIDForAnalysis() == 2,
            "FAT writer copies a named-object name and rejects duplicate objects");

        spMemoryStream discardedFileIndex;
        const std::uint32_t fileCount = 1;
        const std::uint32_t fileID = 77;
        Require(discardedFileIndex.Open(nullptr)
                && discardedFileIndex.Write(fileCount)
                && discardedFileIndex.Write(fileID)
                && discardedFileIndex.Write("external.smo")
                && discardedFileIndex.Seek(spStream::SeekSource::essStart, 0)
                && fat->ReadDiscardedFileIndexForAnalysis(discardedFileIndex)
                && fat->GetFileCountForAnalysis() == 0,
            "PS2 file-index compatibility path consumes its grammar without retaining entries");

        spResourceFATHelperForAnalysis invalidFat;
        spMemoryStream invalidFatStream;
        const std::uint32_t oneEntry = 1;
        const std::uint32_t invalidID = 3;
        const spClassID unknownClassID = 0x0BADF00D;
        const std::uint32_t zero = 0;
        Require(invalidFatStream.Open(nullptr)
                && invalidFatStream.Write(oneEntry)
                && invalidFatStream.Write(invalidID)
                && invalidFatStream.Write("Unknown")
                && invalidFatStream.Write(unknownClassID)
                && invalidFatStream.Write(zero)
                && invalidFatStream.Write(zero)
                && invalidFatStream.Seek(spStream::SeekSource::essStart, 0)
                && !invalidFat.LoadIndexForAnalysis(invalidFatStream)
                && invalidFat.GetResourceCountForAnalysis() == 0,
            "FAT index loader rejects class IDs absent from native RTTI");

        auto ps2Load = std::make_unique<SerializerProbe>();
        auto* const ps2LoadPointer = ps2Load.get();
        auto pcLoad = std::make_unique<SerializerProbe>();
        auto* const pcLoadPointer = pcLoad.get();
        auto pcSave = std::make_unique<SerializerProbe>();
        auto* const pcSavePointer = pcSave.get();
        Require(manager.RegisterForAnalysis(0x11223344, std::move(ps2Load),
                    spSerializerManager::PlatformPS2,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(0x11223344, std::move(pcLoad),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(0x11223344, std::move(pcSave),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationSave)
                && !manager.RegisterForAnalysis(0x11223344, nullptr,
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.GetRegistrationCountForAnalysis() == 3,
            "serializer-manager appends owned registration records");

        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPC,
            spSerializerManager::OperationLoad);
        Require(manager.FindForAnalysis(0x11223344) == pcLoadPointer,
            "serializer lookup applies the PC and load masks");
        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPS2,
            spSerializerManager::OperationLoad);
        Require(manager.FindForAnalysis(0x11223344) == ps2LoadPointer,
            "serializer lookup applies the PS2 and load masks");
        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPC,
            spSerializerManager::OperationSave);
        Require(manager.FindForAnalysis(0x11223344) == pcSavePointer,
            "serializer lookup distinguishes the save direction");

        auto firstDuplicate = std::make_unique<SerializerProbe>();
        auto* const firstDuplicatePointer = firstDuplicate.get();
        auto laterDuplicate = std::make_unique<SerializerProbe>();
        Require(manager.RegisterForAnalysis(0x55667788,
                    std::move(firstDuplicate),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(0x55667788,
                    std::move(laterDuplicate),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad),
            "serializer manager permits native duplicate registrations");
        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPC,
            spSerializerManager::OperationLoad);
        Require(manager.FindForAnalysis(0x55667788) == firstDuplicatePointer,
            "serializer lookup is stable and returns the first matching record");

        auto sharedSerializer = std::make_shared<SerializerProbe>();
        auto* const sharedSerializerPointer = sharedSerializer.get();
        std::weak_ptr<SerializerProbe> sharedSerializerLifetime = sharedSerializer;
        Require(manager.RegisterForAnalysis(0x89ABCDEF, sharedSerializer,
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(0x89ABCDEF,
                    std::move(sharedSerializer),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationSave)
                && !sharedSerializerLifetime.expired(),
            "serializer-manager registrations may share one owned native instance");
        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPC,
            spSerializerManager::OperationSave);
        Require(manager.FindForAnalysis(0x89ABCDEF) == sharedSerializerPointer,
            "shared serializer registration remains selectable by operation mask");

        auto nodeSerializer = std::make_unique<SerializerProbe>();
        auto* const nodeSerializerPointer = nodeSerializer.get();
        Require(manager.RegisterForAnalysis(spNode::ClassID,
                    std::move(nodeSerializer),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad),
            "serializer manager accepts an object-class registration");
        manager.SetDispatchContextForAnalysis(
            spSerializerManager::PlatformPC,
            spSerializerManager::OperationLoad);
        spNode node;
        Require(manager.FindForAnalysis(node) == nodeSerializerPointer,
            "object lookup obtains the target ID through native RTTI identity");

        spSerializerFileHeader header{
            spSerializerManager::FileSignature,
            spSerializerManager::FileVersion,
            0x1234,
            0,
            spSerializerManager::PlatformPC,
            0x1C,
            0xFFFFFFFF,
        };
        Require(spSerializerManager::ValidateFileHeaderForAnalysis(
                    header, 0x20, spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::Valid,
            "native header validator accepts FFPS/version/platform/data-offset");
        header.signature = 0;
        Require(spSerializerManager::ValidateFileHeaderForAnalysis(
                    header, 0x20, spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::WrongFileType,
            "header validator rejects a non-FFPS signature first");
        header.signature = spSerializerManager::FileSignature;
        header.version = spSerializerManager::FileVersion + 1;
        Require(spSerializerManager::ValidateFileHeaderForAnalysis(
                    header, 0x20, spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::WrongVersion,
            "header validator rejects the wrong serializer version");
        header.version = spSerializerManager::FileVersion;
        header.platformMask = spSerializerManager::PlatformPS2;
        Require(spSerializerManager::ValidateFileHeaderForAnalysis(
                    header, 0x20, spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::UnsupportedPlatform,
            "PC validation rejects a PS2-only file");
        header.platformMask = spSerializerManager::PlatformCommon;
        header.dataOffset = 0x20;
        Require(spSerializerManager::ValidateFileHeaderForAnalysis(
                    header, 0x20, spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::DataOffsetBeyondEnd,
            "header validator preserves the native strict data-offset bound");

        header.platformMask = spSerializerManager::PlatformPC;
        header.dataOffset = 0x1C;
        spMemoryStream stream;
        Require(stream.ResizeAndSetSize(0x20),
            "header test allocates one complete native prefix");
        std::memcpy(stream.GetBuffer(), &header, sizeof(header));
        spSerializerFileHeader loadedHeader{};
        Require(manager.ReadAndValidateHeaderForAnalysis(stream,
                    spSerializerManager::PlatformPC, &loadedHeader)
                    == spSerializerFileHeaderStatus::Valid
                && loadedHeader.exportTag == header.exportTag
                && manager.GetPlatformMaskForAnalysis()
                    == spSerializerManager::PlatformPC
                && manager.GetOperationMaskForAnalysis()
                    == spSerializerManager::OperationLoad,
            "stream front-end reads 0x1C bytes and installs native load dispatch state");

        HeaderProbeStream failedRead(header, false, true);
        Require(manager.ReadAndValidateHeaderForAnalysis(failedRead,
                    spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::HeaderReadFailed,
            "stream front-end reports a failed native-sized header read");
        HeaderProbeStream failedSize(header, true, false);
        Require(manager.ReadAndValidateHeaderForAnalysis(failedSize,
                    spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::StreamSizeUnavailable,
            "stream front-end keeps header-read and size-query failures distinct");

        auto malformedHeader = header;
        malformedHeader.signature = 0;
        HeaderProbeStream malformedWithoutSize(malformedHeader, true, false);
        Require(manager.ReadAndValidateHeaderForAnalysis(malformedWithoutSize,
                    spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::WrongFileType,
            "signature validation precedes the native stream-size query");
        malformedHeader.signature = spSerializerManager::FileSignature;
        malformedHeader.version = spSerializerManager::FileVersion + 1;
        HeaderProbeStream oldVersionWithoutSize(malformedHeader, true, false);
        Require(manager.ReadAndValidateHeaderForAnalysis(oldVersionWithoutSize,
                    spSerializerManager::PlatformPC)
                    == spSerializerFileHeaderStatus::WrongVersion,
            "version validation also precedes the native stream-size query");

        auto managerCloneBase = manager.Clone();
        auto* const managerClone = dynamic_cast<spSerializerManager*>(
            managerCloneBase.get());
        Require(managerClone != nullptr
                && spSerializerManager::GetInstance() == managerClone
                && managerClone->GetPlatformMaskForAnalysis() == 0
                && managerClone->GetOperationMaskForAnalysis()
                    == spSerializerManager::OperationLoad
                && managerClone->GetSerializationPolicyForAnalysis() == 2
                && managerClone->GetRegistrationCountForAnalysis() == 0
                && managerClone->GetFATForAnalysis() != nullptr
                && managerClone->GetFATForAnalysis()->GetResourceCountForAnalysis()
                    == 0,
            "serializer-manager clone is blank and republishes the native singleton");
        managerCloneBase.reset();
        Require(spSerializerManager::GetInstance() == nullptr,
            "destroying a manager clone reproduces native unconditional singleton clearing");
    }
    Require(spSerializerManager::GetInstance() == nullptr,
        "serializer-manager destructor clears its singleton");
    std::vector<int> serializerDestructionOrder;
    {
        spSerializerManager manager;
        auto first = std::make_shared<LifetimeSerializerProbe>(
            serializerDestructionOrder, 1);
        auto second = std::make_shared<LifetimeSerializerProbe>(
            serializerDestructionOrder, 2);
        Require(manager.RegisterForAnalysis(1, first,
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(2, std::move(second),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad)
                && manager.RegisterForAnalysis(3, std::move(first),
                    spSerializerManager::PlatformPC,
                    spSerializerManager::OperationLoad),
            "serializer-manager accepts interleaved aliases for teardown testing");
    }
    Require(serializerDestructionOrder == std::vector<int>{1, 2},
        "serializer-manager teardown follows first-registration pointer groups");

    const auto& dxHookRecord = spDXSerializerHook::StaticRTTI();
    Require(dxHookRecord.base == &spSerializerHook::StaticRTTI()
            && dxHookRecord.factory != nullptr
            && spDXSerializerHook::ClassID
                == sparkplug::evidence::pc::spDXSerializerHookClassID
            && !spDXSerializerHook::HasCompleteNativeMaterializationForAnalysis(),
        "DX serializer hook preserves native identity while marking the GPU load tail open");
    {
        spMemoryStream meshBodies;
        spDataBlockSerializer fields;
        const std::uint32_t objectMarker = 0x4F4F4253;
        auto writeNativeMesh = [&](const std::uint32_t fvf,
                                   const std::uint32_t vertices,
                                   const std::uint32_t vertexBytes,
                                   const std::uint32_t indexBytes,
                                   const bool indices32,
                                   std::uint32_t& offset) {
            if (!meshBodies.GetCurrentPosition(offset)
                || !meshBodies.Write(spMeshData::ClassID)
                || !meshBodies.Write(objectMarker))
            {
                return false;
            }
            std::array<std::uint8_t, 17> payload{};
            std::memcpy(payload.data(), &fvf, sizeof(fvf));
            std::memcpy(payload.data() + 4, &vertices, sizeof(vertices));
            std::memcpy(payload.data() + 8, &vertexBytes, sizeof(vertexBytes));
            std::memcpy(payload.data() + 12, &indexBytes, sizeof(indexBytes));
            payload[16] = indices32 ? 1 : 0;
            return fields.WriteFieldForAnalysis(meshBodies, 1,
                       payload.data(), static_cast<std::uint32_t>(payload.size()))
                && spDataBlockSerializer::WriteTerminatorForAnalysis(meshBodies);
        };

        std::uint32_t firstOffset = 0;
        std::uint32_t secondOffset = 0;
        Require(meshBodies.Open(nullptr)
                && writeNativeMesh(0x112, 100, 3200, 600, false, firstOffset)
                && writeNativeMesh(0x112, 200, 6400, 1200, true, secondOffset),
            "DX hook fixture emits two native mesh-data blocks");

        spMemoryStream fatIndex;
        Require(fatIndex.Open(nullptr)
                && fatIndex.Write(std::uint32_t{2})
                && fatIndex.Write(std::uint32_t{21})
                && fatIndex.Write("DX mesh A")
                && fatIndex.Write(spMeshData::ClassID)
                && fatIndex.Write(firstOffset)
                && fatIndex.Write(std::uint32_t{28})
                && fatIndex.Write(std::uint32_t{22})
                && fatIndex.Write("DX mesh B")
                && fatIndex.Write(spMeshData::ClassID)
                && fatIndex.Write(secondOffset)
                && fatIndex.Write(std::uint32_t{28})
                && fatIndex.Seek(spStream::SeekSource::essStart, 0),
            "DX hook fixture emits the matching FAT entries");
        spResourceFATHelperForAnalysis fat;
        Require(fat.LoadIndexForAnalysis(fatIndex),
            "DX hook fixture loads through the universal FAT grammar");

        spDXMeshNativeInfoForAnalysis info;
        const auto* firstEntry = fat.FindByIDForAnalysis(21);
        Require(firstEntry != nullptr
                && spDXSerializerHook::ReadDXMeshDataInfoForAnalysis(
                    meshBodies, *firstEntry, info)
                && info.objectClassID == spMeshData::ClassID
                && info.objectMarker == objectMarker
                && info.foundNativeField && info.fvfCode == 0x112
                && info.vertexCount == 100 && info.vertexDataSize == 3200
                && info.indexDataSize == 600 && !info.indicesAre32Bit,
            "DX hook reads the exact five-value native field through spDataBlockSerializer");

        spDXSerializerHook hook;
        spSerializerManager hookManager;
        hookManager.SetDispatchContextForAnalysis(spSerializerManager::PlatformPC,
            spSerializerManager::OperationLoad);
        hook.vfunc_24(&fat, meshBodies);
        const auto& plan = hook.GetLastBatchPlanForAnalysis();
        Require(plan.size() == 1 && plan.front().fvfCode == 0x112
                && plan.front().vertexCount == 300
                && plan.front().vertexDataSize == 9600
                && plan.front().indexDataSize == 1800
                && plan.front().resourceIDs == std::vector<std::uint32_t>{21, 22},
            "DX hook greedily batches matching FVF entries below the strict 0x4E20 limit");
        auto cloneBase = hook.Clone();
        auto* clone = dynamic_cast<spDXSerializerHook*>(cloneBase.get());
        Require(clone != nullptr && clone->GetLastBatchPlanForAnalysis().empty(),
            "DX serializer-hook clone is a blank platform leaf");
    }

    const auto& nodeSerializerRecord = spNodeSerializer::StaticRTTI();
    Require(nodeSerializerRecord.base == &spSerializer::StaticRTTI()
            && nodeSerializerRecord.factory != nullptr,
        "node serializer is a concrete direct spSerializer RTTI class");
    {
        spNodeSerializer serializer;
        spNode defaultNode;
        Require(serializer.GetTargetClassIDForAnalysis() == spNode::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(defaultNode)
                    == std::vector<spNodeSerializer::Field>{
                        spNodeSerializer::Field::IsAnimated},
            "node serializer retains the target ID and mandatory animated field");

        spNode node;
        node.SetPositionForAnalysis({1.0F, 0.0F, 0.0F});
        node.SetOrientationForAnalysis({
            0.0F, -1.0F, 0.0F,
            1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F});
        node.SetScaleForAnalysis({2.0F, 1.0F, 1.0F});
        node.SetBoneForAnalysis(true);
        node.SetStaticForAnalysis(true);
        node.SetBillboardAxisForAnalysis(2);
        Require(node.AttachChildForAnalysis(std::make_shared<spNode>()),
            "node serializer test attaches one relationship");
        Require(serializer.BuildKnownWritePlanForAnalysis(node)
                == std::vector<spNodeSerializer::Field>{
                    spNodeSerializer::Field::Position,
                    spNodeSerializer::Field::Rotation,
                    spNodeSerializer::Field::Scale,
                    spNodeSerializer::Field::IsBone,
                    spNodeSerializer::Field::IsStatic,
                    spNodeSerializer::Field::IsAnimated,
                    spNodeSerializer::Field::Child,
                    spNodeSerializer::Field::BillboardAxis},
            "node serializer write plan preserves native field order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spNodeSerializer*>(cloneBase.get()) != nullptr,
            "node serializer has the native concrete blank-clone behavior");
    }

    const auto& lightSerializerRecord = spLightDataSerializer::StaticRTTI();
    Require(lightSerializerRecord.base == &spSerializer::StaticRTTI()
            && lightSerializerRecord.base != &spNodeSerializer::StaticRTTI()
            && lightSerializerRecord.factory != nullptr,
        "light-data serializer keeps flattened engine RTTI despite its C++ base");
    {
        spLightDataSerializer serializer;
        spLightData light;
        Require(serializer.GetTargetClassIDForAnalysis() == spLightData::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(light)
                    == spLightDataSerializer::KnownWritePlan{
                        {spNodeSerializer::Field::IsAnimated},
                        {spLightDataSerializer::Field::Enabled}},
            "light-data serializer preserves constructor state in native write plans");

        light.SetTypeForAnalysis(spLight::Type::Spot);
        light.SetProjectsShadowVolumeForAnalysis(true);
        light.SetColorForAnalysis({0.25F, 0.5F, 0.75F, 1.0F});
        light.SetUsesAttenuationForAnalysis(true);
        light.SetIntensityForAnalysis(2.0F);
        light.SetRangeForAnalysis(64.0F);
        light.SetHotspotAngleForAnalysis(0.5F);
        light.SetFalloffAngleForAnalysis(1.0F);
        light.SetLightEnabledForAnalysis(false);
        Require(serializer.BuildKnownWritePlanForAnalysis(light).lightFields
                == std::vector<spLightDataSerializer::Field>{
                    spLightDataSerializer::Field::Type,
                    spLightDataSerializer::Field::ProjectShadowVolume,
                    spLightDataSerializer::Field::Color,
                    spLightDataSerializer::Field::Attenuation,
                    spLightDataSerializer::Field::Intensity,
                    spLightDataSerializer::Field::Range,
                    spLightDataSerializer::Field::HotspotAngle,
                    spLightDataSerializer::Field::FalloffAngle},
            "light-data serializer applies native defaults in field order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spLightDataSerializer*>(cloneBase.get()) != nullptr,
            "light-data serializer has the native concrete blank clone");
    }

    const auto& baseLightSerializerRecord = spLightSerializer::StaticRTTI();
    Require(baseLightSerializerRecord.base == &spNodeSerializer::StaticRTTI()
            && baseLightSerializerRecord.factory != nullptr,
        "light serializer is a concrete direct spNodeSerializer RTTI class");
    {
        spLightSerializer serializer;
        spLightData light;
        Require(serializer.GetTargetClassIDForAnalysis() == spLight::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(light)
                    == spLightSerializer::KnownWritePlan{
                        {spNodeSerializer::Field::IsAnimated},
                        {spLightSerializer::Field::Enabled}},
            "light serializer targets spLight and preserves native defaults");

        light.SetTypeForAnalysis(spLight::Type::Spot);
        light.SetProjectsShadowVolumeForAnalysis(true);
        light.SetColorForAnalysis({0.25F, 0.5F, 0.75F, 1.0F});
        light.SetUsesAttenuationForAnalysis(true);
        light.SetIntensityForAnalysis(2.0F);
        light.SetRangeForAnalysis(64.0F);
        light.SetHotspotAngleForAnalysis(0.5F);
        light.SetFalloffAngleForAnalysis(1.0F);
        light.SetLightEnabledForAnalysis(false);
        Require(serializer.BuildKnownWritePlanForAnalysis(light).lightFields
                == std::vector<spLightSerializer::Field>{
                    spLightSerializer::Field::Type,
                    spLightSerializer::Field::ProjectShadowVolume,
                    spLightSerializer::Field::Color,
                    spLightSerializer::Field::Attenuation,
                    spLightSerializer::Field::Intensity,
                    spLightSerializer::Field::Range,
                    spLightSerializer::Field::HotspotAngle,
                    spLightSerializer::Field::FalloffAngle},
            "light serializer applies native defaults in field order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spLightSerializer*>(cloneBase.get()) != nullptr,
            "light serializer has the native concrete blank clone");
    }

    const auto& cameraSerializerRecord = spCameraSerializer::StaticRTTI();
    Require(cameraSerializerRecord.base == &spNodeSerializer::StaticRTTI()
            && cameraSerializerRecord.factory != nullptr,
        "camera serializer is a concrete direct spNodeSerializer class");
    {
        using Field = spCameraSerializer::Field;
        spCameraSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spCameraSerializer::TargetClassID,
            "camera serializer exposes the confirmed target class");
        Require(spCameraSerializer::BuildWritePlanForAnalysis(false)
                    == std::vector<Field>{Field::Camera}
                && spCameraSerializer::BuildWritePlanForAnalysis(true)
                    == std::vector<Field>{Field::Camera,
                        Field::TwoDimensional},
            "camera serializer always writes projection values and suppresses false 2D");
        Require(spCameraSerializer::IsKnownReadFieldForAnalysis(0)
                && spCameraSerializer::IsKnownReadFieldForAnalysis(1)
                && !spCameraSerializer::IsKnownReadFieldForAnalysis(2),
            "camera serializer recognizes only the confirmed fields");

        const spCameraSerializer::CameraPayload payload{
            0.1F, 1000.0F, 60.0F, 1.0F};
        Require(payload.nearClipPlane == 0.1F
                && payload.farClipPlane == 1000.0F
                && payload.viewAngle == 60.0F
                && payload.pixelAspectRatio == 1.0F,
            "camera payload preserves the confirmed four-float wire order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spCameraSerializer*>(cloneBase.get()) != nullptr,
            "camera serializer has the native concrete blank clone");
    }

    const auto& cameraDataSerializerRecord =
        spCameraDataSerializer::StaticRTTI();
    Require(cameraDataSerializerRecord.base
                == &spCameraSerializer::StaticRTTI()
            && cameraDataSerializerRecord.factory != nullptr,
        "camera-data serializer is a concrete direct camera-serializer leaf");
    {
        using Field = spCameraSerializer::Field;
        spCameraDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spCameraSerializer::TargetClassID
                && serializer.GetTargetClassIDForAnalysis()
                    != 0x24BB4C41,
            "camera-data serializer keeps the executable's spCamera target");
        Require(serializer.BuildWritePlanForAnalysis(true)
                    == std::vector<Field>{Field::Camera,
                        Field::TwoDimensional},
            "camera-data serializer reuses the complete camera field grammar");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spCameraDataSerializer*>(cloneBase.get())
                    != nullptr,
            "camera-data serializer has the native concrete blank clone");
    }

    Require(spCamera::StaticRTTI().base == &spNode::StaticRTTI()
            && spCamera::StaticRTTI().factory == nullptr
            && spCameraData::StaticRTTI().base == &spCamera::StaticRTTI()
            && spCameraData::StaticRTTI().factory != nullptr
            && spDXCamera::StaticRTTI().base == &spCamera::StaticRTTI()
            && spPS2Camera::StaticRTTI().base == &spCamera::StaticRTTI(),
        "runtime camera hierarchy preserves abstract base and concrete leaves");
    Require(sizeof(sparkplug::evidence::pc::spCameraObservedLayout) == 0x238
            && offsetof(sparkplug::evidence::pc::spCameraObservedLayout,
                nearClipPlane) == 0xBC
            && offsetof(sparkplug::evidence::pc::spCameraObservedLayout,
                dirtyFlags) == 0x224
            && sizeof(sparkplug::evidence::ps2::spCameraLayout) == 0x250
            && sizeof(sparkplug::evidence::ps2::spPS2CameraLayout) == 0x340
            && offsetof(sparkplug::evidence::ps2::spPS2CameraLayout,
                viewportWidth) == 0x330,
        "camera evidence layouts preserve PC shift and exact PS2 allocations");
    {
        auto cameraObject = spRTTIManager::Instance().Create(
            spCameraData::ClassID);
        auto dxObject = spRTTIManager::Instance().Create(spDXCamera::ClassID);
        auto ps2Object = spRTTIManager::Instance().Create(spPS2Camera::ClassID);
        auto* const camera = dynamic_cast<spCameraData*>(cameraObject.get());
        Require(spRTTIManager::Instance().Create(spCamera::ClassID) == nullptr
                && camera != nullptr
                && dynamic_cast<spDXCamera*>(dxObject.get()) != nullptr
                && dynamic_cast<spPS2Camera*>(ps2Object.get()) != nullptr,
            "camera RTTI factories match native abstract and concrete status");
        Require(camera->GetNearClipPlane() == 1.0F
                && camera->GetFarClipPlane() == 10000.0F
                && camera->GetPixelAspectRatio() == 1.0F
                && !camera->Is2DMode()
                && !camera->ConfigureViewportForAnalysis(0, 600, 1.0F, 10)
                && camera->ConfigureViewportForAnalysis(800, 600, 1.0F, 10)
                && camera->IsViewportActiveForAnalysis()
                && camera->GetViewportRatioForAnalysis() == 0.75F
                && camera->GetBackendModeForAnalysis() == 10
                && (camera->GetCameraDirtyFlagsForAnalysis() & 0x7F) == 0x7F,
            "camera defaults and viewport configuration follow native common state");

        spCamera::Matrix4 perspective{};
        spCamera::Matrix4 orthographic{};
        Require(camera->BuildProjectionMatrixForAnalysis(perspective)
                && camera->BuildProjectionMatrixForAnalysis(
                    orthographic, true)
                && std::abs(perspective[0] - 1.7320508F) < 0.0001F
                && std::abs(perspective[5] - 2.3094010F) < 0.0001F
                && perspective[11] == 1.0F
                && perspective[15] == 0.0F
                && std::abs(orthographic[0] - 3.4641016F) < 0.0001F
                && orthographic[11] == 0.0F
                && orthographic[15] == 1.0F,
            "portable camera reproduces both native projection-matrix branches");

        camera->SetNearClipPlane(0.5F);
        camera->SetFarClipPlane(500.0F);
        camera->SetViewAngle(0.75F);
        camera->SetPixelAspectRatio(1.25F);
        camera->Set2DModeForAnalysis(true);
        camera->SetViewAngle(0.75F);
        Require(!camera->Is2DMode(),
            "PC angle setter exits serialized2D mode independently of projection branch");
        camera->Set2DModeForAnalysis(true);
        auto cloneBase = camera->Clone();
        auto* const clone = dynamic_cast<spCameraData*>(cloneBase.get());
        Require(clone != nullptr
                && clone->GetNearClipPlane()
                    == spCamera::DefaultNearClipPlane
                && clone->GetFarClipPlane()
                    == spCamera::DefaultFarClipPlane
                && !clone->Is2DMode(),
            "camera leaf clone inherits native node-only copy semantics");
        camera->DeactivateViewportForAnalysis();
        Require(!camera->IsViewportActiveForAnalysis()
                && camera->GetCameraDirtyFlagsForAnalysis() == 0,
            "camera viewport teardown clears active and dirty state");
    }

    const auto& fogSerializerRecord = spFogSerializer::StaticRTTI();
    Require(fogSerializerRecord.base == &spSerializer::StaticRTTI()
            && fogSerializerRecord.factory != nullptr,
        "fog serializer is a concrete direct spSerializer RTTI class");
    {
        using Field = spFogSerializer::Field;
        spFogSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spFogSerializer::TargetClassID,
            "fog serializer exposes the confirmed spFog target");
        Require(spFogSerializer::BuildWritePlanForAnalysis()
                    == std::vector<Field>{Field::Fog},
            "fog serializer always emits its one five-value field");
        Require(spFogSerializer::IsKnownReadFieldForAnalysis(0)
                && !spFogSerializer::IsKnownReadFieldForAnalysis(1),
            "fog serializer recognizes only the confirmed field");

        const spFogSerializer::FogPayload payload{
            3, 0xFF102030, 100.0F, 1000.0F, 0.25F};
        Require(payload == spFogSerializer::FogPayload{
                    3, 0xFF102030, 100.0F, 1000.0F, 0.25F},
            "fog payload preserves type, ARGB and three floats in wire order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spFogSerializer*>(cloneBase.get()) != nullptr,
            "fog serializer has the native concrete blank clone");
    }

    const auto& matColorSerializerRecord =
        spMatColorControllerSerializer::StaticRTTI();
    Require(matColorSerializerRecord.base == &spSerializer::StaticRTTI()
            && matColorSerializerRecord.factory != nullptr,
        "material-color-controller serializer is a concrete direct serializer");
    {
        using Serializer = spMatColorControllerSerializer;
        using Field = Serializer::Field;
        using Kind = Serializer::EvaluatorKind;
        using Role = Serializer::EvaluatorRole;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "material-color-controller serializer exposes its confirmed target");
        Require(Serializer::BuildWritePlanForAnalysis()
                    == std::vector<Field>{Field::MaterialColorController}
                && Serializer::IsKnownReadFieldForAnalysis(0)
                && !Serializer::IsKnownReadFieldForAnalysis(1),
            "material-color-controller serializer has one outer field");
        const auto evaluatorPlan = Serializer::BuildEvaluatorPlanForAnalysis();
        Require(evaluatorPlan == Serializer::EvaluatorPlan{{
                    {Role::Ambient, Kind::ColorFunctional, 0x68},
                    {Role::Diffuse, Kind::ColorFunctional, 0xB8},
                    {Role::Specular, Kind::ColorFunctional, 0x108},
                    {Role::Emissive, Kind::ColorFunctional, 0x158},
                    {Role::Alpha, Kind::Functional, 0x1A8},
                }},
            "material-color-controller evaluator order and offsets agree");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "material-color-controller serializer has a concrete blank clone");
    }

    const auto& lightControllerSerializerRecord =
        spLightControllerSerializer::StaticRTTI();
    Require(lightControllerSerializerRecord.base == &spSerializer::StaticRTTI()
            && lightControllerSerializerRecord.factory != nullptr,
        "light-controller serializer is a concrete direct serializer");
    {
        using Serializer = spLightControllerSerializer;
        using Default = Serializer::DefaultRule;
        using Field = Serializer::Field;
        using Kind = Serializer::WireKind;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "light-controller serializer exposes its confirmed target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == Serializer::FieldSchema{{
                        {Field::Enabled, Kind::Boolean, Default::Zero, 0x10},
                        {Field::Color1, Kind::ColorARGB,
                            Default::PlatformColor, 0x2C},
                        {Field::Color2, Kind::ColorARGB,
                            Default::PlatformColor, 0x30},
                        {Field::Type, Kind::UInt32, Default::Zero, 0x68},
                        {Field::Frequency, Kind::Float32, Default::One, 0x48},
                        {Field::Amplitude, Kind::Float32, Default::One, 0x50},
                        {Field::Offset, Kind::Float32, Default::Zero, 0x58},
                        {Field::Pitch, Kind::Float32, Default::Zero, 0x5C},
                        {Field::Light, Kind::Relationship, Default::Always, 0x6C},
                    }},
            "light-controller field order, wire kinds and target offsets agree");

        Serializer::WriteShape pcDefaults;
        pcDefaults.color1ARGB =
            sparkplug::evidence::pc::spLightControllerSerializerDefaultColorARGB;
        pcDefaults.color2ARGB = pcDefaults.color1ARGB;
        Require(Serializer::BuildWritePlanForAnalysis(pcDefaults,
                    pcDefaults.color1ARGB)
                    == std::vector<Field>{Field::Light},
            "PC defaults suppress every scalar but retain the light relationship");

        Serializer::WriteShape ps2Defaults;
        Require(Serializer::BuildWritePlanForAnalysis(ps2Defaults,
                    sparkplug::evidence::ps2::
                        spLightControllerSerializerDefaultColorARGB)
                    == std::vector<Field>{Field::Light},
            "PS2 defaults use its distinct color value and retain the relationship");

        Serializer::WriteShape changed = pcDefaults;
        changed.enabled = true;
        changed.color1ARGB = 0xFF102030;
        changed.type = 2;
        changed.frequency = 4.0F;
        changed.amplitude = 3.0F;
        changed.offset = -2.0F;
        changed.pitch = 0.5F;
        Require(Serializer::BuildWritePlanForAnalysis(changed,
                    sparkplug::evidence::pc::
                        spLightControllerSerializerDefaultColorARGB)
                    == std::vector<Field>{Field::Enabled, Field::Color1,
                        Field::Type, Field::Frequency, Field::Amplitude,
                        Field::Offset, Field::Pitch, Field::Light},
            "light-controller writer preserves native default suppression order");
        const auto frequency = Serializer::DecodeFrequencyForAnalysis(4.0F);
        Require(frequency.frequency == 4.0F && frequency.reciprocal == 0.25F,
            "light-controller reader stores frequency and its reciprocal");
        Require(Serializer::IsKnownReadFieldForAnalysis(8)
                && !Serializer::IsKnownReadFieldForAnalysis(9),
            "light-controller reader recognizes exactly nine field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "light-controller serializer has a concrete blank clone");
    }

    const auto& animTexControllerSerializerRecord =
        spAnimTexControllerSerializer::StaticRTTI();
    Require(animTexControllerSerializerRecord.base == &spSerializer::StaticRTTI()
            && animTexControllerSerializerRecord.factory != nullptr,
        "animated-texture-controller serializer is a concrete direct serializer");
    {
        using Serializer = spAnimTexControllerSerializer;
        using Field = Serializer::Field;
        using Segment = Serializer::PayloadSegment;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "animated-texture-controller serializer exposes its confirmed target");
        Require(Serializer::BuildWritePlanForAnalysis()
                    == std::vector<Field>{Field::ControllerBase}
                && Serializer::IsKnownReadFieldForAnalysis(0)
                && !Serializer::IsKnownReadFieldForAnalysis(1),
            "animated-texture-controller serializer has one mandatory outer field");
        Require(Serializer::BuildPayloadPlanForAnalysis(38)
                    == Serializer::PayloadPlan{{
                        {Segment::FrameCount, 1, 4, false},
                        {Segment::TimeArray, 38, 152, false},
                        {Segment::TextureRelationships, 38, 0, true},
                    }},
            "animated-texture payload keeps count, contiguous times and relationships");
        Require(Serializer::BuildPayloadPlanForAnalysis(0)
                    == Serializer::PayloadPlan{{
                        {Segment::FrameCount, 1, 4, false},
                        {Segment::TimeArray, 0, 0, false},
                        {Segment::TextureRelationships, 0, 0, true},
                    }}
                && Serializer::HasConsistentTrackShapeForAnalysis(974, 974)
                && !Serializer::HasConsistentTrackShapeForAnalysis(38, 37),
            "animated-texture shape uses one count for both parallel arrays");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "animated-texture-controller serializer has a concrete blank clone");
    }

    const auto& uvControllerSerializerRecord =
        spUVControllerSerializer::StaticRTTI();
    Require(uvControllerSerializerRecord.base == &spSerializer::StaticRTTI()
            && uvControllerSerializerRecord.factory != nullptr,
        "UV-controller serializer is a concrete direct serializer");
    {
        using Serializer = spUVControllerSerializer;
        using Field = Serializer::Field;
        using EvaluatorRole = Serializer::EvaluatorRole;
        using VectorRole = Serializer::VectorRole;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "UV-controller serializer exposes its confirmed target");
        Require(Serializer::BuildWritePlanForAnalysis()
                    == std::vector<Field>{Field::UVController}
                && Serializer::IsKnownReadFieldForAnalysis(0)
                && !Serializer::IsKnownReadFieldForAnalysis(1),
            "UV-controller serializer has one mandatory outer field");
        Require(Serializer::BuildEvaluatorPlanForAnalysis()
                    == Serializer::EvaluatorPlan{{
                        {EvaluatorRole::TranslationX, 0x10, 0x5C},
                        {EvaluatorRole::TranslationY, 0x48, 0x94},
                        {EvaluatorRole::TranslationZ, 0x80, 0xCC},
                        {EvaluatorRole::ScaleX, 0xB8, 0x104},
                        {EvaluatorRole::ScaleY, 0xF0, 0x13C},
                        {EvaluatorRole::ScaleZ, 0x128, 0x174},
                        {EvaluatorRole::Rotation, 0x178, 0x1C4},
                    }},
            "UV-controller evaluator order and embedded offsets agree");
        Require(Serializer::BuildVectorPlanForAnalysis()
                    == Serializer::VectorPlan{{
                        {VectorRole::UVPivot, 0x160, 0x1AC},
                        {VectorRole::RotationAxis, 0x16C, 0x1B8},
                    }},
            "UV-controller pivot and rotation-axis offsets agree");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "UV-controller serializer has a concrete blank clone");
    }

    const auto& transFunctionEvalSerializerRecord =
        spTransFunctionEvalSerializer::StaticRTTI();
    Require(transFunctionEvalSerializerRecord.base == &spSerializer::StaticRTTI()
            && transFunctionEvalSerializerRecord.factory != nullptr,
        "transform-function-evaluator serializer is a concrete direct serializer");
    {
        using Serializer = spTransFunctionEvalSerializer;
        using Field = Serializer::Field;
        using EvaluatorRole = Serializer::EvaluatorRole;
        using VectorRole = Serializer::VectorRole;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "transform-function-evaluator serializer exposes its target");
        Require(Serializer::TransformFieldWireType == 5
                && Serializer::BuildWritePlanForAnalysis()
                    == std::vector<Field>{Field::TransformFunctions}
                && Serializer::IsKnownReadFieldForAnalysis(0)
                && !Serializer::IsKnownReadFieldForAnalysis(1),
            "transform-function-evaluator serializer has one type-5 field");
        Require(Serializer::BuildEvaluatorPlanForAnalysis()
                    == Serializer::EvaluatorPlan{{
                        {EvaluatorRole::TranslationX, 0x10},
                        {EvaluatorRole::TranslationY, 0x48},
                        {EvaluatorRole::TranslationZ, 0x80},
                        {EvaluatorRole::ScaleX, 0xB8},
                        {EvaluatorRole::ScaleY, 0xF0},
                        {EvaluatorRole::ScaleZ, 0x128},
                        {EvaluatorRole::Rotation, 0x178},
                    }},
            "transform evaluator order and target offsets agree");
        Require(Serializer::BuildVectorPlanForAnalysis()
                    == Serializer::VectorPlan{{
                        {VectorRole::UVPivot, 0x160},
                        {VectorRole::RotationAxis, 0x16C},
                    }},
            "transform evaluator vector offsets agree");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "transform-function-evaluator serializer has a concrete blank clone");
    }

    const auto& functionEvalSerializerRecord =
        spFunctionEvalSerializer::StaticRTTI();
    Require(functionEvalSerializerRecord.base == &spSerializer::StaticRTTI()
            && functionEvalSerializerRecord.factory != nullptr,
        "function-evaluator serializer is a concrete direct serializer");
    {
        using Serializer = spFunctionEvalSerializer;
        using Default = Serializer::DefaultRule;
        using Field = Serializer::Field;
        using Kind = Serializer::WireKind;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "function-evaluator serializer exposes its confirmed target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == Serializer::FieldSchema{{
                        {Field::FunctionType, Kind::UInt32, Default::Zero, 0x34},
                        {Field::Frequency, Kind::Float32, Default::One, 0x14},
                        {Field::Amplitude, Kind::Float32, Default::One, 0x1C},
                        {Field::XOffset, Kind::Float32, Default::Zero, 0x20},
                        {Field::YOffset, Kind::Float32, Default::Zero, 0x24},
                        {Field::Pitch, Kind::Float32, Default::Zero, 0x28},
                    }},
            "function-evaluator field order, defaults and offsets agree");
        Require(Serializer::BuildWritePlanForAnalysis({}).empty(),
            "function-evaluator default state emits no scalar fields");
        Serializer::WriteShape changed;
        changed.functionType = 8;
        changed.frequency = 4.0F;
        changed.amplitude = 3.0F;
        changed.xOffset = -2.0F;
        changed.yOffset = 0.5F;
        changed.pitch = 1.25F;
        Require(Serializer::BuildWritePlanForAnalysis(changed)
                    == std::vector<Field>{Field::FunctionType,
                        Field::Frequency, Field::Amplitude, Field::XOffset,
                        Field::YOffset, Field::Pitch},
            "function-evaluator writer preserves native suppression order");
        const auto frequency = Serializer::DecodeFrequencyForAnalysis(4.0F);
        Require(frequency.frequency == 4.0F && frequency.reciprocal == 0.25F,
            "function-evaluator reader stores frequency and its reciprocal");
        Require(Serializer::IsKnownReadFieldForAnalysis(5)
                && !Serializer::IsKnownReadFieldForAnalysis(6),
            "function-evaluator reader recognizes exactly six field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "function-evaluator serializer has a concrete blank clone");
    }

    const auto& colorFuncEvalSerializerRecord =
        spColorFuncEvalSerializer::StaticRTTI();
    Require(colorFuncEvalSerializerRecord.base == &spSerializer::StaticRTTI()
            && colorFuncEvalSerializerRecord.factory != nullptr,
        "color-function-evaluator serializer is a concrete direct serializer");
    {
        using Serializer = spColorFuncEvalSerializer;
        using Default = Serializer::DefaultRule;
        using Field = Serializer::Field;
        using Kind = Serializer::WireKind;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "color-function-evaluator serializer exposes its target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == Serializer::FieldSchema{{
                        {Field::Color1, Kind::ColorARGB,
                            Default::PlatformColor, 0x10},
                        {Field::Color2, Kind::ColorARGB,
                            Default::PlatformColor, 0x14},
                        {Field::FunctionType, Kind::UInt32, Default::Zero, 0x4C},
                        {Field::Frequency, Kind::Float32, Default::One, 0x2C},
                        {Field::Amplitude, Kind::Float32, Default::One, 0x34},
                        {Field::XOffset, Kind::Float32, Default::Zero, 0x38},
                        {Field::YOffset, Kind::Float32, Default::Zero, 0x3C},
                        {Field::Pitch, Kind::Float32, Default::Zero, 0x40},
                    }},
            "color-function-evaluator schema, defaults and offsets agree");
        Serializer::WriteShape pcDefaults;
        pcDefaults.color1ARGB = sparkplug::evidence::pc::
            spColorFuncEvalSerializerDefaultColorARGB;
        pcDefaults.color2ARGB = pcDefaults.color1ARGB;
        Require(Serializer::BuildWritePlanForAnalysis(pcDefaults,
                    pcDefaults.color1ARGB).empty(),
            "PC color-function defaults emit no scalar fields");
        Serializer::WriteShape ps2Defaults;
        Require(Serializer::BuildWritePlanForAnalysis(ps2Defaults,
                    sparkplug::evidence::ps2::
                        spColorFuncEvalSerializerDefaultColorARGB).empty(),
            "PS2 color-function defaults retain their distinct color value");
        Serializer::WriteShape changed = pcDefaults;
        changed.color1ARGB = 0xFF102030;
        changed.functionType = 6;
        changed.frequency = 0.1F;
        changed.amplitude = 0.5F;
        changed.xOffset = -1.0F;
        changed.yOffset = 1.0F;
        changed.pitch = 2.0F;
        Require(Serializer::BuildWritePlanForAnalysis(changed,
                    pcDefaults.color1ARGB)
                    == std::vector<Field>{Field::Color1, Field::FunctionType,
                        Field::Frequency, Field::Amplitude, Field::XOffset,
                        Field::YOffset, Field::Pitch},
            "color-function writer preserves native suppression order");
        const auto frequency = Serializer::DecodeFrequencyForAnalysis(4.0F);
        Require(frequency.frequency == 4.0F && frequency.reciprocal == 0.25F,
            "color-function reader stores frequency and its reciprocal");
        Require(Serializer::IsKnownReadFieldForAnalysis(7)
                && !Serializer::IsKnownReadFieldForAnalysis(8),
            "color-function reader recognizes exactly eight field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "color-function-evaluator serializer has a concrete blank clone");
    }

    const auto& sphereBVSerializerRecord = spSphereBVSerializer::StaticRTTI();
    Require(sphereBVSerializerRecord.base == &spSerializer::StaticRTTI()
            && sphereBVSerializerRecord.factory != nullptr,
        "sphere-BV serializer is a concrete direct serializer");
    {
        using Serializer = spSphereBVSerializer;
        using Field = Serializer::Field;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "sphere-BV serializer exposes the corpus-confirmed target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == std::vector<Serializer::FieldBinding>{
                        {Field::Position, 0x28, 0x18, false},
                        {Field::Radius, 0x34, 0x24, true},
                    },
            "sphere-BV wire sources and decoded mirrors agree");
        Serializer::WriteShape centered;
        centered.position = {0.001F, -0.001F, 0.0F};
        centered.radius = 2.0F;
        Require(Serializer::IsPositionSuppressedForAnalysis(centered.position)
                && Serializer::BuildWritePlanForAnalysis(centered)
                    == std::vector<Field>{Field::Radius},
            "sphere-BV suppresses a position within the native epsilon");
        centered.position.x = 0.0011F;
        Require(!Serializer::IsPositionSuppressedForAnalysis(centered.position)
                && Serializer::BuildWritePlanForAnalysis(centered)
                    == std::vector<Field>{Field::Position, Field::Radius},
            "sphere-BV writes a displaced position and always writes radius");
        Require(Serializer::IsKnownReadFieldForAnalysis(1)
                && !Serializer::IsKnownReadFieldForAnalysis(2),
            "sphere-BV reader recognizes exactly two field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "sphere-BV serializer has a concrete blank clone");
    }

    const auto& boxBVSerializerRecord = spBoxBVSerializer::StaticRTTI();
    Require(boxBVSerializerRecord.base == &spSerializer::StaticRTTI()
            && boxBVSerializerRecord.factory != nullptr,
        "box-BV serializer is a concrete direct serializer");
    {
        using Serializer = spBoxBVSerializer;
        using Field = Serializer::Field;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "box-BV serializer exposes the corpus-confirmed target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == std::vector<Serializer::FieldBinding>{
                        {Field::Position, 0x28, 0x18, false},
                        {Field::Size, 0x34, 0, true},
                    },
            "box-BV wire sources and decoded position mirror agree");
        Serializer::WriteShape shape;
        shape.position = {0.001F, -0.001F, 0.0F};
        shape.size = {3.0F, 4.0F, 12.0F};
        Require(Serializer::BuildWritePlanForAnalysis(shape)
                    == std::vector<Field>{Field::Size},
            "box-BV suppresses a position within epsilon and always writes size");
        shape.position.z = 0.0011F;
        Require(Serializer::BuildWritePlanForAnalysis(shape)
                    == std::vector<Field>{Field::Position, Field::Size},
            "box-BV writes a displaced position before size");
        const auto derived = Serializer::DecodeSizeForAnalysis(shape.size);
        Require(derived.halfExtents == Serializer::Vector3{1.5F, 2.0F, 6.0F}
                && derived.boundingSphereRadius == 6.5F,
            "box-BV reader derives half-extents and bounding sphere radius");
        Require(Serializer::IsKnownReadFieldForAnalysis(1)
                && !Serializer::IsKnownReadFieldForAnalysis(2),
            "box-BV reader recognizes exactly two field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "box-BV serializer has a concrete blank clone");
    }

    const auto& obbBVSerializerRecord = spOBBBVSerializer::StaticRTTI();
    Require(obbBVSerializerRecord.base == &spSerializer::StaticRTTI()
            && obbBVSerializerRecord.factory != nullptr,
        "OBB-BV serializer is a concrete direct serializer");
    {
        using Serializer = spOBBBVSerializer;
        using Field = Serializer::Field;
        using Encoding = Serializer::WireEncoding;
        Serializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == Serializer::TargetClassID,
            "OBB-BV serializer exposes the corpus-confirmed target");
        Require(Serializer::BuildFieldSchemaForAnalysis()
                    == std::vector<Serializer::FieldBinding>{
                        {Field::Position, 0x4C, 0x18,
                            Encoding::Vector3, false},
                        {Field::Size, 0x58, 0x58,
                            Encoding::Vector3, true},
                        {Field::Rotation, 0x28, 0x28,
                            Encoding::QuaternionFromMatrix3, false},
                    },
            "OBB-BV schema preserves sources, targets and rotation conversion");
        Serializer::WriteShape shape;
        shape.position = {0.001F, -0.001F, 0.0F};
        shape.size = {3.0F, 4.0F, 12.0F};
        Require(Serializer::BuildWritePlanForAnalysis(shape)
                    == std::vector<Field>{Field::Size},
            "OBB-BV suppresses centered identity state and always writes size");
        shape.position.z = 0.0011F;
        shape.rotation[1] = 0.0011F;
        Require(Serializer::BuildWritePlanForAnalysis(shape)
                    == std::vector<Field>{
                        Field::Position, Field::Size, Field::Rotation},
            "OBB-BV writes displaced position and non-identity rotation");
        const auto derived = Serializer::DecodeSizeForAnalysis(shape.size);
        Require(derived.halfExtents == Serializer::Vector3{1.5F, 2.0F, 6.0F}
                && derived.boundingSphereRadius == 6.5F,
            "OBB-BV reader derives half-extents and bounding sphere radius");
        Require(Serializer::IsKnownReadFieldForAnalysis(2)
                && !Serializer::IsKnownReadFieldForAnalysis(3),
            "OBB-BV reader recognizes exactly three field IDs");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<Serializer*>(cloneBase.get()) != nullptr,
            "OBB-BV serializer has a concrete blank clone");
    }

    const auto& materialSerializerRecord = spMaterialSerializer::StaticRTTI();
    Require(materialSerializerRecord.base == &spSerializer::StaticRTTI()
            && materialSerializerRecord.factory != nullptr,
        "material serializer is a concrete direct spSerializer RTTI class");
    {
        using Field = spMaterialSerializer::Field;
        using Relationship = spMaterialSerializer::Relationship;
        spMaterialSerializer serializer;
        spMaterialSerializer::MaterialWriteShape material;
        material.useVertexAlpha = true;
        spMaterialSerializer::PassWriteShape pass;
        pass.finalBlendOperation = 2;
        pass.layers.push_back({
            spMaterialSerializer::StandardLayerClassID, true, true, true, true});
        material.passes.push_back(pass);

        const auto writePlan =
            spMaterialSerializer::BuildStandardWritePlanForAnalysis(material);
        Require(writePlan.valid
                && writePlan.fields == std::vector<Field>{
                    Field::VertexAlpha,
                    Field::RenderStates,
                    Field::Pass,
                    Field::Layer,
                    Field::TextureStates,
                    Field::StaticUVTransform,
                    Field::Texture,
                    Field::AnimationController,
                    Field::UVController,
                    Field::Color,
                    Field::ColorController},
            "material serializer preserves the confirmed standard-layer order");
        Require(spMaterialSerializer::BuildStandardIndexPlanForAnalysis(material)
                == std::vector<Relationship>{
                    Relationship::Texture,
                    Relationship::UVController,
                    Relationship::AnimationController,
                    Relationship::ColorController},
            "material serializer indexes layer relations before color control");
        Require(spMaterialSerializer::IsKnownReadFieldForAnalysis(8)
                && spMaterialSerializer::IsKnownReadFieldForAnalysis(17)
                && !spMaterialSerializer::IsKnownReadFieldForAnalysis(5),
            "material serializer separates legacy/current states and unknown fields");

        auto& layer = material.passes.front().layers.front();
        layer.classID = spMaterialSerializer::EnvironmentMapLayerClassID;
        Require(spMaterialSerializer::
                    BuildStandardWritePlanForAnalysis(material).fields.back()
                    == Field::ColorController
                && spMaterialSerializer::
                    BuildStandardWritePlanForAnalysis(material).fields[9]
                    == Field::UVGeneration,
            "environment-map layer appends its UV-generation field");

        layer.classID = spMaterialSerializer::CameraViewLayerClassID;
        layer.hasCameraName = true;
        const auto cameraPlan =
            spMaterialSerializer::BuildStandardWritePlanForAnalysis(material);
        Require(cameraPlan.valid
                && cameraPlan.fields[9] == Field::RenderTarget
                && cameraPlan.fields[10] == Field::Camera,
            "camera-view layer writes common target settings before camera name");

        layer.classID = spMaterialSerializer::MirrorLayerClassID;
        const auto mirrorPlan =
            spMaterialSerializer::BuildStandardWritePlanForAnalysis(material);
        Require(mirrorPlan.valid
                && mirrorPlan.fields[9] == Field::RenderTarget
                && mirrorPlan.fields[10] == Field::CubeMap,
            "mirror layer owns the dynamic cube-map texture branch");

        layer.classID = spMaterialSerializer::MovieLayerClassID;
        const auto moviePlan =
            spMaterialSerializer::BuildStandardWritePlanForAnalysis(material);
        Require(moviePlan.valid
                && moviePlan.fields == std::vector<Field>{
                    Field::VertexAlpha, Field::RenderStates, Field::Pass,
                    Field::Layer, Field::Movie, Field::Color,
                    Field::ColorController}
                && spMaterialSerializer::BuildStandardIndexPlanForAnalysis(
                    material) == std::vector<Relationship>{
                        Relationship::ColorController},
            "movie layer serializes only its movie filename payload");

        layer.classID = 0xDEADBEEF;
        Require(!spMaterialSerializer::
                    BuildStandardWritePlanForAnalysis(material).valid
                && spMaterialSerializer::
                    BuildStandardIndexPlanForAnalysis(material).empty(),
            "material planner still refuses unknown layer classes");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spMaterialSerializer*>(cloneBase.get()) != nullptr,
            "material serializer has the native concrete blank clone");
    }

    const auto& materialDataSerializerRecord =
        spMaterialDataSerializer::StaticRTTI();
    Require(materialDataSerializerRecord.base
                == &spMaterialSerializer::StaticRTTI()
            && materialDataSerializerRecord.factory != nullptr,
        "material-data serializer is a concrete spMaterialSerializer leaf");
    {
        spMaterialDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spMaterialDataSerializer::TargetClassID,
            "material-data serializer exposes the confirmed target class");
        Require(!spMaterialDataSerializer::CanReadIntoObjectForAnalysis(false)
                && spMaterialDataSerializer::CanReadIntoObjectForAnalysis(true),
            "material-data serializer preserves the native null-target guard");

        spMaterialSerializer::MaterialWriteShape material;
        material.passes.push_back({});
        const auto inheritedPlan =
            serializer.BuildStandardWritePlanForAnalysis(material);
        Require(inheritedPlan.valid
                && inheritedPlan.fields
                    == std::vector<spMaterialSerializer::Field>{
                        spMaterialSerializer::Field::RenderStates,
                        spMaterialSerializer::Field::Pass,
                        spMaterialSerializer::Field::Color,
                        spMaterialSerializer::Field::ColorController},
            "material-data serializer reuses the common material grammar");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spMaterialDataSerializer*>(cloneBase.get())
                    != nullptr,
            "material-data serializer has the native concrete blank clone");
    }

    const auto& dxMaterialDataSerializerRecord =
        spDXMaterialDataSerializer::StaticRTTI();
    const auto& ps2MaterialDataSerializerRecord =
        spPS2MaterialDataSerializer::StaticRTTI();
    Require(dxMaterialDataSerializerRecord.base
                == &spMaterialSerializer::StaticRTTI()
            && ps2MaterialDataSerializerRecord.base
                == &spMaterialSerializer::StaticRTTI()
            && dxMaterialDataSerializerRecord.factory != nullptr
            && ps2MaterialDataSerializerRecord.factory != nullptr,
        "platform material-data serializers are direct concrete material leaves");
    {
        spDXMaterialDataSerializer dxSerializer;
        spPS2MaterialDataSerializer ps2Serializer;
        spMaterialSerializer::MaterialWriteShape material;
        material.passes.push_back({});
        const auto dxPlan =
            dxSerializer.BuildStandardWritePlanForAnalysis(material);
        const auto ps2Plan =
            ps2Serializer.BuildStandardWritePlanForAnalysis(material);
        Require(dxPlan.valid && ps2Plan.valid
                && dxPlan.fields == ps2Plan.fields,
            "platform material-data markers share the common material grammar");

        auto dxClone = dxSerializer.Clone();
        auto ps2Clone = ps2Serializer.Clone();
        Require(dynamic_cast<spDXMaterialDataSerializer*>(dxClone.get())
                    != nullptr
                && dynamic_cast<spPS2MaterialDataSerializer*>(ps2Clone.get())
                    != nullptr,
            "platform material-data serializers preserve concrete blank clones");
    }

    const auto& renderableSerializerRecord = spRenderableSerializer::StaticRTTI();
    Require(renderableSerializerRecord.base == &spSerializer::StaticRTTI()
            && renderableSerializerRecord.factory != nullptr,
        "renderable serializer is a concrete direct spSerializer RTTI class");
    {
        spRenderableSerializer serializer;
        spModel renderable;
        Require(serializer.GetTargetClassIDForAnalysis() == spRenderable::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(renderable)
                    == std::vector<spRenderableSerializer::Field>{
                        spRenderableSerializer::Field::AlphaSortEnable,
                        spRenderableSerializer::Field::AlphaSortPriority},
            "renderable serializer always emits both alpha-sort scalars");

        auto material = std::make_shared<spBaseObject>();
        auto fog = std::make_shared<spBaseObject>();
        renderable.SetMaterialForAnalysis(material);
        renderable.SetFogForAnalysis(fog);
        Require(serializer.BuildKnownWritePlanForAnalysis(renderable)
                == std::vector<spRenderableSerializer::Field>{
                    spRenderableSerializer::Field::Material,
                    spRenderableSerializer::Field::Fog,
                    spRenderableSerializer::Field::AlphaSortEnable,
                    spRenderableSerializer::Field::AlphaSortPriority},
            "renderable serializer preserves native relationship and scalar order");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spRenderableSerializer*>(cloneBase.get()) != nullptr,
            "renderable serializer has the native concrete blank clone");
    }

    const auto& modelSerializerRecord = spModelSerializer::StaticRTTI();
    Require(modelSerializerRecord.base == &spRenderableSerializer::StaticRTTI()
            && modelSerializerRecord.factory != nullptr,
        "model serializer is a concrete direct spRenderableSerializer class");
    {
        spModelSerializer serializer;
        spModel model;
        Require(serializer.GetTargetClassIDForAnalysis() == spModel::ClassID
                && spModelSerializer::MeshRelationshipClassID == spMesh::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(model)
                    == spModelSerializer::KnownWritePlan{
                        {spRenderableSerializer::Field::AlphaSortEnable,
                            spRenderableSerializer::Field::AlphaSortPriority},
                        {spModelSerializer::Field::ProjectionGroup}},
            "model serializer uses the base mesh type and mandatory projection group");

        auto meshData = std::make_shared<spMeshData>();
        model.SetBaseMeshForAnalysis(meshData);
        Require(serializer.BuildKnownWritePlanForAnalysis(model).modelFields
                == std::vector<spModelSerializer::Field>{
                    spModelSerializer::Field::Base,
                    spModelSerializer::Field::ProjectionGroup},
            "model serializer conditionally precedes projection group with mesh");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spModelSerializer*>(cloneBase.get()) != nullptr,
            "model serializer has the native concrete blank clone");
    }

    const auto& skinRecord = spSkin::StaticRTTI();
    Require(skinRecord.base == &spModel::StaticRTTI()
            && skinRecord.factory != nullptr,
        "skin is a concrete direct spModel class");
    const auto& skinSerializerRecord = spSkinSerializer::StaticRTTI();
    Require(skinSerializerRecord.base == &spModelSerializer::StaticRTTI()
            && skinSerializerRecord.factory != nullptr,
        "skin serializer is a concrete direct spModelSerializer class");
    {
        spSkin skin;
        spSkinSerializer serializer;
        Require(skin.GetWeightCountForAnalysis() == 4
                && skin.GetBoneCountForAnalysis() == 0
                && serializer.GetTargetClassIDForAnalysis() == spSkin::ClassID
                && spSkinSerializer::BoneRelationshipClassID == spNode::ClassID,
            "skin defaults and serializer target agree with the PC bodies");

        spSkin::Matrix4 identity{};
        identity[0] = identity[5] = identity[10] = identity[15] = 1.0F;
        auto bone = std::make_shared<spNode>();
        Require(skin.SetPaletteForAnalysis(4, {{bone, identity}})
                && skin.GetWeightCountForAnalysis() == 4
                && skin.GetBoneCountForAnalysis() == 1
                && skin.GetBoneBindingsForAnalysis()[0].GetBoneForAnalysis() == bone,
            "skin stores weight count and parallel bone/matrix palette");
        Require(!skin.SetPaletteForAnalysis(2, {{nullptr, identity}})
                && skin.GetWeightCountForAnalysis() == 4
                && skin.GetBoneCountForAnalysis() == 1,
            "safe skin facade rejects native-reader-invalid null bones atomically");

        using Segment = spSkinSerializer::PayloadSegment;
        const auto plan = serializer.BuildKnownWritePlanForAnalysis(skin);
        Require(plan.skinFields
                    == std::vector<spSkinSerializer::Field>{
                        spSkinSerializer::Field::Skin}
                && plan.payload
                    == std::vector<spSkinSerializer::PayloadPlanEntry>{
                        {Segment::WeightCount, 1, 4},
                        {Segment::BoneCount, 1, 4},
                        {Segment::BoneRelationship, 1, 0},
                        {Segment::InverseBindMatrix, 1, 64}}
                && spSkinSerializer::IsKnownReadFieldForAnalysis(0)
                && !spSkinSerializer::IsKnownReadFieldForAnalysis(1),
            "skin serializer preserves its one-field PC payload order");

        auto skinCloneBase = skin.Clone();
        const auto* skinClone = dynamic_cast<spSkin*>(skinCloneBase.get());
        Require(skinClone != nullptr
                && skinClone->GetWeightCountForAnalysis() == 4
                && skinClone->GetBoneCountForAnalysis() == 1
                && skinClone->GetBoneBindingsForAnalysis()[0].GetBoneForAnalysis() != bone
                && skinClone->GetBoneBindingsForAnalysis()[0].inverseBindMatrix == identity,
            "skin clone creates an unmapped bone and preserves inverse-bind matrices");
        auto serializerCloneBase = serializer.Clone();
        Require(dynamic_cast<spSkinSerializer*>(serializerCloneBase.get()) != nullptr,
            "skin serializer has the native concrete blank clone");
    }

    const auto& meshDataSerializerRecord = spMeshDataSerializer::StaticRTTI();
    Require(meshDataSerializerRecord.base == &spSerializer::StaticRTTI()
            && meshDataSerializerRecord.factory != nullptr,
        "mesh-data serializer is a concrete direct spSerializer class");
    {
        spMeshDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis() == spMeshData::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(0)
                    == std::vector<spMeshDataSerializer::Field>{
                        spMeshDataSerializer::Field::CrossPlatform}
                && serializer.BuildKnownWritePlanForAnalysis(2)
                    == std::vector<spMeshDataSerializer::Field>{
                        spMeshDataSerializer::Field::CrossPlatform}
                && serializer.BuildKnownWritePlanForAnalysis(1).empty()
                && serializer.BuildKnownWritePlanForAnalysis(3).empty(),
            "mesh-data serializer preserves the proven native mode gate");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spMeshDataSerializer*>(cloneBase.get()) != nullptr,
            "mesh-data serializer has the native concrete blank clone");
    }

    const auto& ps2MeshDataSerializerRecord =
        spPS2MeshDataSerializer::StaticRTTI();
    Require(ps2MeshDataSerializerRecord.base
                == &spMeshDataSerializer::StaticRTTI()
            && ps2MeshDataSerializerRecord.factory != nullptr,
        "PS2 mesh-data serializer is a concrete mesh-data serializer leaf");
    {
        using Field = spPS2MeshDataSerializer::Field;
        spPS2MeshDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spPS2MeshData::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(0)
                    == std::vector<Field>{Field::CrossPlatform,
                        Field::PlatformSpecific, Field::BoundingBox}
                && serializer.BuildKnownWritePlanForAnalysis(2)
                    == std::vector<Field>{Field::CrossPlatform,
                        Field::PlatformSpecific, Field::BoundingBox}
                && serializer.BuildKnownWritePlanForAnalysis(1)
                    == std::vector<Field>{Field::PlatformSpecific,
                        Field::BoundingBox},
            "PS2 mesh-data writer preserves the three-field order and mode gate");
        Require(!spPS2MeshDataSerializer::PCLoadsNativePayloadForAnalysis(0)
                && spPS2MeshDataSerializer::PCLoadsNativePayloadForAnalysis(2)
                && !spPS2MeshDataSerializer::PS2LoadsNativePayloadForAnalysis(2)
                && spPS2MeshDataSerializer::PS2LoadsNativePayloadForAnalysis(8),
            "PS2 mesh-data reader keeps the distinct PC and PS2 native flags");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spPS2MeshDataSerializer*>(cloneBase.get()) != nullptr,
            "PS2 mesh-data serializer has the native concrete blank clone");
    }

    const auto& dxMeshDataSerializerRecord =
        spDXMeshDataSerializer::StaticRTTI();
    Require(dxMeshDataSerializerRecord.base
                == &spMeshDataSerializer::StaticRTTI()
            && dxMeshDataSerializerRecord.factory != nullptr,
        "DX mesh-data serializer is a concrete mesh-data serializer leaf");
    {
        using Field = spDXMeshDataSerializer::Field;
        spDXMeshDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spDXMeshData::ClassID
                && serializer.BuildKnownWritePlanForAnalysis(0)
                    == std::vector<Field>{Field::CrossPlatform,
                        Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(2)
                    == std::vector<Field>{Field::CrossPlatform,
                        Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(1)
                    == std::vector<Field>{Field::PlatformSpecific},
            "DX mesh-data writer preserves its two-field order and mode gate");
        Require(!spDXMeshDataSerializer::PCLoadsNativePayloadForAnalysis(0)
                && spDXMeshDataSerializer::PCLoadsNativePayloadForAnalysis(2)
                && !spDXMeshDataSerializer::PS2LoadsNativePayloadForAnalysis(2)
                && spDXMeshDataSerializer::PS2LoadsNativePayloadForAnalysis(8),
            "DX mesh-data reader keeps the distinct PC and PS2 native flags");

        spIndexBuffer indices;
        spVertexBuffer vertices;
        spMeshData meshData;
        Require(indices.InitializeForAnalysis(
                    2, spIndexBuffer::eIndexBufferType::Type2, 1)
                && vertices.InitializeForAnalysis(0x20, 3)
                && meshData.InitializeForAnalysis(indices, vertices),
            "DX serializer header fixture creates valid owned mesh buffers");
        const auto header =
            spDXMeshDataSerializer::BuildNativePayloadHeaderForAnalysis(meshData);
        Require(header.valid && header.fvfCode == 0x20
                && header.vertexCount == 3
                && header.vertexDataSize == 84
                && header.indexDataSize == 24
                && header.indicesAre32Bit,
            "DX serializer reproduces the proven native payload header arithmetic");

        spMeshData emptyMesh;
        Require(!spDXMeshDataSerializer::
                    BuildNativePayloadHeaderForAnalysis(emptyMesh).valid,
            "DX serializer rejects a missing-buffer analysis fixture safely");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spDXMeshDataSerializer*>(cloneBase.get()) != nullptr,
            "DX mesh-data serializer has the native concrete blank clone");
    }

    const auto& templateRecord = spTemplateManager::StaticRTTI();
    Require(templateRecord.base == &spBaseObject::StaticRTTI()
            && templateRecord.factory != nullptr,
        "template manager is a concrete direct spBaseObject class");
    {
        spTemplateManager manager;
        auto first = std::make_shared<spNamedObject>("hero.tpl");
        auto second = std::make_shared<spNamedObject>("prop.tpl");
        Require(spTemplateManager::GetInstance() == &manager
                && manager.AddForAnalysis(first)
                && manager.AddForAnalysis(second)
                && manager.AddForAnalysis(first),
            "template registry retains native append/refcount behavior");
        Require(manager.GetTemplateCountForAnalysis() == 3
                && manager.FindForAnalysis("hero.tpl") == first.get()
                && manager.FindForAnalysis("missing.tpl") == nullptr,
            "template lookup returns the first normalized-name match");
        manager.ClearForAnalysis();
        Require(manager.GetTemplateCountForAnalysis() == 0
                && first.use_count() == 1,
            "template clear releases every retained reference");
        auto cloneBase = manager.Clone();
        auto* clone = dynamic_cast<spTemplateManager*>(cloneBase.get());
        Require(clone != nullptr && clone->GetTemplateCountForAnalysis() == 0,
            "template-manager clone starts with an empty registry");
    }
    Require(spTemplateManager::GetInstance() == nullptr,
        "template-manager destruction clears its singleton");

    const auto& templateInstanceRecord = spTemplateInstance::StaticRTTI();
    Require(templateInstanceRecord.base == &spNamedObject::StaticRTTI()
            && templateInstanceRecord.factory != nullptr,
        "template instance is a concrete direct spNamedObject class");
    {
        spTemplateInstance instance;
        instance.SetName("Encounter");
        Require(std::strcmp(
                    instance.GetInstanceRootForAnalysis().GetName(),
                    "Instance Root") == 0
                && instance.GetAttachedObjectCountForAnalysis() == 0,
            "template-instance construction creates the named empty root");
        auto cloneBase = instance.Clone();
        auto* clone = dynamic_cast<spTemplateInstance*>(cloneBase.get());
        Require(clone != nullptr
                && std::strcmp(clone->GetName(), "Encounter") == 0
                && std::strcmp(
                    clone->GetInstanceRootForAnalysis().GetName(),
                    "Instance Root") == 0
                && clone->GetAttachedObjectCountForAnalysis() == 0,
            "template-instance clone copies its inherited name but rebuilds runtime state");
    }

    const auto& templateObjectRecord = spTemplateObject::StaticRTTI();
    Require(templateObjectRecord.base == &spNamedObject::StaticRTTI()
            && templateObjectRecord.factory != nullptr,
        "template object is a concrete direct spNamedObject class");
    {
        spTemplateObject object;
        Require(!object.HasSerializedDescriptorForAnalysis()
                && !object.GetNativeStateForAnalysis().has_value()
                && object.GetResourcePathForAnalysis() == nullptr
                && object.GetField12CForAnalysis() == 0
                && object.GetField130ForAnalysis() == -1,
            "template-object safe construction exposes native initialized fields only");
        Require(object.SetSerializedDescriptorForAnalysis(2, "actors/flora.smo")
                && !object.SetSerializedDescriptorForAnalysis(4, "invalid")
                && !object.SetSerializedDescriptorForAnalysis(
                    0, std::string(spTemplateObject::NativePathCapacity, 'x')),
            "template serialized descriptor preserves four states and 0x100-byte path ABI");
        object.SetName("source name is not cloned");
        auto loaded = std::make_shared<spNamedObject>("loaded");
        object.SetLoadedObjectForAnalysis(loaded);
        auto cloneBase = object.Clone();
        auto* clone = dynamic_cast<spTemplateObject*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() == nullptr
                && clone->GetNativeStateForAnalysis().value_or(99) == 2
                && std::strcmp(
                    clone->GetResourcePathForAnalysis(), "actors/flora.smo") == 0
                && clone->GetLoadedObjectForAnalysis() == loaded.get(),
            "template-object clone copies serialized/runtime fields but not inherited name");
    }

    const auto& templateSerializerRecord = spTemplateSerializer::StaticRTTI();
    Require(templateSerializerRecord.base == &spBaseObject::StaticRTTI()
            && templateSerializerRecord.factory != nullptr,
        "template serializer is a concrete direct spBaseObject class");
    {
        spTemplateSerializer serializer;
        spTemplateObject target;
        spMemoryStream input;
        Require(serializer.GetTargetForAnalysis() == nullptr
                && serializer.GetInputForAnalysis() == nullptr
                && serializer.GetParentIDForAnalysis() == -1
                && !serializer.HasOutputForAnalysis(),
            "template-serializer constructor preserves its proven blank state");
        serializer.BindForAnalysis(&target, &input);
        Require(serializer.GetTargetForAnalysis() == &target
                && serializer.GetInputForAnalysis() == &input,
            "template serializer retains its non-owning target/input binding");
        auto cloneBase = serializer.Clone();
        auto* clone = dynamic_cast<spTemplateSerializer*>(cloneBase.get());
        Require(clone != nullptr && clone->GetTargetForAnalysis() == nullptr
                && clone->GetInputForAnalysis() == nullptr
                && clone->GetParentIDForAnalysis() == -1
                && !clone->HasOutputForAnalysis(),
            "template-serializer clone intentionally starts completely blank");
    }

    const auto& gameLevelRecord = spGameLevel::StaticRTTI();
    Require(gameLevelRecord.base == &spNamedObject::StaticRTTI()
            && gameLevelRecord.factory != nullptr,
        "game level is a concrete direct spNamedObject class");
    {
        spGameLevel level;
        level.SetName("Alfea02");
        auto first = std::make_unique<spTemplateInstance>();
        auto second = std::make_unique<spTemplateInstance>();
        auto* const firstRaw = first.get();
        Require(level.AddInstanceForAnalysis(std::move(first))
                && level.AddInstanceForAnalysis(std::move(second))
                && !level.AddInstanceForAnalysis(nullptr)
                && level.GetInstanceCountForAnalysis() == 2,
            "game-level instance list owns appended template instances");
        auto removed = level.RemoveInstanceForAnalysis(*firstRaw);
        Require(removed.get() == firstRaw
                && level.GetInstanceCountForAnalysis() == 1,
            "game-level removal transfers instance ownership safely");
        auto cloneBase = level.Clone();
        auto* clone = dynamic_cast<spGameLevel*>(cloneBase.get());
        Require(clone != nullptr && std::strcmp(clone->GetName(), "Alfea02") == 0
                && clone->GetInstanceCountForAnalysis() == 0,
            "game-level clone copies its name but starts with no runtime instances");
        level.ClearInstancesForAnalysis();
        Require(level.GetInstanceCountForAnalysis() == 0,
            "game-level clear releases all retained instances");
    }

    const auto& gameLevelSerializerRecord = spGameLevelSerializer::StaticRTTI();
    Require(gameLevelSerializerRecord.base == &spBaseObject::StaticRTTI()
            && gameLevelSerializerRecord.factory != nullptr,
        "game-level serializer is a concrete direct spBaseObject class");
    {
        spGameLevelSerializer serializer;
        spGameLevel target;
        spMemoryStream input;
        serializer.BindForAnalysis(&target, &input);
        Require(serializer.GetTargetForAnalysis() == &target
                && serializer.GetInputForAnalysis() == &input,
            "game-level serializer retains non-owning target/input binding");
        auto cloneBase = serializer.Clone();
        auto* clone = dynamic_cast<spGameLevelSerializer*>(cloneBase.get());
        Require(clone != nullptr && clone->GetTargetForAnalysis() == nullptr
                && clone->GetInputForAnalysis() == nullptr,
            "game-level serializer clone starts with no transaction state");
    }

    const auto& indexBufferRecord = spIndexBuffer::StaticRTTI();
    Require(indexBufferRecord.base == &spBaseObject::StaticRTTI()
            && indexBufferRecord.factory != nullptr,
        "index buffer is a concrete direct spBaseObject class");
    {
        using Type = spIndexBuffer::eIndexBufferType;
        spIndexBuffer buffer;
        Require(!buffer.IsInitializedForAnalysis()
                && buffer.GetTypeForAnalysis() == Type::Type2,
            "index-buffer constructor preserves native blank/default-type state");
        Require(buffer.InitializeForAnalysis(4, Type::Type1)
                && buffer.GetIndexCountForAnalysis() == 4,
            "type 1 keeps one index per primitive");
        Require(buffer.InitializeForAnalysis(4, Type::Type2)
                && buffer.GetIndexCountForAnalysis() == 12,
            "type 2 expands each primitive to three indices");
        Require(buffer.InitializeForAnalysis(4, Type::Type3)
                && buffer.GetIndexCountForAnalysis() == 6,
            "type 3 adds the native two-index strip prefix");
        Require(buffer.InitializeForAnalysis(4, Type::Type4, 1)
                && buffer.GetIndexCountForAnalysis() == 8
                && buffer.GetIndexElementSizeForAnalysis() == 4,
            "type 4 doubles indices and format bit zero selects 32-bit storage");
        for (std::uint32_t index = 0; index < 8; ++index)
        {
            Require(buffer.SetIndexForAnalysis(index, 70000U + index),
                "32-bit index storage retains values above uint16 range");
        }

        auto copy = buffer.CopyBufferForAnalysis();
        Require(copy != nullptr && copy->IsInitializedForAnalysis()
                && copy->GetIndexForAnalysis(7).value_or(0) == 70007,
            "independent native-style copy duplicates index payload");
        auto cloneBase = buffer.Clone();
        auto* clone = dynamic_cast<spIndexBuffer*>(cloneBase.get());
        Require(clone != nullptr && !clone->IsInitializedForAnalysis()
                && clone->GetTypeForAnalysis() == Type::Type2,
            "RTTI clone remains constructor-blank and differs from buffer copy");

        spMemoryStream serialized;
        Require(serialized.Open("index-buffer-round-trip")
                && buffer.WriteForAnalysis(serialized)
                && serialized.Seek(spStream::SeekSource::essStart, 0),
            "index buffer writes its three-field header and payload");
        spIndexBuffer roundTrip;
        Require(roundTrip.ReadForAnalysis(serialized)
                && roundTrip.GetPrimitiveCountForAnalysis() == 4
                && roundTrip.GetIndexCountForAnalysis() == 8
                && roundTrip.GetFormatFlagsForAnalysis() == 1
                && roundTrip.GetIndexForAnalysis(7).value_or(0) == 70007,
            "index buffer round-trips the native 32-bit stream grammar");

        spIndexBuffer narrow;
        Require(narrow.InitializeForAnalysis(1, Type::Type2)
                && !narrow.SetIndexForAnalysis(0, 65536),
            "16-bit index storage rejects values that native bytes cannot retain");
        buffer.ReleaseForAnalysis();
        Require(!buffer.IsInitializedForAnalysis()
                && buffer.GetPrimitiveCountForAnalysis() == 4,
            "release clears ownership/state while retaining native metadata fields");
    }

    const auto& vertexBufferRecord = spVertexBuffer::StaticRTTI();
    Require(vertexBufferRecord.base == &spBaseObject::StaticRTTI()
            && vertexBufferRecord.factory != nullptr,
        "vertex buffer is a concrete direct spBaseObject class");
    {
        spVertexBuffer buffer;
        Require(!buffer.IsInitializedForAnalysis()
                && buffer.GetVertexStrideForAnalysis() == 0
                && buffer.GetComponentCountForAnalysis() == 0,
            "vertex-buffer constructor preserves its native zero state");
        Require(buffer.InitializeForAnalysis(0x0840, 2, 7)
                && buffer.GetVertexStrideForAnalysis() == 32
                && buffer.GetComponentCountForAnalysis() == 8
                && buffer.GetVertexSizeForAnalysis() == 64
                && buffer.GetComponentOffsetsForAnalysis()[0] == 0
                && buffer.GetComponentOffsetsForAnalysis()[7] == 3
                && buffer.GetComponentOffsetsForAnalysis()[12] == 6,
            "0x0840 layout maps XYZ, three-component field32 and UV0 exactly");

        std::vector<std::byte> bytes(buffer.GetVertexSizeForAnalysis());
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            bytes[index] = static_cast<std::byte>(index);
        }
        Require(buffer.SetDataForAnalysis(bytes),
            "vertex payload accepts exactly one computed native allocation");
        auto copy = buffer.CopyBufferForAnalysis();
        Require(copy != nullptr && copy->IsInitializedForAnalysis()
                && copy->GetDataForAnalysis() == bytes,
            "independent vertex-buffer copy duplicates metadata and payload");
        auto cloneBase = buffer.Clone();
        auto* clone = dynamic_cast<spVertexBuffer*>(cloneBase.get());
        Require(clone != nullptr && !clone->IsInitializedForAnalysis()
                && clone->GetVertexStrideForAnalysis() == 0,
            "RTTI clone remains constructor-blank and differs from buffer copy");

        spMemoryStream serialized;
        Require(serialized.Open("vertex-buffer-round-trip")
                && buffer.WriteForAnalysis(serialized)
                && serialized.Seek(spStream::SeekSource::essStart, 0),
            "vertex buffer writes its three-word header and computed payload");
        spVertexBuffer roundTrip;
        Require(roundTrip.ReadForAnalysis(serialized)
                && roundTrip.GetComponentFlagsForAnalysis() == 0x0840
                && roundTrip.GetVertexCountForAnalysis() == 2
                && roundTrip.GetFlagsForAnalysis() == 7
                && roundTrip.GetDataForAnalysis() == bytes,
            "vertex buffer round-trips the native stream grammar");

        buffer.ReleaseForAnalysis();
        Require(!buffer.IsInitializedForAnalysis()
                && buffer.GetVertexCountForAnalysis() == 2
                && buffer.GetVertexStrideForAnalysis() == 32,
            "release clears ownership/state while retaining native metadata");
        Require(buffer.InitializeRawForAnalysis(17)
                && buffer.GetVertexSizeForAnalysis() == 17
                && buffer.GetVertexCountForAnalysis() == 0
                && buffer.GetVertexStrideForAnalysis() == 32,
            "raw initialization preserves the native stale-layout edge case");
    }

    const auto& meshDataRecord = spMeshData::StaticRTTI();
    Require(meshDataRecord.base == &spMesh::StaticRTTI()
            && meshDataRecord.factory != nullptr,
        "mesh data is a concrete direct spMesh class");
    {
        using Type = spIndexBuffer::eIndexBufferType;
        spIndexBuffer indices;
        Require(indices.InitializeForAnalysis(1, Type::Type2)
                && indices.SetIndexForAnalysis(0, 2)
                && indices.SetIndexForAnalysis(1, 0)
                && indices.SetIndexForAnalysis(2, 1),
            "mesh-data fixture creates one indexed triangle");

        const std::array<spMesh::Position, 3> positions{{
            {-4.0F, 7.0F, 2.0F},
            {3.0F, -5.0F, 8.0F},
            {1.0F, 4.0F, -6.0F},
        }};
        spVertexBuffer vertices;
        Require(vertices.InitializeForAnalysis(0, 3),
            "position-only vertex buffer uses the native 12-byte layout");
        std::vector<std::byte> vertexBytes(vertices.GetVertexSizeForAnalysis());
        std::memcpy(vertexBytes.data(), positions.data(), vertexBytes.size());
        Require(vertices.SetDataForAnalysis(vertexBytes),
            "mesh-data fixture stores three XYZ positions");

        spMeshData meshData;
        meshData.SetName("mesh-data-probe");
        Require(meshData.InitializeForAnalysis(indices, vertices)
                && meshData.GetIndexBufferForAnalysis() != &indices
                && meshData.GetVertexBufferForAnalysis() != &vertices
                && meshData.HasBoundsForAnalysis()
                && meshData.GetMinimumForAnalysis()
                    == spMesh::Position{-4.0F, -5.0F, -6.0F}
                && meshData.GetMaximumForAnalysis()
                    == spMesh::Position{3.0F, 7.0F, 8.0F},
            "mesh-data init owns deep buffer copies and runs inherited bounds");

        auto deepCopy = meshData.CopyMeshDataForAnalysis();
        Require(deepCopy != nullptr
                && deepCopy->GetIndexBufferForAnalysis() != nullptr
                && deepCopy->GetVertexBufferForAnalysis() != nullptr
                && !deepCopy->HasBoundsForAnalysis()
                && deepCopy->GetMinimumForAnalysis()
                    == meshData.GetMinimumForAnalysis(),
            "mesh-data deep copy transfers buffers and floats but not validity byte");
        auto cloneBase = meshData.Clone();
        auto* clone = dynamic_cast<spMeshData*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "mesh-data-probe") == 0
                && clone->GetIndexBufferForAnalysis() == nullptr
                && clone->GetVertexBufferForAnalysis() == nullptr,
            "mesh-data RTTI clone copies only the inherited resource name");
    }

    const auto& platformMeshRecord =
        spPlatformSpecificMeshData::StaticRTTI();
    Require(platformMeshRecord.base == &spNamedObject::StaticRTTI()
            && platformMeshRecord.factory != nullptr,
        "platform mesh-data base is a concrete storage-free named class");
    {
        spPlatformSpecificMeshData platformMesh;
        platformMesh.SetName("platform-mesh");
        auto cloneBase = platformMesh.Clone();
        auto* clone =
            dynamic_cast<spPlatformSpecificMeshData*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "platform-mesh") == 0,
            "platform mesh-data clone retains only the inherited shared name");
    }

    const auto& dxMeshRecord = spDXMeshData::StaticRTTI();
    Require(dxMeshRecord.base == &spPlatformSpecificMeshData::StaticRTTI()
            && dxMeshRecord.factory != nullptr,
        "DX mesh-data is a concrete platform-mesh-data leaf");
    {
        using Type = spIndexBuffer::eIndexBufferType;
        spIndexBuffer indices;
        Require(indices.InitializeForAnalysis(1, Type::Type2)
                && indices.SetIndexForAnalysis(0, 0)
                && indices.SetIndexForAnalysis(1, 1)
                && indices.SetIndexForAnalysis(2, 2),
            "DX mesh-data fixture creates one indexed triangle");
        spVertexBuffer vertices;
        Require(vertices.InitializeForAnalysis(0, 3),
            "DX mesh-data fixture creates a position-only vertex buffer");

        const std::array<spMesh::Position, 3> positions{{
            {0.0F, 0.0F, 0.0F},
            {1.0F, 0.0F, 0.0F},
            {0.0F, 1.0F, 0.0F},
        }};
        std::vector<std::byte> bytes(vertices.GetVertexSizeForAnalysis());
        std::memcpy(bytes.data(), positions.data(), bytes.size());
        Require(vertices.SetDataForAnalysis(bytes),
            "DX mesh-data fixture stores vertex payload");

        spMeshData meshData;
        Require(meshData.InitializeForAnalysis(indices, vertices),
            "DX mesh-data fixture initializes common mesh data");
        spDXMeshData dxMeshData;
        dxMeshData.SetName("dx-mesh-data");
        Require(dxMeshData.InitializeFromMeshDataForAnalysis(meshData)
                && dxMeshData.GetIndexBufferForAnalysis()
                    != meshData.GetIndexBufferForAnalysis()
                && dxMeshData.GetVertexBufferForAnalysis()
                    != meshData.GetVertexBufferForAnalysis(),
            "DX conversion owns independent copies of both common buffers");

        auto cloneBase = dxMeshData.Clone();
        auto* clone = dynamic_cast<spDXMeshData*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "dx-mesh-data") == 0
                && clone->GetIndexBufferForAnalysis() == nullptr
                && clone->GetVertexBufferForAnalysis() == nullptr,
            "DX mesh-data RTTI clone copies name but no buffer payload");
        dxMeshData.ReleaseBuffersForAnalysis();
        Require(dxMeshData.GetIndexBufferForAnalysis() == nullptr
                && dxMeshData.GetVertexBufferForAnalysis() == nullptr,
            "DX mesh-data release clears both owned buffers");
    }

    const auto& ps2MeshRecord = spPS2MeshData::StaticRTTI();
    Require(ps2MeshRecord.base == &spPlatformSpecificMeshData::StaticRTTI()
            && ps2MeshRecord.factory != nullptr,
        "PS2 mesh-data is a concrete platform-mesh-data leaf");
    {
        spPS2MeshData ps2MeshData;
        const auto& field48 = ps2MeshData.GetField48ForAnalysis();
        const auto& fieldA0 = ps2MeshData.GetFieldA0ForAnalysis();
        Require(ps2MeshData.GetField14ForAnalysis() == 4
                && ps2MeshData.GetFieldF8ForAnalysis() == 1
                && field48[0] == 0x0001006CU
                && field48[1] == 0xFFFFFFFFU
                && field48[7] == 0x0001006FU
                && field48[9] == 0x0000006EU
                && field48[12] == 0x00010064U
                && fieldA0[0] == 12
                && fieldA0[7] == 19
                && fieldA0[8] == 9
                && fieldA0[9] == 7
                && fieldA0[10] == 0
                && fieldA0[11] == -1
                && !ps2MeshData.HasPreparedPacketForAnalysis()
                && !ps2MeshData.HasExternalPacketForAnalysis(),
            "PS2 mesh-data preserves exact constructor tables and safe packet state");

        using Type = spIndexBuffer::eIndexBufferType;
        spIndexBuffer indices;
        spVertexBuffer vertices;
        Require(indices.InitializeForAnalysis(2, Type::Type2)
                && vertices.InitializeForAnalysis(0, 3),
            "PS2 mesh-data planning fixture initializes common buffers");
        const auto plan = spPS2MeshData::PlanPreparationForAnalysis(
            indices, vertices);
        Require(plan.accepted
                && !plan.alreadyPrepared
                && plan.requiresExpandedVertexBuffer
                && plan.sourceComponentFlags == 0
                && plan.effectiveComponentFlags == 0x000900U
                && plan.field14 == 3
                && plan.field18 == 3
                && plan.field20 == 3
                && plan.field38 == 0
                && plan.field3C == 0
                && plan.sourceIndexCount == 6,
            "PS2 mesh-data plan reproduces native component normalization");

        spIndexBuffer preparedIndices;
        Require(preparedIndices.InitializeForAnalysis(1, Type::Type4),
            "PS2 mesh-data accepts the native already-prepared index type");
        const auto preparedPlan =
            spPS2MeshData::PlanPreparationForAnalysis(
                preparedIndices, vertices);
        Require(preparedPlan.accepted && preparedPlan.alreadyPrepared,
            "PS2 mesh-data preserves the native type-4 early return");

        ps2MeshData.SetName("ps2-mesh-data");
        auto cloneBase = ps2MeshData.Clone();
        auto* clone = dynamic_cast<spPS2MeshData*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "ps2-mesh-data") == 0
                && !clone->HasPreparedPacketForAnalysis(),
            "PS2 mesh-data RTTI clone copies name but no runtime packet");
    }

    const auto& resourceRecord = spResource::StaticRTTI();
    Require(resourceRecord.base == &spNamedObject::StaticRTTI()
            && resourceRecord.factory != nullptr,
        "resource is a concrete storage-free spNamedObject class");
    {
        spResource resource;
        resource.SetName("shared-resource");
        auto cloneBase = resource.Clone();
        auto* clone = dynamic_cast<spResource*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "shared-resource") == 0,
            "resource clone retains only its inherited shared name");
    }

    const auto& resourceManagerRecord = spResourceManager::StaticRTTI();
    Require(resourceManagerRecord.base == &spBaseObject::StaticRTTI()
            && resourceManagerRecord.factory != nullptr
            && spResourceManager::ClassID
                == sparkplug::evidence::pc::spResourceManagerClassID
            && spResourceManager::ClassID
                == sparkplug::evidence::ps2::spResourceManagerClassID,
        "resource manager is a concrete direct spBaseObject descendant");
    {
        spResourceManager manager;
        Require(spResourceManager::GetInstance() == &manager
                && !manager.IsReserveEnabledForAnalysis()
                && manager.GetReserveCountForAnalysis() == 0
                && manager.GetField1CForAnalysis() == -1
                && manager.GetResourceCountForAnalysis() == 0,
            "resource-manager construction restores the native singleton and defaults");
        Require(manager.ConfigureReserveForAnalysis(true, 2500)
                && manager.IsReserveEnabledForAnalysis()
                && manager.GetReserveCountForAnalysis() == 2500,
            "resource-manager reserve configuration accepts the game startup count");

        spTextureData texture;
        texture.SetName("shared-graphic");
        spMeshData mesh;
        mesh.SetName("shared-graphic");
        spTextureData unnamedTexture;
        spResource unsupported;
        unsupported.SetName("shared-graphic");

        Require(spResourceManager::ClassifyForAnalysis(spTextureData::ClassID)
                    == spResourceManager::ResourceKindForAnalysis::Texture
                && spResourceManager::ClassifyForAnalysis(spMeshData::ClassID)
                    == spResourceManager::ResourceKindForAnalysis::Mesh
                && spResourceManager::ClassifyForAnalysis(spResource::ClassID)
                    == spResourceManager::ResourceKindForAnalysis::Unsupported,
            "resource-manager classification follows the texture/mesh RTTI branches");
        Require(manager.RegisterForAnalysis(texture)
                && manager.RegisterForAnalysis(mesh)
                && !manager.RegisterForAnalysis(unnamedTexture)
                && !manager.RegisterForAnalysis(unsupported)
                && manager.GetResourceCountForAnalysis() == 2,
            "resource manager caches only named texture and mesh descendants");
        Require(manager.FindForAnalysis(spTexture::ClassID, "shared-graphic")
                    == &texture
                && manager.FindForAnalysis(
                       spTextureData::ClassID, "shared-graphic") == &texture
                && manager.FindForAnalysis(spMesh::ClassID, "shared-graphic")
                    == &mesh
                && manager.FindForAnalysis(spResource::ClassID, "shared-graphic")
                    == nullptr
                && manager.FindForAnalysis(spTexture::ClassID, nullptr) == nullptr,
            "resource lookup uses graphic category plus exact name, not exact leaf RTTI");

        spResourceFATHelperForAnalysis cacheProbeFAT;
        spMemoryStream cacheProbeStream;
        const std::uint32_t cacheProbeCount = 3;
        const std::uint32_t zeroPayloadField = 0;
        Require(cacheProbeStream.Open(nullptr)
                && cacheProbeStream.Write(cacheProbeCount)
                && cacheProbeStream.Write(std::uint32_t{31})
                && cacheProbeStream.Write("shared-graphic")
                && cacheProbeStream.Write(spTextureData::ClassID)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Write(std::uint32_t{32})
                && cacheProbeStream.Write("shared-graphic")
                && cacheProbeStream.Write(spMeshData::ClassID)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Write(std::uint32_t{33})
                && cacheProbeStream.Write("uncached-node")
                && cacheProbeStream.Write(spNode::ClassID)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Write(zeroPayloadField)
                && cacheProbeStream.Seek(spStream::SeekSource::essStart, 0)
                && cacheProbeFAT.LoadIndexForAnalysis(cacheProbeStream),
            "cache-resolution fixture loads three native FAT records");
        auto* const deferredMesh = cacheProbeFAT.FindByIDForAnalysis(32);
        Require(deferredMesh != nullptr, "cache-resolution mesh entry exists");
        deferredMesh->fileID = 9;
        Require(cacheProbeFAT.ResolveCachedResourcesForAnalysis(manager) == 1
                && cacheProbeFAT.FindByIDForAnalysis(31)->object == &texture
                && deferredMesh->object == nullptr
                && cacheProbeFAT.FindByIDForAnalysis(33)->object == nullptr,
            "FAT resolves only an inline cached graphical entry on its first pass");
        deferredMesh->fileID = 0;
        Require(cacheProbeFAT.ResolveCachedResourcesForAnalysis(manager) == 1
                && deferredMesh->object == &mesh
                && cacheProbeFAT.ResolveCachedResourcesForAnalysis(manager) == 0,
            "FAT cache pass resolves deferred mesh once and skips materialized entries");

        Require(manager.RegisterForAnalysis(texture)
                && manager.GetResourceCountForAnalysis() == 3
                && manager.UnregisterForAnalysis(texture)
                && manager.GetResourceCountForAnalysis() == 2
                && manager.FindForAnalysis(spTexture::ClassID, "shared-graphic")
                    == &texture,
            "native-compatible cache permits duplicates and removes the first pointer");

        const auto countBeforeTemporary = manager.GetResourceCountForAnalysis();
        {
            auto temporary = std::make_unique<spTextureData>();
            temporary->SetName("temporary-texture");
            Require(manager.RegisterForAnalysis(*temporary)
                    && manager.GetResourceCountForAnalysis()
                        == countBeforeTemporary + 1,
                "temporary resource enters the non-owning cache");
        }
        Require(manager.GetResourceCountForAnalysis() == countBeforeTemporary
                && manager.FindForAnalysis(
                       spTextureData::ClassID, "temporary-texture") == nullptr,
            "resource destruction unregisters the cached pointer");
    }
    Require(spResourceManager::GetInstance() == nullptr,
        "resource-manager destruction clears its singleton");

    const auto& textureRecord = spTexture::StaticRTTI();
    Require(textureRecord.base == &spNamedObject::StaticRTTI()
            && textureRecord.baseClassID == spNamedObject::ClassID
            && textureRecord.factory == nullptr,
        "texture keeps its distinct registration and C++ inheritance graphs");
    {
        TextureProbe texture;
        Require(!texture.IsInitializedForAnalysis()
                && texture.GetField1CForAnalysis() == 0
                && texture.GetTextureFlagsForAnalysis() == 0
                && !texture.WereDimensionsUnchangedForAnalysis()
                && !texture.GetField31ForAnalysis()
                && texture.GetField34ForAnalysis() == 0
                && texture.Clone() == nullptr,
            "texture begins in safe native-equivalent control state and cannot clone");

        Require(spTexture::NormalizeDimensionForAnalysis(0) == 1
                && spTexture::NormalizeDimensionForAnalysis(1) == 2
                && spTexture::NormalizeDimensionForAnalysis(2) == 2
                && spTexture::NormalizeDimensionForAnalysis(3) == 4
                && spTexture::NormalizeDimensionForAnalysis(1024) == 1024
                && spTexture::NormalizeDimensionForAnalysis(0x80000000U)
                    == 0x80000000U
                && spTexture::NormalizeDimensionForAnalysis(0x80000001U) == 1,
            "texture dimension helper matches x86 and MIPS masked shifts");

        const auto changed = texture.ApplyBufferStateForAnalysis(
            300, 511, 1, 0, true);
        Require(changed.sourceWidth == 300
                && changed.sourceHeight == 511
                && changed.effectiveWidth == 512
                && changed.effectiveHeight == 512
                && !changed.dimensionsUnchanged
                && texture.IsInitializedForAnalysis()
                && texture.GetField18ForAnalysis() == 0
                && texture.GetField1CForAnalysis() == 1
                && texture.GetTextureFlagsForAnalysis() == 0
                && texture.GetWidthForAnalysis() == 512
                && texture.GetHeightForAnalysis() == 512
                && !texture.WereDimensionsUnchangedForAnalysis(),
            "texture buffer plan reproduces normalized Init state without hardware calls");

        const auto unchanged = texture.ApplyBufferStateForAnalysis(
            256, 128, 4, 0x20, true);
        Require(unchanged.dimensionsUnchanged
                && texture.GetField1CForAnalysis() == 4
                && texture.GetTextureFlagsForAnalysis() == 0x20
                && texture.WereDimensionsUnchangedForAnalysis(),
            "texture records exact-power-of-two dimensions as unchanged");

        const auto unnormalized = texture.ApplyBufferStateForAnalysis(
            17, 19, 2, 3, false);
        Require(unnormalized.effectiveWidth == 17
                && unnormalized.effectiveHeight == 19
                && texture.GetWidthForAnalysis() == 17
                && texture.GetHeightForAnalysis() == 19
                && texture.WereDimensionsUnchangedForAnalysis(),
            "non-normalizing Init preserves dimensions and the native sticky flag");
    }

    const auto& textureBufferRecord = spTextureBuffer::StaticRTTI();
    Require(textureBufferRecord.base == &spBaseObject::StaticRTTI()
            && textureBufferRecord.factory != nullptr,
        "texture buffer is a concrete direct spBaseObject class");
    {
        spTextureBuffer buffer;
        Require(!buffer.IsInitializedForAnalysis()
                && buffer.GetWidthForAnalysis() == 0
                && buffer.GetHeightForAnalysis() == 0
                && buffer.GetDepthForAnalysis() == 0
                && buffer.GetPixelFormatForAnalysis() == 6
                && buffer.GetPixelSizeForAnalysis() == 0
                && buffer.GetBufferForAnalysis().empty()
                && !buffer.HasAuxiliaryObjectForAnalysis(),
            "texture buffer constructor matches the zero/default-format state");

        Require(spTextureBuffer::PixelSizeForFormatForAnalysis(0) == 4
                && spTextureBuffer::PixelSizeForFormatForAnalysis(1) == 4
                && spTextureBuffer::PixelSizeForFormatForAnalysis(2) == 1
                && spTextureBuffer::PixelSizeForFormatForAnalysis(3) == 2
                && spTextureBuffer::PixelSizeForFormatForAnalysis(4) == 2
                && spTextureBuffer::PixelSizeForFormatForAnalysis(5) == 0,
            "texture buffer reproduces the native pixel-format jump table");

        Require(buffer.InitializeForAnalysis(2, 3, 1, 0)
                && buffer.GetWidthForAnalysis() == 2
                && buffer.GetHeightForAnalysis() == 3
                && buffer.GetDepthForAnalysis() == 1
                && buffer.GetPixelFormatForAnalysis() == 0
                && buffer.GetPixelSizeForAnalysis() == 4
                && buffer.GetBufferForAnalysis().size() == 24,
            "texture buffer Init calculates and allocates the exact byte count");
        std::vector<std::byte> pixels(24, std::byte{0x5A});
        Require(buffer.SetDataForAnalysis(pixels)
                && buffer.GetBufferForAnalysis() == pixels
                && !buffer.SetDataForAnalysis(
                    std::vector<std::byte>(23, std::byte{0})),
            "texture buffer accepts only an exact-sized pixel payload");

        auto cloneBase = buffer.Clone();
        auto* clone = dynamic_cast<spTextureBuffer*>(cloneBase.get());
        Require(clone != nullptr
                && !clone->IsInitializedForAnalysis()
                && clone->GetPixelFormatForAnalysis() == 6
                && clone->GetBufferForAnalysis().empty(),
            "texture-buffer RTTI clone deliberately leaves pixel state blank");

        Require(!buffer.InitializeForAnalysis(7, 9, 1, 5)
                && !buffer.IsInitializedForAnalysis()
                && buffer.GetBufferForAnalysis().empty()
                && buffer.GetWidthForAnalysis() == 7
                && buffer.GetHeightForAnalysis() == 9
                && buffer.GetPixelFormatForAnalysis() == 5
                && buffer.GetPixelSizeForAnalysis() == 4,
            "invalid texture format releases storage but preserves native sticky scalars");
        Require(!buffer.InitializeForAnalysis(65535, 65535, 2, 0)
                && !buffer.IsInitializedForAnalysis()
                && buffer.GetBufferForAnalysis().empty(),
            "portable texture buffer rejects native 32-bit allocation overflow safely");
    }

    const auto& textureDataRecord = spTextureData::StaticRTTI();
    Require(textureDataRecord.base == &spTexture::StaticRTTI()
            && textureDataRecord.factory != nullptr,
        "texture data is a concrete direct spTexture class");
    {
        spTextureBuffer source;
        Require(source.InitializeForAnalysis(3, 5, 1, 0),
            "texture-data source buffer initializes");
        std::vector<std::byte> pixels(60, std::byte{0x2C});
        Require(source.SetDataForAnalysis(pixels),
            "texture-data source payload initializes");

        spTextureData textureData;
        textureData.SetName("texture-data");
        Require(!textureData.GetField68ForAnalysis()
                && !textureData.GetFieldAfterFirstContainerForAnalysis()
                && textureData.InitializeFromTextureBufferForAnalysis(
                    source, 1, 0, true)
                && textureData.IsInitializedForAnalysis()
                && textureData.GetWidthForAnalysis() == 4
                && textureData.GetHeightForAnalysis() == 8
                && textureData.GetTextureBufferForAnalysis()
                    .GetWidthForAnalysis() == 3
                && textureData.GetTextureBufferForAnalysis()
                    .GetHeightForAnalysis() == 5
                && textureData.GetTextureBufferForAnalysis()
                    .GetBufferForAnalysis() == pixels,
            "texture data copies CPU pixels while retaining normalized logical dimensions");

        textureData.SetField68ForAnalysis(true);
        textureData.SetFieldAfterFirstContainerForAnalysis(true);
        auto cloneBase = textureData.Clone();
        auto* clone = dynamic_cast<spTextureData*>(cloneBase.get());
        Require(clone != nullptr
                && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "texture-data") == 0
                && !clone->IsInitializedForAnalysis()
                && !clone->GetTextureBufferForAnalysis()
                    .IsInitializedForAnalysis()
                && !clone->GetField68ForAnalysis()
                && !clone->GetFieldAfterFirstContainerForAnalysis(),
            "texture-data clone keeps only the inherited name and blank runtime state");
    }

    const auto& textureDataSerializerRecord =
        spTextureDataSerializer::StaticRTTI();
    Require(textureDataSerializerRecord.base == &spSerializer::StaticRTTI()
            && textureDataSerializerRecord.factory != nullptr,
        "texture-data serializer is a concrete direct spSerializer class");
    {
        using Field = spTextureDataSerializer::Field;
        using Source = spTextureDataSerializer::DataSourceKind;
        spTextureDataSerializer serializer;

        Require(serializer.GetTargetClassIDForAnalysis() == spTextureData::ClassID,
            "texture-data serializer targets spTextureData");
        Require(serializer.BuildKnownWritePlanForAnalysis(
                    Source::EmbeddedMemoryStream, 0)
                == std::vector<Field>{Field::SourceEmbeded}
                && serializer.BuildKnownWritePlanForAnalysis(
                    Source::ReferencedStream, 0)
                == std::vector<Field>{Field::SourceReference},
            "texture-data serializer source streams terminate the local payload");
        Require(serializer.BuildKnownWritePlanForAnalysis(Source::None, 0)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 2)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 1)
                == std::vector<Field>{Field::SourceNone},
            "texture-data serializer reproduces the mode-dependent base write plan");

        spTextureBuffer source;
        Require(source.InitializeForAnalysis(3, 5, 1, 0)
                && source.SetDataForAnalysis(
                    std::vector<std::byte>(60, std::byte{0x4D})),
            "texture-data serializer fixture initializes cross-platform pixels");
        spTextureData textureData;
        Require(textureData.InitializeFromTextureBufferForAnalysis(
                    source, 0, 0, false),
            "texture-data serializer fixture copies the texture buffer");
        const auto header = spTextureDataSerializer::
            BuildCrossPlatformPayloadHeaderForAnalysis(textureData);
        Require(header.valid
                && header.width == 3
                && header.height == 5
                && header.pixelFormat == 0
                && header.pixelSize == 4
                && header.payloadSize == 60,
            "texture-data serializer describes the exact native field-5 payload");

        spTextureData blank;
        Require(!spTextureDataSerializer::
                    BuildCrossPlatformPayloadHeaderForAnalysis(blank).valid,
            "texture-data serializer rejects an uninitialized cross-platform buffer");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spTextureDataSerializer*>(cloneBase.get()) != nullptr,
            "texture-data serializer RTTI clone preserves its concrete type");
    }

    const auto& dxTextureDataSerializerRecord =
        spDXTextureDataSerializer::StaticRTTI();
    Require(dxTextureDataSerializerRecord.base
                == &spTextureDataSerializer::StaticRTTI()
            && dxTextureDataSerializerRecord.factory != nullptr,
        "DX texture-data serializer directly extends the shared serializer");
    {
        using Field = spTextureDataSerializer::Field;
        using Source = spTextureDataSerializer::DataSourceKind;
        spDXTextureDataSerializer serializer;

        Require(serializer.GetTargetClassIDForAnalysis()
                    == spDXTextureDataSerializer::TargetClassID,
            "DX texture-data serializer exposes its exact target identity");
        Require(serializer.BuildKnownWritePlanForAnalysis(
                    Source::EmbeddedMemoryStream, 0)
                == std::vector<Field>{Field::SourceEmbeded}
                && serializer.BuildKnownWritePlanForAnalysis(
                    Source::ReferencedStream, 0)
                == std::vector<Field>{Field::SourceReference},
            "DX texture-data serializer preserves terminal external sources");
        Require(serializer.BuildKnownWritePlanForAnalysis(Source::None, 0)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform, Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 2)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform, Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 1)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::PlatformSpecific},
            "DX texture-data serializer reproduces its exact local field order");
        Require(spDXTextureDataSerializer::PlatformTypeForAnalysis(0) == 7
                && spDXTextureDataSerializer::PlatformTypeForAnalysis(2) == 7
                && spDXTextureDataSerializer::PlatformTypeForAnalysis(1) == 6,
            "DX texture-data serializer writes the confirmed 6/7 platform code");
        Require(!spDXTextureDataSerializer::PCLoadsNativePayloadForAnalysis(0)
                && spDXTextureDataSerializer::PCLoadsNativePayloadForAnalysis(2)
                && !spDXTextureDataSerializer::PS2LoadsNativePayloadForAnalysis(2)
                && spDXTextureDataSerializer::PS2LoadsNativePayloadForAnalysis(8),
            "DX texture-data serializer keeps PC and PS2 load masks separate");

        const auto nativeHeader =
            spDXTextureDataSerializer::DescribeNativePayloadForAnalysis(
                true, 128, 64, 5, true, 4);
        Require(nativeHeader.valid
                && nativeHeader.hasPlatformSpecificData
                && nativeHeader.width == 128
                && nativeHeader.height == 64
                && nativeHeader.pixelFormat == 5
                && nativeHeader.hasPixelData
                && nativeHeader.mipCount == 4,
            "DX texture-data serializer describes the confirmed native prefix");
        Require(!spDXTextureDataSerializer::DescribeNativePayloadForAnalysis(
                    true, 128, 64, 5, true, 0).valid,
            "DX texture-data serializer rejects the native writer's zero-mip case");

        const auto mipHeader =
            spDXTextureDataSerializer::BuildNativeMipHeaderForAnalysis(
                16, 8, 64);
        Require(mipHeader.valid
                && mipHeader.width == 16
                && mipHeader.height == 8
                && mipHeader.rowStride == 64
                && mipHeader.payloadSize == 512,
            "DX texture-data serializer preserves width/stride/height wire sizing");
        Require(!spDXTextureDataSerializer::BuildNativeMipHeaderForAnalysis(
                    1, 0xFFFFFFFFU, 2).valid,
            "DX texture-data serializer rejects overflowing mip payload sizes");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spDXTextureDataSerializer*>(cloneBase.get())
                    != nullptr,
            "DX texture-data serializer RTTI clone preserves its concrete type");
    }

    const auto& ps2TextureDataSerializerRecord =
        spPS2TextureDataSerializer::StaticRTTI();
    Require(ps2TextureDataSerializerRecord.base
                == &spTextureDataSerializer::StaticRTTI()
            && ps2TextureDataSerializerRecord.factory != nullptr,
        "PS2 texture-data serializer directly extends the shared serializer");
    {
        using Field = spTextureDataSerializer::Field;
        using Source = spTextureDataSerializer::DataSourceKind;
        spPS2TextureDataSerializer serializer;

        Require(serializer.GetTargetClassIDForAnalysis()
                    == spPS2TextureDataSerializer::TargetClassID,
            "PS2 texture-data serializer exposes its exact target identity");
        Require(serializer.BuildKnownWritePlanForAnalysis(
                    Source::EmbeddedMemoryStream, 0)
                == std::vector<Field>{Field::SourceEmbeded}
                && serializer.BuildKnownWritePlanForAnalysis(
                    Source::ReferencedStream, 0)
                == std::vector<Field>{Field::SourceReference},
            "PS2 texture-data serializer preserves terminal external sources");
        Require(serializer.BuildKnownWritePlanForAnalysis(Source::None, 0)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform, Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 2)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::CrossPlatform, Field::PlatformSpecific}
                && serializer.BuildKnownWritePlanForAnalysis(Source::None, 1)
                == std::vector<Field>{Field::SourceNone, Field::PlatformType,
                    Field::PlatformSpecific},
            "PS2 texture-data serializer reproduces its exact local field order");
        Require(spPS2TextureDataSerializer::PlatformTypeForAnalysis(0) == 9
                && spPS2TextureDataSerializer::PlatformTypeForAnalysis(2) == 9
                && spPS2TextureDataSerializer::PlatformTypeForAnalysis(1) == 8,
            "PS2 texture-data serializer writes the confirmed 8/9 platform code");
        Require(!spPS2TextureDataSerializer::PCLoadsNativePayloadForAnalysis(0)
                && spPS2TextureDataSerializer::PCLoadsNativePayloadForAnalysis(2)
                && !spPS2TextureDataSerializer::PS2LoadsNativePayloadForAnalysis(2)
                && spPS2TextureDataSerializer::PS2LoadsNativePayloadForAnalysis(8),
            "PS2 texture-data serializer keeps PC and PS2 load masks separate");
        Require(spPS2TextureDataSerializer::PaletteByteCountForAnalysis(0)
                    == 0x40
                && spPS2TextureDataSerializer::PaletteByteCountForAnalysis(1)
                    == 0x400
                && spPS2TextureDataSerializer::PaletteByteCountForAnalysis(3)
                    == 0,
            "PS2 texture-data serializer preserves native palette sizes");

        const auto nativeHeader =
            spPS2TextureDataSerializer::DescribeNativePayloadForAnalysis(
                true, 1, 128, 64, 7, 5);
        Require(nativeHeader.hasPlatformSpecificData
                && nativeHeader.pixelFormat == 1
                && nativeHeader.width == 128
                && nativeHeader.height == 64
                && nativeHeader.auxiliaryValue == 7
                && nativeHeader.mipCount == 5
                && nativeHeader.paletteByteCount == 0x400,
            "PS2 texture-data serializer describes the confirmed native prefix");

        auto cloneBase = serializer.Clone();
        Require(dynamic_cast<spPS2TextureDataSerializer*>(cloneBase.get())
                    != nullptr,
            "PS2 texture-data serializer RTTI clone preserves its concrete type");
    }

    const auto& meshRecord = spMesh::StaticRTTI();
    Require(meshRecord.base == &spResource::StaticRTTI()
            && meshRecord.factory == nullptr,
        "common mesh is a non-factory spResource class");
    {
        using Type = spIndexBuffer::eIndexBufferType;
        spIndexBuffer indices;
        Require(indices.InitializeForAnalysis(1, Type::Type2)
                && indices.SetIndexForAnalysis(0, 2)
                && indices.SetIndexForAnalysis(1, 0)
                && indices.SetIndexForAnalysis(2, 1),
            "mesh bounds fixture creates one indexed primitive");
        const std::vector<spMesh::Position> positions{
            {-4.0F, 7.0F, 2.0F},
            {3.0F, -5.0F, 8.0F},
            {1.0F, 4.0F, -6.0F},
        };
        spMesh mesh;
        Require(mesh.ComputeBoundsForAnalysis(indices, positions)
                && mesh.HasBoundsForAnalysis()
                && mesh.GetMinimumForAnalysis()
                    == spMesh::Position{-4.0F, -5.0F, -6.0F}
                && mesh.GetMaximumForAnalysis()
                    == spMesh::Position{3.0F, 7.0F, 8.0F},
            "mesh computes the native min/max envelope over indexed positions");
        Require(mesh.Clone() == nullptr,
            "common mesh preserves its native null-clone contract");
        mesh.InvalidateBoundsForAnalysis();
        Require(!mesh.HasBoundsForAnalysis(),
            "mesh bounds validity is independently invalidated");
    }

    const auto& renderableRecord = spRenderable::StaticRTTI();
    Require(renderableRecord.base == &spNamedObject::StaticRTTI()
            && renderableRecord.factory == nullptr,
        "renderable is the non-factory named base of model");
    const auto& modelRecord = spModel::StaticRTTI();
    Require(modelRecord.base == &renderableRecord
            && modelRecord.factory != nullptr,
        "model is a concrete spRenderable class");
    {
        auto material = std::make_shared<spResource>();
        auto fog = std::make_shared<spResource>();
        auto meshData = std::make_shared<spMeshData>();
        spModel model;
        Require(model.GetProjectionGroupForAnalysis()
                == spModel::NativeDefaultProjectionGroup,
            "model preserves the native PS2 constructor default group 3");
        model.SetName("model-probe");
        model.SetMaterialForAnalysis(material);
        model.SetFogForAnalysis(fog);
        model.SetAlphaSortEnabledForAnalysis(false);
        model.SetPriorityForAnalysis(30);
        model.SetBaseMeshForAnalysis(meshData);
        model.SetProjectionGroupForAnalysis(0);

        auto cloneBase = model.Clone();
        auto* clone = dynamic_cast<spModel*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() != nullptr
                && std::strcmp(clone->GetName(), "model-probe") == 0
                && clone->GetMaterialForAnalysis() == material
                && clone->GetFogForAnalysis() == fog
                && clone->GetBaseMeshForAnalysis() == meshData
                && !clone->IsAlphaSortEnabledForAnalysis()
                && clone->GetPriorityForAnalysis() == 30
                && clone->GetProjectionGroupForAnalysis() == 0,
            "model clone copies inherited render state and both model fields");
    }

    const auto* inputRecord =
        spRTTIManager::Instance().Find(spInputManager::ClassID);
    Require(inputRecord != nullptr
            && inputRecord->base == &spCrossPlatform::StaticRTTI()
            && inputRecord->factory == nullptr,
        "common input manager preserves its native base and null factory");
    const auto& ps2InputRecord = spPS2InputManager::StaticRTTI();
    Require(ps2InputRecord.base == inputRecord
            && ps2InputRecord.factory != nullptr,
        "PS2 input-manager leaf is concrete");
    {
        spPS2InputManager manager;
        Require(spInputManager::GetInstance() == &manager
                && manager.GetMaximumControllerCountForAnalysis() == 2,
            "PS2 input manager publishes itself and exposes two pad slots");
        Require(manager.InitializeForAnalysis()
                && manager.IsInitializedForAnalysis()
                && manager.GetConnectedControllerCountForAnalysis() == 0,
            "PS2 input initialization starts with no connected analytical pads");
        Require(manager.SetControllerConnectedForAnalysis(0, true)
                && manager.SetControllerConnectedForAnalysis(1, true)
                && !manager.SetControllerConnectedForAnalysis(2, true)
                && manager.GetConnectedControllerCountForAnalysis() == 2,
            "PS2 controller range is bounded to its two native slots");
        manager.ShutdownForAnalysis();
        Require(!manager.IsInitializedForAnalysis()
                && manager.GetConnectedControllerCountForAnalysis() == 0,
            "PS2 shutdown clears runtime input state");
        auto cloneBase = manager.Clone();
        auto* clone = dynamic_cast<spPS2InputManager*>(cloneBase.get());
        Require(clone != nullptr && !clone->IsInitializedForAnalysis(),
            "PS2 input clone starts with empty device state");
    }
    Require(spInputManager::GetInstance() == nullptr,
        "input-manager destruction clears the singleton");

#if defined(_WIN32)
    const auto& dxInputRecord = spDXInputManager::StaticRTTI();
    Require(dxInputRecord.base == inputRecord && dxInputRecord.factory != nullptr,
        "DirectInput leaf is concrete");
    {
        spDXInputManager manager;
        Require(manager.GetMaximumControllerCountForAnalysis() == 4
                && manager.InitializeForAnalysis(),
            "DirectInput manager exposes four native controller slots");
        Require(manager.SetControllerConnectedForAnalysis(3, true)
                && !manager.SetControllerConnectedForAnalysis(4, true)
                && manager.GetConnectedControllerCountForAnalysis() == 1,
            "DirectInput controller range is safely bounded");
    }
#endif

    const auto* fontRecord =
        spRTTIManager::Instance().Find(spFontManager::ClassID);
    Require(fontRecord != nullptr
            && fontRecord->base == &spCrossPlatform::StaticRTTI(),
        "spFontManager registration preserves its native direct base");
    Require(fontRecord->factory == nullptr,
        "common spFontManager registration has no native factory");

    const auto& ps2FontRecord = spPS2FontManager::StaticRTTI();
    Require(ps2FontRecord.base == fontRecord && ps2FontRecord.factory != nullptr,
        "PS2 font-manager leaf is concrete and derives from the common manager");
    {
        spPS2FontManager manager;
        Require(spFontManager::GetInstance() == &manager,
            "font-manager construction publishes the singleton");

        auto first = std::make_shared<spNamedObject>("Default");
        auto second = std::make_shared<spNamedObject>("Dialog");
        Require(manager.AddFontForAnalysis(first)
                && manager.AddFontForAnalysis(second)
                && manager.AddFontForAnalysis(first),
            "font insertion retains native append semantics including duplicates");
        Require(manager.GetFontCountForAnalysis() == 3
                && manager.FindFontForAnalysis("Default") == first.get()
                && manager.FindFontForAnalysis("Missing") == nullptr,
            "font lookup returns the first exact named entry");
        auto firstMaterial=std::make_shared<spMaterialData>();
        auto secondMaterial=std::make_shared<spMaterialData>();
        manager.SetPrimaryMaterialForAnalysis(firstMaterial);
        manager.SetFallbackMaterialForAnalysis(secondMaterial);
        Require(manager.GetPrimaryMaterialForAnalysis() == firstMaterial.get()
                && manager.GetFallbackMaterialForAnalysis() == secondMaterial.get(),
            "both native material reference roles remain independent");
        Require(!manager.InitializeForAnalysis()
                && !manager.IsInitializedForAnalysis(),
            "Unprovided PS2 material factory does not report synthetic startup success");

        auto cloneBase = manager.Clone();
        auto* clone = dynamic_cast<spPS2FontManager*>(cloneBase.get());
        Require(clone != nullptr && clone->GetFontCountForAnalysis() == 0
                && !clone->IsInitializedForAnalysis(),
            "platform font-manager clone starts with empty runtime state");
    }
    Require(spFontManager::GetInstance() == nullptr,
        "font-manager destruction clears the singleton");

#if defined(_WIN32)
    const auto& pcFontRecord = spPCFontManager::StaticRTTI();
    Require(pcFontRecord.base == fontRecord && pcFontRecord.factory != nullptr,
        "PC font-manager leaf is concrete and derives from the common manager");
    {
        spPCFontManager manager;
        Require(!manager.IsPlatformBufferReadyForAnalysis()
                && !manager.InitializeForAnalysis()
                && !manager.IsPlatformBufferReadyForAnalysis()
                && manager.GetFallbackMaterialForAnalysis()!=nullptr,
            "PC material prefix completes but unresolved system Font production remains explicit");
    }
#endif

    const auto* record = spRTTIManager::Instance().Find(spEngineCore::ClassID);
    Require(record != nullptr, "spEngineCore registration is installed");
    Require(record->base == &spBaseObject::StaticRTTI(),
        "spEngineCore derives directly from spBaseObject");
    Require(record->factory != nullptr, "spEngineCore has a native factory");
    Require(record->propertyRegistrar == nullptr,
        "spEngineCore has no native property callback");

    spEngineCore core;
    Require(spEngineCore::GetInstance() == &core,
        "constructor publishes the spEngineCore singleton");
    Require(!core.IsInitializedForAnalysis(),
        "constructor clears the initialized byte");

    Probe first;
    Probe second;
    Probe third;
    Probe fourth;
    spEngineCore::AnalysisStages stages{{
        {&RunProbe, &first},
        {&RunProbe, &second},
        {&RunProbe, &third},
        {&RunProbe, &fourth},
    }};
    third.result = false;
    Require(!core.InitializeForAnalysis(stages),
        "initialization fails when a stage fails");
    Require(first.calls == 1 && second.calls == 1 && third.calls == 1
            && fourth.calls == 0,
        "initialization short-circuits in native stage order");
    Require(!core.IsInitializedForAnalysis(),
        "failed initialization does not set the native state byte");

    third.result = true;
    Require(core.InitializeForAnalysis(stages),
        "all successful stages complete initialization");
    Require(core.IsInitializedForAnalysis(),
        "successful initialization sets the native state byte");

    Probe frameFirst;
    Probe frameSecond;
    core.SetFrameCallbacksForAnalysis(
        &RunProbe, &frameFirst, &RunProbe, &frameSecond);
    Require(core.InvokeFirstFrameCallbackForAnalysis()
            && core.InvokeSecondFrameCallbackForAnalysis(),
        "both nullable native callback roles can be dispatched safely");
    Require(frameFirst.calls == 1 && frameSecond.calls == 1,
        "frame callbacks are invoked exactly once");

    core.ShutdownForAnalysis();
    Require(!core.IsInitializedForAnalysis(),
        "shutdown clears the native initialized byte");

    auto created = spRTTIManager::Instance().Create(spEngineCore::ClassID);
    Require(dynamic_cast<spEngineCore*>(created.get()) != nullptr,
        "RTTI factory creates spEngineCore");
    created.reset();

    Require(spEngineCore::ClassID == sparkplug::evidence::pc::spEngineCoreClassID
            && spEngineCore::ClassID
                == sparkplug::evidence::ps2::spEngineCoreClassID,
        "portable and native class IDs agree");
    Require(sizeof(sparkplug::evidence::pc::spEngineCoreLayout) == 0x158,
        "PC allocation size is preserved in evidence");
    Require(sizeof(sparkplug::evidence::pc::spDebugManagerLayout) == 0x38
            && sizeof(sparkplug::evidence::ps2::spDebugManagerLayout) == 0x38
            && spDebugManager::ClassID
                == sparkplug::evidence::pc::spDebugManagerClassID
            && spDebugManager::ClassID
                == sparkplug::evidence::ps2::spDebugManagerClassID,
        "debug-manager size and class identity agree across both binaries");
    Require(sizeof(sparkplug::evidence::pc::spEntityManagerObservedPrefixLayout)
                == 0x20
            && sizeof(sparkplug::evidence::ps2::spEntityManagerLayout) == 0x20
            && spEntityManager::ClassID
                == sparkplug::evidence::pc::spEntityManagerClassID
            && spEntityManager::ClassID
                == sparkplug::evidence::ps2::spEntityManagerClassID,
        "entity-manager observed PC prefix and exact PS2 allocation agree");
    Require(sizeof(sparkplug::evidence::pc::spTemplateManagerObservedPrefixLayout)
                == 0x24
            && sizeof(sparkplug::evidence::ps2::spTemplateManagerLayout) == 0x24
            && spTemplateManager::ClassID
                == sparkplug::evidence::pc::spTemplateManagerClassID
            && spTemplateManager::ClassID
                == sparkplug::evidence::ps2::spTemplateManagerClassID,
        "template-manager observed PC prefix and exact PS2 allocation agree");
    Require(sizeof(sparkplug::evidence::pc::spTemplateInstanceLayout) == 0x28
            && sizeof(sparkplug::evidence::ps2::spTemplateInstanceLayout) == 0x28
            && spTemplateInstance::ClassID
                == sparkplug::evidence::pc::spTemplateInstanceClassID
            && spTemplateInstance::ClassID
                == sparkplug::evidence::ps2::spTemplateInstanceClassID,
        "template-instance exact layouts and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spNodeLayout) == 0xB4
            && sizeof(sparkplug::evidence::ps2::spNodeLayout) == 0xC0
            && offsetof(sparkplug::evidence::pc::spNodeLayout, flags) == 0xB0
            && offsetof(sparkplug::evidence::ps2::spNodeLayout, flags) == 0xB4
            && spNode::ClassID == sparkplug::evidence::pc::spNodeClassID
            && spNode::ClassID == sparkplug::evidence::ps2::spNodeClassID
            && spNode::NativeDefaultFlags
                == sparkplug::evidence::pc::spNodeDefaultFlags
            && spNode::NativeDefaultFlags
                == sparkplug::evidence::ps2::spNodeDefaultFlags,
        "node platform layouts, flags and IDs match both native builds");
    Require(sizeof(sparkplug::evidence::pc::spLightObservedLayout) == 0xF0
            && sizeof(sparkplug::evidence::ps2::spLightLayout) == 0x100
            && offsetof(sparkplug::evidence::pc::spLightObservedLayout,
                    intensity) == 0xD8
            && offsetof(sparkplug::evidence::ps2::spLightLayout,
                    intensity) == 0xE4
            && offsetof(sparkplug::evidence::pc::spLightObservedLayout,
                    projectShadow) == 0xEC
            && offsetof(sparkplug::evidence::ps2::spLightLayout,
                    projectShadow) == 0xF8
            && spLight::ClassID == sparkplug::evidence::pc::spLightClassID
            && spLight::ClassID == sparkplug::evidence::ps2::spLightClassID
            && spLightData::ClassID
                == sparkplug::evidence::pc::spLightDataClassID
            && spLightData::ClassID
                == sparkplug::evidence::ps2::spLightDataClassID,
        "light platform layouts, serializer offsets and IDs match both native builds");
    Require(sizeof(sparkplug::evidence::pc::spSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::spSerializerLayout) == 0x14
            && offsetof(sparkplug::evidence::pc::spSerializerObservedLayout,
                    serializerVTable) == 0x10
            && offsetof(sparkplug::evidence::ps2::spSerializerLayout,
                    serializerVTable) == 0x10
            && spSerializer::ClassID
                == sparkplug::evidence::pc::spSerializerClassID
            && spSerializer::ClassID
                == sparkplug::evidence::ps2::spSerializerClassID,
        "serializer dual-vptr layout and identity agree across native builds");
    Require(sizeof(sparkplug::evidence::pc::spNodeSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::spNodeSerializerLayout) == 0x14
            && spNodeSerializer::ClassID
                == sparkplug::evidence::pc::spNodeSerializerClassID
            && spNodeSerializer::ClassID
                == sparkplug::evidence::ps2::spNodeSerializerClassID
            && spNodeSerializer::TargetClassID
                == sparkplug::evidence::pc::spNodeClassID
            && spNodeSerializer::TargetClassID
                == sparkplug::evidence::ps2::spNodeClassID,
        "node-serializer layout, serializer identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::spLightDataSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spLightDataSerializerLayout)
                == 0x14
            && spLightDataSerializer::ClassID
                == sparkplug::evidence::pc::spLightDataSerializerClassID
            && spLightDataSerializer::ClassID
                == sparkplug::evidence::ps2::spLightDataSerializerClassID
            && spLightDataSerializer::TargetClassID
                == sparkplug::evidence::pc::spLightDataClassID
            && spLightDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::spLightDataClassID,
        "light-data serializer layout, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::spLightSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spLightSerializerLayout)
                == 0x14
            && spLightSerializer::ClassID
                == sparkplug::evidence::pc::spLightSerializerClassID
            && spLightSerializer::ClassID
                == sparkplug::evidence::ps2::spLightSerializerClassID
            && spLightSerializer::TargetClassID
                == sparkplug::evidence::pc::spLightClassID
            && spLightSerializer::TargetClassID
                == sparkplug::evidence::ps2::spLightClassID,
        "light serializer layout, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::spCameraSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spCameraSerializerLayout)
                == 0x14
            && spCameraSerializer::ClassID
                == sparkplug::evidence::pc::spCameraSerializerClassID
            && spCameraSerializer::ClassID
                == sparkplug::evidence::ps2::spCameraSerializerClassID
            && spCameraSerializer::TargetClassID
                == sparkplug::evidence::pc::spCameraSerializerTargetClassIDValue
            && spCameraSerializer::TargetClassID
                == sparkplug::evidence::ps2::spCameraSerializerTargetClassIDValue,
        "camera serializer layout, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spCameraDataSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spCameraDataSerializerLayout) == 0x14
            && spCameraDataSerializer::ClassID
                == sparkplug::evidence::pc::spCameraDataSerializerClassID
            && spCameraDataSerializer::ClassID
                == sparkplug::evidence::ps2::spCameraDataSerializerClassID
            && spCameraDataSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spCameraDataSerializerTargetClassIDValue
            && spCameraDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spCameraDataSerializerTargetClassIDValue,
        "camera-data serializer extent, identity and actual target agree");
    Require(sizeof(sparkplug::evidence::pc::spFogSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spFogSerializerLayout)
                == 0x14
            && spFogSerializer::ClassID
                == sparkplug::evidence::pc::spFogSerializerClassID
            && spFogSerializer::ClassID
                == sparkplug::evidence::ps2::spFogSerializerClassID
            && spFogSerializer::TargetClassID
                == sparkplug::evidence::pc::spFogSerializerTargetClassIDValue
            && spFogSerializer::TargetClassID
                == sparkplug::evidence::ps2::spFogSerializerTargetClassIDValue,
        "fog serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spMatColorControllerSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spMatColorControllerSerializerLayout) == 0x14
            && spMatColorControllerSerializer::ClassID
                == sparkplug::evidence::pc::
                    spMatColorControllerSerializerClassID
            && spMatColorControllerSerializer::ClassID
                == sparkplug::evidence::ps2::
                    spMatColorControllerSerializerClassID
            && spMatColorControllerSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spMatColorControllerSerializerTargetClassIDValue
            && spMatColorControllerSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spMatColorControllerSerializerTargetClassIDValue,
        "material-color-controller serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spLightControllerSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spLightControllerSerializerLayout) == 0x14
            && spLightControllerSerializer::ClassID
                == sparkplug::evidence::pc::
                    spLightControllerSerializerClassID
            && spLightControllerSerializer::ClassID
                == sparkplug::evidence::ps2::
                    spLightControllerSerializerClassID
            && spLightControllerSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spLightControllerSerializerTargetClassIDValue
            && spLightControllerSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spLightControllerSerializerTargetClassIDValue
            && spLightControllerSerializer::LightClassID
                == sparkplug::evidence::pc::
                    spLightControllerSerializerLightClassID
            && spLightControllerSerializer::LightClassID
                == sparkplug::evidence::ps2::
                    spLightControllerSerializerLightClassID,
        "light-controller serializer extent, identity and relationship target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spAnimTexControllerSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spAnimTexControllerSerializerLayout) == 0x14
            && spAnimTexControllerSerializer::ClassID
                == sparkplug::evidence::pc::
                    spAnimTexControllerSerializerClassID
            && spAnimTexControllerSerializer::ClassID
                == sparkplug::evidence::ps2::
                    spAnimTexControllerSerializerClassID
            && spAnimTexControllerSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spAnimTexControllerSerializerTargetClassIDValue
            && spAnimTexControllerSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spAnimTexControllerSerializerTargetClassIDValue
            && spAnimTexControllerSerializer::TextureRelationshipClassID
                == sparkplug::evidence::pc::
                    spAnimTexControllerSerializerTextureClassID
            && spAnimTexControllerSerializer::TextureRelationshipClassID
                == sparkplug::evidence::ps2::
                    spAnimTexControllerSerializerTextureClassID,
        "animated-texture serializer extent, identity and relationship base agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spUVControllerSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spUVControllerSerializerLayout) == 0x14
            && spUVControllerSerializer::ClassID
                == sparkplug::evidence::pc::spUVControllerSerializerClassID
            && spUVControllerSerializer::ClassID
                == sparkplug::evidence::ps2::spUVControllerSerializerClassID
            && spUVControllerSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spUVControllerSerializerTargetClassIDValue
            && spUVControllerSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spUVControllerSerializerTargetClassIDValue,
        "UV-controller serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spTransFunctionEvalSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spTransFunctionEvalSerializerLayout) == 0x14
            && spTransFunctionEvalSerializer::ClassID
                == sparkplug::evidence::pc::
                    spTransFunctionEvalSerializerClassID
            && spTransFunctionEvalSerializer::ClassID
                == sparkplug::evidence::ps2::
                    spTransFunctionEvalSerializerClassID
            && spTransFunctionEvalSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spTransFunctionEvalSerializerTargetClassIDValue
            && spTransFunctionEvalSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spTransFunctionEvalSerializerTargetClassIDValue,
        "transform-function-evaluator serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spFunctionEvalSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spFunctionEvalSerializerLayout) == 0x14
            && spFunctionEvalSerializer::ClassID
                == sparkplug::evidence::pc::spFunctionEvalSerializerClassID
            && spFunctionEvalSerializer::ClassID
                == sparkplug::evidence::ps2::spFunctionEvalSerializerClassID
            && spFunctionEvalSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spFunctionEvalSerializerTargetClassIDValue
            && spFunctionEvalSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spFunctionEvalSerializerTargetClassIDValue,
        "function-evaluator serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spColorFuncEvalSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spColorFuncEvalSerializerLayout) == 0x14
            && spColorFuncEvalSerializer::ClassID
                == sparkplug::evidence::pc::spColorFuncEvalSerializerClassID
            && spColorFuncEvalSerializer::ClassID
                == sparkplug::evidence::ps2::spColorFuncEvalSerializerClassID
            && spColorFuncEvalSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spColorFuncEvalSerializerTargetClassIDValue
            && spColorFuncEvalSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spColorFuncEvalSerializerTargetClassIDValue,
        "color-function serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spSphereBVSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spSphereBVSerializerLayout) == 0x14
            && spSphereBVSerializer::ClassID
                == sparkplug::evidence::pc::spSphereBVSerializerClassID
            && spSphereBVSerializer::ClassID
                == sparkplug::evidence::ps2::spSphereBVSerializerClassID
            && spSphereBVSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spSphereBVSerializerTargetClassIDValue
            && spSphereBVSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spSphereBVSerializerTargetClassIDValue,
        "sphere-BV serializer extent, identity and analytical target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spBoxBVSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spBoxBVSerializerLayout) == 0x14
            && spBoxBVSerializer::ClassID
                == sparkplug::evidence::pc::spBoxBVSerializerClassID
            && spBoxBVSerializer::ClassID
                == sparkplug::evidence::ps2::spBoxBVSerializerClassID
            && spBoxBVSerializer::TargetClassID
                == sparkplug::evidence::pc::spBoxBVSerializerTargetClassIDValue
            && spBoxBVSerializer::TargetClassID
                == sparkplug::evidence::ps2::spBoxBVSerializerTargetClassIDValue,
        "box-BV serializer extent, identity and analytical target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spOBBBVSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spOBBBVSerializerLayout) == 0x14
            && spOBBBVSerializer::ClassID
                == sparkplug::evidence::pc::spOBBBVSerializerClassID
            && spOBBBVSerializer::ClassID
                == sparkplug::evidence::ps2::spOBBBVSerializerClassID
            && spOBBBVSerializer::TargetClassID
                == sparkplug::evidence::pc::spOBBBVSerializerTargetClassIDValue
            && spOBBBVSerializer::TargetClassID
                == sparkplug::evidence::ps2::spOBBBVSerializerTargetClassIDValue,
        "OBB-BV serializer extent, identity and analytical target agree");
    Require(sizeof(sparkplug::evidence::pc::spMaterialSerializerObservedLayout)
                == 0x3C
            && sizeof(sparkplug::evidence::ps2::spMaterialSerializerLayout)
                == 0x3C
            && offsetof(sparkplug::evidence::pc::
                    spMaterialSerializerObservedLayout,
                    dataBlockSerializerState) == 0x14
            && offsetof(sparkplug::evidence::ps2::spMaterialSerializerLayout,
                    dataBlockSerializerState) == 0x14
            && spMaterialSerializer::ClassID
                == sparkplug::evidence::pc::spMaterialSerializerClassID
            && spMaterialSerializer::ClassID
                == sparkplug::evidence::ps2::spMaterialSerializerClassID,
        "material serializer extent, helper offset and identity agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spMaterialDataSerializerObservedLayout) == 0x3C
            && sizeof(sparkplug::evidence::ps2::
                    spMaterialDataSerializerLayout) == 0x3C
            && spMaterialDataSerializer::ClassID
                == sparkplug::evidence::pc::spMaterialDataSerializerClassID
            && spMaterialDataSerializer::ClassID
                == sparkplug::evidence::ps2::spMaterialDataSerializerClassID
            && spMaterialDataSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spMaterialDataSerializerTargetClassIDValue
            && spMaterialDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spMaterialDataSerializerTargetClassIDValue,
        "material-data serializer extent, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spDXMaterialDataSerializerObservedLayout) == 0x3C
            && sizeof(sparkplug::evidence::ps2::
                    spDXMaterialDataSerializerLayout) == 0x3C
            && sizeof(sparkplug::evidence::pc::
                    spPS2MaterialDataSerializerObservedLayout) == 0x3C
            && sizeof(sparkplug::evidence::ps2::
                    spPS2MaterialDataSerializerLayout) == 0x3C
            && spDXMaterialDataSerializer::ClassID
                == sparkplug::evidence::pc::spDXMaterialDataSerializerClassID
            && spDXMaterialDataSerializer::ClassID
                == sparkplug::evidence::ps2::spDXMaterialDataSerializerClassID
            && spPS2MaterialDataSerializer::ClassID
                == sparkplug::evidence::pc::spPS2MaterialDataSerializerClassID
            && spPS2MaterialDataSerializer::ClassID
                == sparkplug::evidence::ps2::spPS2MaterialDataSerializerClassID,
        "platform material-data serializer extents and identities agree");
    Require(sizeof(sparkplug::evidence::pc::spRenderableSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spRenderableSerializerLayout)
                == 0x14
            && spRenderableSerializer::ClassID
                == sparkplug::evidence::pc::spRenderableSerializerClassID
            && spRenderableSerializer::ClassID
                == sparkplug::evidence::ps2::spRenderableSerializerClassID
            && spRenderableSerializer::TargetClassID
                == sparkplug::evidence::pc::spRenderableClassID
            && spRenderableSerializer::TargetClassID
                == sparkplug::evidence::ps2::spRenderableClassID,
        "renderable serializer layout, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::spModelSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spModelSerializerLayout)
                == 0x14
            && spModelSerializer::ClassID
                == sparkplug::evidence::pc::spModelSerializerClassID
            && spModelSerializer::ClassID
                == sparkplug::evidence::ps2::spModelSerializerClassID
            && spModelSerializer::TargetClassID
                == sparkplug::evidence::pc::spModelClassID
            && spModelSerializer::TargetClassID
                == sparkplug::evidence::ps2::spModelClassID,
        "model serializer layout, identity and target agree");
    Require(sizeof(sparkplug::evidence::pc::spSkinObservedLayout) == 0x70
            && offsetof(sparkplug::evidence::pc::spSkinObservedLayout,
                weightCount) == 0x60
            && offsetof(sparkplug::evidence::pc::spSkinObservedLayout,
                boneCount) == 0x64
            && offsetof(sparkplug::evidence::pc::spSkinObservedLayout,
                bones) == 0x68
            && offsetof(sparkplug::evidence::pc::spSkinObservedLayout,
                inverseBindMatrices) == 0x6C
            && sizeof(sparkplug::evidence::pc::spSkinSerializerObservedLayout)
                == 0x14
            && spSkin::ClassID == sparkplug::evidence::pc::spSkinClassID
            && spSkinSerializer::ClassID
                == sparkplug::evidence::pc::spSkinSerializerClassID,
        "PC skin and serializer field extents match executable evidence");
    Require(sizeof(sparkplug::evidence::pc::spAnimationLayout)
                == 0x84
            && offsetof(
                sparkplug::evidence::pc::spAnimationLayout,
                totalTime) == 0x14
            && offsetof(
                sparkplug::evidence::pc::spAnimationLayout,
                tracks) == 0x1C
            && offsetof(
                sparkplug::evidence::pc::spAnimationLayout,
                tagsBegin) == 0x2C
            && offsetof(
                sparkplug::evidence::pc::spAnimationLayout,
                auxiliaryBuffers) == 0x38
            && sparkplug::evidence::pc::spAnimationClassID == 0x56EE563A
            && sparkplug::evidence::pc::spAnimationSerializerClassID
                == 0xC0ACBFA6
            && sparkplug::evidence::pc::spControllerClassID == 0x4FAD24F1
            && sparkplug::evidence::pc::spSubControllerClassID == 0x062C22ED,
        "PC complete animation layout and controller registry identities match executable evidence");
    Require(sizeof(
                sparkplug::evidence::pc::spTransformTrackEvalObservedLayout)
                == 0x78
            && offsetof(
                sparkplug::evidence::pc::spTransformTrackEvalObservedLayout,
                boundTransformSlot) == 0x10
            && offsetof(
                sparkplug::evidence::pc::spTransformTrackEvalObservedLayout,
                blendInputCount) == 0x14
            && offsetof(
                sparkplug::evidence::pc::spTransformTrackEvalObservedLayout,
                blendInputs) == 0x18
            && sparkplug::evidence::pc::spTransformTrackEvalClassID
                == 0x5DAF152D
            && sparkplug::evidence::pc::spTransformEvalClassID
                == 0x87B0E260,
        "PC transform-track evaluator layout and registry identity match executable evidence");
    Require(sizeof(sparkplug::evidence::pc::spMeshDataSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spMeshDataSerializerLayout)
                == 0x14
            && spMeshDataSerializer::ClassID
                == sparkplug::evidence::pc::spMeshDataSerializerClassID
            && spMeshDataSerializer::ClassID
                == sparkplug::evidence::ps2::spMeshDataSerializerClassID
            && spMeshDataSerializer::TargetClassID
                == sparkplug::evidence::pc::spMeshDataClassID
            && spMeshDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::spMeshDataClassID,
        "mesh-data serializer layout, identity and target agree");
    Require(sizeof(
                sparkplug::evidence::pc::spPS2MeshDataSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spPS2MeshDataSerializerLayout)
                == 0x14
            && spPS2MeshDataSerializer::ClassID
                == sparkplug::evidence::pc::spPS2MeshDataSerializerClassID
            && spPS2MeshDataSerializer::ClassID
                == sparkplug::evidence::ps2::spPS2MeshDataSerializerClassID
            && spPS2MeshDataSerializer::TargetClassID
                == sparkplug::evidence::pc::spPS2MeshDataClassID
            && spPS2MeshDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::spPS2MeshDataClassID
            && spPS2MeshDataSerializer::PCNativeLoadFlagMask
                == sparkplug::evidence::pc::
                    spPS2MeshDataSerializerNativeLoadFlagMask
            && spPS2MeshDataSerializer::PS2NativeLoadFlagMask
                == sparkplug::evidence::ps2::
                    spPS2MeshDataSerializerNativeLoadFlagMask,
        "PS2 mesh-data serializer layout, identity, target and platform flags agree");
    Require(sizeof(
                sparkplug::evidence::pc::spDXMeshDataSerializerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spDXMeshDataSerializerLayout)
                == 0x14
            && spDXMeshDataSerializer::ClassID
                == sparkplug::evidence::pc::spDXMeshDataSerializerClassID
            && spDXMeshDataSerializer::ClassID
                == sparkplug::evidence::ps2::spDXMeshDataSerializerClassID
            && spDXMeshDataSerializer::TargetClassID
                == sparkplug::evidence::pc::spDXMeshDataClassID
            && spDXMeshDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::spDXMeshDataClassID
            && spDXMeshDataSerializer::PCNativeLoadFlagMask
                == sparkplug::evidence::pc::
                    spDXMeshDataSerializerNativeLoadFlagMask
            && spDXMeshDataSerializer::PS2NativeLoadFlagMask
                == sparkplug::evidence::ps2::
                    spDXMeshDataSerializerNativeLoadFlagMask,
        "DX mesh-data serializer layout, identity, target and platform flags agree");
    Require(sizeof(sparkplug::evidence::pc::spTemplateObjectLayout) == 0x1B0
            && sizeof(sparkplug::evidence::ps2::spTemplateObjectLayout) == 0x1B0
            && spTemplateObject::ClassID
                == sparkplug::evidence::pc::spTemplateObjectClassID
            && spTemplateObject::ClassID
                == sparkplug::evidence::ps2::spTemplateObjectClassID,
        "template-object exact layouts and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spTemplateSerializerLayout) == 0x1F4
            && sizeof(sparkplug::evidence::ps2::spTemplateSerializerLayout) == 0x1F4
            && spTemplateSerializer::ClassID
                == sparkplug::evidence::pc::spTemplateSerializerClassID
            && spTemplateSerializer::ClassID
                == sparkplug::evidence::ps2::spTemplateSerializerClassID,
        "template-serializer exact layouts and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spGameLevelLayout) == 0x2C
            && sizeof(sparkplug::evidence::ps2::spGameLevelLayout) == 0x2C
            && spGameLevel::ClassID
                == sparkplug::evidence::pc::spGameLevelClassID
            && spGameLevel::ClassID
                == sparkplug::evidence::ps2::spGameLevelClassID,
        "game-level exact layouts and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spGameLevelSerializerLayout) == 0x184
            && sizeof(sparkplug::evidence::ps2::spGameLevelSerializerLayout)
                == 0x184
            && spGameLevelSerializer::ClassID
                == sparkplug::evidence::pc::spGameLevelSerializerClassID
            && spGameLevelSerializer::ClassID
                == sparkplug::evidence::ps2::spGameLevelSerializerClassID
            && spGameLevelSerializer::BinaryHeaderMagic == 0x351E46AE,
        "game-level serializer layout, identity and binary magic agree");
    Require(sizeof(sparkplug::evidence::pc::spIndexBufferLayout) == 0x28
            && sizeof(sparkplug::evidence::ps2::spIndexBufferLayout) == 0x28
            && spIndexBuffer::ClassID
                == sparkplug::evidence::pc::spIndexBufferClassID
            && spIndexBuffer::ClassID
                == sparkplug::evidence::ps2::spIndexBufferClassID,
        "index-buffer exact layout and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spVertexBufferLayout) == 0x5C
            && sizeof(sparkplug::evidence::ps2::spVertexBufferLayout) == 0x5C
            && spVertexBuffer::ClassID
                == sparkplug::evidence::pc::spVertexBufferClassID
            && spVertexBuffer::ClassID
                == sparkplug::evidence::ps2::spVertexBufferClassID,
        "vertex-buffer exact layout and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spResourceLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::spResourceLayout) == 0x14
            && spResource::ClassID
                == sparkplug::evidence::pc::spResourceClassID
            && spResource::ClassID
                == sparkplug::evidence::ps2::spResourceClassID,
        "resource exact storage-free layout and identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spResourceCacheEntryLayout) == 0x08
            && sizeof(sparkplug::evidence::ps2::spResourceCacheEntryLayout)
                == 0x08
            && sizeof(
                sparkplug::evidence::pc::spResourceManagerObservedLayout)
                == 0x30
            && sizeof(sparkplug::evidence::ps2::spResourceManagerLayout)
                == 0x2C
            && offsetof(
                sparkplug::evidence::pc::spResourceManagerObservedLayout,
                entriesBegin) == 0x24
            && offsetof(sparkplug::evidence::ps2::spResourceManagerLayout,
                entries) == 0x20,
        "resource-manager cache ABI preserves the platform vector split");
    Require(sizeof(sparkplug::evidence::pc::spTextureObservedLayout) == 0x38
            && sizeof(sparkplug::evidence::ps2::spTextureLayout) == 0x38
            && offsetof(sparkplug::evidence::pc::spTextureObservedLayout,
                    iTextureVTable) == 0x14
            && offsetof(sparkplug::evidence::ps2::spTextureLayout,
                    dimensionsUnchanged) == 0x30
            && spTexture::ClassID
                == sparkplug::evidence::pc::spTextureClassID
            && spTexture::ClassID
                == sparkplug::evidence::ps2::spTextureClassID
            && sparkplug::evidence::pc::spTextureRegisteredBaseClassID
                == spNamedObject::ClassID
            && sparkplug::evidence::ps2::spCubeTextureAllocationSize == 0x38,
        "texture layout, identity and distinct registration base agree");
    Require(sizeof(sparkplug::evidence::pc::spTextureBufferLayout) == 0x30
            && sizeof(sparkplug::evidence::ps2::spTextureBufferLayout) == 0x30
            && offsetof(sparkplug::evidence::pc::spTextureBufferLayout,
                    buffer) == 0x1C
            && offsetof(sparkplug::evidence::ps2::spTextureBufferLayout,
                    initialized) == 0x2C
            && spTextureBuffer::ClassID
                == sparkplug::evidence::pc::spTextureBufferClassID
            && spTextureBuffer::ClassID
                == sparkplug::evidence::ps2::spTextureBufferClassID,
        "texture-buffer exact layout and class identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spTextureDataLayout) == 0x4A0
            && sizeof(sparkplug::evidence::ps2::spTextureDataLayout) == 0x498
            && offsetof(sparkplug::evidence::pc::spTextureDataLayout,
                    textureBuffer) == 0x38
            && offsetof(sparkplug::evidence::ps2::spTextureDataLayout,
                    textureBuffer) == 0x38
            && offsetof(sparkplug::evidence::pc::spTextureDataLayout,
                    records490) == 0x490
            && offsetof(sparkplug::evidence::ps2::spTextureDataLayout,
                    records48C) == 0x48C
            && spTextureData::ClassID
                == sparkplug::evidence::pc::spTextureDataClassID
            && spTextureData::ClassID
                == sparkplug::evidence::ps2::spTextureDataClassID,
        "texture-data platform layouts, embedded buffer and identity agree");
    Require(sizeof(
                sparkplug::evidence::pc::spTextureDataSerializerObservedLayout)
                == 0x14
            && sizeof(
                sparkplug::evidence::ps2::spTextureDataSerializerLayout)
                == 0x14
            && spTextureDataSerializer::ClassID
                == sparkplug::evidence::pc::spTextureDataSerializerClassID
            && spTextureDataSerializer::ClassID
                == sparkplug::evidence::ps2::spTextureDataSerializerClassID
            && spTextureDataSerializer::TargetClassID
                == sparkplug::evidence::pc::spTextureDataSerializerTargetClassIDValue
            && spTextureDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::spTextureDataSerializerTargetClassIDValue,
        "texture-data serializer layout and identities agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::
                    spDXTextureDataSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spDXTextureDataSerializerLayout) == 0x14
            && spDXTextureDataSerializer::ClassID
                == sparkplug::evidence::pc::spDXTextureDataSerializerClassID
            && spDXTextureDataSerializer::ClassID
                == sparkplug::evidence::ps2::spDXTextureDataSerializerClassID
            && spDXTextureDataSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spDXTextureDataSerializerTargetClassIDValue
            && spDXTextureDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spDXTextureDataSerializerTargetClassIDValue
            && spDXTextureDataSerializer::PCNativeLoadFlagMask
                == sparkplug::evidence::pc::
                    spDXTextureDataSerializerNativeLoadFlagMask
            && spDXTextureDataSerializer::PS2NativeLoadFlagMask
                == sparkplug::evidence::ps2::
                    spDXTextureDataSerializerNativeLoadFlagMask,
        "DX texture-data serializer layout, identities and load masks agree");
    Require(sizeof(sparkplug::evidence::pc::
                    spPS2TextureDataSerializerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::
                    spPS2TextureDataSerializerLayout) == 0x14
            && spPS2TextureDataSerializer::ClassID
                == sparkplug::evidence::pc::spPS2TextureDataSerializerClassID
            && spPS2TextureDataSerializer::ClassID
                == sparkplug::evidence::ps2::spPS2TextureDataSerializerClassID
            && spPS2TextureDataSerializer::TargetClassID
                == sparkplug::evidence::pc::
                    spPS2TextureDataSerializerTargetClassIDValue
            && spPS2TextureDataSerializer::TargetClassID
                == sparkplug::evidence::ps2::
                    spPS2TextureDataSerializerTargetClassIDValue
            && spPS2TextureDataSerializer::PCNativeLoadFlagMask
                == sparkplug::evidence::pc::
                    spPS2TextureDataSerializerNativeLoadFlagMask
            && spPS2TextureDataSerializer::PS2NativeLoadFlagMask
                == sparkplug::evidence::ps2::
                    spPS2TextureDataSerializerNativeLoadFlagMask,
        "PS2 texture-data serializer layout, identities and load masks agree");
    Require(sizeof(sparkplug::evidence::pc::spPlatformSpecificMeshDataLayout)
                == 0x14
            && sizeof(
                sparkplug::evidence::ps2::spPlatformSpecificMeshDataLayout)
                == 0x14
            && spPlatformSpecificMeshData::ClassID
                == sparkplug::evidence::pc::spPlatformSpecificMeshDataClassID
            && spPlatformSpecificMeshData::ClassID
                == sparkplug::evidence::ps2::spPlatformSpecificMeshDataClassID,
            "platform mesh-data base layout and identity agree across binaries");
    Require(sizeof(sparkplug::evidence::pc::spDXMeshDataObservedPrefixLayout)
                == 0x20
            && sizeof(sparkplug::evidence::ps2::spDXMeshDataLayout) == 0x44
            && offsetof(sparkplug::evidence::pc::spDXMeshDataObservedPrefixLayout,
                    indexBuffer) == 0x18
            && offsetof(sparkplug::evidence::ps2::spDXMeshDataLayout,
                    vertexBuffer) == 0x1C
            && spDXMeshData::ClassID
                == sparkplug::evidence::pc::spDXMeshDataClassID
            && spDXMeshData::ClassID
                == sparkplug::evidence::ps2::spDXMeshDataClassID,
        "DX mesh-data proven prefix, PS2 exact layout and identity agree");
    Require(sizeof(sparkplug::evidence::pc::spPS2MeshDataObservedLayout)
                == 0x100
            && sizeof(sparkplug::evidence::ps2::spPS2MeshDataLayout) == 0x100
            && offsetof(sparkplug::evidence::pc::spPS2MeshDataObservedLayout,
                    packet) == 0x40
            && offsetof(sparkplug::evidence::ps2::spPS2MeshDataLayout,
                    fieldFC) == 0xFC
            && spPS2MeshData::ClassID
                == sparkplug::evidence::pc::spPS2MeshDataClassID
            && spPS2MeshData::ClassID
                == sparkplug::evidence::ps2::spPS2MeshDataClassID,
        "PS2 mesh-data observed PC extent, exact PS2 layout and identity agree");
    Require(sizeof(sparkplug::evidence::pc::spMeshLayout) == 0x50
            && sizeof(sparkplug::evidence::ps2::spMeshLayout) == 0x50
            && spMesh::ClassID == sparkplug::evidence::pc::spMeshClassID
            && spMesh::ClassID == sparkplug::evidence::ps2::spMeshClassID,
        "mesh exact layout and identity agree across binaries");
    const auto& renderMeshRecord = spRenderMesh::StaticRTTI();
    Require(sizeof(sparkplug::evidence::pc::spRenderMeshLayout) == 0x50
            && sizeof(sparkplug::evidence::ps2::spRenderMeshLayout) == 0x50
            && spRenderMesh::ClassID
                == sparkplug::evidence::pc::spRenderMeshClassID
            && spRenderMesh::ClassID
                == sparkplug::evidence::ps2::spRenderMeshClassID
            && renderMeshRecord.baseClassID == spMesh::ClassID
            && renderMeshRecord.factory == nullptr,
        "render-mesh is a storage-free abstract identity layer over spMesh");
    const auto& ps2RenderMeshRecord = spPS2Mesh::StaticRTTI();
    Require(sizeof(sparkplug::evidence::ps2::spPS2MeshLayout) == 0x58
            && offsetof(sparkplug::evidence::ps2::spPS2MeshLayout,
                    preparedMeshData) == 0x50
            && offsetof(sparkplug::evidence::ps2::spPS2MeshLayout,
                    packetEmitter) == 0x54
            && spPS2Mesh::ClassID
                == sparkplug::evidence::ps2::spPS2MeshClassID
            && ps2RenderMeshRecord.base == &spRenderMesh::StaticRTTI()
            && ps2RenderMeshRecord.factory != nullptr,
        "PS2 render mesh has exact leaf layout and concrete RTTI factory");
    {
        spPS2Mesh mesh;
        auto data = std::make_unique<spPS2MeshData>();
        data->SetName("prepared-ps2-data");
        const auto* const expectedData = data.get();
        Require(mesh.AttachPreparedDataForAnalysis(
                    std::move(data), 17, 51)
                && mesh.GetPreparedDataForAnalysis() == expectedData
                && mesh.GetPrimitiveCountForAnalysis() == 17
                && mesh.GetVertexCountForAnalysis() == 51,
            "PS2 render mesh owns prepared data and copies native counts");
        mesh.SetName("ps2-render-mesh");
        auto cloneBase = mesh.Clone();
        auto* clone = dynamic_cast<spPS2Mesh*>(cloneBase.get());
        Require(clone != nullptr && clone->GetName() == nullptr
                && clone->GetPreparedDataForAnalysis() == nullptr,
            "PS2 render-mesh RTTI clone is blank like the native no-op copy");
        mesh.ReleaseForAnalysis();
        Require(mesh.GetPreparedDataForAnalysis() == nullptr,
            "PS2 render-mesh release drops the owned prepared data");
    }
    const auto& dxVertexRecord = spDXVertexBuffer::StaticRTTI();
    const auto& dxIndexRecord = spDXIndexBuffer::StaticRTTI();
    Require(sizeof(sparkplug::evidence::pc::spDXVertexBufferLayout) == 0x20
            && offsetof(sparkplug::evidence::pc::spDXVertexBufferLayout,
                    direct3DVertexBuffer) == 0x10
            && offsetof(sparkplug::evidence::pc::spDXVertexBufferLayout,
                    byteSize) == 0x1C
            && spDXVertexBuffer::ClassID
                == sparkplug::evidence::pc::spDXVertexBufferClassID
            && dxVertexRecord.baseClassID == spBaseObject::ClassID
            && dxVertexRecord.factory != nullptr,
        "DX vertex-buffer exact PC layout and RTTI factory agree");
    Require(sizeof(sparkplug::evidence::pc::spDXIndexBufferLayout) == 0x1C
            && offsetof(sparkplug::evidence::pc::spDXIndexBufferLayout,
                    direct3DIndexBuffer) == 0x10
            && offsetof(sparkplug::evidence::pc::spDXIndexBufferLayout,
                    byteSize) == 0x18
            && spDXIndexBuffer::ClassID
                == sparkplug::evidence::pc::spDXIndexBufferClassID
            && dxIndexRecord.baseClassID == spBaseObject::ClassID
            && dxIndexRecord.factory != nullptr,
        "DX index-buffer exact PC layout and RTTI factory agree");
    {
        spDXVertexBuffer vertexBuffer;
        Require(vertexBuffer.InitializeForAnalysis(96, 8, 0x112, 1)
                && vertexBuffer.IsInitializedForAnalysis()
                && vertexBuffer.GetDataForAnalysis().size() == 96
                && vertexBuffer.GetByteSizeForAnalysis() == 96
                && vertexBuffer.GetUsageForAnalysis() == 8
                && vertexBuffer.GetFVFCodeForAnalysis() == 0x112
                && vertexBuffer.GetPoolForAnalysis() == 1,
            "DX vertex-buffer host seam preserves CreateVertexBuffer arguments");
        auto clone = vertexBuffer.Clone();
        auto* blankClone = dynamic_cast<spDXVertexBuffer*>(clone.get());
        Require(blankClone != nullptr
                && !blankClone->IsInitializedForAnalysis()
                && blankClone->GetByteSizeForAnalysis() == 0,
            "DX vertex-buffer native RTTI clone is blank");
        vertexBuffer.ReleaseDeviceBufferForAnalysis();
        Require(!vertexBuffer.IsInitializedForAnalysis()
                && vertexBuffer.GetDataForAnalysis().empty()
                && vertexBuffer.GetByteSizeForAnalysis() == 96,
            "DX vertex-buffer release drops device storage but retains metadata");
    }
    {
        spDXIndexBuffer indexBuffer;
        Require(indexBuffer.InitializeForAnalysis(18, 8, 0x65, 1)
                && indexBuffer.IsInitializedForAnalysis()
                && indexBuffer.GetDataForAnalysis().size() == 18
                && indexBuffer.GetByteSizeForAnalysis() == 18
                && indexBuffer.GetUsageForAnalysis() == 8
                && indexBuffer.GetFormatForAnalysis() == 0x65
                && indexBuffer.GetPoolForAnalysis() == 1,
            "DX index-buffer host seam preserves CreateIndexBuffer arguments");
        auto clone = indexBuffer.Clone();
        auto* blankClone = dynamic_cast<spDXIndexBuffer*>(clone.get());
        Require(blankClone != nullptr
                && !blankClone->IsInitializedForAnalysis()
                && blankClone->GetByteSizeForAnalysis() == 0,
            "DX index-buffer native RTTI clone is blank");
        indexBuffer.ReleaseDeviceBufferForAnalysis();
        Require(!indexBuffer.IsInitializedForAnalysis()
                && indexBuffer.GetDataForAnalysis().empty()
                && indexBuffer.GetByteSizeForAnalysis() == 18,
            "DX index-buffer release drops device storage but retains metadata");
    }
    {
        spDXMeshCombiner combiner;
        Require(combiner.InitializeForAnalysis(0x112, 3, 36, 12, 0xDEADBEEF)
                && combiner.GetTargetVertexCountForAnalysis() == 3
                && combiner.GetFVFCodeForAnalysis() == 0x112
                && combiner.IsLockedForAnalysis()
                && !combiner.IsFullForAnalysis()
                && combiner.GetVertexBufferForAnalysis() != nullptr
                && combiner.GetVertexBufferForAnalysis()
                    ->GetUsageForAnalysis() == 8
                && combiner.GetVertexBufferForAnalysis()
                    ->GetPoolForAnalysis() == 1
                && combiner.GetIndexBufferForAnalysis() != nullptr
                && combiner.GetIndexBufferForAnalysis()
                    ->GetFormatForAnalysis() == 0x65,
            "DX mesh combiner initializes the native dynamic INDEX16 buffers");
        Require(!combiner.CommitForAnalysis(4, 36, 6, 12)
                && combiner.GetWrittenVertexCountForAnalysis() == 0
                && combiner.GetVertexWriteOffsetForAnalysis() == 0,
            "DX mesh combiner rejects overflow without partially advancing");
        Require(combiner.CommitForAnalysis(1, 12, 3, 6)
                && combiner.GetWrittenVertexCountForAnalysis() == 1
                && combiner.GetWrittenIndexCountForAnalysis() == 3
                && combiner.GetVertexWriteOffsetForAnalysis() == 12
                && combiner.GetIndexWriteOffsetForAnalysis() == 6
                && combiner.IsLockedForAnalysis(),
            "DX mesh combiner advances both locked cursors together");
        Require(combiner.CommitForAnalysis(2, 24, 3, 6)
                && combiner.IsFullForAnalysis()
                && !combiner.IsLockedForAnalysis(),
            "DX mesh combiner unlocks both buffers at exact vertex capacity");
        Require(!combiner.CommitForAnalysis(0, 0, 0, 0),
            "DX mesh combiner rejects commits after native unlock boundary");
    }
    {
        const std::vector<std::byte> indices{
            std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0},
            std::byte{2}, std::byte{0},
        };
        const std::vector<std::byte> vertices{
            std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
            std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8},
        };
        spDXSharedMeshData shared;
        Require(shared.InitializeForAnalysis(indices, vertices)
                && shared.GetIndexBufferForAnalysis() != nullptr
                && shared.GetIndexBufferForAnalysis()->GetFormatForAnalysis()
                    == 0x65
                && shared.GetIndexBufferForAnalysis()->GetDataForAnalysis()
                    == indices
                && shared.GetVertexBufferForAnalysis() != nullptr
                && shared.GetVertexBufferForAnalysis()->GetFVFCodeForAnalysis()
                    == 0
                && shared.GetVertexBufferForAnalysis()->GetDataForAnalysis()
                    == vertices,
            "DX shared mesh data atomically materializes both native buffers");
        auto clone = shared.Clone();
        auto* blankClone = dynamic_cast<spDXSharedMeshData*>(clone.get());
        Require(blankClone != nullptr
                && blankClone->GetIndexBufferForAnalysis() == nullptr
                && blankClone->GetVertexBufferForAnalysis() == nullptr,
            "DX shared mesh data RTTI clone leaves device buffers blank");
        shared.ReleaseBuffersForAnalysis();
        Require(shared.GetIndexBufferForAnalysis() == nullptr
                && shared.GetVertexBufferForAnalysis() == nullptr,
            "DX shared mesh data releases both owned device buffers");
    }
    Require(sizeof(sparkplug::evidence::pc::spDXSharedMeshDataObservedLayout)
                == 0x1C
            && spDXSharedMeshData::ClassID
                == sparkplug::evidence::pc::spDXSharedMeshDataClassID,
        "DX shared mesh-data observed ABI and portable identity agree");
    Require(sizeof(sparkplug::evidence::pc::spDXCombinedVBObservedLayout)
                == 0x3C
            && offsetof(sparkplug::evidence::pc::spDXCombinedVBObservedLayout,
                vertexData) == 0x2C
            && offsetof(sparkplug::evidence::pc::spDXCombinedVBObservedLayout,
                indexByteSize) == 0x38
            && spDXCombinedVB::ClassID
                == sparkplug::evidence::pc::spDXCombinedVBClassID,
        "DX combined-VB serializer source preserves its observed PC ABI");
    {
        spDXSharedMeshDataSerializer serializer;
        Require(serializer.GetTargetClassIDForAnalysis()
                    == spDXSharedMeshData::ClassID
                && serializer.GetSourceClassIDForAnalysis() == 0x4B18E622
                && serializer.ResolveClassIDForAnalysis(0x4B18E622)
                    == spDXSharedMeshData::ClassID
                && serializer.ResolveClassIDForAnalysis(0xFFFFFFFF)
                    == spDXSharedMeshData::ClassID
                && serializer.vfunc_18().baseClassID == spSerializer::ClassID,
            "DX shared mesh serializer maps combined-VB source to shared data");

        const std::vector<std::byte> payloadIndices{
            std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0},
            std::byte{2}, std::byte{0},
        };
        const std::vector<std::byte> payloadVertices{
            std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
            std::byte{0x44},
        };
        spDXCombinedVB payload;
        spMemoryStream stream;
        Require(payload.InitializePayloadForAnalysis(
                    payloadIndices, payloadVertices)
                && stream.Open("dx-shared-mesh")
                && serializer.WritePayloadForAnalysis(stream, payload)
                && stream.Seek(spStream::SeekSource::essStart, 0),
            "DX shared mesh serializer writes its complete native payload");
        const auto* const wire =
            static_cast<const std::uint8_t*>(stream.GetBuffer());
        std::uint32_t wireIndexSize = 0;
        std::uint32_t wireVertexSize = 0;
        std::memcpy(&wireIndexSize, wire, sizeof(wireIndexSize));
        std::memcpy(&wireVertexSize, wire + 4, sizeof(wireVertexSize));
        Require(wireIndexSize == payloadIndices.size()
                && wireVertexSize == payloadVertices.size()
                && std::memcmp(wire + 8, payloadIndices.data(),
                    payloadIndices.size()) == 0,
            "DX shared mesh wire order starts sizes then index bytes");

        spDXSharedMeshData decoded;
        Require(serializer.ReadPayloadForAnalysis(stream, decoded)
                && decoded.GetIndexBufferForAnalysis()->GetDataForAnalysis()
                    == payloadIndices
                && decoded.GetVertexBufferForAnalysis()->GetDataForAnalysis()
                    == payloadVertices,
            "DX shared mesh payload round-trips into both GPU-buffer facades");

        spMemoryStream truncated;
        const std::uint32_t declaredSize = 16;
        Require(truncated.Open("truncated")
                && truncated.Write(declaredSize)
                && truncated.Write(declaredSize)
                && truncated.Seek(spStream::SeekSource::essStart, 0)
                && !serializer.ReadPayloadForAnalysis(truncated, decoded),
            "DX shared mesh reader safely rejects a truncated native payload");
        auto serializerClone = serializer.Clone();
        Require(dynamic_cast<spDXSharedMeshDataSerializer*>(
                    serializerClone.get()) != nullptr,
            "DX shared mesh serializer RTTI clone remains the concrete type");
        auto combinedClone = payload.Clone();
        auto* blankCombined = dynamic_cast<spDXCombinedVB*>(combinedClone.get());
        Require(blankCombined != nullptr
                && blankCombined->GetIndexDataForAnalysis().empty()
                && blankCombined->GetVertexDataForAnalysis().empty(),
            "DX combined-VB RTTI clone leaves serialization payload blank");
    }
    {
        const auto& record = spDXMesh::StaticRTTI();
        Require(sizeof(sparkplug::evidence::pc::spDXMeshObservedLayout) == 0x88
                && offsetof(sparkplug::evidence::pc::spDXMeshObservedLayout,
                    indexType) == 0x50
                && offsetof(sparkplug::evidence::pc::spDXMeshObservedLayout,
                    vertexDeclaration) == 0x84
                && spDXMesh::ClassID
                    == sparkplug::evidence::pc::spDXMeshClassID
                && record.baseClassID == spRenderMesh::ClassID
                && record.factory != nullptr,
            "DX mesh observed PC prefix and concrete RTTI identity agree");
        Require(sizeof(sparkplug::evidence::pc::spDXMeshSerializerObservedLayout)
                    == 0x14
                && offsetof(
                    sparkplug::evidence::pc::spDXMeshSerializerObservedLayout,
                    serializerInterfaceVTable) == 0x10
                && spDXMeshSerializer::ClassID
                    == sparkplug::evidence::pc::spDXMeshSerializerClassID,
            "DX mesh serializer preserves its observed secondary-interface ABI");
        Require(spDXMesh::ComponentFlagsToFVFForAnalysis(0) == 0x02
                && spDXMesh::ComponentFlagsToFVFForAnalysis(0x40) == 0x12
                && spDXMesh::ComponentFlagsToFVFForAnalysis(
                    0x80 | 0x100 | 0x200 | 0x400 | 0x40000 | 0x10 | 0x20)
                    == (0x02U | 0x20U | 0x40U | 0x80U | 0x10100U
                        | 0x800U | 0x0CU | 0x1000U)
                && spDXMesh::ComponentWeightCountForAnalysis(0x10) == 4
                && spDXMesh::ComponentWeightCountForAnalysis(0x08) == 3
                && spDXMesh::ComponentWeightCountForAnalysis(0x04) == 2
                && spDXMesh::ComponentWeightCountForAnalysis(0x02) == 1,
            "DX mesh reproduces native component-to-FVF and component-weight-count maps");

        spIndexBuffer indices;
        spVertexBuffer vertices;
        Require(indices.InitializeForAnalysis(
                    1, spIndexBuffer::eIndexBufferType::Type2)
                && indices.SetIndexForAnalysis(0, 0)
                && indices.SetIndexForAnalysis(1, 1)
                && indices.SetIndexForAnalysis(2, 2)
                && vertices.InitializeForAnalysis(0, 3),
            "DX mesh test source buffers initialize");
        auto vertexBytes = vertices.GetDataForAnalysis();
        for (std::size_t index = 0; index < vertexBytes.size(); ++index)
        {
            vertexBytes[index] = static_cast<std::byte>(index & 0xFFU);
        }
        Require(vertices.SetDataForAnalysis(vertexBytes),
            "DX mesh test source vertices accept deterministic bytes");

        spDXMesh standalone;
        Require(standalone.InitializeFromBuffersForAnalysis(indices, vertices)
                && standalone.GetIndexTypeForAnalysis()
                    == spIndexBuffer::eIndexBufferType::Type2
                && standalone.GetPrimitiveCountForAnalysis() == 1
                && standalone.GetVertexCountForAnalysis() == 3
                && standalone.GetVertexComponentFlagsForAnalysis() == 0
                && standalone.GetIndexByteSizeForAnalysis() == 6
                && standalone.GetVertexByteSizeForAnalysis() == 36
                && standalone.GetFVFCodeForAnalysis() == 0x02
                && standalone.GetVertexStrideForAnalysis() == 12
                && standalone.GetDXIndexBufferForAnalysis() != nullptr
                && standalone.GetDXVertexBufferForAnalysis() != nullptr
                && standalone.GetDXVertexBufferForAnalysis()
                    ->GetDataForAnalysis() == vertexBytes,
            "DX mesh materializes standalone INDEX16 and vertex buffers");
        auto meshClone = standalone.Clone();
        auto* blankMeshClone = dynamic_cast<spDXMesh*>(meshClone.get());
        Require(blankMeshClone != nullptr
                && blankMeshClone->GetDXIndexBufferForAnalysis() == nullptr
                && blankMeshClone->GetFVFCodeForAnalysis() == 0,
            "DX mesh RTTI clone leaves backend payload blank");

        spVertexBuffer packedVertices;
        std::vector<std::byte> packedBytes(16, std::byte{0});
        packedBytes[12] = std::byte{1};
        packedBytes[13] = std::byte{2};
        packedBytes[14] = std::byte{3};
        packedBytes[15] = std::byte{4};
        Require(packedVertices.InitializeFromDataForAnalysis(
                    0x20, 1, 0, packedBytes),
            "packed-field source vertex initializes");
        spIndexBuffer pointIndex;
        Require(pointIndex.InitializeForAnalysis(
                    1, spIndexBuffer::eIndexBufferType::Type1)
                && pointIndex.SetIndexForAnalysis(0, 0),
            "packed-field source index initializes");
        spDXMesh cpuMesh;
        Require(cpuMesh.InitializeFromBuffersForAnalysis(
                    pointIndex, packedVertices, true)
                && cpuMesh.GetDXVertexBufferForAnalysis() == nullptr
                && cpuMesh.GetCPUVertexDataForAnalysis().size() == 28
                && cpuMesh.GetVertexStrideForAnalysis() == 28,
            "DX mesh CPU mode expands the four-byte field by twelve bytes");
        for (std::size_t component = 0; component < 4; ++component)
        {
            float expanded = 0.0F;
            std::memcpy(&expanded,
                cpuMesh.GetCPUVertexDataForAnalysis().data() + 12
                    + component * sizeof(float),
                sizeof(expanded));
            Require(expanded == static_cast<float>(component + 1),
                "DX mesh packed bytes expand to four unnormalised floats");
        }

        spDXMeshCombiner combiner;
        Require(combiner.InitializeForAnalysis(0x02, 6, 72, 12),
            "DX mesh batch combiner initializes for two triangles");
        spDXMesh firstCombined;
        spDXMesh secondCombined;
        Require(firstCombined.InitializeFromBuffersForAnalysis(
                    indices, vertices, false, &combiner)
                && firstCombined.GetIndexBeginForAnalysis() == 0
                && firstCombined.GetVertexBeginForAnalysis() == 0
                && secondCombined.InitializeFromBuffersForAnalysis(
                    indices, vertices, false, &combiner)
                && secondCombined.GetIndexBeginForAnalysis() == 3
                && secondCombined.GetVertexBeginForAnalysis() == 3
                && combiner.IsFullForAnalysis()
                && !combiner.IsLockedForAnalysis()
                && firstCombined.GetDXIndexBufferForAnalysis()
                    == secondCombined.GetDXIndexBufferForAnalysis(),
            "DX mesh batch path assigns disjoint ranges in common buffers");

        auto shared = std::make_shared<spDXSharedMeshData>();
        const std::vector<std::byte> sharedIndices(12, std::byte{0});
        const std::vector<std::byte> sharedVertices(72, std::byte{0});
        const spMesh::BoundingSphere sphere{1.0F, 2.0F, 3.0F, 4.0F};
        spDXMesh ranged;
        Require(shared->InitializeForAnalysis(sharedIndices, sharedVertices)
                && ranged.InitializeSharedForAnalysis(shared, 0, 0x02,
                    spIndexBuffer::eIndexBufferType::Type2,
                    3, 3, 3, 3, 12, &sphere)
                && ranged.GetSharedMeshDataForAnalysis() == shared
                && ranged.GetIndexBeginForAnalysis() == 3
                && ranged.GetVertexBeginForAnalysis() == 3
                && ranged.GetPrimitiveCountForAnalysis() == 1
                && ranged.GetBoundingSphereForAnalysis() == sphere,
            "DX mesh attaches a validated range of serialized shared buffers");

        auto combined = std::make_shared<spDXCombinedVB>();
        spDXMeshSerializer meshSerializer;
        spDXMeshWirePayloadForAnalysis meshPayload;
        const spDXCombinedVBRangeForAnalysis rangedSlice{
            3, 3, 3, 3, 12};
        Require(combined->InitializePayloadForAnalysis(
                    sharedIndices, sharedVertices)
                && !spDXMeshSerializer::BuildPayloadForAnalysis(
                    ranged, combined, meshPayload)
                && combined->RegisterMeshRangeForAnalysis(
                    ranged, rangedSlice)
                && combined->GetMeshRangeCountForAnalysis() == 1
                && meshSerializer.GetTargetClassIDForAnalysis()
                    == spDXMesh::ClassID
                && meshSerializer.vfunc_18().baseClassID
                    == spSerializer::ClassID
                && spDXMeshSerializer::SharedMeshDataClassID
                    == spDXSharedMeshData::ClassID
                && spDXMeshSerializer::BuildPayloadForAnalysis(
                    ranged, combined, meshPayload)
                && meshPayload.indexType == 2
                && meshPayload.indexBegin == 3
                && meshPayload.vertexBegin == 3
                && meshPayload.indexCount == 3
                && meshPayload.vertexCount == 3
                && meshPayload.vertexStride == 12,
            "DX mesh serializer builds the renderer-backed native scalar view");
        Require(!combined->RegisterMeshRangeForAnalysis(
                    ranged, spDXCombinedVBRangeForAnalysis{
                        6, 6, 1, 1, 12})
                && combined->GetMeshRangeCountForAnalysis() == 1,
            "DX combined-VB rejects ranges beyond its native byte payload");

        struct RelationshipContext final
        {
            std::shared_ptr<spBaseObject> writeExpected;
            std::shared_ptr<spBaseObject> readResult;
            std::uint32_t token;
        } relationshipContext{combined, shared, 0xA1B2C3D4};
        const spDXMeshRelationshipCodecForAnalysis relationshipCodec{
            &relationshipContext,
            +[](void* context, spStream& source,
                 std::shared_ptr<spBaseObject>& relationship) -> bool
            {
                auto& state = *static_cast<RelationshipContext*>(context);
                std::uint32_t token = 0;
                if (!source.Read(token) || token != state.token)
                {
                    return false;
                }
                relationship = state.readResult;
                return true;
            },
            +[](void* context, spStream& destination,
                 const std::shared_ptr<spBaseObject>& relationship) -> bool
            {
                const auto& state =
                    *static_cast<const RelationshipContext*>(context);
                return relationship == state.writeExpected
                    && destination.Write(state.token);
            },
        };
        spMemoryStream meshStream;
        Require(meshStream.Open("dx-mesh")
                && meshSerializer.WritePayloadForAnalysis(
                    meshStream, meshPayload, relationshipCodec),
            "DX mesh serializer writes its complete native field sequence");
        std::uint32_t meshWireSize = 0;
        Require(meshStream.GetSize(&meshWireSize) && meshWireSize == 49,
            "DX mesh wire scalars occupy 45 bytes around the relationship token");
        const auto* const meshWire =
            static_cast<const std::uint8_t*>(meshStream.GetBuffer());
        std::uint32_t encoded = 0;
        std::memcpy(&encoded, meshWire + 1, sizeof(encoded));
        Require(meshWire[0] == 2 && encoded == 0,
            "DX mesh wire begins with packed u8 type then unaligned u32 components");
        std::memcpy(&encoded, meshWire + 9, sizeof(encoded));
        Require(encoded == relationshipContext.token,
            "DX mesh relationship occurs immediately after FVF");
        std::memcpy(&encoded, meshWire + 13, sizeof(encoded));
        Require(encoded == 3,
            "DX mesh range starts immediately after its relationship");

        spDXMesh decodedMesh;
        Require(meshStream.Seek(spStream::SeekSource::essStart, 0)
                && meshSerializer.ReadPayloadForAnalysis(
                    meshStream, decodedMesh, relationshipCodec)
                && decodedMesh.GetSharedMeshDataForAnalysis() == shared
                && decodedMesh.GetIndexBeginForAnalysis() == 3
                && decodedMesh.GetVertexBeginForAnalysis() == 3
                && decodedMesh.GetPrimitiveCountForAnalysis() == 1
                && decodedMesh.GetBoundingSphereForAnalysis() == sphere,
            "DX mesh native payload round-trips through the shared buffer relationship");

        spMemoryStream truncatedMeshStream;
        Require(truncatedMeshStream.Open("truncated-dx-mesh")
                && truncatedMeshStream.WriteData(meshWire, meshWireSize - 1)
                && truncatedMeshStream.Seek(spStream::SeekSource::essStart, 0)
                && !meshSerializer.ReadPayloadForAnalysis(
                    truncatedMeshStream, decodedMesh, relationshipCodec),
            "DX mesh serializer safely rejects a truncated scalar tail");
        auto meshSerializerClone = meshSerializer.Clone();
        Require(dynamic_cast<spDXMeshSerializer*>(
                    meshSerializerClone.get()) != nullptr,
            "DX mesh serializer RTTI clone remains the concrete type");
    }
    Require(sizeof(sparkplug::evidence::pc::spMeshDataLayout) == 0x58
            && sizeof(sparkplug::evidence::ps2::spMeshDataLayout) == 0x58
            && spMeshData::ClassID
                == sparkplug::evidence::pc::spMeshDataClassID
            && spMeshData::ClassID
                == sparkplug::evidence::ps2::spMeshDataClassID,
        "mesh-data two-buffer ABI agrees across binaries");
    Require(sizeof(sparkplug::evidence::pc::spRenderableLayout) == 0x58
            && sizeof(sparkplug::evidence::ps2::spRenderableLayout) == 0x50
            && spRenderable::ClassID
                == sparkplug::evidence::pc::spRenderableClassID
            && spRenderable::ClassID
                == sparkplug::evidence::ps2::spRenderableClassID,
        "renderable preserves the compiler-specific vector ABI split");
    Require(sizeof(sparkplug::evidence::pc::spRenderNodeObservedPrefixLayout)
                == 0xC8
            && sizeof(sparkplug::evidence::ps2::spRenderNodeLayout) == 0x1E0
            && spRenderNode::ClassID
                == sparkplug::evidence::pc::spRenderNodeClassID
            && spRenderNode::ClassID
                == sparkplug::evidence::ps2::spRenderNodeClassID,
        "render node preserves the observed PC vector prefix and exact PS2 extent");
    Require(offsetof(
                sparkplug::evidence::pc::spRenderNodeObservedPrefixLayout,
                renderableBegin) == 0xBC
            && offsetof(sparkplug::evidence::ps2::spRenderNodeLayout,
                renderableCount) == 0xD0,
        "render-node renderable containers retain their platform-specific offsets");
    Require(sizeof(
                sparkplug::evidence::pc::spRenderNodeSerializerObservedLayout)
                == 0x14
            && sizeof(
                sparkplug::evidence::ps2::spRenderNodeSerializerLayout)
                == 0x14
            && spRenderNodeSerializer::ClassID
                == sparkplug::evidence::pc::spRenderNodeSerializerClassID
            && spRenderNodeSerializer::ClassID
                == sparkplug::evidence::ps2::spRenderNodeSerializerClassID,
        "render-node serializer preserves its storage-free platform ABI and ID");
    Require(sizeof(
                sparkplug::evidence::pc::spSceneGraphOptimizerObservedPrefixLayout)
                == 0x38
            && offsetof(
                sparkplug::evidence::pc::spSceneGraphOptimizerObservedPrefixLayout,
                temporaryBegin) == 0x24
            && offsetof(
                sparkplug::evidence::pc::spSceneGraphOptimizerObservedPrefixLayout,
                detachListHead) == 0x34,
        "scene optimizer preserves the observed temporary-container prefix");
    Require(sizeof(
                sparkplug::evidence::pc::spDXSceneGraphOptimizerObservedPrefixLayout)
                == 0x54
            && offsetof(
                sparkplug::evidence::pc::spDXSceneGraphOptimizerObservedPrefixLayout,
                batchListSentinel) == 0x40
            && offsetof(
                sparkplug::evidence::pc::spDXSceneGraphOptimizerObservedPrefixLayout,
                groupMapSentinel) == 0x4C,
        "DX scene optimizer preserves its observed list/map container prefix");
    Require(sizeof(sparkplug::evidence::pc::spModelLayout) == 0x60
            && sizeof(sparkplug::evidence::ps2::spModelLayout) == 0x58
            && spModel::ClassID == sparkplug::evidence::pc::spModelClassID
            && spModel::ClassID == sparkplug::evidence::ps2::spModelClassID,
        "model adds one relationship and one word after each native base");
    Require(sizeof(sparkplug::evidence::ps2::spEngineCoreLayout) == 0x150,
        "PS2 allocation size is preserved in evidence");
    Require(sizeof(sparkplug::evidence::pc::spFontManagerLayout) == 0x3C
            && sizeof(sparkplug::evidence::ps2::spFontManagerLayout) == 0x38,
        "font-manager layouts preserve the four-byte platform container shift");
    Require(offsetof(sparkplug::evidence::pc::spFontManagerLayout,
                primaryMaterial) == 0x34
            && offsetof(sparkplug::evidence::ps2::spFontManagerLayout,
                primaryMaterial) == 0x30,
        "native default-font references follow their platform container ABIs");
    Require(spFontManager::ClassID
                == sparkplug::evidence::pc::spFontManagerClassID
            && spFontManager::ClassID
                == sparkplug::evidence::ps2::spFontManagerClassID
            && spPS2FontManager::ClassID
                == sparkplug::evidence::ps2::spPS2FontManagerClassID,
        "portable and native common/PS2 font-manager IDs agree");
#if defined(_WIN32)
    Require(spPCFontManager::ClassID
            == sparkplug::evidence::pc::spPCFontManagerClassID,
        "portable PC font-manager preserves its native class ID");
#endif
    Require(sizeof(sparkplug::evidence::pc::spDXInputManagerObservedLayout)
                == 0x50
            && offsetof(sparkplug::evidence::pc::spDXInputManagerObservedLayout,
                controllers) == 0x40,
        "PC DirectInput observed extent includes four controller pointers");
    Require(sizeof(sparkplug::evidence::ps2::spPS2InputManagerLayout) == 0x48
            && offsetof(sparkplug::evidence::ps2::spPS2InputManagerLayout,
                controllers) == 0x20,
        "PS2 input-manager exact layout begins with two controller pointers");
    Require(spInputManager::ClassID
                == sparkplug::evidence::pc::spInputManagerClassID
            && spInputManager::ClassID
                == sparkplug::evidence::ps2::spInputManagerClassID
            && spPS2InputManager::ClassID
                == sparkplug::evidence::ps2::spPS2InputManagerClassID,
        "portable and native common/PS2 input-manager IDs agree");

    Require(spMaterial::ClassID
                == sparkplug::evidence::pc::spMaterialClassID
            && spMaterial::ClassID
                == sparkplug::evidence::ps2::spMaterialClassID
            && spMaterialData::ClassID
                == sparkplug::evidence::pc::spMaterialDataClassID
            && spMaterialData::ClassID
                == sparkplug::evidence::ps2::spMaterialDataClassID
            && spFog::ClassID == sparkplug::evidence::pc::spFogClassID
            && spFog::ClassID == sparkplug::evidence::ps2::spFogClassID,
        "material and fog class identities agree across native builds");
    Require(sizeof(sparkplug::evidence::pc::spMaterialObservedLayout) == 0x78
            && sizeof(sparkplug::evidence::ps2::spMaterialLayout) == 0x80
            && sizeof(
                sparkplug::evidence::pc::spMaterialDataObservedLayout) == 0xBC
            && sizeof(sparkplug::evidence::ps2::spMaterialDataLayout) == 0xD0
            && sizeof(sparkplug::evidence::pc::spFogObservedLayout) == 0x28
            && sizeof(sparkplug::evidence::ps2::spFogLayout) == 0x28,
        "material/fog evidence preserves observed PC and exact PS2 extents");
    Require(spMaterialPassLayer::ClassID
                == sparkplug::evidence::pc::spMaterialPassLayerClassID
            && spMaterialPassLayer::ClassID
                == sparkplug::evidence::ps2::spMaterialPassLayerClassID
            && spMaterialTextureLayer::ClassID
                == sparkplug::evidence::pc::spMaterialTextureLayerClassID
            && spMaterialTextureLayer::ClassID
                == sparkplug::evidence::ps2::spMaterialTextureLayerClassID
            && spStdLayer::ClassID
                == sparkplug::evidence::pc::spStdLayerClassID
            && spStdLayer::ClassID
                == sparkplug::evidence::ps2::spStdLayerClassID,
        "material pass/texture/standard layer identities agree across builds");
    Require(sizeof(
                sparkplug::evidence::pc::spMaterialPassLayerObservedLayout)
                == 0x38
            && sizeof(sparkplug::evidence::ps2::spMaterialPassLayerLayout)
                == 0x38
            && sizeof(sparkplug::evidence::pc::
                spMaterialTextureLayerObservedLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::spMaterialTextureLayerLayout)
                == 0x14
            && sizeof(sparkplug::evidence::pc::spStdLayerObservedLayout)
                == 0x14
            && sizeof(sparkplug::evidence::ps2::spStdLayerLayout) == 0x14,
        "material layer evidence preserves shared outer object extents");
    {
        const auto& passRecord = spMaterialPassLayer::StaticRTTI();
        const auto& textureLayerRecord = spMaterialTextureLayer::StaticRTTI();
        const auto& stdRecord = spStdLayer::StaticRTTI();
        auto passObject = spRTTIManager::Instance().Create(
            spMaterialPassLayer::ClassID);
        auto textureLayerObject = spRTTIManager::Instance().Create(
            spMaterialTextureLayer::ClassID);
        auto stdObject = spRTTIManager::Instance().Create(spStdLayer::ClassID);
        auto* const pass = dynamic_cast<spMaterialPassLayer*>(passObject.get());
        auto* const textureLayer = dynamic_cast<spMaterialTextureLayer*>(
            textureLayerObject.get());
        auto* const stdLayer = dynamic_cast<spStdLayer*>(stdObject.get());
        Require(passRecord.baseClassID == spBaseObject::ClassID
                && textureLayerRecord.baseClassID == spBaseObject::ClassID
                && stdRecord.baseClassID == spMaterialTextureLayer::ClassID
                && pass != nullptr && textureLayer != nullptr && stdLayer != nullptr
                && pass->GetFinalBlendOperationForAnalysis() == 0
                && pass->GetLayerCountForAnalysis() == 0
                && textureLayer->GetMaterialTextureForAnalysis() == nullptr
                && stdLayer->GetMaterialTextureForAnalysis() != nullptr,
            "layer factories preserve native inheritance and default payloads");

        auto layer = std::make_unique<spStdLayer>();
        auto* const originalLayer=layer.get();
        layer->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(3, 7);
        pass->SetFinalBlendOperationForAnalysis(2);
        Require(pass->SetLayerForAnalysis(0, std::move(layer))
                && !pass->SetLayerForAnalysis(
                    spMaterialPassLayer::MaximumLayerCount, std::make_unique<spStdLayer>())
                && pass->GetLayerCountForAnalysis() == 1,
            "material pass keeps the native eight-slot bounded relationship array");
        auto cloneBase = pass->Clone();
        auto* const clone = dynamic_cast<spMaterialPassLayer*>(cloneBase.get());
        auto* const clonedStd = clone != nullptr
            ? dynamic_cast<spStdLayer*>(clone->GetLayerForAnalysis(0).get()) : nullptr;
        Require(clone != nullptr
                && clone->GetFinalBlendOperationForAnalysis() == 2
                && clone->GetLayerCountForAnalysis() == 1
                && clonedStd != nullptr
                && clonedStd != originalLayer
                && clonedStd->GetMaterialTextureForAnalysis() != nullptr
                && clonedStd->GetMaterialTextureForAnalysis().get()
                    != originalLayer->GetMaterialTextureForAnalysis().get()
                && clonedStd->GetMaterialTextureForAnalysis()
                    ->GetTextureStatesForAnalysis()[3] == 7,
            "material pass clone deep-clones layers and their material texture");
    }
    {
        const auto& materialRecord = spMaterial::StaticRTTI();
        const auto& dataRecord = spMaterialData::StaticRTTI();
        auto object = spRTTIManager::Instance().Create(spMaterialData::ClassID);
        auto* const material = dynamic_cast<spMaterialData*>(object.get());
        const spMaterial::RenderStates expectedStates{
            0u, 0u, 1u, 2u, 1u, 1u, 3u, 0u, 4u, 1u, 6u};
        const spMaterialData::ColorRGBA white{1.0F, 1.0F, 1.0F, 1.0F};
        const spMaterialData::ColorRGBA black{0.0F, 0.0F, 0.0F, 1.0F};
        Require(materialRecord.factory == nullptr
                && dataRecord.factory != nullptr
                && dataRecord.baseClassID == spMaterial::ClassID
                && material != nullptr
                && material->GetRenderStatesForAnalysis() == expectedStates
                && material->GetPassCountForAnalysis() == 0
                && !material->UsesVertexAlphaForAnalysis()
                && !material->GetRenderOverrideFlagForAnalysis()
                && material->GetMaterialColorControllerForAnalysis() == nullptr
                && material->GetDiffuseColorForAnalysis() == white
                && material->GetAmbientColorForAnalysis() == black
                && material->GetSpecularColorForAnalysis() == white
                && material->GetEmissiveColorForAnalysis() == black
                && material->GetSpecularPowerForAnalysis() == 0.0F,
            "material-data factory preserves native state and color defaults");
        const spMaterialData::ColorRGBA changed{0.1F, 0.2F, 0.3F, 0.4F};
        material->SetDiffuseColorForAnalysis(changed);
        Require(dynamic_cast<spNamedObject*>(object.get()) != nullptr
                && !material->IsKindOf(spNamedObject::ClassID)
                && material->GetName() == nullptr,
            "material physical name does not change its engine RTTI ancestry");
        material->SetName("material-prefix");
        material->SetUsesVertexAlphaForAnalysis(true);
        Require(material->SetRenderStateForAnalysis(3, 9)
                && !material->SetRenderStateForAnalysis(
                    spMaterial::RenderStateCount, 9),
            "material analysis facade bounds-checks native render-state slots");
        auto cloneBase = material->Clone();
        auto* const clone = dynamic_cast<spMaterialData*>(cloneBase.get());
        Require(clone != nullptr
                && clone->GetDiffuseColorForAnalysis() == white
                && clone->GetRenderStatesForAnalysis() == expectedStates
                && !clone->UsesVertexAlphaForAnalysis()
                && clone->GetName() == nullptr,
            "material-data clone preserves the native blank-state copy stub");
    }
    {
        const auto& record = spPS2Material::StaticRTTI();
        auto object = spRTTIManager::Instance().Create(spPS2Material::ClassID);
        auto* const material = dynamic_cast<spPS2Material*>(object.get());
        const spPS2Material::ColorRGBA white{1.0F, 1.0F, 1.0F, 1.0F};
        const spPS2Material::ColorRGBA black{0.0F, 0.0F, 0.0F, 1.0F};
        const spPS2Material::ColorRGBA changed{0.2F, 0.3F, 0.4F, 0.5F};
        Require(record.factory != nullptr
                && record.baseClassID == spMaterial::ClassID
                && material != nullptr
                && material->GetDiffuseColorForAnalysis() == white
                && material->GetAmbientColorForAnalysis() == black
                && material->GetSpecularColorForAnalysis() == white
                && material->GetEmissiveColorForAnalysis() == black,
            "PS2 material factory preserves the four native color defaults");
        material->SetDiffuseColorForAnalysis(changed);
        material->SetSpecularPowerForAnalysis(13.0F);
        material->SetUsesVertexAlphaForAnalysis(true);
        Require(material->SetRenderStateForAnalysis(2, 9),
            "PS2 material accepts common material state");
        auto cloneBase = material->Clone();
        auto* const clone = dynamic_cast<spPS2Material*>(cloneBase.get());
        Require(clone != nullptr
                && clone->GetDiffuseColorForAnalysis() == changed
                && clone->GetSpecularPowerForAnalysis() == 13.0F
                && clone->UsesVertexAlphaForAnalysis()
                && clone->GetRenderStateForAnalysis(2) == 9,
            "PS2 material clone copies common state and the complete color tail");
        Require(sizeof(sparkplug::evidence::ps2::spPS2MaterialLayout) == 0xD0
                && spPS2Material::ClassID
                    == sparkplug::evidence::ps2::spPS2MaterialClassID,
            "PS2 material portable identity matches the exact native ABI");
    }
    {
        const auto& record = spFog::StaticRTTI();
        auto object = spRTTIManager::Instance().Create(spFog::ClassID);
        auto* const fog = dynamic_cast<spFog*>(object.get());
        Require(record.factory != nullptr
                && record.baseClassID == spBaseObject::ClassID
                && fog != nullptr
                && fog->GetTypeForAnalysis() == spFog::Type::Disabled
                && fog->GetColorARGBForAnalysis() == 0xFF000000
                && fog->GetStartForAnalysis() == 0.0F
                && fog->GetEndForAnalysis() == 1.0F
                && fog->GetDensityForAnalysis() == 1.0F,
            "fog factory preserves the complete native five-field defaults");
        fog->SetTypeForAnalysis(spFog::Type::Linear);
        fog->SetEndForAnalysis(50.0F);
        Require(dynamic_cast<spNamedObject*>(object.get()) != nullptr
                && !fog->IsKindOf(spNamedObject::ClassID)
                && fog->GetName() == nullptr,
            "Fog physical named prefix does not change the engine RTTI parent");
        fog->SetName("fog-prefix");
        auto cloneBase = fog->Clone();
        auto* const clone = dynamic_cast<spFog*>(cloneBase.get());
        Require(clone != nullptr
                && clone->GetTypeForAnalysis() == spFog::Type::Disabled
                && clone->GetEndForAnalysis() == 1.0F
                && clone->GetName() == fog->GetName(),
            "fog clone shares the physical name but retains default fog payload");
    }

    Require(spRenderer::ClassID
                == sparkplug::evidence::pc::spRendererClassID
            && spRenderer::ClassID
                == sparkplug::evidence::ps2::spRendererClassID
            && spRenderer::PlatformInterfaceSlotCount == 29
            && spRenderer::StaticRTTI().factory == nullptr
            && spDXRenderer::StaticRTTI().factory == nullptr,
        "common and DX renderers preserve abstract native identity");
    Require(
        spRenderer::GetPlatformInterfaceSlotForAnalysis(
            spRendererPlatformForAnalysis::PC,
            spRendererPlatformOperationForAnalysis::SubmitMesh) == 9
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::SubmitMesh) == 9,
        "model mesh submission preserves the shared renderer-interface ordinal");
    Require(
        spRenderer::GetPlatformInterfaceSlotForAnalysis(
            spRendererPlatformForAnalysis::PC,
            spRendererPlatformOperationForAnalysis::SetTextureTransform) == 23
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::SetTextureTransform) == 23,
        "material UV transforms preserve the shared renderer-interface ordinal");
    Require(
        spRenderer::GetPlatformInterfaceSlotForAnalysis(
            spRendererPlatformForAnalysis::PC,
            spRendererPlatformOperationForAnalysis::BindRenderTarget) == 1
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::BindRenderTarget) == 0
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PC,
                spRendererPlatformOperationForAnalysis::BindCubeRenderTarget) == 0
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::BindCubeRenderTarget) == 1,
        "render-target slots preserve their confirmed PC/PS2 reversal");
    Require(
        spRenderer::GetPlatformInterfaceSlotForAnalysis(
            spRendererPlatformForAnalysis::PC,
            spRendererPlatformOperationForAnalysis::SetProjectionMatrix) == 12
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::SetViewMatrix) == 13
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PC,
                spRendererPlatformOperationForAnalysis::SetWorldMatrix) == 14
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::SetViewport) == 22,
        "camera operations preserve the confirmed platform-interface ordinals");
    Require(
        spRenderer::GetPlatformInterfaceSlotForAnalysis(
            spRendererPlatformForAnalysis::PC,
            spRendererPlatformOperationForAnalysis::SetFog) == 26
            && spRenderer::GetPlatformInterfaceSlotForAnalysis(
                spRendererPlatformForAnalysis::PS2,
                spRendererPlatformOperationForAnalysis::SetFog) == 26,
        "renderable fog selection preserves callable slot 26 on both platforms");
    {
        const auto& ps2RendererRecord = spPS2Renderer::StaticRTTI();
        auto rendererObject = spRTTIManager::Instance().Create(
            spPS2Renderer::ClassID);
        auto* const renderer =
            dynamic_cast<spPS2Renderer*>(rendererObject.get());
        Require(ps2RendererRecord.factory != nullptr
                && renderer != nullptr
                && spRenderer::GetInstance() == renderer
                && renderer->GetRenderStateCacheCountForAnalysis() == 12
                && renderer->GetTextureStateCacheCountForAnalysis() == 96
                && renderer->GetRenderStateCacheForAnalysis(11)
                    == std::numeric_limits<std::uint32_t>::max()
                && renderer->GetTextureStateCacheForAnalysis(95)
                    == std::numeric_limits<std::uint32_t>::max()
                && renderer->GetTextureStateCacheForAnalysis(96) == 0,
            "PS2 renderer factory preserves native state-cache dimensions");
    }
    Require(spRenderer::GetInstance() == nullptr,
        "renderer destruction clears the process singleton");
#if defined(_WIN32)
    Require(spDXInputManager::ClassID
            == sparkplug::evidence::pc::spDXInputManagerClassID,
        "portable DirectInput manager preserves its native class ID");
    {
        const auto& pcRendererRecord = spPCRenderer::StaticRTTI();
        auto rendererObject = spRTTIManager::Instance().Create(
            spPCRenderer::ClassID);
        auto* const renderer =
            dynamic_cast<spPCRenderer*>(rendererObject.get());
        Require(pcRendererRecord.factory != nullptr
                && renderer != nullptr
                && spRenderer::GetInstance() == renderer
                && renderer->GetRenderStateCacheCountForAnalysis() == 12
                && renderer->GetTextureStateCacheCountForAnalysis() == 72
                && renderer->GetRenderStateCacheForAnalysis(0)
                    == std::numeric_limits<std::uint32_t>::max()
                && renderer->GetTextureStateCacheForAnalysis(71)
                    == std::numeric_limits<std::uint32_t>::max(),
            "PC renderer factory preserves native state-cache dimensions");
    }
#endif
    Require(sizeof(sparkplug::evidence::pc::spRendererLayout) == 0xCA08
            && offsetof(sparkplug::evidence::pc::spRendererLayout,
                renderQueueEnabled) == 0xC050
            && offsetof(sparkplug::evidence::pc::spRendererLayout,
                renderStateCache) == 0xC868
            && sizeof(sparkplug::evidence::pc::spPCRendererLayout) == 0xF368,
        "PC renderer lifetime and factory boundaries preserve exact extents");
    Require(sizeof(sparkplug::evidence::ps2::spRendererLayout) == 0xCBB0
            && offsetof(sparkplug::evidence::ps2::spRendererLayout,
                renderQueueEnabled) == 0xC050
            && offsetof(sparkplug::evidence::ps2::spRendererLayout,
                textureStateCache) == 0xCA00
            && sizeof(sparkplug::evidence::ps2::spPS2RendererLayout)
                == 0x19D00,
        "PS2 renderer constructor boundaries preserve exact extents");

    Require(spMaterialTexture::ClassID
                == sparkplug::evidence::pc::spMaterialTextureClassID
            && spMaterialTexture::ClassID
                == sparkplug::evidence::ps2::spMaterialTextureClassID
            && spMaterialRenderTargetTexture::ClassID
                == sparkplug::evidence::ps2::
                    spMaterialRenderTargetTextureClassID
            && spMaterialRenderTargetTexture::StaticRTTI().factory == nullptr
            && spMaterialCameraViewTexture::StaticRTTI().factory != nullptr
            && spMaterialCubeMapTexture::StaticRTTI().factory != nullptr,
        "material target-texture hierarchy preserves native abstract/concrete RTTI");
    Require(sizeof(sparkplug::evidence::pc::spMaterialTextureObservedLayout)
                == 0x68
            && sizeof(sparkplug::evidence::pc::
                spMaterialRenderTargetTextureObservedLayout) == 0x8C
            && sizeof(sparkplug::evidence::pc::
                spMaterialCubeMapTextureLayout) == 0x9C
            && offsetof(sparkplug::evidence::pc::
                spMaterialRenderTargetTextureObservedLayout,
                maxRecursionLevel) == 0x6C,
        "PC material-target observed extents preserve the nine-state ABI shift");
    Require(sizeof(sparkplug::evidence::ps2::spMaterialTextureLayout) == 0x80
            && sizeof(sparkplug::evidence::ps2::
                spMaterialRenderTargetTextureLayout) == 0xA0
            && sizeof(sparkplug::evidence::ps2::
                spMaterialCameraViewTextureLayout) == 0xB0
            && offsetof(sparkplug::evidence::ps2::
                spMaterialCubeMapTextureLayout, facesPerTick) == 0xA8,
        "PS2 material-target allocations preserve twelve states and leaf tails");

    Require(spRenderTarget::ClassID
                == sparkplug::evidence::pc::spRenderTargetClassID
            && spRenderTarget::ClassID
                == sparkplug::evidence::ps2::spRenderTargetClassID
            && spCubeRenderTarget::ClassID
                == sparkplug::evidence::ps2::spCubeRenderTargetClassID
            && spRenderTarget::StaticRTTI().factory == nullptr
            && spCubeRenderTarget::StaticRTTI().factory == nullptr,
        "common render-target identities preserve their abstract registrations");
    Require(sizeof(sparkplug::evidence::pc::spRenderTargetObservedPrefixLayout)
                == 0x28
            && sizeof(sparkplug::evidence::pc::spDXRenderTargetObservedPrefixLayout)
                == 0x2C
            && sizeof(sparkplug::evidence::pc::spDXCubeRenderTargetObservedPrefixLayout)
                == 0x44
            && offsetof(
                sparkplug::evidence::pc::spDXCubeRenderTargetObservedPrefixLayout,
                faceSurfaces) == 0x2C,
        "PC target prefixes preserve the ordinary surface and six cube faces");
    Require(sizeof(sparkplug::evidence::ps2::spRenderTargetLayout) == 0x28
            && sizeof(sparkplug::evidence::ps2::spPS2RenderTargetLayout) == 0x30
            && sizeof(sparkplug::evidence::ps2::spPS2CubeRenderTargetLayout) == 0x2C
            && offsetof(sparkplug::evidence::ps2::spPS2RenderTargetLayout,
                active) == 0x2C,
        "PS2 target allocations and backend-active byte are exact");
    Require(sizeof(sparkplug::evidence::pc::spRenderTargetManagerObservedLayout)
                == 0x44
            && sizeof(sparkplug::evidence::ps2::spRenderTargetManagerLayout)
                == 0x44
            && offsetof(
                sparkplug::evidence::ps2::spRenderTargetManagerLayout,
                layerTargets) == 0x30,
        "target managers preserve three distinct native target lists");
    Require(spDXRenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format5)
            && !spDXRenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format2)
            && spDXRenderTarget::ToD3DFormatForAnalysis(
                eTBPixelFormat::Format5) == 0x19
            && spDXCubeRenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format4)
            && !spDXCubeRenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format5),
        "DX ordinary and cube targets preserve their distinct format matrices");
    Require(spPS2RenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format5)
            && !spPS2RenderTarget::IsSupportedFormatForAnalysis(
                eTBPixelFormat::Format4)
            && spPS2RenderTarget::ToPS2FormatForAnalysis(
                eTBPixelFormat::Format5) == 10,
        "PS2 target preserves its three-format mapping");
    {
        const auto& managerRecord = spPS2RenderTargetManager::StaticRTTI();
        const auto& targetRecord = spPS2RenderTarget::StaticRTTI();
        const auto& cubeRecord = spPS2CubeRenderTarget::StaticRTTI();
        auto managerObject = spRTTIManager::Instance().Create(
            spPS2RenderTargetManager::ClassID);
        auto targetObject = spRTTIManager::Instance().Create(
            spPS2RenderTarget::ClassID);
        auto cubeObject = spRTTIManager::Instance().Create(
            spPS2CubeRenderTarget::ClassID);
        auto* const manager = dynamic_cast<spPS2RenderTargetManager*>(
            managerObject.get());
        auto* const target = dynamic_cast<spPS2RenderTarget*>(
            targetObject.get());
        auto* const cube = dynamic_cast<spPS2CubeRenderTarget*>(cubeObject.get());
        TextureProbe fallbackTexture;
        TextureProbe renderedTexture;
        spMaterialCameraViewTexture cameraTexture;
        spMaterialCubeMapTexture cubeTexture;
        Require(managerRecord.factory != nullptr
                && targetRecord.factory != nullptr
                && cubeRecord.factory != nullptr
                && manager != nullptr && target != nullptr && cube != nullptr
                && target->GetWidthForAnalysis() == 256
                && target->Init(320, 240, eTBPixelFormat::Format5)
                && target->HasBackendTargetForAnalysis()
                && cube->Init(64, 64, eTBPixelFormat::Format0)
                && !cube->HasBackendTargetForAnalysis()
                && manager->RegisterTargetForAnalysis(*target)
                && manager->RegisterTargetForAnalysis(*cube)
                && manager->GetActiveOrdinaryTargetCountForAnalysis() == 1
                && manager->GetActiveCubeTargetCountForAnalysis() == 1,
            "PS2 target factories preserve defaults, lifecycle and list classes");
        target->SetBackingTextureForAnalysis(&renderedTexture);
        cameraTexture.SetFallBackTextureForAnalysis(&fallbackTexture);
        Require(cameraTexture.SetRenderTargetForAnalysis(0, target)
                && cameraTexture.GetTextureForAnalysis() == &renderedTexture
                && manager->SetCurrentTargetIndexForAnalysis(1)
                && cameraTexture.GetTextureForAnalysis() == &fallbackTexture
                && manager->SetCurrentTargetIndexForAnalysis(0)
                && manager->GetActiveLayerTargetCountForAnalysis() == 2,
            "material camera/cube textures register in the third manager list and select fallback by sentinel slot");
        cameraTexture.SetCameraNameForAnalysis("PortalCamera");
        cameraTexture.SetMaxRecursionLevelForAnalysis(1);
        Require(cameraTexture.TryEnterRenderForAnalysis()
                && !cameraTexture.TryEnterRenderForAnalysis(),
            "material target texture enforces its native recursion limit");
        cameraTexture.LeaveRenderForAnalysis();
        cubeTexture.SetNumFacesToRenderPerTickForAnalysis(0);
        Require(cubeTexture.GetNumFacesToRenderPerTickForAnalysis() == 1,
            "cube-map reader normalizes zero faces per tick to one");
        cubeTexture.SetNumFacesToRenderPerTickForAnalysis(9);
        Require(cubeTexture.GetNumFacesToRenderPerTickForAnalysis() == 6,
            "cube-map reader clamps excessive faces per tick to six");
        auto cameraCloneBase = cameraTexture.Clone();
        auto* const cameraClone =
            dynamic_cast<spMaterialCameraViewTexture*>(cameraCloneBase.get());
        Require(cameraClone != nullptr
                && cameraClone->GetCameraNameForAnalysis() != nullptr
                && std::strcmp(cameraClone->GetCameraNameForAnalysis(),
                    "PortalCamera") == 0
                && cameraClone->GetCurrentRecursionLevelForAnalysis() == 0,
            "camera-view clone copies configuration but rebuilds runtime recursion state");
        manager->ReleaseTargetsForDeviceReset();
        Require(!target->HasBackendTargetForAnalysis()
                && manager->ReinitTargetsForDeviceReset()
                && target->HasBackendTargetForAnalysis()
                && !cube->HasBackendTargetForAnalysis(),
            "target manager performs release then platform reinitialization");
        Require(manager->DeactivateTargetForAnalysis(*cube)
                && manager->GetCubeTargetCountForAnalysis() == 1
                && manager->GetActiveCubeTargetCountForAnalysis() == 0,
            "native-style target deactivation retains an inactive list node");
    }
    Require(spRenderTargetManager::GetInstance() == nullptr,
        "PS2 target-manager destruction clears its singleton");
#if defined(_WIN32)
    {
        const auto& managerRecord = spPCRenderTargetManager::StaticRTTI();
        const auto& targetRecord = spPCRenderTarget::StaticRTTI();
        const auto& cubeRecord = spDXCubeRenderTarget::StaticRTTI();
        auto managerObject = spRTTIManager::Instance().Create(
            spPCRenderTargetManager::ClassID);
        auto targetObject = spRTTIManager::Instance().Create(
            spPCRenderTarget::ClassID);
        auto cubeObject = spRTTIManager::Instance().Create(
            spDXCubeRenderTarget::ClassID);
        auto* const manager = dynamic_cast<spPCRenderTargetManager*>(
            managerObject.get());
        auto* const target = dynamic_cast<spPCRenderTarget*>(targetObject.get());
        auto* const cube = dynamic_cast<spDXCubeRenderTarget*>(cubeObject.get());
        Require(managerRecord.factory != nullptr
                && targetRecord.factory != nullptr
                && cubeRecord.factory != nullptr
                && manager != nullptr && target != nullptr && cube != nullptr
                && target->Init(800, 600, eTBPixelFormat::Format4)
                && cube->Init(128, 128, eTBPixelFormat::Format3)
                && cube->GetReadyFaceCountForAnalysis() == 6
                && manager->RegisterTargetForAnalysis(*target)
                && manager->RegisterTargetForAnalysis(*cube),
            "PC target factories and six-face cube lifecycle are usable");
        manager->ReleaseTargetsForDeviceReset();
        Require(cube->GetReadyFaceCountForAnalysis() == 0
                && manager->ReinitTargetsForDeviceReset()
                && cube->GetReadyFaceCountForAnalysis() == 6,
            "PC reset cycle releases and rebuilds every cube face");
    }
#endif
    Require(offsetof(sparkplug::evidence::pc::spEngineCoreLayout,
                firstFrameCallback) == 0x30
            && offsetof(sparkplug::evidence::ps2::spEngineCoreLayout,
                firstFrameCallback) == 0x2C,
        "platform container ABI shifts the callbacks by four bytes");
    Require(sparkplug::evidence::pc::spEngineCoreLocalSlotCount == 18
            && sparkplug::evidence::ps2::spEngineCoreLocalSlotCount == 18,
        "both binaries expose eighteen engine-core slots");
    Require(sparkplug::evidence::pc::spEngineCoreLocalTargets[11]
                == sparkplug::evidence::pc::spEngineCoreLocalTargets[16],
        "PC keeps two slots bound to the same true-returning stub");
    Require(sparkplug::evidence::ps2::spEngineCoreLocalTargets[11] == 0x00132A70
            && sparkplug::evidence::ps2::spEngineCoreLocalTargets[16]
                == 0x00132A60,
        "PS2 emits distinct true-returning stubs for the two slots");

    // Keep the process-lifetime fallback test last: native sub_00208E70 also
    // leaves the lazily allocated manager alive after returning.
    Require(spSerializerManager::GetInstance() == nullptr,
        "serializer-hook fallback test begins without a manager");
    {
        spPS2SerializerHook hook;
        spMemoryStream ignoredHookStream;
        hook.vfunc_24(nullptr, ignoredHookStream);
        Require(spSerializerManager::GetInstance() != nullptr,
            "PS2 serializer hook lazily installs a manager when none exists");
    }

    std::cout << "spEngineCore reconstruction tests passed\n";
    return EXIT_SUCCESS;
}
