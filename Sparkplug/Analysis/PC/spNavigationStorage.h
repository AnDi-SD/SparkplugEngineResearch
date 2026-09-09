#pragma once
// Portable storage for the original non-RTTI navigation bit-matrix template.
// PC447BE0/447C60/447CA0, CRT initializers6D31E0/6D3200/6D3230.
#include <cstdint>
#include <vector>
namespace sparkplug::evidence::pc
{
    struct NavigationTransitionTableForAnalysis
    {
        std::uint32_t rows=0,columns=0,stride=0;
        std::vector<std::uint8_t> packed;
        bool Resize(std::uint32_t r,std::uint32_t c)
        {
            if(r>4096||c>4096||std::uint64_t(r)*c>1048576)return false;
            rows=r;columns=c;stride=(c+3)/4;packed.assign(std::size_t(r)*stride,0);return true;
        }
        std::uint32_t Get(std::uint32_t r,std::uint32_t c) const noexcept
        {
            const auto index=std::size_t(r)*columns+c;
            return (packed[index>>2]>>((index&3)*2))&3;
        }
        void Set(std::uint32_t r,std::uint32_t c,std::uint32_t value) noexcept
        {
            const auto index=std::size_t(r)*columns+c;const auto shift=(index&3)*2;
            auto& word=packed[index>>2];word=std::uint8_t((word&~(3u<<shift))|((value&3u)<<shift));
        }
    };
}
