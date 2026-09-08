#pragma once
// PC413270/4132B0 and49DE60..49E31B. Analytical names, not original class claims.
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sparkplug::evidence::pc
{
    struct ParticleRandomForAnalysis
    {
        std::array<std::uint32_t,624> state{};
        std::uint32_t index=625;
        void Seed(std::uint32_t value) noexcept
        {
            state[0]=value;
            for(index=1;index<624;++index)state[index]=1812433253u*(state[index-1]^(state[index-1]>>30))+index;
        }
        std::uint32_t Next() noexcept
        {
            if(index>=624)
            {
                if(index==625)Seed(5489);
                for(unsigned i=0;i<624;++i)
                {
                    const auto y=(state[i]&0x80000000u)|(state[(i+1)%624]&0x7fffffffu);
                    state[i]=state[(i+397)%624]^(y>>1)^((y&1)?0x9908b0dfu:0u);
                }
                index=0;
            }
            auto y=state[index++];y^=y>>11;y^=(y<<7)&0x9d2c5680u;y^=(y<<15)&0xefc60000u;return y^(y>>18);
        }
    };

    inline bool SampleParticleRegionForAnalysis(std::uint32_t tag,const std::vector<float>& p,
        ParticleRandomForAnalysis& random,std::uint32_t count,std::vector<std::array<float,3>>& output)
    {
        constexpr std::array<unsigned,8> sizes{0,3,6,4,8,4,5,6};
        if(tag<1||tag>7||p.size()!=sizes[tag]||count>128)return false;
        for(float v:p)if(!std::isfinite(v)||std::abs(v)>65536)return false;
        output.clear();output.reserve(count);
        constexpr double Unit=0x1p-32;
        const auto unit=[&](){return double(random.Next())*Unit;};
        const auto signedUnit=[&](){return double(random.Next())*(2*Unit)-1.0;};
        for(std::uint32_t i=0;i<count;++i)
        {
            std::array<float,3> result{};
            if(tag==1)result={p[0],p[1],p[2]};
            else if(tag==4)
            {
                // Original ignores normal and samples the positive X/Z rectangle.
                const float x=float(double(random.Next())*double(p[6])*Unit);
                const double z=double(random.Next())*double(p[7])*Unit;
                result={float(double(x)+p[0]),p[1],float(z+p[2])};
            }
            else if(tag==2)
            {
                // Original consumes Y before X and retains Z in x87 until final store.
                const float y=float(unit()-.5),x=float(unit()-.5);const double z=unit()-.5;
                result={float(double(x)*p[3]+p[0]),float(double(y)*p[4]+p[1]),float(z*p[5]+p[2])};
            }
            else if(tag==3)
            {
                float x=0,y=0;double z=0;unsigned attempts=0;
                do {if(++attempts>4096)return false;x=float(signedUnit());y=float(signedUnit());z=signedUnit();}
                while((z*z+double(y)*y)+double(x)*x>1);
                const float xs=float(double(x)*p[3]),ys=float(double(y)*p[3]);
                result={float(double(xs)+p[0]),float(double(ys)+p[1]),float(z*p[3]+p[2])};
            }
            else if(tag==5||tag==6)
            {
                float x=0;double z=0;unsigned attempts=0;
                do {if(++attempts>4096)return false;x=float(signedUnit());z=signedUnit();}
                while(std::sqrt(z*z+double(x)*x)>1);
                const float radius=p[tag==5?3:4];
                const float ys=float(double(radius)*0.0);
                if(tag==5)
                {
                    const float xs=float(double(x)*radius);
                    result={float(double(xs)+p[0]),float(double(ys)+p[1]),float(z*radius+p[2])};
                }
                else
                {
                    const float zs=float(z*radius),originY=float(double(ys)+p[1]);
                    result={float(double(x)*radius+p[0]),float((unit()-.5)*p[3]+originY),float(double(zs)+p[2])};
                }
            }
            else
            {
                // Native cone interpolation uses physical Y, without dividing by height.
                const float y=float((unit()-.5)*p[3]);
                const double radius=(double(p[5])-p[4])*y+p[4];
                const float radiusSquared=float(radius*radius),maximum=p[4]>p[5]?p[4]:p[5];
                float x=0,z=0;unsigned attempts=0;
                do
                {
                    if(++attempts>4096)return false;
                    x=float(signedUnit()*maximum);const double extendedZ=signedUnit()*maximum;z=float(extendedZ);
                    if(extendedZ*double(z)+double(x)*x<=radiusSquared)break;
                }while(true);
                result={float(double(x)+p[0]),float(double(y)+p[1]),float(double(z)+p[2])};
            }
            output.push_back(result);
        }
        return true;
    }
}
