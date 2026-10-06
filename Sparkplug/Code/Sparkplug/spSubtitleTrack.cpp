#include "spSubtitleTrack.h"

namespace sparkplug::reconstruction
{
    const spRTTIRecord& spSubtitleTrack::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spNamedObject::ClassID,
            "spSubtitleTrack", &spNamedObject::StaticRTTI(),
            +[]() -> std::unique_ptr<spBaseObject> { return std::make_unique<spSubtitleTrack>(); }, nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }

    const spRTTIRecord& spSubtitleTrack::vfunc_18() const noexcept { return StaticRTTI(); }

    std::unique_ptr<spBaseObject> spSubtitleTrack::vfunc_10(spCloneManager& manager) const
    {
        // Native601A20 invokes inherited NamedCopy413120. Only the name is
        // copied; all own arrays, selected table and byte remain fresh.
        auto result = std::make_unique<spSubtitleTrack>();
        manager.RegisterCloneForAnalysis(*this, *result);
        return spNamedObject::vfunc_14(*result, manager) ? std::move(result) : nullptr;
    }

    bool spSubtitleTrack::LoadForAnalysis(spStream& stream, std::uint32_t maxInputBytes)
    {
        if (loaded_)
            return false;
        std::uint32_t size = 0, position = 0;
        const std::uint32_t origin = stream.GetLogicalOriginForAnalysis();
        if (!stream.GetSize(&size) || !stream.GetCurrentPosition(position) || origin > size ||
            position > size - origin || size - origin - position > maxInputBytes)
            return false;
        std::uint32_t remaining = size - origin - position;
        const auto word = [&stream, &remaining](std::uint32_t& value) {
            if (remaining < 4)
                return false;
            std::array<std::uint8_t, 4> bytes{};
            if (!stream.ReadData(bytes.data(), 4))
                return false;
            remaining -= 4;
            value = std::uint32_t(bytes[0]) | std::uint32_t(bytes[1]) << 8 |
                std::uint32_t(bytes[2]) << 16 | std::uint32_t(bytes[3]) << 24;
            return true;
        };
        std::uint32_t count = 0;
        if (!word(count) || count > remaining / 16)
            return false;
        std::vector<RecordWordsForAnalysis> records(count);
        for (auto& record : records)
            for (auto& value : record)
                if (!word(value))
                    return false;
        std::uint32_t blobSize = 0;
        if (!word(blobSize) || blobSize > remaining)
            return false;
        std::vector<std::uint8_t> blob(blobSize);
        if (blobSize && !stream.ReadData(blob.data(), blobSize))
            return false;
        remaining -= blobSize;
        if (!word(count) || count > remaining / 4)
            return false;
        std::vector<TableForAnalysis> tables(count);
        for (auto& table : tables)
        {
            std::uint32_t entries = 0;
            if (!word(entries) || entries > remaining / 8)
                return false;
            table.resize(entries);
            for (auto& entry : table)
                if (!word(entry.key) || !word(entry.blobOffset) || entry.blobOffset >= blob.size())
                    return false;
        }
        records_ = std::move(records);
        blob_ = std::move(blob);
        tables_ = std::move(tables);
        loaded_ = true;
        // Native read preserves selected index +28 and opaque byte +14.
        return true;
    }

    void spSubtitleTrack::ClearForAnalysis() noexcept
    {
        // Native releases blob, records, every table and table array, then
        // clears all six own words and byte14. Name/base remain unchanged.
        std::vector<std::uint8_t>().swap(blob_);
        std::vector<RecordWordsForAnalysis>().swap(records_);
        std::vector<TableForAnalysis>().swap(tables_);
        selectedTable_ = 0;
        opaqueByte_ = 0;
        loaded_ = false;
    }

    const char* spSubtitleTrack::LookupForAnalysis(std::uint32_t key) const noexcept
    {
        if (selectedTable_ >= tables_.size())
            return nullptr;
        for (const auto& entry : tables_[selectedTable_])
            if (entry.key == key)
                return reinterpret_cast<const char*>(blob_.data() + entry.blobOffset);
        return nullptr;
    }
}
