#pragma once

// PC64B5C0 weighted RGB endpoint optimization, called from64BB40.
// Explicit binary32 stores preserve the original nearest/truncate phases.
#include "spTextureBlockCodec.h"
#include "spFloat80TowardZero.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace sparkplug::evidence::pc::texture_blocks
{
    using RGB=std::array<float,3>;
    inline float TruncateFloat(double value)
    {
        const auto rounded=static_cast<float>(value);
        return std::abs(double(rounded))>std::abs(value)?std::nextafter(rounded,0.0F):rounded;
    }
    inline RGB Weights(){return {Constant(0x3E981530),1,Constant(0x3DCE6734)};}
    using OptimizerStep=std::array<float,17>;
    inline bool OptimizeRGB(const Block& points,unsigned steps,RGB& first,RGB& second,std::vector<OptimizerStep>* trace=nullptr)
    {
        if(steps!=3&&steps!=4)return false;
        const auto weights=Weights();RGB low=weights,high{};
        for(const auto& point:points)for(unsigned c=0;c<3;++c)
        {
            if(!std::isfinite(point[c])||point[c]<0||point[c]>weights[c])return false;
            low[c]=std::min(low[c],point[c]);high[c]=std::max(high[c],point[c]);
        }
        double difference[3]={float(double(high[0])-low[0]),float(double(high[1])-low[1]),double(high[2])-low[2]};
        const double length=(difference[2]*difference[2]+difference[1]*difference[1])+difference[0]*difference[0];
        if(length<Constant(0x00800000)){first=low;second=high;return true;}
        const auto savedLength=static_cast<float>(length);RGB direction{},midpoint{};
        for(unsigned c=0;c<3;++c)
        {direction[c]=static_cast<float>(difference[c]*(1.0/length));midpoint[c]=static_cast<float>((double(low[c])+high[c])*.5);}
        double score0=0;std::array<float,3> otherScores{};
        for(const auto& point:points)
        {
            const double r=float((double(point[0])-midpoint[0])*direction[0]);
            const double g=float((double(point[1])-midpoint[1])*direction[1]);
            const double b=(double(point[2])-midpoint[2])*direction[2];
            const double projections[4]={(g+b)+r,(g+r)-b,(r-g)+b,(r-g)-b};
            score0+=projections[0]*projections[0];
            for(unsigned i=0;i<3;++i)otherScores[i]=static_cast<float>(double(otherScores[i])+projections[i+1]*projections[i+1]);
        }
        unsigned orientation=0;double best=score0;
        for(unsigned i=1;i<4;++i)if(best<otherScores[i-1]){best=otherScores[i-1];orientation=i;}
        if(orientation&2)std::swap(low[1],high[1]);if(orientation&1)std::swap(low[2],high[2]);
        if(savedLength<Constant(0x39800000)){first=low;second=high;return true;}
        const std::array<float,4> blendHigh=steps==3?std::array<float,4>{0,.5F,1,0}
            :std::array<float,4>{0,Constant(0x3EAAAAAB),Constant(0x3F2AAAAB),1};
        const std::array<float,4> blendLow=steps==3?std::array<float,4>{1,.5F,0,0}
            :std::array<float,4>{1,Constant(0x3F2AAAAB),Constant(0x3EAAAAAB),0};
        for(unsigned iteration=0;iteration<8;++iteration)
        {
            std::array<RGB,4> palette{};
            for(unsigned i=0;i<steps;++i)for(unsigned c=0;c<3;++c)
                palette[i][c]=TruncateFloat(double(low[c])*blendLow[i]+double(high[c])*blendHigh[i]);
            difference[0]=double(high[0])-low[0];difference[1]=TruncateFloat(double(high[1])-low[1]);difference[2]=double(high[2])-low[2];
            const double square=(difference[2]*difference[2]+difference[1]*difference[1])+difference[0]*difference[0];
            if(square<Constant(0x39800000))break;
            const double scale=double(steps-1)/square;
            for(unsigned c=0;c<3;++c)direction[c]=TruncateFloat(difference[c]*scale);
            RGB gradientLow{},gradientHigh{};float curvatureLow=0,curvatureHigh=0;
            for(const auto& point:points)
            {
                const double projection=((double(point[1])-low[1])*direction[1]+(double(point[0])-low[0])*direction[0])
                    +(double(point[2])-low[2])*direction[2];
                const auto saved=TruncateFloat(projection);
                const int index=projection>=steps-1?int(steps-1):static_cast<int>(TruncateFloat(double(saved)+.5));
                if(index<0||index>=int(steps))return false;
                const double a=double(blendLow[index])*.125,b=double(blendHigh[index])*.125;
                curvatureLow=TruncateFloat(double(curvatureLow)+a*blendLow[index]);
                curvatureHigh=TruncateFloat(double(curvatureHigh)+b*blendHigh[index]);
                for(unsigned c=0;c<3;++c)
                {
                    const double residual=c==1?TruncateFloat(double(palette[index][c])-point[c]):double(palette[index][c])-point[c];
                    gradientLow[c]=TruncateFloat(double(gradientLow[c])+a*residual);
                    gradientHigh[c]=TruncateFloat(double(gradientHigh[c])+b*residual);
                }
            }
            if(trace)trace->push_back({low[0],low[1],low[2],high[0],high[1],high[2],direction[0],direction[1],direction[2],
                gradientLow[0],gradientLow[1],gradientLow[2],gradientHigh[0],gradientHigh[1],gradientHigh[2],curvatureLow,curvatureHigh});
            if(curvatureLow>0)for(unsigned c=0;c<3;++c)low[c]=float80_rtz::UpdateEndpoint(low[c],gradientLow[c],curvatureLow);
            if(curvatureHigh>0)for(unsigned c=0;c<3;++c)high[c]=float80_rtz::UpdateEndpoint(high[c],gradientHigh[c],curvatureHigh);
            bool converged=true;for(unsigned c=0;c<3;++c)if(double(gradientLow[c])*gradientLow[c]>=Constant(0x37800000)
                ||double(gradientHigh[c])*gradientHigh[c]>=Constant(0x37800000))converged=false;
            if(converged)break;
        }
        first=low;second=high;return true;
    }
}
