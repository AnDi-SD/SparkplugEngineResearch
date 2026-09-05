// Exact original PC source path:
//   Z:\Sparkplug\Code\SparkBase\spErrorManager.cpp

#include "spErrorManager.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateError()
        {
            return std::make_unique<spError>();
        }

        const spRTTIRecord ErrorRecord{
            spError::ClassID,
            spBaseObject::ClassID,
            "spError",
            &spBaseObject::StaticRTTI(),
            &CreateError,
            nullptr,
        };

        const spRTTIRecord ErrorManagerRecord{
            spErrorManager::ClassID,
            spBaseObject::ClassID,
            "spErrorManager",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        [[nodiscard]] const char* SeverityLabel(
            const spErrorSeverity severity) noexcept
        {
            switch (severity)
            {
            case spErrorSeverity::Information:
                return "";
            case spErrorSeverity::Warning:
                return "WARNING";
            case spErrorSeverity::Error:
                return "ERROR";
            case spErrorSeverity::Fatal:
                return "FATAL ERROR";
            default:
                return "UNKNOWN";
            }
        }
    }

    spErrorManager* spErrorManager::instance_ = nullptr;

    spError::spError(
        const spErrorSeverity severity,
        const std::uint32_t code,
        const char* const sourceFile,
        const std::uint32_t sourceLine) noexcept
        : code_(code),
          severity_(severity),
          sourceFile_(sourceFile),
          sourceLine_(sourceLine)
    {
    }

    spError::~spError() = default;

    const spRTTIRecord& spError::StaticRTTI() noexcept
    {
        return ErrorRecord;
    }

    std::unique_ptr<spBaseObject> spError::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spError>();
        manager.RegisterClone(*this, *clone);
        // Native spError uses the inherited empty copy slot.  Error payload,
        // source pointers and chain links are deliberately not cloned.
        if (!spBaseObject::vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    const spRTTIRecord& spError::vfunc_18() const noexcept
    {
        return ErrorRecord;
    }

    std::string spError::DescribeForAnalysis() const
    {
        switch (static_cast<spErrorCode>(code_))
        {
        case spErrorCode::None:
            return "No error";
        case spErrorCode::Message:
            if (!message_.empty())
            {
                return message_;
            }
            break;
        case spErrorCode::ManagerDataStackOverflow:
            return "Error manager data stack overflow";
        default:
            break;
        }

        std::ostringstream text;
        text << "Unknown " << ErrorNameForAnalysis() << " : " << code_;
        if (!message_.empty())
        {
            text << " - " << message_;
        }
        return text.str();
    }

    std::string spError::DescribeSeverityForAnalysis() const
    {
        const auto* label = SeverityLabel(severity_);
        if (*label == '\0')
        {
            return DescribeForAnalysis();
        }
        return std::string{label} + ": " + DescribeForAnalysis();
    }

    std::string spError::DescribeSourceForAnalysis() const
    {
        auto text = DescribeSeverityForAnalysis();
        if (sourceFile_ != nullptr)
        {
            text += "\n - ";
            text += sourceFile_;
            text += "(";
            text += std::to_string(sourceLine_);
            text += ")";
        }
        return text;
    }

    const char* spError::ErrorNameForAnalysis() const noexcept
    {
        return "error";
    }

    spErrorSeverity spError::GetSeverityForAnalysis() const noexcept
    {
        return severity_;
    }

    std::uint32_t spError::GetCodeForAnalysis() const noexcept
    {
        return code_;
    }

    const char* spError::GetSourceFileForAnalysis() const noexcept
    {
        return sourceFile_;
    }

    std::uint32_t spError::GetSourceLineForAnalysis() const noexcept
    {
        return sourceLine_;
    }

    const char* spError::GetMessageForAnalysis() const noexcept
    {
        return message_.empty() ? nullptr : message_.c_str();
    }

    void spError::SetMessageForAnalysis(const char* const message)
    {
        message_ = message == nullptr ? "" : message;
    }

    spErrorManager::spErrorManager(const std::size_t dataCapacity)
        : dataStack_(dataCapacity)
    {
        instance_ = this;
    }

    spErrorManager::~spErrorManager()
    {
        ClearForAnalysis();
        instance_ = nullptr;
    }

    const spRTTIRecord& spErrorManager::StaticRTTI() noexcept
    {
        return ErrorManagerRecord;
    }

    spErrorManager* spErrorManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spErrorManager::vfunc_10(
        spCloneManager&) const
    {
        // Both abstract native tables use the shared null-clone target.
        return nullptr;
    }

    const spRTTIRecord& spErrorManager::vfunc_18() const noexcept
    {
        return ErrorManagerRecord;
    }

    bool spErrorManager::StoreMessageForAnalysis(
        spError& error,
        const char* const message)
    {
        if (message == nullptr)
        {
            error.message_.clear();
            return true;
        }

        const auto length = std::strlen(message) + 1;
        // Native allocation requires the new end to remain strictly below
        // the fixed capacity (PS2 0x00107B5C..0x00107B84).
        if (length >= dataStack_.size() - std::min(dataUsed_, dataStack_.size()))
        {
            dataOverflow_ = true;
            return false;
        }

        std::memcpy(dataStack_.data() + dataUsed_, message, length);
        error.message_.assign(dataStack_.data() + dataUsed_, length - 1);
        dataUsed_ += length;
        return true;
    }

    void spErrorManager::LinkErrorForAnalysis(spError& error) noexcept
    {
        error.next_ = errorHead_;
        errorHead_ = &error;
    }

    std::string spErrorManager::FormatChainForAnalysis(
        const bool detailed) const
    {
        std::string text;
        for (auto* error = errorHead_; error != nullptr; error = error->next_)
        {
            if (!text.empty())
            {
                text += "\n";
            }
            text += detailed
                ? error->DescribeSourceForAnalysis()
                : error->DescribeSeverityForAnalysis();
        }
        return text;
    }

    bool spErrorManager::HandleForAnalysis(
        const spErrorSeverity minimumSeverity)
    {
        if (errorHead_ == nullptr
            || static_cast<std::uint32_t>(errorHead_->severity_)
                < static_cast<std::uint32_t>(minimumSeverity))
        {
            return false;
        }

        if (!handlerResolved_)
        {
            handler_ = ResolveHandlerForAnalysis();
            handlerResolved_ = true;
        }

        const auto text = FormatChainForAnalysis(true);
        const AnalysisDispatch dispatch{
            text.c_str(),
            errorHead_->severity_,
            errorHead_->sourceFile_,
            errorHead_->sourceLine_,
        };
        if (handler_.handler != nullptr)
        {
            handler_.handler(dispatch, handler_.context);
        }

        hadFatal_ = static_cast<std::uint32_t>(errorHead_->severity_)
            >= static_cast<std::uint32_t>(spErrorSeverity::Fatal);
        ClearForAnalysis();
        return true;
    }

    void spErrorManager::ClearForAnalysis() noexcept
    {
        while (errorHead_ != nullptr)
        {
            auto* current = errorHead_;
            errorHead_ = current->next_;
            current->next_ = nullptr;
        }
        dataUsed_ = 0;
    }

    std::size_t spErrorManager::GetDataUsedForAnalysis() const noexcept
    {
        return dataUsed_;
    }

    std::size_t spErrorManager::GetDataCapacityForAnalysis() const noexcept
    {
        return dataStack_.size();
    }

    bool spErrorManager::HadDataOverflowForAnalysis() const noexcept
    {
        return dataOverflow_;
    }

    bool spErrorManager::HadFatalForAnalysis() const noexcept
    {
        return hadFatal_;
    }
}
