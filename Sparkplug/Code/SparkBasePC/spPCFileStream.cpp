// Exact original implementation path recovered from the PC executable:
//   Z:\Sparkplug\Code\SparkBasePC\spPCFileStream.cpp

#include "spPCFileStream.h"

#define NOMINMAX
#include <Windows.h>

#include <new>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCFileStream()
        {
            return std::make_unique<spPCFileStream>();
        }
    }

    spPCFileStream::~spPCFileStream()
    {
        // PC 0x006BE240 tests only for zero.  INVALID_HANDLE_VALUE therefore
        // also reaches Close after a failed Open, matching the native state.
        if (handle_ != nullptr)
        {
            (void)Close();
        }
    }

    const spRTTIRecord& spPCFileStream::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{
            ClassID,
            spFileStream::ClassID,
            "spPCFileStream",
            &spFileStream::StaticRTTI(),
            &CreatePCFileStream,
            nullptr,
        };
        return record;
    }

    std::unique_ptr<spBaseObject> spPCFileStream::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCFileStream>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spPCFileStream::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    bool spPCFileStream::Open(
        const std::uint32_t mode,
        const char* streamName)
    {
        if (handle_ != nullptr)
        {
            return false;
        }

        DWORD desiredAccess = 0;
        DWORD creationDisposition = 0;
        DWORD flagsAndAttributes = 0;

        // PC 0x006BD81B checks the bits in this priority order.  These names
        // are analytical; the original enum declaration has not survived.
        if ((mode & 0x01U) != 0)
        {
            desiredAccess = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
            flagsAndAttributes = FILE_ATTRIBUTE_READONLY;
        }
        else if ((mode & 0x02U) != 0)
        {
            desiredAccess = GENERIC_WRITE;
            creationDisposition = (mode & 0x08U) != 0
                ? OPEN_ALWAYS
                : CREATE_ALWAYS;
            flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        }
        else if ((mode & 0x04U) != 0)
        {
            desiredAccess = GENERIC_READ | GENERIC_WRITE;
            creationDisposition = (mode & 0x08U) != 0
                ? OPEN_ALWAYS
                : CREATE_ALWAYS;
            flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        }

        const HANDLE opened = CreateFileA(
            streamName,
            desiredAccess,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            creationDisposition,
            flagsAndAttributes,
            nullptr);
        handle_ = opened;

        // The native method stores INVALID_HANDLE_VALUE and returns false; it
        // does not restore the closed/null state after CreateFileA failure.
        if (opened == nullptr || opened == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        (void)SetStreamName(streamName);
        if ((mode & 0x08U) != 0)
        {
            (void)Seek(SeekSource::essEnd, 0);
        }
        return true;
    }

    bool spPCFileStream::Close()
    {
        if (handle_ == nullptr)
        {
            return false;
        }

        (void)CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
        return true;
    }

    bool spPCFileStream::Seek(
        const SeekSource source,
        const std::int32_t offset)
    {
        if (handle_ == nullptr)
        {
            return false;
        }

        DWORD moveMethod = FILE_BEGIN;
        LONG nativeOffset = offset;
        switch (source)
        {
        case SeekSource::essStart:
            nativeOffset += static_cast<LONG>(GetLogicalOrigin());
            moveMethod = FILE_BEGIN;
            break;
        case SeekSource::essEnd:
            moveMethod = FILE_END;
            break;
        case SeekSource::essCurrent:
            moveMethod = FILE_CURRENT;
            break;
        default:
            break;
        }

        // Native code treats every 0xFFFFFFFF result as failure without the
        // documented GetLastError disambiguation.
        return SetFilePointer(
            static_cast<HANDLE>(handle_), nativeOffset, nullptr, moveMethod)
            != INVALID_SET_FILE_POINTER;
    }

    bool spPCFileStream::GetCurrentPosition(std::uint32_t& position) const
    {
        if (handle_ == nullptr)
        {
            return false;
        }

        const DWORD absolute = SetFilePointer(
            static_cast<HANDLE>(handle_), 0, nullptr, FILE_CURRENT);
        if (absolute == INVALID_SET_FILE_POINTER)
        {
            return false;
        }

        position = absolute - GetLogicalOrigin();
        return true;
    }

    bool spPCFileStream::ReadData(
        void* destination,
        const std::uint32_t byteCount)
    {
        if (handle_ == nullptr)
        {
            return false;
        }

        DWORD bytesRead = 0;
        if (ReadFile(
                static_cast<HANDLE>(handle_),
                destination,
                byteCount,
                &bytesRead,
                nullptr)
            == FALSE)
        {
            return false;
        }

        // PC treats a successful zero-byte read as EOF/failure and accepts a
        // short non-zero read as success.
        return bytesRead != 0;
    }

    bool spPCFileStream::WriteData(
        const void* source,
        const std::uint32_t byteCount)
    {
        if (handle_ == nullptr)
        {
            return false;
        }

        DWORD bytesWritten = 0;
        return WriteFile(
                   static_cast<HANDLE>(handle_),
                   source,
                   byteCount,
                   &bytesWritten,
                   nullptr)
            != FALSE;
    }

    bool spPCFileStream::vfunc_WriteFromStream(
        spStream* source,
        const std::uint32_t byteCount)
    {
        if (source == nullptr)
        {
            return false;
        }

        void* data = source->GetBuffer();
        bool ownsTemporary = false;
        if (data == nullptr)
        {
            data = ::operator new(byteCount, std::nothrow);
            if (data == nullptr && byteCount != 0)
            {
                return false;
            }

            // PC 0x006BDE67 ignores the source read result.
            (void)source->ReadData(data, byteCount);
            ownsTemporary = true;
        }
        else
        {
            // A direct-buffer source is advanced, but the pointer itself is
            // not offset: bytes are written from the start of its buffer.
            (void)source->Seek(SeekSource::essCurrent,
                static_cast<std::int32_t>(byteCount));
        }

        const bool result = WriteData(data, byteCount);
        if (ownsTemporary)
        {
            ::operator delete(data);
        }
        return result;
    }

    bool spPCFileStream::GetSize(std::uint32_t* size) const
    {
        if (handle_ == nullptr || size == nullptr)
        {
            return false;
        }

        const DWORD result = GetFileSize(static_cast<HANDLE>(handle_), nullptr);
        if (result == INVALID_FILE_SIZE)
        {
            return false;
        }
        *size = result;
        return true;
    }

    bool spPCFileStream::IsOpen() const noexcept
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }
}
