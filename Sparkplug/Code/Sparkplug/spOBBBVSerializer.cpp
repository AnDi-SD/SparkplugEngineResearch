#include "spOBBBVSerializer.h"
#include "spOBBBV.h"
#include "Analysis/PC/spSectionCursor.h"
#include "Analysis/PC/spAnimationMath.h"
#include <algorithm>

#include <cmath>
#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateOBBBVSerializer()
        {
            return std::make_unique<spOBBBVSerializer>();
        }

        const spRTTIRecord OBBBVSerializerRecord{
            spOBBBVSerializer::ClassID,
            spSerializer::ClassID,
            "spOBBBVSerializer",
            &spSerializer::StaticRTTI(),
            &CreateOBBBVSerializer,
            nullptr,
        };

        const bool OBBBVSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(OBBBVSerializerRecord);

        constexpr spOBBBVSerializer::Matrix3 IdentityMatrix{
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
    }

    bool spOBBBVSerializer::Vector3::operator==(
        const Vector3& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool spOBBBVSerializer::FieldBinding::operator==(
        const FieldBinding& other) const noexcept
    {
        return field == other.field
            && wireSourceOffset == other.wireSourceOffset
            && decodedTargetOffset == other.decodedTargetOffset
            && encoding == other.encoding
            && alwaysWritten == other.alwaysWritten;
    }

    spOBBBVSerializer::~spOBBBVSerializer() = default;

    const spRTTIRecord& spOBBBVSerializer::StaticRTTI() noexcept
    {
        (void)OBBBVSerializerRegistered;
        return OBBBVSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spOBBBVSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spOBBBVSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spOBBBVSerializer::vfunc_18() const noexcept
    {
        return OBBBVSerializerRecord;
    }

    spClassID spOBBBVSerializer::GetTargetClassIDForAnalysis() const noexcept
    {
        return TargetClassID;
    }

    std::vector<spOBBBVSerializer::FieldBinding>
    spOBBBVSerializer::BuildFieldSchemaForAnalysis()
    {
        return {
            {Field::Position, 0x4C, 0x18, WireEncoding::Vector3, false},
            {Field::Size, 0x58, 0x58, WireEncoding::Vector3, true},
            {Field::Rotation, 0x28, 0x28,
                WireEncoding::QuaternionFromMatrix3, false},
        };
    }

    bool spOBBBVSerializer::IsPositionSuppressedForAnalysis(
        const Vector3& position) noexcept
    {
        return std::fabs(position.x) <= SuppressionEpsilon
            && std::fabs(position.y) <= SuppressionEpsilon
            && std::fabs(position.z) <= SuppressionEpsilon;
    }

    bool spOBBBVSerializer::IsRotationSuppressedForAnalysis(
        const Matrix3& rotation) noexcept
    {
        for (std::size_t index = 0; index < rotation.size(); ++index)
        {
            if (std::fabs(rotation[index] - IdentityMatrix[index])
                > SuppressionEpsilon)
            {
                return false;
            }
        }
        return true;
    }

    std::vector<spOBBBVSerializer::Field>
    spOBBBVSerializer::BuildWritePlanForAnalysis(const WriteShape& shape)
    {
        std::vector<Field> plan;
        if (!IsPositionSuppressedForAnalysis(shape.position))
        {
            plan.push_back(Field::Position);
        }
        plan.push_back(Field::Size);
        if (!IsRotationSuppressedForAnalysis(shape.rotation))
        {
            plan.push_back(Field::Rotation);
        }
        return plan;
    }

    spOBBBVSerializer::DerivedSizeState
    spOBBBVSerializer::DecodeSizeForAnalysis(const Vector3& fullSize) noexcept
    {
        const Vector3 half{
            fullSize.x * 0.5F,
            fullSize.y * 0.5F,
            fullSize.z * 0.5F,
        };
        return {half, std::sqrt(
            half.x * half.x + half.y * half.y + half.z * half.z)};
    }

    bool spOBBBVSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        return fieldID <= static_cast<std::uint32_t>(Field::Rotation);
    }

    bool spOBBBVSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& stream,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();
        evidence::pc::serialization::SectionCursor cursor(context,stream,size,true,error);
        auto* obb=dynamic_cast<spOBBBV*>(&object);if(!obb)return cursor.Fail("OBBBV target mismatch");
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID<=1)
            {
                spOBBBV::Vector3 value{};
                if(!cursor.Read(value)||!std::all_of(value.begin(),value.end(),[](float v){return std::isfinite(v);}))
                    return cursor.Fail("Invalid OBBBV vector");
                if(header->fieldID==0)obb->SetPositionForAnalysis(value);else obb->SetSizeForAnalysis(value);
            }
            else if(header->fieldID==2)
            {
                evidence::pc::animation_math::Quaternion q{};
                if(!cursor.Read(q)||!std::all_of(q.begin(),q.end(),[](float v){return std::isfinite(v);}))
                    return cursor.Fail("Invalid OBBBV quaternion");
                obb->SetOrientationForAnalysis(evidence::pc::animation_math::ToMatrix(q));
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip OBBBV field");
        }
        return false;
    }
    bool spOBBBVSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const
    {return object.IsExactly(TargetClassID);}
    bool spOBBBVSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto fail=[&](const char* message){if(error)*error=message;return false;};
        const auto* obb=dynamic_cast<const spOBBBV*>(&object);if(!obb)return fail("OBBBV write target mismatch");
        const auto finite=[](const auto& v){return std::all_of(v.begin(),v.end(),[](float f){return std::isfinite(f);});};
        const auto& p=obb->GetPositionForAnalysis();const auto& s=obb->GetSizeForAnalysis();
        const auto& r=obb->GetOrientationForAnalysis();
        if(!finite(p)||!finite(s)||!finite(r))return fail("Nonfinite OBBBV parameters cannot be safely written");
        const WriteShape shape{{p[0],p[1],p[2]},{s[0],s[1],s[2]},r};
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&object))return false;
        for(const auto field:BuildWritePlanForAnalysis(shape))
        {
            bool written=false;
            if(field==Field::Position)written=blocks.WriteFieldForAnalysis(stream,0,p.data(),sizeof(p));
            else if(field==Field::Size)written=blocks.WriteFieldForAnalysis(stream,1,s.data(),sizeof(s));
            else
            {
                const auto q=evidence::pc::animation_math::FromMatrix(r);
                if(!finite(q))return fail("Invalid OBBBV rotation conversion");
                written=blocks.WriteFieldForAnalysis(stream,2,q.data(),sizeof(q));
            }
            if(!written)return fail("Cannot write OBBBV field");
        }
        return blocks.FinalizeObjectForAnalysis();
    }
}
