#include "spPS2Helper.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePS2Helper()
        {
            return std::make_unique<spPS2Helper>();
        }

        const spRTTIRecord PS2HelperRecord{
            spPS2Helper::ClassID,
            spBaseObject::ClassID,
            "spPS2Helper",
            &spBaseObject::StaticRTTI(),
            &CreatePS2Helper,
            nullptr,
        };

        [[nodiscard]] bool StartsWith(
            const std::string& value,
            const char* prefix) noexcept
        {
            const auto prefixLength = std::strlen(prefix);
            return value.size() >= prefixLength
                && value.compare(0, prefixLength, prefix) == 0;
        }

        void UppercaseAscii(std::string& value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::toupper(character));
                });
        }
    }

    spPS2Helper* spPS2Helper::instance_ = nullptr;

    spPS2Helper::spPS2Helper() noexcept
        : pathPrefix_("host0:")
    {
        instance_ = this;
    }

    spPS2Helper::~spPS2Helper()
    {
        instance_ = nullptr;
    }

    const spRTTIRecord& spPS2Helper::StaticRTTI() noexcept
    {
        return PS2HelperRecord;
    }

    spPS2Helper* spPS2Helper::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spPS2Helper::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2Helper>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    bool spPS2Helper::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // The native slot is the empty spBaseObject copy implementation.
        // mode_, field18_ and the prefix consequently remain constructor
        // defaults in a clone.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPS2Helper::vfunc_18() const noexcept
    {
        return PS2HelperRecord;
    }

    std::string spPS2Helper::sub_001E91B0(const char* path) const
    {
        const std::string input = path == nullptr ? "" : path;

        // An already qualified disc path is accepted only when it has the
        // ISO-9660 version suffix.  A host0 path is always passed through.
        if ((StartsWith(input, "\\") && input.find(";1") != std::string::npos)
            || StartsWith(input, "host0:"))
        {
            auto result = input;
            // The native branch calls strupr only when a backslash occurs.
            if (input.find('\\') != std::string::npos)
            {
                UppercaseAscii(result);
            }
            return result;
        }

        if (mode_ == 0 || mode_ == 2)
        {
            auto result = pathPrefix_ + input;
            std::replace(result.begin(), result.end(), '\\', '/');
            return result;
        }

        std::size_t first = 0;
        if (StartsWith(input, "./") || StartsWith(input, ".\\"))
        {
            first = 2;
        }

        auto result = std::string{"\\"} + input.substr(first) + ";1";
        UppercaseAscii(result);
        std::replace(result.begin(), result.end(), '/', '\\');
        return result;
    }

    void spPS2Helper::sub_001E93A0(const std::uint32_t mode) noexcept
    {
        mode_ = mode;
    }

    std::uint32_t spPS2Helper::GetMode() const noexcept
    {
        return mode_;
    }

    std::uint32_t spPS2Helper::GetField18() const noexcept
    {
        return field18_;
    }

    const std::string& spPS2Helper::GetPathPrefix() const noexcept
    {
        return pathPrefix_;
    }

    bool spPS2Helper::SetPathPrefixForAnalysis(const char* prefix)
    {
        if (prefix == nullptr || std::strlen(prefix) >= 0x100)
        {
            return false;
        }
        pathPrefix_ = prefix;
        return true;
    }
}
