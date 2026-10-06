#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace winx::evidence::pc
{
    // Technical finite-input adapter for the PC's wider intermediates. This
    // does not claim arbitrary x87 precision, nonfinite values or EE behavior.
    inline void ButterflyNormalizeForAnalysis(std::array<float, 3>& value)
    {
        const double x=value[0], y=value[1], z=value[2];
        const double length=std::sqrt((x*x+y*y)+z*z);
        if(length>double(.001f)) for(float& v:value) v=float(double(v)*(1.0/length));
        else value={};
    }
    inline float ButterflyRandomScaleForAnalysis(unsigned random)
    {
        // The exact uint32 product needs up to56 significant bits. Retain
        // its residual when rounding the final float, including midpoint ties.
        constexpr double factor=double(0x1.999998p-35f), bias=double(.7f);
        const double product=double(random)*factor;
        const double residual=std::fma(double(random),factor,-product);
        const double high=product+bias, split=high-product;
        const double low=((product-(high-split))+(bias-split))+residual;
        float result=float(high);
        const double difference=(high-double(result))+low;
        if(difference==0)return result;
        const float next=std::nextafter(result,difference>0?INFINITY:-INFINITY);
        const double half=(double(next)-double(result))*.5;
        if((difference>0&&difference>half)||(difference<0&&difference<half))return next;
        if(difference==half)
        {
            // A float32 midpoint uses its even low significand bit.
            int exponent=0;std::frexp(double(result),&exponent);
            if(std::fmod(std::ldexp(double(result),24-exponent),2.0)!=0)return next;
        }
        return result;
    }
    inline float ButterflyRandomHeightForAnalysis(unsigned random,float originY)
    {
        // The product/subtraction are exact in binary64. The final addition
        // may need the x87's64-bit significand: an origin too small for
        // binary64 can still select the other side of a binary32 midpoint.
        const double base=double(random)*0x1.ep-27-30.0;
        const double high=base+double(originY),split=high-base;
        double low=(base-(high-split))+(double(originY)-split);
        if(high==0)return float(low);
        if(!std::isfinite(high))return float(high);
        int exponent=0;const double significand=std::frexp(std::abs(high),&exponent);
        if(significand==.5&&((high>0&&low<0)||(high<0&&low>0)))--exponent;
        const double quantum=std::ldexp(1.0,exponent-64);
        low=std::nearbyint(low/quantum)*quantum;
        float result=float(high);const double difference=(high-double(result))+low;
        if(difference==0)return result;
        const float next=std::nextafter(result,difference>0?INFINITY:-INFINITY);
        const double half=(double(next)-double(result))*.5;
        if((difference>0&&difference>half)||(difference<0&&difference<half))return next;
        unsigned word=0;std::memcpy(&word,&result,4);
        return difference==half&&(word&1)?next:result;
    }
}
