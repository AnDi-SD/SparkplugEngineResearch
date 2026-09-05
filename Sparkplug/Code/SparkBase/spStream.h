#pragma once

// Inferred header path: neither shipped executable contains an original
// spStream header path.  The class, inheritance, class ID, per-platform vtable
// membership/order and the source spellings explicitly called out below are
// executable-backed.  Portable storage/ownership and helper names are
// reconstruction choices.

#include "spBaseObject.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace sparkplug::reconstruction
{
    class spStream : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x6CC80D8A;

        // The original enum type name is not present in the executables.
        // essStart and essCurrent are preserved literally in assertion text;
        // value 2 is the independently observed end-relative switch branch.
        enum class SeekSource : std::uint32_t
        {
            essStart = 1,
            essEnd = 2,
            essCurrent = 4,
        };

        spStream() noexcept = default;
        ~spStream() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Nine consecutive pure slots are present in both native vtables.
        // PC fills them with _purecall; PS2 stores null entries.  Open, Close
        // and the raw stream operations below are named from behavior and
        // surviving call-site text.  The second Open mode type/name and the
        // stream-to-stream slot's original name remain unknown.  The two
        // write operations exchange relative order between the PC and PS2
        // ABIs; this portable declaration follows the PS2 logical order and
        // does not claim host vtable compatibility.
        [[nodiscard]] virtual bool Open(const char* streamName) = 0;
        [[nodiscard]] virtual bool Open(
            std::uint32_t mode,
            const char* streamName) = 0;
        [[nodiscard]] virtual bool Close() = 0;
        [[nodiscard]] virtual bool Seek(
            SeekSource source,
            std::int32_t offset) = 0;
        [[nodiscard]] virtual bool GetCurrentPosition(
            std::uint32_t& position) const = 0;
        [[nodiscard]] virtual bool ReadData(
            void* destination,
            std::uint32_t byteCount) = 0;
        [[nodiscard]] virtual bool WriteData(
            const void* source,
            std::uint32_t byteCount) = 0;
        [[nodiscard]] virtual bool vfunc_WriteFromStream(
            spStream* source,
            std::uint32_t byteCount) = 0;
        [[nodiscard]] virtual bool GetSize(std::uint32_t* size) const = 0;

        // This final slot is virtual but not pure.  Native spStream and file
        // streams return null; spMemoryStream overrides it with its buffer.
        [[nodiscard]] virtual void* GetBuffer() noexcept;

        // Executable-backed behavior exposed through portable names.  These
        // helpers are deliberately not claimed as exact original declarations.
        [[nodiscard]] const char* GetStreamName() const noexcept;

        template <typename T, std::enable_if_t<!std::is_array_v<T>, int> = 0>
        [[nodiscard]] bool Read(T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T)
                <= static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max()));
            return ReadData(&value, static_cast<std::uint32_t>(sizeof(T)));
        }

        template <typename T, std::enable_if_t<!std::is_array_v<T>, int> = 0>
        [[nodiscard]] bool Write(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T)
                <= static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max()));
            return WriteData(&value, static_cast<std::uint32_t>(sizeof(T)));
        }

        // Mirrors the native length-prefixed C-string helper: the prefix is a
        // 16-bit byte count including the terminator.  Without a prefix only
        // the string bytes are written.
        [[nodiscard]] bool Write(const char* value, bool writeLength = true);

        // Safe portable counterpart of the native char** helper.  The binary
        // allocates a C buffer; the reconstruction keeps ownership explicit.
        [[nodiscard]] bool ReadString(std::string& value);

        // Native sub_00114E60 asks this stream for its full size, then invokes
        // destination's stream-to-stream slot.  That slot is +0x40 on PS2 but
        // +0x34 on PC because the two Write overloads exchange ABI order.  The
        // helper reports success once the slot was called, regardless of that
        // slot's return value; this method preserves that.
        [[nodiscard]] bool CopyTo(spStream& destination);

    protected:
        // Native sub_001148C0 owns a heap C string at +0x18.  This safe
        // equivalent preserves null separately from an empty string.
        [[nodiscard]] bool SetStreamName(const char* streamName);

        [[nodiscard]] std::uint32_t GetLogicalOrigin() const noexcept;
        void SetLogicalOrigin(std::uint32_t origin) noexcept;

    private:
        // Portable equivalents of native PS2 +0x14 and +0x18.  Their original
        // member names are still unknown; this class is not host-ABI exact.
        std::uint32_t logicalOrigin_ = 0;
        std::optional<std::string> streamName_;
    };
}
