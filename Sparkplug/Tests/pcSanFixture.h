#pragma once

// Bounded read-only test fixture for single-object PC SAN envelopes. Not a
// replacement resource loader. Production reconstruction uses existing field core.
#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace sparkplug::tests
{
    inline std::unique_ptr<reconstruction::spAnimation> ReadOwnedSan(
        const char* path, reconstruction::spAnimationManager& manager)
    {
        using namespace reconstruction;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto length = file.tellg();
        if (!file || length < 32 || length > spAnimationSerializer::MaximumFieldBytesForAnalysis)
            throw std::runtime_error("Bounded SAN fixture extent");
        std::vector<std::uint8_t> raw(static_cast<std::size_t>(length));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(raw.data()), length))
            throw std::runtime_error("SAN fixture read failed");
        const auto word = [&](std::size_t offset) {
            if (offset + 4 > raw.size())
                throw std::runtime_error("Truncated SAN word");
            std::uint32_t result;
            std::memcpy(&result, raw.data() + offset, 4);
            return result;
        };
        const auto start = word(20), size = word(24);
        if (std::memcmp(raw.data(), "FFPS", 4) || word(4) != 0x26 || word(12) != raw.size() ||
            word(28) != 1 || start > raw.size() || size != raw.size() - start || size < 8 ||
            word(start) != spAnimation::ClassID || word(start + 4) != 0x4f4f4253)
            throw std::runtime_error("Not a single-object PC SAN fixture");
        spMemoryStream stream;
        if (!stream.ResizeAndSetSize(size - 8))
            throw std::runtime_error("SAN fixture allocation");
        std::memcpy(stream.GetBuffer(), raw.data() + start + 8, size - 8);
        std::string error;
        auto animation = spAnimationSerializer{}.ReadFieldsWithBindingsForAnalysis(
            stream, size - 8, manager, nullptr, &error);
        if (!animation)
            throw std::runtime_error(error);
        return animation;
    }
} // namespace sparkplug::tests
