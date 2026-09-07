#include "Code/Sparkplug/spNodeController.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Require(bool value, const char* text)
    {
        ++checks;
        if (!value)
        {
            std::cerr << "FAIL " << text << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
    template <std::size_t N> bool Near(const std::array<float, N>& a, const std::array<float, N>& b)
    {
        for (std::size_t i = 0; i < N; ++i)
            if (std::fabs(a[i] - b[i]) > 0.0001F)
                return false;
        return true;
    }
    class RotationOnly final : public spTransformEval
    {
        SampleForAnalysis EvaluateForAnalysis(float) override
        {
            SampleForAnalysis sample;
            sample.hasRotation = true;
            sample.rotation = {0, 0, 1, 0};
            return sample;
        }
    };
} // namespace

int main(int argc,char** argv)
{
    namespace math = sparkplug::evidence::pc::node_math;
    if(argc==2&&std::string(argv[1])=="--math-bits")
    {
        char kind;unsigned count=0;
        while(std::cin>>kind)
        {
            Require(++count<=512&&(kind=='v'||kind=='m'),"bounded math input batch");
            std::array<float,18> input{};
            for(unsigned i=0;i<(kind=='v'?12u:18u);++i)
            {std::uint32_t bits;Require(bool(std::cin>>bits),"complete input words");std::memcpy(&input[i],&bits,4);Require(std::isfinite(input[i]),"finite math input");}
            const auto emit=[](const auto& values)
            {
                std::cout<<'[';bool first=true;
                for(float value:values){std::uint32_t bits;std::memcpy(&bits,&value,4);if(!first)std::cout<<',';first=false;std::cout<<bits;}
                std::cout<<"]\n";
            };
            math::Matrix3 a{},b{};
            if(kind=='v')
            {std::copy_n(input.begin()+3,9,b.begin());emit(math::Transform({input[0],input[1],input[2]},b));}
            else
            {std::copy_n(input.begin(),9,a.begin());std::copy_n(input.begin()+9,9,b.begin());emit(math::Multiply(a,b));}
        }
        return 0;
    }
    Require(argc==1,"known NodeWorld command");
    const math::Matrix3 ones{1,1,1,1,1,1,1,1,1};
    Require(math::Transform({100000000,1,-100000000},ones)==math::Vector3{1,1,1},
            "x87 destination rounding retains cancellation residue");
    Require(math::Multiply({100000000,1,-100000000,100000000,1,-100000000,100000000,1,-100000000},ones)==ones,
            "all nine x87 matrix cells retain cancellation residue");
    const spNode::Matrix3 turn{0, 1, 0, -1, 0, 0, 0, 0, 1};
    for (unsigned mask = 0; mask < 8; ++mask)
    {
        auto root = std::make_shared<spNode>();
        auto child = std::make_shared<spNode>();
        root->SetPositionForAnalysis({10, 20, 30});
        root->SetScaleForAnalysis({2, 3, 4});
        root->SetOrientationForAnalysis(turn);
        child->SetPositionForAnalysis({1, 2, 3});
        child->SetScaleForAnalysis({5, 6, 7});
        child->SetOrientationForAnalysis(turn);
        Require(child->UpdateWorldForAnalysis(1), "seed old child cache before attachment");
        Require(root->AttachChildForAnalysis(child), "attach child");
        child->SetInheritanceForAnalysis(mask & 1, mask & 2, mask & 4);
        Require(root->UpdateWorldForAnalysis(1), "update inherited world");
        const spNode::Vector3 position =
            mask & 1 ? (mask & 4 ? spNode::Vector3{4, 22, 42} : spNode::Vector3{8, 21, 33})
                     : spNode::Vector3{1, 2, 3};
        const spNode::Vector3 scale =
            mask & 4 ? spNode::Vector3{10, 18, 28} : spNode::Vector3{5, 6, 7};
        const spNode::Matrix3 rotation =
            mask & 2 ? spNode::Matrix3{-1, 0, 0, 0, -1, 0, 0, 0, 1} : turn;
        Require(Near(child->GetWorldPositionForAnalysis(), position),
                "position inherits or preserves cache");
        Require(Near(child->GetWorldScaleForAnalysis(), scale), "scale inheritance mask");
        Require(Near(child->GetWorldOrientationForAnalysis(), rotation),
                "orientation local times parent");
        Require(Near(child->GetWorldMatrixForAnalysis(), math::Affine(position, rotation, scale)),
                "affine builder");
        root->SetPositionForAnalysis({100, 200, 300});
        Require(root->UpdateWorldForAnalysis() &&
                    root->GetWorldPositionForAnalysis() == spNode::Vector3{10, 20, 30},
                "clean preserves root cache");
        Require((child->GetFlagsForAnalysis() & 7) == 0, "recursive cleanup");
    }
    auto root = std::make_shared<spNode>();
    auto child = std::make_shared<spNode>();
    auto grand = std::make_shared<spNode>();
    root->SetPositionForAnalysis({2, 3, 4});
    root->SetScaleForAnalysis({2, 3, 4});
    child->SetPositionForAnalysis({1, 2, 3});
    grand->SetPositionForAnalysis({1, 1, 1});
    Require(root->AttachChildForAnalysis(child) && child->AttachChildForAnalysis(grand),
            "three-level tree");
    root->SetEnabledForAnalysis(false);
    root->MarkLocalTransformDirtyForAnalysis();
    Require(root->UpdateWorldForAnalysis() &&
                Near(grand->GetWorldPositionForAnalysis(), spNode::Vector3{6, 12, 20}),
            "dirty reaches disabled descendants");
    child->SetPositionForAnalysis({3, 4, 5});
    child->MarkLocalTransformDirtyForAnalysis();
    Require(root->UpdateWorldForAnalysis() &&
                Near(grand->GetWorldPositionForAnalysis(), spNode::Vector3{10, 18, 28}),
            "clean ancestor still traverses dirty child");

    spNode bill;
    bill.SetBillboardAxisForAnalysis(2);
    const spNode::Matrix3 camera{1, 0, 0, 0, .8F, -.6F, 0, .6F, .8F};
    Require(bill.UpdateWorldForAnalysis(0, &camera), "billboard updates while clean");
    Require(Near(bill.GetWorldOrientationForAnalysis(),
                 spNode::Matrix3{-1, 0, 0, 0, .8F, -.6F, 0, -.6F, -.8F}),
            "camera-facing billboard basis");
    bill.SetBillboardAxisForAnalysis(1);
    Require(bill.UpdateWorldForAnalysis(0, &camera) &&
                Near(bill.GetWorldOrientationForAnalysis(),
                     spNode::Matrix3{-1, 0, 0, 0, 1, 0, 0, 0, -1}),
            "fixed-up billboard");
    Require(bill.UpdateWorldForAnalysis() &&
                bill.GetWorldOrientationForAnalysis() == math::Identity,
            "missing camera identity");
    const spNode::Matrix3 invalid{};
    Require(!bill.UpdateWorldForAnalysis(1, &invalid) &&
                bill.GetWorldOrientationForAnalysis() == math::Identity,
            "host-only degenerate camera rejection");

    spNodeController controller;
    auto animated = std::make_shared<spNode>();
    controller.SetNodeForAnalysis(animated);
    controller.SetEvaluatorForAnalysis(std::make_unique<RotationOnly>());
    controller.ApplyForAnalysis(0);
    Require((animated->GetFlagsForAnalysis() & 1) != 0, "rotation-only setter marks dirty");
    Require(animated->UpdateWorldForAnalysis() &&
                Near(animated->GetWorldOrientationForAnalysis(),
                     spNode::Matrix3{-1, 0, 0, 0, -1, 0, 0, 0, 1}),
            "rotation-only reaches world cache");
    animated->SetOrientationForAnalysis(math::Identity);
    controller.BlendForAnalysis(0, .5F);
    Require((animated->GetFlagsForAnalysis() & 1) != 0, "blended rotation-only marks dirty");
    std::cout << "PASS " << checks << '/' << checks << " node world reconstruction checks\n";
}
