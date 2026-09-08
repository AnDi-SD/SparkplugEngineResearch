#pragma once

// Original PC64B299, caller64C9EB. Its final high coefficient for the eight
// step optimizer is8/7 in the image, retained exactly instead of normalized.
#include "spTextureBlockOptimizer.h"

namespace sparkplug::evidence::pc::texture_blocks
{
    inline std::array<float,2> OptimizeAlpha(const std::array<float,16>& points,unsigned steps)
    {
        float low=1,high=0;
        for(float value:points)
        {
            if(value<low&&(steps==8||value>0))low=value;
            if(value>high&&(steps==8||value<1))high=value;
        }
        if(steps==6&&low==high)high=1;
        const std::array<float,8> lowBlend=steps==6?
            std::array<float,8>{1,Constant(0x3F4CCCCD),Constant(0x3F19999A),Constant(0x3ECCCCCD),Constant(0x3E4CCCCD),0,0,0}:
            std::array<float,8>{1,Constant(0x3F5B6DB7),Constant(0x3F36DB6E),Constant(0x3F124925),Constant(0x3EDB6DB7),Constant(0x3E924925),Constant(0x3E124925),0};
        const std::array<float,8> highBlend=steps==6?
            std::array<float,8>{0,Constant(0x3E4CCCCD),Constant(0x3ECCCCCD),Constant(0x3F19999A),Constant(0x3F4CCCCD),1,0,0}:
            std::array<float,8>{0,Constant(0x3E124925),Constant(0x3E924925),Constant(0x3EDB6DB7),Constant(0x3F124925),Constant(0x3F36DB6E),Constant(0x3F5B6DB7),Constant(0x3F924925)};
        for(unsigned iteration=0;iteration<8;++iteration)
        {
            const double range=double(high)-low;if(range<Constant(0x3B800000))break;
            const float scale=TruncateFloat(double(steps-1)/range);std::array<float,8> palette{};
            for(unsigned i=0;i<steps;++i)palette[i]=TruncateFloat(double(high)*highBlend[i]+double(low)*lowBlend[i]);
            float gradientLow=0,gradientHigh=0,curvatureLow=0,curvatureHigh=0;
            for(float value:points)
            {
                const double projection=(double(value)-low)*scale;unsigned index;
                if(projection<=0){if(steps==6&&value<=double(low)*.5)continue;index=0;}
                else if(projection>=steps-1){if(steps==6&&value>=(double(high)+1)*.5)continue;index=steps-1;}
                else index=static_cast<unsigned>(TruncateFloat(projection+.5));
                const double residual=double(value)-palette[index];
                gradientLow=TruncateFloat(double(gradientLow)+residual*lowBlend[index]);
                gradientHigh=TruncateFloat(double(gradientHigh)+residual*highBlend[index]);
                curvatureLow=TruncateFloat(double(curvatureLow)+double(lowBlend[index])*lowBlend[index]);
                curvatureHigh=TruncateFloat(double(curvatureHigh)+double(highBlend[index])*highBlend[index]);
            }
            if(curvatureLow>0)low=float80_rtz::SubtractQuotient(low,gradientLow,curvatureLow);
            if(curvatureHigh>0)high=float80_rtz::SubtractQuotient(high,gradientHigh,curvatureHigh);
            if(low>high)std::swap(low,high);
            if(double(gradientLow)*gradientLow<Constant(0x3C800000)&&double(gradientHigh)*gradientHigh<Constant(0x3C800000))break;
        }
        return {std::clamp(low,0.0F,1.0F),std::clamp(high,0.0F,1.0F)};
    }
}
