#pragma once
// Modern backend topology projection, not a recovered game draw operation.
// Keep raw IB bytes separately. Strip degenerates consume parity but need no
// vertex fetch in the emitted triangle list, including legacy CDCD tails.
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace sparkplug::host::render_topology {
inline constexpr std::size_t MaximumTriangleIndices=16u*1024u*1024u;
inline std::vector<std::uint32_t> Triangles(const std::vector<std::uint32_t>& source,
    std::uint32_t primitiveType,std::uint32_t vertices) {
    if(primitiveType!=2&&primitiveType!=3)throw std::runtime_error("Host triangle projection requires list or strip");
    if(primitiveType==2){
        if(source.size()%3)throw std::runtime_error("Host triangle-list extent is not divisible by three");
        if(source.size()>MaximumTriangleIndices)throw std::runtime_error("Host triangle projection exceeds64MiB");
        for(const auto index:source)if(index>=vertices)throw std::runtime_error("Host triangle index is outside its vertex buffer");
        return source; // Preserve authored list degenerates, as the previous host did.
    }
    std::vector<std::uint32_t> result;
    for(std::size_t window=0;window+2<source.size();++window){
        const auto a=source[window],b=source[window+1],c=source[window+2];
        if(a==b||b==c||a==c)continue;
        if(a>=vertices||b>=vertices||c>=vertices)throw std::runtime_error("Host triangle index is outside its vertex buffer");
        if(result.size()>MaximumTriangleIndices-3)throw std::runtime_error("Host triangle projection exceeds64MiB");
        result.push_back(window&1?b:a);result.push_back(window&1?a:b);result.push_back(c);
    }
    return result;
}
}
