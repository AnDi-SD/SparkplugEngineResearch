#pragma once

// Small exact arithmetic for PC64BA3D..64BA9A: float endpoint plus
// float gradient times the x87 64-significand-bit reciprocal of curvature.
// Each extended operation and the final float store truncate toward zero.
// This is not a general x87 implementation (no NaN/Inf or exception state).
#include <array>
#include <cstdint>
#include <cstring>

namespace sparkplug::evidence::pc::float80_rtz
{
    struct Wide
    {
        std::array<std::uint32_t,4> words{};
        static Wide Shifted(std::uint64_t value,unsigned shift)
        {
            Wide out;for(unsigned i=0;i<64;++i)if((value>>i)&1)out.words[(i+shift)/32]|=std::uint32_t(1)<<((i+shift)%32);
            return out;
        }
        unsigned Highest() const
        {for(unsigned i=128;i--;)if((words[i/32]>>(i%32))&1)return i;return 0;}
        std::uint64_t Extract(unsigned shift) const
        {
            std::uint64_t out=0;for(unsigned i=0;i<64&&i+shift<128;++i)
                if((words[(i+shift)/32]>>((i+shift)%32))&1)out|=std::uint64_t(1)<<i;
            return out;
        }
        void Add(const Wide& other)
        {std::uint64_t carry=0;for(unsigned i=0;i<4;++i){carry+=std::uint64_t(words[i])+other.words[i];words[i]=std::uint32_t(carry);carry>>=32;}}
        void Subtract(const Wide& other)
        {std::uint64_t borrow=0;for(unsigned i=0;i<4;++i){const std::uint64_t sub=std::uint64_t(other.words[i])+borrow;borrow=std::uint64_t(words[i])<sub;words[i]=std::uint32_t(std::uint64_t(words[i])-sub);}}
        void Multiply(std::uint32_t factor)
        {std::uint64_t carry=0;for(auto& word:words){carry+=std::uint64_t(word)*factor;word=std::uint32_t(carry);carry>>=32;}}
        void Divide(std::uint32_t denominator)
        {std::uint64_t remainder=0;for(unsigned i=4;i--;){const auto value=(remainder<<32)|words[i];words[i]=std::uint32_t(value/denominator);remainder=value%denominator;}}
    };
    struct Number
    {
        std::uint64_t mantissa=0;int exponent=0;bool negative=false;
    };
    inline Number Normalize(const Wide& value,int exponent,bool negative)
    {
        const auto top=value.Highest();const int shift=int(top)-63;
        const auto mantissa=shift>=0?value.Extract(unsigned(shift)):value.Extract(0)<<unsigned(-shift);
        return {mantissa,exponent+shift,negative};
    }
    inline Number FromFloat(float value)
    {
        std::uint32_t bits;std::memcpy(&bits,&value,4);const auto field=(bits>>23)&255;
        const auto significand=(bits&0x7fffff)|(field?0x800000:0);
        return Normalize(Wide::Shifted(significand,0),int(field?field:1)-150,bool(bits>>31));
    }
    inline Number NegativeReciprocal(float positive)
    {
        std::uint32_t bits;std::memcpy(&bits,&positive,4);const auto field=(bits>>23)&255;
        const auto significand=(bits&0x7fffff)|(field?0x800000:0);
        unsigned top=0;for(auto n=significand;n>>=1;)++top;
        const unsigned power=top+64-((significand&(significand-1))==0);
        auto wide=Wide::Shifted(1,power);wide.Divide(significand);
        return Normalize(wide,150-int(field?field:1)-int(power),true);
    }
    inline Number MultiplyFloat(Number extended,float value)
    {
        std::uint32_t bits;std::memcpy(&bits,&value,4);const auto field=(bits>>23)&255;
        const auto significand=(bits&0x7fffff)|(field?0x800000:0);
        auto wide=Wide::Shifted(extended.mantissa,0);wide.Multiply(significand);
        return Normalize(wide,extended.exponent+int(field?field:1)-150,extended.negative!=bool(bits>>31));
    }
    inline Number Add(Number a,Number b)
    {
        if(!a.mantissa)return b;if(!b.mantissa)return a;
        if(a.exponent<b.exponent||(a.exponent==b.exponent&&a.mantissa<b.mantissa)){const auto saved=a;a=b;b=saved;}
        const unsigned difference=unsigned(a.exponent-b.exponent);
        if(difference>=64)
        {
            if(a.negative!=b.negative){--a.mantissa;if(!(a.mantissa>>63)){a.mantissa<<=1;--a.exponent;}}
            return a;
        }
        auto wide=Wide::Shifted(a.mantissa,difference);const auto small=Wide::Shifted(b.mantissa,0);
        if(a.negative==b.negative)wide.Add(small);else wide.Subtract(small);
        return Normalize(wide,b.exponent,a.negative);
    }
    inline float ToFloat(Number value)
    {
        std::uint32_t bits=0;
        if(value.mantissa)
        {
            const int field=value.exponent+63+127;
            const int shift=40+(field<=0?1-field:0);
            const auto significand=shift>=64?0u:std::uint32_t(value.mantissa>>unsigned(shift));
            bits=(value.negative?0x80000000u:0)|(field>0?std::uint32_t(field)<<23:0)|(significand&0x7fffff);
        }
        float out;std::memcpy(&out,&bits,4);return out;
    }
    inline float UpdateEndpoint(float endpoint,float gradient,float positiveCurvature)
    {return ToFloat(Add(FromFloat(endpoint),MultiplyFloat(NegativeReciprocal(positiveCurvature),gradient)));}
}
