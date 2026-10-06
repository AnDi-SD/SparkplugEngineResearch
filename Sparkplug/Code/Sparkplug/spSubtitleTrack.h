#pragma once

// Original class identity, native reader/lookup/clear/lifecycle. Field and API
// names below are analytical; portable vectors replace owned native arrays.
#include "../SparkBase/spStream.h"
#include <array>
#include <vector>

namespace sparkplug::reconstruction
{
    class spSubtitleTrack final : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x0FD11616;
        using RecordWordsForAnalysis = std::array<std::uint32_t, 4>;
        struct TableEntryForAnalysis final
        {
            std::uint32_t key = 0, blobOffset = 0;
        };
        using TableForAnalysis = std::vector<TableEntryForAnalysis>;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native601790 has no checked return contract. This host wrapper
        // checks reads/counts/offsets atomically and requires Clear before reuse.
        // maxInputBytes is an explicit host input-byte budget, not a game
        // constant. Empty-table vector metadata may cost6x these input bytes.
        [[nodiscard]] bool LoadForAnalysis(spStream&, std::uint32_t maxInputBytes = 8U * 1024U * 1024U);
        void ClearForAnalysis() noexcept;
        // Native601750 scans the selected table in order and returns first
        // matching pointer into the owned blob. Host returns null for absent
        // tables/out-of-range selection instead of native invalid memory access.
        [[nodiscard]] const char* LookupForAnalysis(std::uint32_t key) const noexcept;
        void SetSelectedTableForAnalysis(std::uint32_t index) noexcept { selectedTable_ = index; }
        [[nodiscard]] std::uint32_t GetSelectedTableForAnalysis() const noexcept { return selectedTable_; }
        // Native +14 byte is initialized/cleared, but its consumer meaning is
        // not established. Reader and lookup leave it unchanged.
        void SetOpaqueByteForAnalysis(std::uint8_t value) noexcept { opaqueByte_ = value; }
        [[nodiscard]] std::uint8_t GetOpaqueByteForAnalysis() const noexcept { return opaqueByte_; }
        [[nodiscard]] const std::vector<RecordWordsForAnalysis>& GetRecordsForAnalysis() const noexcept { return records_; }
        [[nodiscard]] const std::vector<std::uint8_t>& GetBlobForAnalysis() const noexcept { return blob_; }
        [[nodiscard]] const std::vector<TableForAnalysis>& GetTablesForAnalysis() const noexcept { return tables_; }
        [[nodiscard]] bool IsLoadedForAnalysis() const noexcept { return loaded_; }

    private:
        std::uint8_t opaqueByte_ = 0;
        std::uint32_t selectedTable_ = 0;
        std::vector<RecordWordsForAnalysis> records_;
        std::vector<std::uint8_t> blob_;
        std::vector<TableForAnalysis> tables_;
        bool loaded_ = false; // host protocol guard, no corresponding native field
    };
}
