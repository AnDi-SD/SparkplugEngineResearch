#pragma once

// Shared HOST encoder of the confirmed FFPS/inline FAT reader grammar.
// Not a reconstructed original SaveResources body. Preserve supplied raw
// names and opaque object extents; object selection/relocation is external.
#include "../../Code/SparkBase/spStream.h"
#include <cstdint>
#include <limits>

namespace sparkplug::host::resource_envelope
{
    struct Entry final
    {
        std::uint32_t id, classID, offset, size;
        const std::uint8_t* name;
        std::uint32_t nameSize;
    };

    struct Header final
    {
        std::uint32_t signature, version, exportTag, platformMask;
    };

    struct Layout final { std::uint32_t dataOffset, fileSize; };

    inline bool IndexSize(const Entry* entries, std::uint32_t count,
        std::uint32_t& size) noexcept
    {
        if (!entries && count) return false;
        std::uint64_t total = 4;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto& entry = entries[i];
            if (entry.nameSize > std::numeric_limits<std::uint16_t>::max()
                || (!entry.name && entry.nameSize)) return false;
            total += 18ull + entry.nameSize;
            if (total > std::numeric_limits<std::uint32_t>::max()) return false;
        }
        size = static_cast<std::uint32_t>(total);
        return true;
    }

    inline bool Measure(const Entry* entries, std::uint32_t count,
        std::uint32_t dataSize, Layout& result) noexcept
    {
        std::uint32_t indexSize = 0;
        if (!IndexSize(entries, count, indexSize)) return false;
        // Seven-word header, FAT (including its count), zero external-file count.
        const auto prefix = 28ull + indexSize + 4ull;
        const auto file = prefix + dataSize;
        if (file > std::numeric_limits<std::uint32_t>::max()) return false;
        result = {static_cast<std::uint32_t>(prefix), static_cast<std::uint32_t>(file)};
        return true;
    }

    inline bool WriteIndex(reconstruction::spStream& output, const Entry* entries,
        std::uint32_t count)
    {
        std::uint32_t size = 0;
        if (!IndexSize(entries, count, size) || !output.Write(count)) return false;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto& entry = entries[i];
            if (!output.Write(entry.id)
                || !output.Write(static_cast<std::uint16_t>(entry.nameSize))
                || (entry.nameSize && !output.WriteData(entry.name, entry.nameSize))
                || !output.Write(entry.classID) || !output.Write(entry.offset)
                || !output.Write(entry.size)) return false;
        }
        return true;
    }

    inline bool WritePrefix(reconstruction::spStream& output, const Header& header,
        const Entry* entries, std::uint32_t count, std::uint32_t dataSize)
    {
        Layout layout{};
        if (!Measure(entries, count, dataSize, layout)) return false;
        return output.Write(header.signature) && output.Write(header.version)
            && output.Write(header.exportTag) && output.Write(layout.fileSize)
            && output.Write(header.platformMask) && output.Write(layout.dataOffset)
            && output.Write(dataSize) && WriteIndex(output, entries, count)
            && output.Write(std::uint32_t{0});
    }
}
