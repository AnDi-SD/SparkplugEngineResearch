#include "spNodeSerializer.h"

#include "spNode.h"

#include <array>
#include <cmath>
#include <memory>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateNodeSerializer()
        {
            return std::make_unique<spNodeSerializer>();
        }

        const spRTTIRecord NodeSerializerRecord{
            spNodeSerializer::ClassID,
            spSerializer::ClassID,
            "spNodeSerializer",
            &spSerializer::StaticRTTI(),
            &CreateNodeSerializer,
            nullptr,
        };

        const bool NodeSerializerRegistered =
            spRTTIManager::Instance().Register(NodeSerializerRecord);

        template <std::size_t Size>
        bool ApproximatelyEqual(
            const std::array<float, Size>& left,
            const std::array<float, Size>& right) noexcept
        {
            for (std::size_t index = 0; index < Size; ++index)
            {
                if (std::fabs(left[index] - right[index])
                    > spNodeSerializer::DefaultComparisonTolerance)
                {
                    return false;
                }
            }
            return true;
        }
    }

    spNodeSerializer::~spNodeSerializer() = default;

    const spRTTIRecord& spNodeSerializer::StaticRTTI() noexcept
    {
        (void)NodeSerializerRegistered;
        return NodeSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spNodeSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spNodeSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spNodeSerializer::vfunc_18() const noexcept
    {
        return NodeSerializerRecord;
    }

    spClassID spNodeSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return spNode::ClassID;
    }

    std::vector<spNodeSerializer::Field>
    spNodeSerializer::BuildKnownWritePlanForAnalysis(const spNode& node) const
    {
        static constexpr spNode::Vector3 ZeroVector{0.0F, 0.0F, 0.0F};
        static constexpr spNode::Vector3 UnitVector{1.0F, 1.0F, 1.0F};
        static constexpr spNode::Matrix3 IdentityMatrix{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };

        std::vector<Field> fields;
        if (!ApproximatelyEqual(node.GetPositionForAnalysis(), ZeroVector))
        {
            fields.push_back(Field::Position);
        }
        if (!ApproximatelyEqual(node.GetOrientationForAnalysis(), IdentityMatrix))
        {
            fields.push_back(Field::Rotation);
        }
        if (!ApproximatelyEqual(node.GetScaleForAnalysis(), UnitVector))
        {
            fields.push_back(Field::Scale);
        }
        if (node.IsBoneForAnalysis())
        {
            fields.push_back(Field::IsBone);
        }
        if (node.IsStaticForAnalysis())
        {
            fields.push_back(Field::IsStatic);
        }

        // Both native writers emit the current animated state explicitly.
        fields.push_back(Field::IsAnimated);

        for (std::size_t index = 0;
             index < node.GetChildCountForAnalysis(); ++index)
        {
            if (node.GetChildForAnalysis(index) != nullptr)
            {
                fields.push_back(Field::Child);
            }
        }

        if (node.GetBillboardAxisForAnalysis() != 0)
        {
            fields.push_back(Field::BillboardAxis);
        }

        return fields;
    }
}
