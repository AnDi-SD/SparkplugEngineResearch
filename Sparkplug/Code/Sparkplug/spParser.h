#pragma once
// Class name is exact PC RTTI; file/module placement and API are inferred.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction
{
    class spStream;
    class spParser : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x3EC20087;
        struct NormalizedForAnalysis
        {
            std::string bytes; // includes the final native NUL
            std::size_t nativeAllocationRequest=0;
            std::size_t nativeEndOffset=0; // still relative to original input
            std::uint32_t state20=1;
            bool nativeOwnership=true;
        };
        struct FileTextForAnalysis
        {
            std::unique_ptr<std::uint8_t[]> bytes;
            std::uint32_t size=0,capacity=0; // size includes four zero bytes
        };
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] static const std::array<bool,255>& DelimitersForAnalysis() noexcept;
        // PC4D0BF0 ->4D0AA0. Host stores enough bytes and rejects an unclosed
        // quote; native can overrun its exact input-length allocation, retain a
        // stale end pointer, or read beyond an unterminated quoted input.
        [[nodiscard]] static std::optional<NormalizedForAnalysis> NormalizeForAnalysis(std::string_view input);
        // PC free helper4D0A30; grouping here is analytical. Supplied stream
        // replaces only factory choice; actual source is PCFileStream. Native
        // ignores CopyTo's return and appends a zero uint32 even after EOF.
        // Host RAII also destroys a stream whose Open fails (native leaks it).
        [[nodiscard]] static std::optional<FileTextForAnalysis> ReadFileTextForAnalysis(
            std::unique_ptr<spStream> source,const char* name);
        [[nodiscard]] std::uint32_t State20ForAnalysis() const noexcept{return state20_;}
        void SetState20ForAnalysis(std::uint32_t value) noexcept{state20_=value;}
    private:
        std::uint32_t state20_=0;
    };
}
