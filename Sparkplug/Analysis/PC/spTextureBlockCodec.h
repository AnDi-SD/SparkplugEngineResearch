#pragma once

// Original PC64C493/64C5D5/64C65A block decoders, selected by627873.
// Channels are normalized binary32 R,G,B,A. Preserve the original shared
// DXT1 color branch even for DXT3/5; alpha is overwritten afterwards.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sparkplug::evidence::pc::texture_blocks
{
    using Color=std::array<float,4>;
    using Block=std::array<Color,16>;
    inline float Constant(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
    inline Color Decode565(std::uint16_t value)
    {
        return {static_cast<float>(double(value>>11)*Constant(0x3D042108)),
                static_cast<float>(double((value>>5)&63)*Constant(0x3C820821)),
                static_cast<float>(double(value&31)*Constant(0x3D042108)),1};
    }
    inline bool Decode(unsigned flags,const std::byte* packed,std::size_t size,Block& output)
    {
        if(!packed||flags<1||flags>3||size!=(flags==1?8u:16u))return false;
        const auto byte=[&](std::size_t index){return static_cast<unsigned>(packed[index]);};
        const auto offset=flags==1?0u:8u;
        const auto first=static_cast<std::uint16_t>(byte(offset)|(byte(offset+1)<<8));
        const auto second=static_cast<std::uint16_t>(byte(offset+2)|(byte(offset+3)<<8));
        std::array<Color,4> palette{};palette[0]=Decode565(first);palette[1]=Decode565(second);
        for(unsigned c=0;c<4;++c)
        {
            const double difference=double(palette[1][c])-palette[0][c];
            if(first<=second)palette[2][c]=static_cast<float>(difference*.5+palette[0][c]);
            else
            {
                palette[2][c]=static_cast<float>(difference*Constant(0x3EAAAAAB)+palette[0][c]);
                //64C53E/54F/560 spill G,B,A differences before palette[3].
                const double saved=c?static_cast<float>(difference):difference;
                palette[3][c]=static_cast<float>(saved*Constant(0x3F2AAAAB)+palette[0][c]);
            }
        }
        std::uint32_t indices=0;for(unsigned i=0;i<4;++i)indices|=byte(offset+4+i)<<(8*i);
        Block decoded{};for(auto& color:decoded){color=palette[indices&3];indices>>=2;}
        if(flags==2)
        {
            for(unsigned i=0;i<16;++i)decoded[i][3]=static_cast<float>(double((byte(i/2)>>(4*(i%2)))&15)*Constant(0x3D888889));
        }
        if(flags==3)
        {
            std::array<float,8> alpha{};
            alpha[0]=static_cast<float>(double(byte(0))*Constant(0x3B808081));
            alpha[1]=static_cast<float>(double(byte(1))*Constant(0x3B808081));
            const unsigned steps=byte(0)>byte(1)?7:5;
            for(unsigned i=1;i<steps;++i)alpha[i+1]=static_cast<float>((double(steps-i)*alpha[0]+double(i)*alpha[1])
                *Constant(steps==7?0x3E124925:0x3E4CCCCD));
            if(steps==5){alpha[6]=0;alpha[7]=1;}
            std::uint64_t bits=0;for(unsigned i=0;i<6;++i)bits|=std::uint64_t(byte(2+i))<<(8*i);
            for(auto& color:decoded){color[3]=alpha[bits&7];bits>>=3;}
        }
        output=decoded;return true;
    }
}
