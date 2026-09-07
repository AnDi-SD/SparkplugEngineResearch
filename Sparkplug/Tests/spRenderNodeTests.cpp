#include "Analysis/PC/spRenderNodeMath.h"
#include "Analysis/PC/SparkplugAbi.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spMesh.h"
#include "Code/Sparkplug/spLightData.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace math = sparkplug::evidence::pc::render_node_math;
using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(label);
    }
    template <std::size_t N> void Read(std::array<float, N>& values)
    {
        for (auto& value : values)
            if (!(std::cin >> value))
                throw std::runtime_error("incomplete finite batch input");
    }
    template <std::size_t N> void Write(const std::array<float, N>& values, bool& first)
    {
        for (auto value : values)
        {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << value;
        }
    }
    class LiteralMesh : public spMesh
    {
      public:
        explicit LiteralMesh(const BoundingSphere& sphere)
        {
            SetSphere(sphere);
        }
        void SetSphere(const BoundingSphere& sphere)
        {
            SetBoundingSphereForAnalysis(sphere);
        }
    };
    std::shared_ptr<spModel> ModelWith(const std::shared_ptr<LiteralMesh>& mesh)
    {
        auto model = std::make_shared<spModel>();
        model->SetBaseMeshForAnalysis(mesh);
        return model;
    }
    void WriteSpheres(const spRenderNode& node, bool& first)
    {
        Write(node.GetLocalBoundingSphereForAnalysis(), first);
        Write(node.GetWorldBoundingSphereForAnalysis(), first);
    }
    void WriteDirty(const spRenderNode& node, bool& first)
    {
        Write(std::array<float, 1>{node.AreRenderMatricesDirtyForAnalysis() ? 1.F : 0.F}, first);
    }
    int RuntimeBatch()
    {
        math::Sphere firstSphere{}, secondSphere{}, changedSphere{};
        std::cout << std::setprecision(9);
        while (std::cin >> firstSphere[0])
        {
            for (std::size_t i = 1; i < 4; ++i)
                if (!(std::cin >> firstSphere[i]))
                    throw std::runtime_error("incomplete sphere");
            Read(secondSphere);
            Read(changedSphere);
            math::Vector3 position{}, scale{};
            math::Matrix3 orientation{};
            Read(position);
            Read(scale);
            Read(orientation);
            auto mesh = std::make_shared<LiteralMesh>(firstSphere);
            auto second = std::make_shared<LiteralMesh>(secondSphere);
            spRenderNode node;
            if (!node.AttachRenderableForAnalysis(ModelWith(mesh)) ||
                !node.AttachRenderableForAnalysis(ModelWith(second)))
                throw std::runtime_error("append failed");
            bool first = true;
            std::cout << '[';
            WriteSpheres(node, first);
            node.SetPositionForAnalysis(position);
            node.SetScaleForAnalysis(scale);
            node.SetOrientationForAnalysis(orientation);
            if (!node.UpdateWorldForAnalysis(1))
                throw std::runtime_error("world failed");
            WriteSpheres(node, first);
            Write(node.GetReciprocalWorldScaleForAnalysis(), first);
            WriteDirty(node, first);
            node.UpdateRenderMatricesForAnalysis();
            Write(node.GetCachedRenderMatrixForAnalysis(), first);
            Write(node.GetCachedRenderInverseForAnalysis(), first);
            WriteDirty(node, first);
            mesh->SetSphere(changedSphere);
            node.MarkRenderableBoundsDirtyForAnalysis();
            if (!node.UpdateWorldForAnalysis())
                throw std::runtime_error("bounds update failed");
            WriteSpheres(node, first);
            WriteDirty(node, first);
            if (!node.UpdateWorldForAnalysis(1))
                throw std::runtime_error("PRS update failed");
            WriteSpheres(node, first);
            WriteDirty(node, first);
            std::cout << "]\n";
        }
        return 0;
    }
    int Batch()
    {
        char operation;
        std::cout << std::setprecision(9);
        while (std::cin >> operation)
        {
            math::Sphere sphere{};
            Read(sphere);
            bool first = true;
            std::cout << '[';
            if (operation == 'M')
            {
                math::Sphere other{};
                Read(other);
                math::Merge(sphere, other);
                Write(sphere, first);
            }
            else if (operation == 'W')
            {
                math::Vector3 position{}, scale{};
                math::Matrix3 orientation{};
                Read(position);
                Read(scale);
                Read(orientation);
                Write(math::FromPRS(sphere, position, orientation, scale), first);
                Write(sparkplug::evidence::pc::node_math::Affine(position, orientation, scale),
                      first);
                Write(math::InversePRS(position, orientation,
                                       {1 / scale[0], 1 / scale[1], 1 / scale[2]}),
                      first);
            }
            else if (operation == 'B')
            {
                math::Matrix4 matrix{};
                Read(matrix);
                Write(math::FromCachedMatrix(sphere, matrix), first);
            }
            else if (operation == 'C')
            {
                math::Planes planes{};
                for (auto& plane : planes)
                    Read(plane);
                unsigned bypass;
                if (!(std::cin >> bypass))
                    throw std::runtime_error("missing bypass");
                std::cout << (math::Culled(sphere, planes, bypass != 0) ? 1 : 0);
            }
            else
                throw std::runtime_error("unknown batch operation");
            std::cout << "]\n";
        }
        return 0;
    }
    void UnitTests()
    {
        Check(sizeof(sparkplug::evidence::pc::spRenderNodeLayout) == 0x1d4,
              "complete PC render-node allocation");
        math::Sphere sphere{0, 0, 0, 2};
        math::Merge(sphere, {2, 0, 0, 2});
        Check(sphere == math::Sphere{1, 0, 0, 3}, "overlapping sphere union");
        math::Merge(sphere, {2, 0, 0, 0});
        Check(sphere == math::Sphere{1, 0, 0, 3}, "tiny sphere ignored");
        math::Merge(sphere, {1, 0, 0, 10});
        Check(sphere == math::Sphere{1, 0, 0, 10}, "containing sphere replaces target");
        math::Planes planes{};
        planes[0] = {1, 0, 0, 11};
        Check(!math::Culled(sphere, planes, false), "tangent sphere retained");
        planes[0][3] = 12;
        Check(math::Culled(sphere, planes, false), "outside sphere rejected");
        Check(!math::Culled(sphere, planes, true), "explicit bypass");
        sphere[3] = .001F;
        Check(!math::Culled(sphere, planes, false), "threshold radius bypasses cull");
        sphere = {1, 2, 3, 2};
        const math::Vector3 position{10, 20, 30}, scale{2, -3, 4};
        const math::Matrix3 turn{0, 1, 0, -1, 0, 0, 0, 0, 1};
        const auto world = sparkplug::evidence::pc::node_math::Affine(position, turn, scale);
        Check(math::FromPRS(sphere, position, turn, scale) == math::Sphere{16, 22, 42, 8},
              "PRS sphere maximum absolute scale");
        Check(math::FromCachedMatrix(sphere, world) == math::Sphere{16, 22, 42, 4},
              "cached helper measures X radius, not maximum scale");
        const auto inverse = math::InversePRS(position, turn, {.5F, -1 / 3.F, .25F});
        bool identity = true;
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
            {
                float value = 0;
                for (std::size_t k = 0; k < 4; ++k)
                    value += world[r * 4 + k] * inverse[k * 4 + c];
                identity &= std::fabs(value - (r == c ? 1.F : 0.F)) < .0001F;
            }
        Check(identity, "inverse PRS matches signed-scale world matrix");
    }
    void RuntimeUnits()
    {
        spRenderable base;
        Check(base.GetBoundingSphereForAnalysis() == math::Sphere{}, "base virtual sphere is zero");
        auto mesh = std::make_shared<LiteralMesh>(math::Sphere{1, 2, 3, 2});
        auto model = ModelWith(mesh);
        Check(model->GetProjectionGroupForAnalysis() == 3 && !model->HasBoundsForAnalysis() &&
                  &model->GetBoundingSphereForAnalysis() == &mesh->GetBoundingSphereForAnalysis(),
              "PC model default and borrowed sphere getter independent of validity");
        spIndexBuffer indices;
        Check(indices.InitializeForAnalysis(1, spIndexBuffer::eIndexBufferType::Type2) &&
                  indices.SetIndexForAnalysis(0, 0) && indices.SetIndexForAnalysis(1, 1) &&
                  indices.SetIndexForAnalysis(2, 2) &&
                  mesh->ComputeBoundsForAnalysis(indices, {{-1, -2, -3}, {4, 5, 6}, {0, 0, 0}}),
              "literal mesh extent setup through existing bounds pass");
        spRenderable::BoundsPosition minimum{}, maximum{};
        model->GetBoundsForAnalysis(minimum, maximum);
        Check(minimum == math::Vector3{-1, -2, -3} && maximum == math::Vector3{4, 5, 6} &&
                  model->HasBoundsForAnalysis(),
              "model extents directly delegate to mesh");
        model->spRenderable::GetBoundsForAnalysis(minimum, maximum);
        Check(minimum == math::Vector3{-1, 0, 1} && maximum == math::Vector3{3, 4, 5},
              "base extent algorithm dispatches concrete sphere getter");
        auto child = std::make_shared<spRenderNode>();
        Check(child->AttachRenderableForAnalysis(model) &&
                  child->GetLocalBoundingSphereForAnalysis() == math::Sphere{1, 2, 3, 2} &&
                  !child->AreRenderableBoundsDirtyForAnalysis(),
              "append rebuilds cached bounds immediately");
        child->SetScaleForAnalysis({2, -3, 4});
        child->SetOrientationForAnalysis({0, 1, 0, -1, 0, 0, 0, 0, 1});
        spNode parent;
        parent.SetPositionForAnalysis({10, 20, 30});
        Check(parent.AttachChildForAnalysis(child) && parent.UpdateWorldForAnalysis(1) &&
                  child->GetWorldBoundingSphereForAnalysis() == math::Sphere{16, 22, 42, 8} &&
                  child->AreRenderMatricesDirtyForAnalysis(),
              "plain parent dispatches derived world updater through actual virtual method");
        Check(child->GetCachedRenderMatrixForAnalysis() == math::Identity4,
              "world update leaves lazy render matrix untouched");
        child->UpdateRenderMatricesForAnalysis();
        Check(!child->AreRenderMatricesDirtyForAnalysis() &&
                  child->GetCachedRenderMatrixForAnalysis()[12] == 10,
              "explicit matrix preparation consumes dirty state");
        Check(child->AttachRenderableForAnalysis(model) &&
                  child->GetWorldBoundingSphereForAnalysis()[3] == 4,
              "append uses cached X radius even after max-scale world pass");
        auto cloneBase = child->Clone();
        auto* clone = dynamic_cast<spRenderNode*>(cloneBase.get());
        Check(clone && clone->GetRenderableCountForAnalysis() == 2 &&
                  clone->GetRenderableForAnalysis(0) != clone->GetRenderableForAnalysis(1),
              "each PC renderable occurrence cloned separately");
        Check(
            dynamic_cast<spModel*>(clone->GetRenderableForAnalysis(0))->GetBaseMeshForAnalysis() ==
                    mesh &&
                dynamic_cast<spModel*>(clone->GetRenderableForAnalysis(1))
                        ->GetBaseMeshForAnalysis() == mesh,
            "independent model clones retain same base geometry");
        Check(clone->GetWorldBoundingSphereForAnalysis() ==
                      child->GetWorldBoundingSphereForAnalysis() &&
                  clone->GetCachedRenderMatrixForAnalysis() == math::Identity4 &&
                  !clone->AreRenderMatricesDirtyForAnalysis(),
              "clone copies spheres, not source matrix cache");
        mesh->SetSphere({1, 2, 3, 3});
        child->MarkRenderableBoundsDirtyForAnalysis();
        Check(child->UpdateWorldForAnalysis() &&
                  child->GetWorldBoundingSphereForAnalysis()[3] == 6 &&
                  !child->AreRenderableBoundsDirtyForAnalysis(),
              "bounds-only world pass uses cached X scale");
        Check(child->UpdateWorldForAnalysis(1) &&
                  child->GetWorldBoundingSphereForAnalysis()[3] == 12,
              "transform-dirty pass replaces radius with max absolute scale");
        math::Planes planes{};
        planes[0] = {1, 0, 0, 100};
        Check(child->IsCulledForAnalysis(planes), "class cull method uses world sphere");
        child->SetCullBypassForAnalysis(true);
        Check(!child->IsCulledForAnalysis(planes), "class cull bypass");
        spLightManager manager;
        spLightData light;
        light.SetHierarchyActiveForAnalysis(true);
        Check(manager.RegisterLightForAnalysis(light), "explicit scene light list setup");
        child->SetSceneLightManagerForAnalysis(&manager);
        Check(child->UpdateWorldForAnalysis(1) && child->GetLightCacheForAnalysis().GetCount() == 1,
              "dirty world pass rebuilds from explicit scene light dependency");
        light.SetLightEnabledForAnalysis(false);
        Check(child->UpdateWorldForAnalysis() && child->GetLightCacheForAnalysis().GetCount() == 1,
              "clean world pass does not invent a light-cache refresh");
        Check(child->UpdateWorldForAnalysis(1) && child->GetLightCacheForAnalysis().GetCount() == 0,
              "next dirty world pass refreshes light selection");
        child->SetSceneLightManagerForAnalysis(nullptr);
        child->SetBillboardAxisForAnalysis(1);
        child->UpdateRenderMatricesForAnalysis();
        Check(child->UpdateWorldForAnalysis() && (child->GetFlagsForAnalysis() & 1U) &&
                  !child->AreRenderMatricesDirtyForAnalysis(),
              "clean billboard arms next frame only");
        Check(child->UpdateWorldForAnalysis() && child->AreRenderMatricesDirtyForAnalysis(),
              "following billboard pass updates render cache dirty bit");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
            return Batch();
        if (argc == 2 && std::string(argv[1]) == "--runtime-batch")
            return RuntimeBatch();
        UnitTests();
        RuntimeUnits();
        std::cout << "PASS " << checks << '/' << checks << " render-node reconstruction checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
