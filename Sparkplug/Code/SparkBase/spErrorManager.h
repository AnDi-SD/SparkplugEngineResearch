#pragma once

// spErrorManager.cpp is an exact PC source-path anchor.  No original header
// survives.  Types and members ending in ForAnalysis are portable seams and
// deliberately do not claim the unrecovered native spellings.

#include "spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    enum class spErrorSeverity : std::uint32_t
    {
        Information = 0,
        Warning = 1,
        Error = 2,
        Fatal = 3,
    };

    // Only the three values interpreted by the common spError formatter are
    // named.  Derived error classes use additional class-specific values.
    enum class spErrorCode : std::uint32_t
    {
        None = 0,
        Message = 1,
        ManagerDataStackOverflow = 2,
    };

    class spError : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x789B29B9;

        spError() noexcept = default;
        spError(
            spErrorSeverity severity,
            std::uint32_t code,
            const char* sourceFile,
            std::uint32_t sourceLine) noexcept;
        ~spError() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual std::string DescribeForAnalysis() const;
        [[nodiscard]] virtual std::string DescribeSeverityForAnalysis() const;
        [[nodiscard]] virtual std::string DescribeSourceForAnalysis() const;
        [[nodiscard]] virtual const char* ErrorNameForAnalysis() const noexcept;

        [[nodiscard]] spErrorSeverity GetSeverityForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetCodeForAnalysis() const noexcept;
        [[nodiscard]] const char* GetSourceFileForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetSourceLineForAnalysis() const noexcept;
        [[nodiscard]] const char* GetMessageForAnalysis() const noexcept;

        void SetMessageForAnalysis(const char* message);

    private:
        friend class spErrorManager;

        std::uint32_t code_ = 0;
        spErrorSeverity severity_ = spErrorSeverity::Information;
        const char* sourceFile_ = nullptr;
        std::uint32_t sourceLine_ = 0;
        std::string message_;
        spError* next_ = nullptr;
    };

    class spErrorManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x660E40D8;
        static constexpr std::size_t PcDataCapacity = 0x1000;
        static constexpr std::size_t Ps2DataCapacity = 0x0400;

        struct AnalysisDispatch final
        {
            const char* text = nullptr;
            spErrorSeverity severity = spErrorSeverity::Information;
            const char* sourceFile = nullptr;
            std::uint32_t sourceLine = 0;
        };

        using AnalysisHandler = void (*)(
            const AnalysisDispatch& dispatch,
            void* context);

        struct AnalysisHandlerBinding final
        {
            AnalysisHandler handler = nullptr;
            void* context = nullptr;
        };

        explicit spErrorManager(std::size_t dataCapacity = PcDataCapacity);
        ~spErrorManager() override;

        spErrorManager(const spErrorManager&) = delete;
        spErrorManager& operator=(const spErrorManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spErrorManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // The third native class-local slot is pure in spErrorManager and is
        // supplied by spPCErrorManager/spPS2ErrorManager.  Its source name is
        // unknown; this binding exposes only the proven lazy-handler role.
        [[nodiscard]] virtual AnalysisHandlerBinding
            ResolveHandlerForAnalysis() const noexcept = 0;

        [[nodiscard]] bool StoreMessageForAnalysis(
            spError& error,
            const char* message);
        void LinkErrorForAnalysis(spError& error) noexcept;
        [[nodiscard]] std::string FormatChainForAnalysis(bool detailed) const;
        [[nodiscard]] bool HandleForAnalysis(
            spErrorSeverity minimumSeverity);
        void ClearForAnalysis() noexcept;

        [[nodiscard]] std::size_t GetDataUsedForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetDataCapacityForAnalysis() const noexcept;
        [[nodiscard]] bool HadDataOverflowForAnalysis() const noexcept;
        [[nodiscard]] bool HadFatalForAnalysis() const noexcept;

    private:
        static spErrorManager* instance_;

        std::vector<char> dataStack_;
        std::size_t dataUsed_ = 0;
        spError* errorHead_ = nullptr;
        mutable AnalysisHandlerBinding handler_{};
        mutable bool handlerResolved_ = false;
        bool dataOverflow_ = false;
        bool hadFatal_ = false;
    };
}
