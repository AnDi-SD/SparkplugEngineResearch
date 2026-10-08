#pragma once
// PC488A20..488C65 and actual matrix product420C00..420D0E. Finite inputs,
// default nearest x87 precision; zero terms, stores and signed scale retained.
#include "spTransformConstMath.h"

namespace sparkplug::evidence::pc::capsule_bv_math
{
    namespace extended = transform_const_math;
    namespace scalar = float80_rtz;
    using Number = scalar::Number;
    using Vector3 = std::array<float,3>;
    using Matrix3 = std::array<float,9>;

    inline Number Multiply(Number first, float second) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &second, sizeof(bits));
        const auto field = (bits >> 23) & 255;
        const auto significand = (bits & 0x7FFFFFU) | (field ? 0x800000U : 0);
        auto wide = scalar::Wide::Shifted(first.mantissa, 0);
        wide.Multiply(significand);
        return extended::NormalizeNearest(wide, first.exponent + int(field ? field : 1) - 150,
            first.negative != bool(bits >> 31));
    }
    inline Number Product(float first, float second) noexcept
    { return Multiply(scalar::FromFloat(first), second); }
    inline Number Add(Number first, Number second) noexcept
    { return extended::AddNearest(first, second); }
    inline float Store(Number value) noexcept
    { return extended::StoreNearest(value); }
    template<std::size_t Size> inline bool Finite(const std::array<float,Size>& values) noexcept
    { for(float value : values) if(!std::isfinite(value)) return false; return true; }

    inline Matrix3 MatrixProduct(const Matrix3& first, const Matrix3& second) noexcept
    {
        // Order of the three terms differs by cell in420C00. All final stores
        // round to float, including the three temporaries copied as integers.
        static constexpr unsigned order[9][3]{
            {1,0,2}, {2,0,1}, {2,0,1},
            {2,1,0}, {2,1,0}, {2,1,0},
            {2,0,1}, {0,2,1}, {0,2,1}};
        Matrix3 result;
        for(unsigned row=0;row<3;++row)
            for(unsigned column=0;column<3;++column)
            {
                const unsigned cell=row*3+column;
                auto term=[&](unsigned index){return Product(first[row*3+index],second[index*3+column]);};
                result[cell]=Store(capsule_bv_math::Add(capsule_bv_math::Add(term(order[cell][0]),term(order[cell][1])),term(order[cell][2])));
            }
        return result;
    }

    inline bool Transform(const Vector3& localPosition, const Matrix3& localOrientation,
        float length, Vector3& position, Matrix3& orientation, const Vector3& scale,
        Vector3& first, Vector3& second) noexcept
    {
        if(!Finite(localPosition)||!Finite(localOrientation)||!std::isfinite(length)||
            !Finite(position)||!Finite(orientation)||!Finite(scale))return false;
        Vector3 nextPosition;
        Number translated[3];
        Number scaled[3]{Product(localPosition[0],scale[0]),Product(localPosition[1],scale[1]),
            Product(localPosition[2],scale[2])};
        for(unsigned c=0;c<2;++c)
            translated[c]=capsule_bv_math::Add(capsule_bv_math::Add(Multiply(scaled[0],orientation[c]),Multiply(scaled[2],orientation[6+c])),
                Multiply(scaled[1],orientation[3+c]));
        translated[2]=capsule_bv_math::Add(capsule_bv_math::Add(Multiply(scaled[2],orientation[8]),Multiply(scaled[1],orientation[5])),
            Multiply(scaled[0],orientation[2]));
        for(unsigned c=0;c<3;++c)
        {
            if(c!=2)
            {
                const float offset=Store(translated[c]);
                if(!std::isfinite(offset))return false;
                translated[c]=scalar::FromFloat(offset);
            }
            nextPosition[c]=Store(capsule_bv_math::Add(translated[c],scalar::FromFloat(position[c])));
        }
        const Matrix3 combined=MatrixProduct(localOrientation,orientation);
        if(!Finite(nextPosition)||!Finite(combined))return false;
        // Native publishes position before it rereads scale for the segment.
        // Preserve the legal case where those two Vector3 references alias.
        const auto& endpointScale=&position==&scale?nextPosition:scale;
        const float maximum=endpointScale[0]>endpointScale[1]&&endpointScale[0]>endpointScale[2]?endpointScale[0]:
            endpointScale[1]>endpointScale[2]?endpointScale[1]:endpointScale[2];
        const Number scaledLength=Product(maximum,length);
        const float storedLength=Store(scaledLength);
        if(!std::isfinite(storedLength))return false;
        Number zeroFirst[3],zeroThird[3];
        for(unsigned c=0;c<3;++c)
        {
            zeroFirst[c]=scalar::FromFloat(Store(Product(combined[c],0.0F)));
            zeroThird[c]=scalar::FromFloat(Store(Product(combined[6+c],0.0F)));
        }
        auto endpoint=[&](Number half,Vector3& output)
        {
            for(unsigned c=0;c<3;++c)
            {
                Number offset=capsule_bv_math::Add(capsule_bv_math::Add(Multiply(half,combined[3+c]),zeroFirst[c]),zeroThird[c]);
                if(c!=0)
                {
                    const float storedOffset=Store(offset);
                    if(!std::isfinite(storedOffset))return false;
                    offset=scalar::FromFloat(storedOffset);
                }
                offset=Multiply(offset,endpointScale[c]);
                if(c!=0)
                {
                    const float storedScaledOffset=Store(offset);
                    if(!std::isfinite(storedScaledOffset))return false;
                    offset=scalar::FromFloat(storedScaledOffset);
                }
                output[c]=Store(capsule_bv_math::Add(offset,scalar::FromFloat(nextPosition[c])));
            }
            return Finite(output);
        };
        Vector3 nextFirst,nextSecond;
        // Positive half retains the extended product; negative half reloads
        // the float store at488B1C. The caches are written negative then positive.
        if(!endpoint(Multiply(scaledLength,0.5F),nextSecond)||
            !endpoint(Product(storedLength,-0.5F),nextFirst))return false;
        position=nextPosition;orientation=combined;first=nextFirst;second=nextSecond;
        return true;
    }
}
