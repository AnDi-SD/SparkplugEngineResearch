#pragma once
// Host read-only memory backend, shared by field inspection and resource loads.
// The caller keeps these bytes immutable and alive for the synchronous read.
// This stream owns no input buffer; loaded resources must own their own data.
#include "Code/SparkBase/spStream.h"
#include <cstring>

namespace spvhost {
class BorrowedInput final : public sparkplug::reconstruction::spStream {
    const std::uint8_t* data_;
    std::uint32_t size_, position_ = 0;
public:
    BorrowedInput(const std::uint8_t* data, std::uint32_t size) : data_(data), size_(size) {}
    bool Open(const char*) override { return false; }
    bool Open(std::uint32_t, const char*) override { return false; }
    bool Close() override { return false; }
    bool Seek(SeekSource source, std::int32_t offset) override {
        // The common loader advances the logical origin to FFPS dataOffset.
        // Keep the physical cursor and size; only start seeks and Tell use it.
        if(GetLogicalOriginForAnalysis() > size_) return false;
        std::int64_t base;
        switch(source) {
        case SeekSource::essStart: base = GetLogicalOriginForAnalysis(); break;
        case SeekSource::essCurrent: base = position_; break;
        case SeekSource::essEnd: base = size_; break;
        default: return false;
        }
        const auto next = base + offset;
        if(next < 0 || next > size_) return false;
        position_ = static_cast<std::uint32_t>(next); return true;
    }
    bool GetCurrentPosition(std::uint32_t& position) const override {
        const auto origin = GetLogicalOriginForAnalysis();
        if(position_ < origin) return false;
        position = position_ - origin; return true;
    }
    bool GetSize(std::uint32_t* size) const override {
        if(!size) return false;
        *size = size_; return true;
    }
    bool ReadData(void* destination, std::uint32_t count) override {
        if(position_ < GetLogicalOriginForAnalysis() || count > size_ - position_
            || ((!destination || !data_) && count)) return false;
        if(count) std::memcpy(destination, data_ + position_, count);
        position_ += count; return true;
    }
    bool WriteData(const void*, std::uint32_t) override { return false; }
    bool vfunc_WriteFromStream(spStream*, std::uint32_t) override { return false; }
    // GetBuffer intentionally retains spStream's null result: input is immutable.
};
}
