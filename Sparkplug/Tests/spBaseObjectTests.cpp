#include "Code/SparkBase/spBaseObject.h"
#include "Code/SparkBase/spApp.h"
#include "Code/SparkBase/spFileStream.h"
#include "Code/SparkBase/spAsyncFileStreamManager.h"
#include "Code/SparkBase/spErrorManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/SparkBase/spPCKManager.h"
#include "Code/SparkBase/spStream.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Code/SparkBasePS2/spPS2Helper.h"
#include "Code/SparkBasePS2/spPS2IOPModuleManager.h"
#include "Code/SparkplugPS2/spPS2App.h"
#include "Code/SparkplugPS2/spPS2ErrorManager.h"
#if defined(_WIN32)
#include "Code/SparkBasePC/spPCAsyncFileStreamManager.h"
#include "Code/SparkBasePC/spPCFileStream.h"
#include "Code/SparkplugPC/spPCApp.h"
#include "Code/SparkplugPC/spPCErrorManager.h"
#endif
#include "Analysis/PC/SparkBaseAbi.h"
#include "Analysis/PS2/SparkBaseAbi.h"
#include "Analysis/PS2/spPS2FileStreamState.h"
#include "Analysis/PS2/spPS2AsyncFileStreamManagerState.h"

#include <algorithm>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
    void Require(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    struct TestPCKFile final
    {
        const char* directory;
        const char* fileName;
        std::uint32_t logicalSector;
        std::uint32_t byteCount;
    };

    void StoreLittleEndian32(
        std::vector<std::uint8_t>& bytes,
        const std::size_t offset,
        const std::uint32_t value)
    {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
        bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16U);
        bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24U);
    }

    std::vector<std::uint8_t> BuildTestPCK(
        const std::vector<TestPCKFile>& files)
    {
        std::vector<std::uint8_t> strings;
        std::vector<std::uint32_t> directoryOffsets;
        std::vector<std::uint32_t> fileNameOffsets;
        for (const auto& file : files)
        {
            directoryOffsets.push_back(static_cast<std::uint32_t>(strings.size()));
            strings.insert(strings.end(), file.directory,
                file.directory + std::strlen(file.directory) + 1);
            fileNameOffsets.push_back(static_cast<std::uint32_t>(strings.size()));
            strings.insert(strings.end(), file.fileName,
                file.fileName + std::strlen(file.fileName) + 1);
        }

        const auto recordsStart = std::size_t{4} + strings.size() + 4;
        const auto metadataEnd = recordsStart + files.size() * 0x14;
        std::size_t imageSize = metadataEnd;
        for (const auto& file : files)
        {
            imageSize = (std::max)(imageSize,
                static_cast<std::size_t>(file.logicalSector) * 0x800
                    + file.byteCount);
        }

        std::vector<std::uint8_t> image(imageSize);
        StoreLittleEndian32(image, 0, static_cast<std::uint32_t>(strings.size()));
        std::memcpy(image.data() + 4, strings.data(), strings.size());
        StoreLittleEndian32(image, 4 + strings.size(),
            static_cast<std::uint32_t>(files.size()));
        for (std::size_t index = 0; index < files.size(); ++index)
        {
            const auto record = recordsStart + index * 0x14;
            StoreLittleEndian32(image, record, directoryOffsets[index]);
            StoreLittleEndian32(image, record + 4, fileNameOffsets[index]);
            StoreLittleEndian32(image, record + 8, files[index].logicalSector);
            StoreLittleEndian32(image, record + 0x0C,
                files[index].logicalSector * 0x800U);
            StoreLittleEndian32(image, record + 0x10, files[index].byteCount);
        }
        return image;
    }

    class TestStream final : public sparkplug::reconstruction::spStream
    {
    public:
        bool Open(const char* streamName) override
        {
            if (!SetStreamName(streamName))
            {
                return false;
            }
            SetLogicalOrigin(0);
            data_.clear();
            position_ = 0;
            isOpen_ = true;
            return true;
        }

        bool Open(std::uint32_t, const char* streamName) override
        {
            return Open(streamName);
        }

        bool Close() override
        {
            isOpen_ = false;
            return true;
        }

        bool Seek(const SeekSource source, const std::int32_t offset) override
        {
            if (!isOpen_)
            {
                return false;
            }

            std::int64_t target = 0;
            switch (source)
            {
            case SeekSource::essStart:
                target = static_cast<std::int64_t>(GetLogicalOrigin()) + offset;
                break;
            case SeekSource::essEnd:
                target = static_cast<std::int64_t>(data_.size()) - offset - 1;
                break;
            case SeekSource::essCurrent:
                target = static_cast<std::int64_t>(position_) + offset;
                break;
            default:
                return false;
            }

            if (target < 0 || target > static_cast<std::int64_t>(data_.size()))
            {
                return false;
            }
            position_ = static_cast<std::size_t>(target);
            return true;
        }

        bool GetCurrentPosition(std::uint32_t& position) const override
        {
            if (!isOpen_ || position_ < GetLogicalOrigin())
            {
                return false;
            }
            position = static_cast<std::uint32_t>(position_ - GetLogicalOrigin());
            return true;
        }

        bool ReadData(void* destination, const std::uint32_t byteCount) override
        {
            if (!isOpen_ || destination == nullptr
                || byteCount > data_.size() - position_)
            {
                return false;
            }
            std::memcpy(destination, data_.data() + position_, byteCount);
            position_ += byteCount;
            return true;
        }

        bool WriteData(const void* source, const std::uint32_t byteCount) override
        {
            if (!isOpen_ || source == nullptr)
            {
                return false;
            }
            if (position_ + byteCount > data_.size())
            {
                data_.resize(position_ + byteCount);
            }
            std::memcpy(data_.data() + position_, source, byteCount);
            position_ += byteCount;
            return true;
        }

        bool vfunc_WriteFromStream(
            sparkplug::reconstruction::spStream* source,
            const std::uint32_t byteCount) override
        {
            if (rejectStreamCopy_ || source == nullptr)
            {
                return false;
            }
            std::vector<std::uint8_t> temporary(byteCount);
            return source->ReadData(temporary.data(), byteCount)
                && WriteData(temporary.data(), byteCount);
        }

        bool GetSize(std::uint32_t* size) const override
        {
            if (!isOpen_ || size == nullptr)
            {
                return false;
            }
            *size = static_cast<std::uint32_t>(data_.size());
            return true;
        }

        [[nodiscard]] const std::vector<std::uint8_t>& Data() const noexcept
        {
            return data_;
        }

        void RejectStreamCopy(const bool reject) noexcept
        {
            rejectStreamCopy_ = reject;
        }

    private:
        std::vector<std::uint8_t> data_;
        std::size_t position_ = 0;
        bool isOpen_ = false;
        bool rejectStreamCopy_ = false;
    };

    class TestFileStream final : public sparkplug::reconstruction::spFileStream
    {
    public:
        bool Open(std::uint32_t mode, const char* streamName) override
        {
            lastMode = mode;
            lastName = streamName == nullptr ? "" : streamName;
            return openResult;
        }

        bool Close() override { return true; }
        bool Seek(SeekSource, std::int32_t) override { return false; }
        bool GetCurrentPosition(std::uint32_t&) const override { return false; }
        bool ReadData(void*, std::uint32_t) override { return false; }
        bool WriteData(const void*, std::uint32_t) override { return false; }
        bool vfunc_WriteFromStream(
            sparkplug::reconstruction::spStream*, std::uint32_t) override
        {
            return false;
        }
        bool GetSize(std::uint32_t*) const override { return false; }

        std::uint32_t lastMode = 0;
        std::string lastName;
        bool openResult = true;
    };

    class TestAsyncFileStreamManager final
        : public sparkplug::reconstruction::spAsyncFileStreamManager
    {
    public:
        bool vfunc_Request(
            const char* streamName,
            sparkplug::reconstruction::spMemoryStream* destination,
            CompletionCallback completion,
            void* context) override
        {
            lastName = streamName;
            lastDestination = destination;
            if (completion != nullptr)
            {
                completion(context);
            }
            return requestResult;
        }

        const char* lastName = nullptr;
        sparkplug::reconstruction::spMemoryStream* lastDestination = nullptr;
        bool requestResult = true;
    };

    struct ErrorCapture final
    {
        std::size_t calls = 0;
        std::string text;
        sparkplug::reconstruction::spErrorSeverity severity =
            sparkplug::reconstruction::spErrorSeverity::Information;
        std::string sourceFile;
        std::uint32_t sourceLine = 0;
    };

    class TestErrorManager final
        : public sparkplug::reconstruction::spErrorManager
    {
    public:
        TestErrorManager(
            const std::size_t dataCapacity,
            ErrorCapture& capture)
            : spErrorManager(dataCapacity),
              capture_(&capture)
        {
        }

        [[nodiscard]] AnalysisHandlerBinding
            ResolveHandlerForAnalysis() const noexcept override
        {
            ++resolutionCount;
            return {&Capture, capture_};
        }

        mutable std::size_t resolutionCount = 0;

    private:
        static void Capture(
            const AnalysisDispatch& dispatch,
            void* const context)
        {
            auto& capture = *static_cast<ErrorCapture*>(context);
            ++capture.calls;
            capture.text = dispatch.text == nullptr ? "" : dispatch.text;
            capture.severity = dispatch.severity;
            capture.sourceFile = dispatch.sourceFile == nullptr
                ? ""
                : dispatch.sourceFile;
            capture.sourceLine = dispatch.sourceLine;
        }

        ErrorCapture* capture_ = nullptr;
    };

    class SubscriptionProbe final
        : public sparkplug::reconstruction::spBaseObject
    {
    public:
        void vfunc_0C(const void* const notification) noexcept override
        {
            ++calls;
            lastNotification = notification;
            if (order != nullptr)
            {
                order->push_back(this);
            }
        }

        std::size_t calls = 0;
        const void* lastNotification = nullptr;
        std::vector<sparkplug::reconstruction::spBaseObject*>* order = nullptr;
    };
}

int main()
{
    using namespace sparkplug::reconstruction;

    auto& rtti = spRTTIManager::Instance();
    // Static lifetime is mandatory for deferred registrations. Simulate a
    // derived TU being initialized before the TU containing its base record.
    static spRTTIRecord delayedBase{};
    static const spRTTIRecord delayedChild{
        0xFDF00002, 0xFDF00001, "test-delayed-child", &delayedBase, nullptr, nullptr};
    Require(!rtti.Register(delayedChild), "strict registration rejects an uninitialized base");
    Require(rtti.RegisterDeferredForAnalysis(delayedChild), "queue static child before base initialization");
    Require(rtti.RegisterDeferredForAnalysis(delayedChild), "duplicate pending record is harmless");
    Require(rtti.Find(delayedChild.classID) == nullptr, "deferred registration does not bypass base validation");
    delayedBase = {0xFDF00001, 0, "test-delayed-base", nullptr, nullptr, nullptr};
    Require(rtti.RegisterDeferredForAnalysis(delayedBase), "queue initialized static base");
    Require(rtti.Find(delayedChild.classID) == &delayedChild, "child becomes discoverable after base initialization");
    Require(rtti.Find(delayedBase.classID) == &delayedBase, "base is registered too");
    const auto delayedCount = rtti.GetRegistrationCount();
    Require(rtti.Register(delayedChild) && rtti.RegisterDeferredForAnalysis(delayedChild)
        && rtti.GetRegistrationCount() == delayedCount, "repeat registrations do not duplicate records");
    static const spRTTIRecord wrongBase{
        0xFDF00003, 0xFDF00099, "test-wrong-base", &delayedBase, nullptr, nullptr};
    Require(!rtti.Register(wrongBase) && !rtti.Find(wrongBase.classID), "strict mismatched base remains rejected");
    Require(rtti.GetRegistrationCount() >= 5, "SparkBase registrations are installed");

    const auto* baseRecord = rtti.Find(spBaseObject::ClassID);
    const auto* namedRecord = rtti.Find(spNamedObject::ClassID);
    const auto* crossPlatformRecord = rtti.Find(spCrossPlatform::ClassID);
    const auto* appRecord = rtti.Find(spApp::ClassID);
    const auto* asyncManagerRecord = rtti.Find(spAsyncFileStreamManager::ClassID);
    const auto* streamRecord = rtti.Find(spStream::ClassID);
    const auto* fileStreamRecord = rtti.Find(spFileStream::ClassID);
    const auto* memoryStreamRecord = rtti.Find(spMemoryStream::ClassID);
    const auto* pckManagerRecord = rtti.Find(spPCKManager::ClassID);
    const auto* errorRecord = rtti.Find(spError::ClassID);
    const auto* errorManagerRecord = rtti.Find(spErrorManager::ClassID);
    const auto* ps2ErrorManagerRecord = rtti.Find(spPS2ErrorManager::ClassID);
    const auto* subscriptionManagerRecord =
        rtti.Find(spSubscriptionManager::ClassID);
#if defined(_WIN32)
    const auto* pcAsyncManagerRecord = rtti.Find(spPCAsyncFileStreamManager::ClassID);
    const auto* pcFileStreamRecord = rtti.Find(spPCFileStream::ClassID);
    const auto* pcAppRecord = rtti.Find(spPCApp::ClassID);
    const auto* pcErrorManagerRecord = rtti.Find(spPCErrorManager::ClassID);
#endif
    Require(baseRecord != nullptr, "spBaseObject registration is present");
    Require(namedRecord != nullptr, "spNamedObject registration is present");
    Require(crossPlatformRecord != nullptr, "spCrossPlatform registration is present");
    Require(appRecord != nullptr, "spApp registration is present");
    Require(asyncManagerRecord != nullptr,
        "spAsyncFileStreamManager registration is present");
    Require(streamRecord != nullptr, "spStream registration is present");
    Require(fileStreamRecord != nullptr, "spFileStream registration is present");
    Require(memoryStreamRecord != nullptr,
        "spMemoryStream registration is present");
    Require(pckManagerRecord != nullptr,
        "spPCKManager registration is present");
    Require(errorRecord != nullptr, "spError registration is present");
    Require(errorManagerRecord != nullptr,
        "spErrorManager registration is present");
    Require(ps2ErrorManagerRecord != nullptr,
        "spPS2ErrorManager registration is present");
    Require(subscriptionManagerRecord != nullptr,
        "spSubscriptionManager registration is present");
#if defined(_WIN32)
    Require(pcAsyncManagerRecord != nullptr,
        "spPCAsyncFileStreamManager registration is present");
    Require(pcFileStreamRecord != nullptr,
        "spPCFileStream registration is present");
    Require(pcAppRecord != nullptr,
        "spPCApp registration is present");
    Require(pcErrorManagerRecord != nullptr,
        "spPCErrorManager registration is present");
#endif
    Require(baseRecord->base == nullptr, "spBaseObject is the registration root");
    Require(namedRecord->base == baseRecord, "spNamedObject derives from spBaseObject");
    Require(crossPlatformRecord->base == namedRecord,
        "spCrossPlatform derives from spNamedObject");
    Require(appRecord->base == baseRecord,
        "spApp native RTTI is deliberately flattened to spBaseObject");
    Require(streamRecord->base == crossPlatformRecord,
        "spStream derives from spCrossPlatform");
    Require(asyncManagerRecord->base == crossPlatformRecord,
        "spAsyncFileStreamManager derives from spCrossPlatform");
    Require(memoryStreamRecord->base == streamRecord,
        "spMemoryStream derives from spStream");
    Require(fileStreamRecord->base == streamRecord,
        "spFileStream derives from spStream");
    Require(pckManagerRecord->base == baseRecord,
        "spPCKManager derives directly from spBaseObject");
    Require(errorRecord->base == baseRecord,
        "spError derives directly from spBaseObject");
    Require(errorManagerRecord->base == baseRecord,
        "spErrorManager derives directly from spBaseObject");
    Require(ps2ErrorManagerRecord->base == errorManagerRecord,
        "spPS2ErrorManager derives from the common error manager");
    Require(subscriptionManagerRecord->base == baseRecord,
        "spSubscriptionManager derives directly from spBaseObject");
#if defined(_WIN32)
    Require(pcAsyncManagerRecord->base == asyncManagerRecord,
        "spPCAsyncFileStreamManager derives from common async manager");
    Require(pcFileStreamRecord->base == fileStreamRecord,
        "spPCFileStream derives from spFileStream");
    Require(pcAppRecord->base == appRecord,
        "spPCApp derives from spApp in native RTTI");
    Require(pcErrorManagerRecord->base == errorManagerRecord,
        "spPCErrorManager derives from the common error manager");
#endif
    Require(baseRecord->factory == nullptr, "spBaseObject is not directly creatable");
    Require(crossPlatformRecord->factory == nullptr,
        "spCrossPlatform has no native RTTI factory");
    Require(appRecord->factory == nullptr,
        "spApp has no native RTTI factory");
    Require(streamRecord->factory == nullptr, "spStream has no native RTTI factory");
    Require(asyncManagerRecord->factory == nullptr,
        "abstract spAsyncFileStreamManager has no native RTTI factory");
    Require(memoryStreamRecord->factory != nullptr,
        "spMemoryStream has a native RTTI factory");
    Require(fileStreamRecord->factory == nullptr,
        "abstract spFileStream has no native RTTI factory");
    Require(pckManagerRecord->factory != nullptr,
        "spPCKManager has a native RTTI factory");
    Require(errorRecord->factory != nullptr,
        "spError has a native RTTI factory");
    Require(errorManagerRecord->factory == nullptr,
        "abstract spErrorManager has no native RTTI factory");
    Require(ps2ErrorManagerRecord->factory != nullptr,
        "spPS2ErrorManager has a native RTTI factory");
    Require(subscriptionManagerRecord->factory != nullptr,
        "spSubscriptionManager has a native RTTI factory");
#if defined(_WIN32)
    Require(pcAppRecord->factory == nullptr,
        "spPCApp has no native RTTI factory");
    Require(pcErrorManagerRecord->factory != nullptr,
        "spPCErrorManager has a native RTTI factory");
#endif

    Require(rtti.Create(spBaseObject::ClassID) == nullptr, "root factory is null");
    Require(rtti.Create(spCrossPlatform::ClassID) == nullptr,
        "spCrossPlatform factory is null");
    Require(rtti.Create(spApp::ClassID) == nullptr,
        "spApp factory is null");
    Require(rtti.Create(spStream::ClassID) == nullptr, "spStream factory is null");
    Require(rtti.Create(spAsyncFileStreamManager::ClassID) == nullptr,
        "spAsyncFileStreamManager factory is null");
    auto createdMemoryStream = rtti.Create(spMemoryStream::ClassID);
    Require(dynamic_cast<spMemoryStream*>(createdMemoryStream.get()) != nullptr,
        "spMemoryStream factory creates the concrete native type");
    Require(rtti.Create(spFileStream::ClassID) == nullptr,
        "spFileStream cannot be created through RTTI");
    auto createdPCKManager = rtti.Create(spPCKManager::ClassID);
    Require(dynamic_cast<spPCKManager*>(createdPCKManager.get()) != nullptr,
        "spPCKManager factory creates the concrete native type");
    createdPCKManager.reset();
    auto createdError = rtti.Create(spError::ClassID);
    Require(dynamic_cast<spError*>(createdError.get()) != nullptr,
        "spError factory creates the native error type");
    Require(rtti.Create(spErrorManager::ClassID) == nullptr,
        "abstract spErrorManager cannot be created through RTTI");
    auto createdPS2ErrorManager = rtti.Create(spPS2ErrorManager::ClassID);
    auto* ps2ErrorManager =
        dynamic_cast<spPS2ErrorManager*>(createdPS2ErrorManager.get());
    Require(ps2ErrorManager != nullptr
            && ps2ErrorManager->GetDataCapacityForAnalysis()
                == spErrorManager::Ps2DataCapacity,
        "PS2 error-manager factory preserves its 0x400-byte stack");
    createdPS2ErrorManager.reset();
    auto createdSubscriptionManager =
        rtti.Create(spSubscriptionManager::ClassID);
    Require(dynamic_cast<spSubscriptionManager*>(
                createdSubscriptionManager.get()) != nullptr,
        "subscription-manager RTTI factory creates its concrete type");
    createdSubscriptionManager.reset();
#if defined(_WIN32)
    auto createdPCAsyncManager = rtti.Create(spPCAsyncFileStreamManager::ClassID);
    Require(dynamic_cast<spPCAsyncFileStreamManager*>(createdPCAsyncManager.get())
            != nullptr,
        "PC async-manager factory creates the stateless platform leaf");
    createdPCAsyncManager.reset();
    auto createdPCFileStream = rtti.Create(spPCFileStream::ClassID);
    Require(dynamic_cast<spPCFileStream*>(createdPCFileStream.get()) != nullptr,
        "spPCFileStream factory creates the platform leaf");
    auto createdPCErrorManager = rtti.Create(spPCErrorManager::ClassID);
    auto* pcErrorManager =
        dynamic_cast<spPCErrorManager*>(createdPCErrorManager.get());
    Require(pcErrorManager != nullptr
            && pcErrorManager->GetDataCapacityForAnalysis()
                == spErrorManager::PcDataCapacity,
        "PC error-manager factory preserves its 0x1000-byte stack");
    createdPCErrorManager.reset();
#endif
    auto created = rtti.Create(spNamedObject::ClassID);
    Require(created != nullptr, "spNamedObject factory creates an object");
    Require(created->IsExactly(spNamedObject::ClassID), "exact type test uses current RTTI");
    Require(created->IsKindOf(spBaseObject::ClassID), "base-chain type test reaches the root");
    Require(!created->IsExactly(spBaseObject::ClassID), "exact and inherited checks differ");

    auto* named = dynamic_cast<spNamedObject*>(created.get());
    Require(named != nullptr, "factory result has the expected C++ type");
    Require(named->GetName() == nullptr, "new named object has no name entry");
    named->SetName("Alfea02");
    Require(std::strcmp(named->GetName(), "Alfea02") == 0, "name setter stores text");

    auto cloneBase = named->Clone();
    auto* clone = dynamic_cast<spNamedObject*>(cloneBase.get());
    Require(clone != nullptr, "named object clone keeps its type");
    Require(clone != named, "clone is a distinct object");
    Require(std::strcmp(clone->GetName(), "Alfea02") == 0, "clone copies the shared name");

    spBaseObject root;
    Require(root.Clone() == nullptr, "root clone slot returns null");
    root.vfunc_0C(nullptr);

    spCrossPlatform crossPlatform;
    crossPlatform.SetName("platform branch");
    Require(crossPlatform.IsExactly(spCrossPlatform::ClassID),
        "spCrossPlatform exact type uses its own registration");
    Require(crossPlatform.IsKindOf(spNamedObject::ClassID),
        "spCrossPlatform reaches spNamedObject in the RTTI chain");
    Require(crossPlatform.IsKindOf(spBaseObject::ClassID),
        "spCrossPlatform reaches spBaseObject in the RTTI chain");
    Require(crossPlatform.Clone() == nullptr,
        "spCrossPlatform preserves the native null-clone behavior");

    {
        class TestApp final : public spApp
        {
        public:
            bool vfunc_20_Initialize() override { return true; }
            bool vfunc_24_Update() override { return true; }
            void vfunc_28_Shutdown() override {}
            bool vfunc_2C_Run() override { return true; }
        } app;
        Require(spApp::GetInstance() == &app,
            "spApp constructor publishes the process-wide instance");
        Require(app.IsExactly(spApp::ClassID)
                && app.IsKindOf(spBaseObject::ClassID)
                && !app.IsKindOf(spCrossPlatform::ClassID),
            "spApp queries follow native engine RTTI rather than C++ layout");
        Require(dynamic_cast<spCrossPlatform*>(&app) != nullptr,
            "spApp C++ construction still uses spCrossPlatform");
        Require(app.Clone() == nullptr,
            "spApp preserves the native null-clone contract");
        Require(std::strcmp(app.vfunc_GetEmptyString(), "") == 0,
            "spApp default string virtual returns the shared empty literal");
        Require(!app.GetStateFlagForAnalysis()
                && app.GetOwnedTextForAnalysis() == nullptr,
            "spApp constructor zeroes the trailing native fields");
        app.SetStateFlagForAnalysis(true);
        app.SetOwnedTextForAnalysis("Winx");
        Require(app.GetStateFlagForAnalysis()
                && std::strcmp(app.GetOwnedTextForAnalysis(), "Winx") == 0,
            "spApp analytical field seam owns its text safely");
    }
    Require(spApp::GetInstance() == nullptr,
        "spApp destructor clears the process-wide instance");

    {
        ErrorCapture capture;
        TestErrorManager errorManager(64, capture);
        Require(spErrorManager::GetInstance() == &errorManager,
            "spErrorManager constructor publishes the singleton");
        Require(errorManager.IsExactly(spErrorManager::ClassID)
                && errorManager.IsKindOf(spBaseObject::ClassID),
            "spErrorManager follows its exact native RTTI chain");
        Require(errorManager.Clone() == nullptr,
            "abstract spErrorManager preserves the native null clone");

        spError warning(
            spErrorSeverity::Warning,
            static_cast<std::uint32_t>(spErrorCode::Message),
            "warning.cpp",
            17);
        Require(errorManager.StoreMessageForAnalysis(warning, "warning text"),
            "error manager stores a message in its fixed analysis stack");
        errorManager.LinkErrorForAnalysis(warning);

        spError error(
            spErrorSeverity::Error,
            static_cast<std::uint32_t>(spErrorCode::ManagerDataStackOverflow),
            "manager.cpp",
            41);
        errorManager.LinkErrorForAnalysis(error);
        const auto chain = errorManager.FormatChainForAnalysis(true);
        Require(chain.find("ERROR: Error manager data stack overflow")
                != std::string::npos
                && chain.find("manager.cpp(41)") != std::string::npos
                && chain.find("WARNING: warning text") != std::string::npos,
            "error chain formatting preserves severity, source and LIFO order");
        Require(errorManager.HandleForAnalysis(spErrorSeverity::Error),
            "error manager dispatches a chain meeting the threshold");
        Require(capture.calls == 1
                && capture.severity == spErrorSeverity::Error
                && capture.sourceFile == "manager.cpp"
                && capture.sourceLine == 41
                && capture.text == chain,
            "resolved error handler receives the proven four-part dispatch");
        Require(errorManager.resolutionCount == 1
                && errorManager.GetDataUsedForAnalysis() == 0,
            "handler is resolved lazily and handled chains clear stack usage");

        spError second(
            spErrorSeverity::Warning,
            static_cast<std::uint32_t>(spErrorCode::None),
            nullptr,
            0);
        errorManager.LinkErrorForAnalysis(second);
        Require(!errorManager.HandleForAnalysis(spErrorSeverity::Error)
                && capture.calls == 1,
            "below-threshold error remains queued and is not dispatched");
        Require(errorManager.HandleForAnalysis(spErrorSeverity::Warning)
                && capture.calls == 2
                && errorManager.resolutionCount == 1,
            "cached handler is reused for a later accepted chain");

        auto cloneBase = warning.Clone();
        auto* blankClone = dynamic_cast<spError*>(cloneBase.get());
        Require(blankClone != nullptr
                && blankClone->GetCodeForAnalysis() == 0
                && blankClone->GetSeverityForAnalysis()
                    == spErrorSeverity::Information
                && blankClone->GetMessageForAnalysis() == nullptr,
            "spError native clone deliberately leaves the payload blank");
    }
    Require(spErrorManager::GetInstance() == nullptr,
        "spErrorManager destructor clears the singleton");

    {
        ErrorCapture capture;
        TestErrorManager smallManager(8, capture);
        spError overflow;
        Require(!smallManager.StoreMessageForAnalysis(overflow, "1234567")
                && smallManager.HadDataOverflowForAnalysis()
                && smallManager.GetDataUsedForAnalysis() == 0,
            "fixed error stack rejects an allocation ending at capacity");
    }

    {
        spPS2ErrorManager ps2Manager;
        spError warning(
            spErrorSeverity::Warning,
            static_cast<std::uint32_t>(spErrorCode::Message),
            "ps2.cpp",
            9);
        Require(ps2Manager.StoreMessageForAnalysis(warning, "PS2 warning"),
            "PS2 leaf uses the common fixed-stack path");
        ps2Manager.LinkErrorForAnalysis(warning);
        Require(ps2Manager.HandleForAnalysis(spErrorSeverity::Information)
                && ps2Manager.GetDataUsedForAnalysis() == 0,
            "PS2 leaf resolves and invokes its native no-op handler");
    }

    {
        spSubscriptionManager subscriptionManager;
        SubscriptionProbe first;
        SubscriptionProbe second;
        const std::uint32_t notification = 0x12345678;

        Require(spSubscriptionManager::GetInstance() == &subscriptionManager,
            "subscription manager publishes its process-wide instance");
        Require(subscriptionManager.SubscribeForAnalysis(5, first)
                && !subscriptionManager.SubscribeForAnalysis(5, first)
                && subscriptionManager.SubscribeForAnalysis(5, second)
                && subscriptionManager.SubscribeForAnalysis(7, second),
            "subscription manager groups subscribers and suppresses duplicates");
        Require(subscriptionManager.GetGroupCountForAnalysis() == 2
                && subscriptionManager.GetSubscriberCountForAnalysis(5) == 2,
            "subscription-manager analytical counts expose the proven grouping");
        Require(subscriptionManager.DispatchForAnalysis(5, &notification) == 2
                && first.calls == 1 && second.calls == 1
                && first.lastNotification == &notification
                && second.lastNotification == &notification,
            "dispatch invokes the base notification slot of every subscriber");
        Require(subscriptionManager.DispatchForAnalysis(99, &notification) == 0,
            "dispatch of an unknown key has no side effects");
        Require(subscriptionManager.UnsubscribeForAnalysis(5, first)
                && !subscriptionManager.UnsubscribeForAnalysis(5, first)
                && subscriptionManager.GetSubscriberCountForAnalysis(5) == 1,
            "unsubscribe removes only the requested subscriber");
        Require(subscriptionManager.UnsubscribeForAnalysis(5, second)
                && subscriptionManager.GetGroupCountForAnalysis() == 1,
            "removing the final subscriber erases its empty group");

        auto cloneBase = subscriptionManager.Clone();
        auto* clone = dynamic_cast<spSubscriptionManager*>(cloneBase.get());
        Require(clone != nullptr && clone->GetGroupCountForAnalysis() == 0,
            "native subscription-manager clone starts with an empty graph");
    }
    Require(spSubscriptionManager::GetInstance() == nullptr,
        "subscription-manager destruction clears its singleton");

    {
        // Original PC416150/415A20 dispatches in unsigned address order,
        // independently of arrival order (native capture 2026-09-10).
        spSubscriptionManager manager;
        SubscriptionProbe probes[3];
        std::vector<spBaseObject*> expected{&probes[0], &probes[1], &probes[2]};
        std::sort(expected.begin(), expected.end(), std::less<spBaseObject*>{});
        std::vector<spBaseObject*> observed;
        observed.reserve(3);
        for (auto& probe : probes) probe.order = &observed;
        for (auto i = expected.rbegin(); i != expected.rend(); ++i)
            Require(manager.SubscribeForAnalysis(42, **i), "reverse subscription accepted");
        Require(manager.DispatchForAnalysis(42, nullptr) == 3 && observed == expected,
            "original subscription dispatch follows pointer order, not insertion order");
        Require(manager.UnsubscribeForAnalysis(42, *expected[1])
                && manager.SubscribeForAnalysis(42, *expected[1]),
            "native middle subscriber can be removed and reinserted");
        observed.clear();
        Require(manager.DispatchForAnalysis(42, nullptr) == 3 && observed == expected,
            "reinsertion preserves original pointer ordering");
    }

#if defined(_WIN32)
    {
        const auto information = spPCErrorManager::ClassifyForAnalysis(
            spErrorSeverity::Information);
        const auto warning = spPCErrorManager::ClassifyForAnalysis(
            spErrorSeverity::Warning);
        const auto error = spPCErrorManager::ClassifyForAnalysis(
            spErrorSeverity::Error);
        const auto fatal = spPCErrorManager::ClassifyForAnalysis(
            spErrorSeverity::Fatal);
        Require(std::strcmp(information.title, "Information") == 0
                && information.messageBoxFlags == 0x40
                && information.consoleCode == 0x0A,
            "PC information severity maps to native UI/console values");
        Require(std::strcmp(warning.title, "Warning") == 0
                && warning.messageBoxFlags == 0x30
                && warning.consoleCode == 0x0E,
            "PC warning severity maps to native UI/console values");
        Require(std::strcmp(error.title, "Error") == 0
                && error.messageBoxFlags == 0x10
                && error.consoleCode == 0x04,
            "PC error severity maps to native UI/console values");
        Require(std::strcmp(fatal.title, "Fatal error") == 0
                && fatal.messageBoxFlags == 0x10
                && fatal.consoleCode == 0x04,
            "PC fatal severity maps to its distinct native title");

        spPCErrorManager pcManager;
        spError diagnostic(
            spErrorSeverity::Error,
            static_cast<std::uint32_t>(spErrorCode::Message),
            "pc.cpp",
            12);
        Require(pcManager.StoreMessageForAnalysis(diagnostic, "PC error"),
            "PC leaf uses the common fixed-stack path");
        pcManager.LinkErrorForAnalysis(diagnostic);
        Require(pcManager.HandleForAnalysis(spErrorSeverity::Error),
            "PC leaf invokes the safe non-modal reconstruction handler");
    }

    {
        struct PumpProbe final
        {
            int calls = 0;
        } probe;
        const auto pump = [](void* context)
        {
            auto& state = *static_cast<PumpProbe*>(context);
            ++state.calls;
            if (state.calls == 1)
            {
                return spPCApp::PumpResult::DispatchedMessage;
            }
            if (state.calls == 2)
            {
                return spPCApp::PumpResult::NoMessage;
            }
            return spPCApp::PumpResult::Quit;
        };

        spPCApp pcApp;
        Require(spApp::GetInstance() == &pcApp,
            "spPCApp publishes itself through the inherited app singleton");
        Require(pcApp.IsExactly(spPCApp::ClassID)
                && pcApp.IsKindOf(spApp::ClassID),
            "spPCApp preserves its native registration chain");
        Require(pcApp.GetWindowWidthForAnalysis() == 0x400
                && pcApp.GetWindowHeightForAnalysis() == 0x300
                && pcApp.GetWindowStyleForAnalysis() == 0x00CA0000,
            "spPCApp constructor restores native window defaults");
        Require(!pcApp.vfunc_20_Initialize(),
            "safe PC app facade contains real window creation without a backend");
        Require(!pcApp.vfunc_2C_Run(),
            "safe PC app facade refuses an unbounded loop without a message pump");
        pcApp.SetMessagePumpForAnalysis(pump, &probe);
        Require(pcApp.vfunc_2C_Run() && probe.calls == 3
                && pcApp.GetStateFlagForAnalysis(),
            "PC message loop dispatches queued work, updates on idle and exits on quit");
        Require(pcApp.Clone() == nullptr,
            "spPCApp keeps the native null-clone contract");
    }
    Require(spApp::GetInstance() == nullptr,
        "spPCApp destruction clears the inherited singleton");
#endif

    {
        class TestPS2App final : public spPS2App
        {
        public:
            bool vfunc_24_Update() override
            {
                return updates++ < 2;
            }

            int updates = 0;
        } ps2App;

        Require(spPS2App::StaticRTTI().base == &spApp::StaticRTTI()
                && ps2App.IsKindOf(spApp::ClassID),
            "spPS2App preserves the native platform inheritance edge");
        Require(ps2App.vfunc_20_Initialize(),
            "spPS2App default initialize callback returns true");
        Require(!ps2App.vfunc_2C_Run() && ps2App.updates == 3
                && ps2App.GetStateFlagForAnalysis(),
            "spPS2App loop runs the concrete update hook until false");
        Require(ps2App.Clone() == nullptr,
            "spPS2App keeps the native null-clone contract");
    }

    bool asyncCallbackRan = false;
    {
        TestAsyncFileStreamManager asyncManager;
        Require(spAsyncFileStreamManager::GetInstance() == &asyncManager,
            "async manager constructor installs the process-wide instance");
        auto completion = [](void* context)
        {
            *static_cast<bool*>(context) = true;
        };
        Require(asyncManager.vfunc_Request(
                "async.smo", nullptr, completion, &asyncCallbackRan)
                && asyncCallbackRan
                && std::strcmp(asyncManager.lastName, "async.smo") == 0,
            "async manager request preserves the observed callback contract");
        asyncManager.vfunc_Update();
        Require(asyncManager.IsExactly(spAsyncFileStreamManager::ClassID)
                && asyncManager.IsKindOf(spCrossPlatform::ClassID),
            "async manager exposes its native RTTI position");
    }
    Require(spAsyncFileStreamManager::GetInstance() == nullptr,
        "async manager destructor clears the process-wide instance");

    const auto lowPriorityPCK = BuildTestPCK({
        {"data/other", "hero.smo", 2, 17},
        {"data/test", "hero.smo", 3, 29},
    });
    const auto highPriorityPCK = BuildTestPCK({
        {"data/test", "hero.smo", 4, 41},
    });
    {
        spPCKManager pckManager;
        Require(spPCKManager::GetInstance() == &pckManager,
            "PCK manager constructor installs the process-wide instance");
        Require(pckManager.AddPackageImage(
                "low.pck", 2, lowPriorityPCK.data(), lowPriorityPCK.size(), 0x2000),
            "PCK manager parses a synthetic native package index");
        Require(pckManager.AddPackageImage(
                "high.pck", 7, highPriorityPCK.data(), highPriorityPCK.size(), 0x4000),
            "PCK manager accepts a higher-priority package");
        Require(pckManager.GetPackageCount() == 2
                && pckManager.GetPackage(0)->packageName == "high.pck",
            "PCK packages are searched in descending priority order");
        Require(pckManager.AddPackageImage(
                "high.pck", 99, lowPriorityPCK.data(), lowPriorityPCK.size()),
            "native duplicate package request reports success");
        Require(pckManager.GetPackageCount() == 2,
            "duplicate package request does not add an entry");

        char resolvedPackage[64]{};
        std::uint32_t packageSize = 0;
        std::uint32_t packageOrigin = 0;
        std::uint32_t logicalSector = 0;
        std::uint32_t byteOffset = 0;
        std::uint32_t byteCount = 0;
        Require(pckManager.vfunc_Resolve(
                ".\\DATA\\TEST\\HERO.SMO", resolvedPackage,
                &packageSize, &packageOrigin, &logicalSector,
                &byteOffset, &byteCount),
            "PCK resolver strips dot-prefix, normalizes slashes and lowercases");
        Require(std::strcmp(resolvedPackage, "high.pck") == 0
                && packageSize == highPriorityPCK.size()
                && packageOrigin == 0x4000
                && logicalSector == 4
                && byteOffset == 0x2000
                && byteCount == 41,
            "PCK resolver returns package and all three file-index outputs");
        Require(!pckManager.vfunc_Resolve(
                "data/missing/hero.smo", resolvedPackage,
                &packageSize, &packageOrigin, &logicalSector,
                &byteOffset, &byteCount),
            "PCK resolver requires both directory and basename to match");

        auto clonedPCKBase = pckManager.Clone();
        auto* clonedPCK = dynamic_cast<spPCKManager*>(clonedPCKBase.get());
        Require(clonedPCK != nullptr && clonedPCK->GetPackageCount() == 0,
            "native PCK clone slot does not copy mounted packages");
        clonedPCKBase.reset();

        Require(pckManager.RemovePackage("high.pck")
                && pckManager.GetPackageCount() == 1,
            "PCK package removal releases one exact-name entry");
        Require(pckManager.vfunc_Resolve(
                "data/test/hero.smo", resolvedPackage,
                &packageSize, &packageOrigin, &logicalSector,
                &byteOffset, &byteCount)
                && std::strcmp(resolvedPackage, "low.pck") == 0
                && logicalSector == 3,
            "PCK resolver falls through to the next mounted package");
    }
    Require(spPCKManager::GetInstance() == nullptr,
        "PCK manager destructor clears the process-wide instance");

    auto malformedPCK = lowPriorityPCK;
    const auto stringBytes = static_cast<std::size_t>(malformedPCK[0])
        | (static_cast<std::size_t>(malformedPCK[1]) << 8U);
    const auto firstPCKRecord = 4 + stringBytes + 4;
    StoreLittleEndian32(malformedPCK, firstPCKRecord, 0xFFFFFFFFU);
    spPCKManager safePCKParser;
    Require(!safePCKParser.AddPackageImage(
            "malformed.pck", 0, malformedPCK.data(), malformedPCK.size()),
        "portable PCK parser contains an out-of-range string offset");

    const auto unsortedPCK = BuildTestPCK({
        {"data", "z.smo", 2, 1},
        {"data", "a.smo", 3, 1},
    });
    Require(!safePCKParser.AddPackageImage(
            "unsorted.pck", 0, unsortedPCK.data(), unsortedPCK.size()),
        "portable parser rejects ordering that breaks the native binary search");

    {
        spPS2Helper ps2Helper;
        Require(spPS2Helper::GetInstance() == &ps2Helper,
            "PS2 helper constructor publishes the singleton instance");
        Require(ps2Helper.StaticRTTI().classID == spPS2Helper::ClassID
                && ps2Helper.StaticRTTI().base == &spBaseObject::StaticRTTI(),
            "PS2 helper preserves its native RTTI identity and base");
        Require(ps2Helper.GetMode() == 0
                && ps2Helper.GetField18() == 0
                && ps2Helper.GetPathPrefix() == "host0:",
            "PS2 helper constructor restores native defaults");
        Require(ps2Helper.sub_001E91B0("data\\models\\flora.smo")
                == "host0:data/models/flora.smo",
            "PS2 host mode prefixes host0 and converts backslashes");
        Require(ps2Helper.sub_001E91B0("host0:data/models/flora.smo")
                == "host0:data/models/flora.smo",
            "PS2 helper passes an already-qualified host path through");
        Require(ps2Helper.sub_001E91B0("host0:data\\models\\flora.smo")
                == "HOST0:DATA\\MODELS\\FLORA.SMO",
            "qualified host path with backslashes follows native uppercase branch");

        ps2Helper.sub_001E93A0(1);
        Require(ps2Helper.sub_001E91B0("./data/models/flora.smo")
                == "\\DATA\\MODELS\\FLORA.SMO;1",
            "PS2 disc mode strips a leading dot component and appends version one");
        Require(ps2Helper.sub_001E91B0(".\\data\\models\\flora.smo")
                == "\\DATA\\MODELS\\FLORA.SMO;1",
            "PS2 disc mode accepts either leading separator spelling");
        Require(ps2Helper.sub_001E91B0("\\DATA\\MODELS\\FLORA.SMO;1")
                == "\\DATA\\MODELS\\FLORA.SMO;1",
            "fully-qualified PS2 disc path is preserved");

        ps2Helper.sub_001E93A0(2);
        Require(ps2Helper.SetPathPrefixForAnalysis("mass:")
                && ps2Helper.sub_001E91B0("a\\b") == "mass:a/b",
            "PS2 mode two uses the configured host-style prefix");
        Require(!ps2Helper.SetPathPrefixForAnalysis(nullptr),
            "safe analytical prefix seam rejects null input");

        auto helperCloneBase = ps2Helper.Clone();
        auto* helperClone = dynamic_cast<spPS2Helper*>(helperCloneBase.get());
        Require(helperClone != nullptr
                && helperClone->GetMode() == 0
                && helperClone->GetField18() == 0
                && helperClone->GetPathPrefix() == "host0:",
            "native PS2 helper clone retains only base state");
        helperCloneBase.reset();
    }
    Require(spPS2Helper::GetInstance() == nullptr,
        "PS2 helper support destructor clears its singleton");

    {
        struct ModuleLoaderProbe final
        {
            std::size_t calls = 0;
            std::size_t failuresRemaining = 0;
            bool failForever = false;
            std::string lastPath;
        } probe;
        const auto loader = [](const char* path, void* context) -> int
        {
            auto& state = *static_cast<ModuleLoaderProbe*>(context);
            ++state.calls;
            state.lastPath = path == nullptr ? "" : path;
            if (state.failForever)
            {
                return -1;
            }
            if (state.failuresRemaining != 0)
            {
                --state.failuresRemaining;
                return -1;
            }
            return 7;
        };

        spPS2IOPModuleManager moduleManager;
        Require(spPS2IOPModuleManager::GetInstance() == &moduleManager,
            "PS2 IOP module manager publishes its singleton");
        Require(moduleManager.StaticRTTI().classID
                    == spPS2IOPModuleManager::ClassID
                && moduleManager.StaticRTTI().base
                    == &spBaseObject::StaticRTTI(),
            "PS2 IOP module manager preserves native RTTI");
        Require(moduleManager.GetModuleRoot()
                    == "host0:c:/usr/local/sce/iop/modules/"
                && !moduleManager.GetField18()
                && moduleManager.GetLoadedModuleCount() == 0,
            "PS2 IOP module manager restores native defaults");
        Require(!moduleManager.sub_001E98C0("SIO2MAN", false),
            "host reconstruction refuses a fake load without a backend");

        probe.failuresRemaining = 2;
        moduleManager.SetModuleLoaderForAnalysis(loader, &probe);
        Require(moduleManager.sub_001E98C0("SIO2MAN", false)
                && probe.calls == 3
                && probe.lastPath
                    == "host0:c:/usr/local/sce/iop/modules/SIO2MAN.IRX"
                && moduleManager.GetLoadedModuleCount() == 1
                && *moduleManager.GetLoadedModule(0) == "SIO2MAN",
            "PS2 module load retries, builds the IRX path and records success");
        Require(moduleManager.sub_001E98C0("SIO2MAN", false)
                && probe.calls == 3,
            "PS2 duplicate module request returns without loading again");
        Require(moduleManager.sub_001E98C0(
                    "SIO2MAN", true, "cdrom0:\\modules\\")
                && probe.calls == 4
                && probe.lastPath == "cdrom0:\\modules\\SIO2MAN.IRX"
                && moduleManager.GetLoadedModuleCount() == 2,
            "forced PS2 module reload uses an override root and appends a duplicate");
        Require(moduleManager.sub_001E9860("mass:/modules/")
                && moduleManager.sub_001E98C0("PADMAN", false)
                && probe.lastPath == "mass:/modules/PADMAN.IRX",
            "PS2 module root setter affects later loads");
        Require(!moduleManager.sub_001E9860(nullptr),
            "safe host root setter contains native null misuse");

        probe.calls = 0;
        probe.failForever = true;
        Require(!moduleManager.sub_001E98C0("MISSING", false)
                && probe.calls
                    == spPS2IOPModuleManager::NativeMaximumLoadAttempts,
            "PS2 module failure follows the exact bounded retry count");
        moduleManager.sub_001E9B20();
        Require(moduleManager.GetField18(),
            "PS2 module manager exposes the one-way native flag setter");

        auto moduleCloneBase = moduleManager.Clone();
        auto* moduleClone = dynamic_cast<spPS2IOPModuleManager*>(
            moduleCloneBase.get());
        Require(moduleClone != nullptr
                && moduleClone->GetModuleRoot()
                    == "host0:c:/usr/local/sce/iop/modules/"
                && !moduleClone->GetField18()
                && moduleClone->GetLoadedModuleCount() == 0,
            "native PS2 module-manager clone retains only base state");
        moduleCloneBase.reset();
    }
    Require(spPS2IOPModuleManager::GetInstance() == nullptr,
        "PS2 IOP module manager support destructor clears its singleton");

    TestStream stream;
    Require(stream.Open("test-memory"), "test stream opens");
    Require(std::strcmp(stream.GetStreamName(), "test-memory") == 0,
        "spStream keeps its owned diagnostic name");
    Require(stream.IsExactly(spStream::ClassID),
        "spStream exact type uses its registration");
    Require(stream.IsKindOf(spCrossPlatform::ClassID),
        "spStream reaches spCrossPlatform in the RTTI chain");
    Require(stream.Clone() == nullptr, "spStream preserves native null-clone behavior");
    Require(stream.GetBuffer() == nullptr, "base spStream buffer slot returns null");

    const std::uint32_t marker = 0x1234ABCD;
    Require(stream.Write(marker), "typed Write forwards to WriteData");
    Require(stream.Write("Bloom"), "string Write stores a length and terminator");
    std::uint32_t streamSize = 0;
    Require(stream.GetSize(&streamSize) && streamSize == 12,
        "typed and length-prefixed writes have the native byte size");
    Require(stream.Seek(spStream::SeekSource::essStart, 0), "seek to stream start");
    std::uint32_t readMarker = 0;
    std::string readString;
    Require(stream.Read(readMarker) && readMarker == marker,
        "typed Read forwards to ReadData");
    Require(stream.ReadString(readString) && readString == "Bloom",
        "length-prefixed string round trip");

    TestStream stringEdges;
    Require(stringEdges.Open("string edges"), "string edge-case stream opens");
    Require(stringEdges.Write(nullptr, true),
        "null string with prefix writes a zero length");
    Require(stringEdges.Data() == std::vector<std::uint8_t>({0, 0}),
        "null string prefix is the native little-endian u16 zero");
    const auto nullPrefixedSize = stringEdges.Data().size();
    Require(stringEdges.Write(nullptr, false),
        "null string without prefix is a successful no-op");
    Require(stringEdges.Data().size() == nullPrefixedSize,
        "null string without prefix writes no bytes");
    Require(stringEdges.Write(""),
        "empty string with prefix writes its terminator");
    Require(stringEdges.Data()
            == std::vector<std::uint8_t>({0, 0, 1, 0, 0}),
        "empty string uses length one followed by a terminator");

    TestStream malformedString;
    Require(malformedString.Open("malformed string"),
        "malformed-string test stream opens");
    const std::uint8_t unterminated[] = {3, 0, 'a', 'b', 'c'};
    Require(malformedString.WriteData(unterminated, sizeof(unterminated)),
        "malformed string bytes are staged");
    Require(malformedString.Seek(spStream::SeekSource::essStart, 0),
        "malformed string stream rewinds");
    std::string safeMalformedRead;
    Require(malformedString.ReadString(safeMalformedRead)
            && safeMalformedRead == "abc",
        "portable reader contains an unterminated native string safely");

    TestStream source;
    TestStream destination;
    Require(source.Open(1, "source") && destination.Open("destination"),
        "both Open slots are callable");
    const std::uint8_t payload[] = {1, 3, 3, 7};
    Require(source.WriteData(payload, sizeof(payload)), "copy source is populated");
    Require(source.Seek(spStream::SeekSource::essStart, 0), "copy source rewinds");
    Require(source.CopyTo(destination),
        "whole-stream helper invokes the platform-specific stream-write slot");
    Require(destination.Data() == std::vector<std::uint8_t>({1, 3, 3, 7}),
        "whole-stream helper copies the full payload");
    TestStream rejectingDestination;
    Require(rejectingDestination.Open("rejecting destination"),
        "rejecting destination opens");
    rejectingDestination.RejectStreamCopy(true);
    Require(source.Seek(spStream::SeekSource::essStart, 0),
        "copy source rewinds for failure-contract test");
    Require(source.CopyTo(rejectingDestination),
        "native whole-stream helper ignores the destination slot result");
    Require(rejectingDestination.Data().empty(),
        "rejecting destination confirms its stream-write slot failed");
    TestStream closedSource;
    Require(!closedSource.CopyTo(destination),
        "whole-stream helper propagates source GetSize failure");
    Require(source.Close() && destination.Close()
            && rejectingDestination.Close() && malformedString.Close()
            && stringEdges.Close() && stream.Close(),
        "Close slot is callable");

    spMemoryStream memory;
    Require(memory.GetBuffer() == nullptr,
        "default memory stream has no allocation");
    Require(!memory.GetSize(&streamSize),
        "default memory stream reports unopened through GetSize");
    Require(memory.Seek(spStream::SeekSource::essStart, 0),
        "native seek fast path accepts zero on a closed empty memory stream");
    Require(memory.ReadData(nullptr, 0),
        "native read fast path accepts a zero-byte closed read");
    Require(!memory.WriteData(nullptr, 0),
        "memory write preflight checks the buffer even for zero bytes");

    Require(memory.Open(0xDEADBEEF, "native-memory"),
        "memory stream ignores its Open mode");
    Require(memory.GetBuffer() != nullptr,
        "memory Open allocates its default buffer");
    Require(memory.GetCapacity() == spMemoryStream::DefaultGrowthQuantum,
        "memory Open uses the native 5000-byte growth quantum");
    std::uint32_t memorySize = 99;
    std::uint32_t memoryPosition = 99;
    Require(memory.GetSize(&memorySize) && memorySize == 0,
        "newly opened memory stream has zero logical size");
    Require(memory.GetCurrentPosition(memoryPosition) && memoryPosition == 0,
        "newly opened memory stream starts at zero");

    const std::uint8_t memoryPayload[] = {4, 8, 15, 16, 23, 42};
    Require(memory.WriteData(memoryPayload, sizeof(memoryPayload)),
        "memory stream accepts raw data");
    Require(memory.GetSize(&memorySize) && memorySize == sizeof(memoryPayload),
        "memory write extends logical size");
    Require(!memory.Seek(spStream::SeekSource::essEnd, 0),
        "memory end seek uses capacity and can exceed logical size");
    Require(memory.Seek(spStream::SeekSource::essEnd,
            static_cast<std::int32_t>(memory.GetCapacity() - memorySize - 1)),
        "capacity-relative end seek reaches the logical end with a compensating offset");
    Require(memory.GetCurrentPosition(memoryPosition)
            && memoryPosition == memorySize,
        "capacity-relative end seek stores the proven native result");
    Require(memory.Seek(spStream::SeekSource::essStart, 0),
        "memory stream rewinds");
    const std::uint8_t overwrite[] = {1, 2};
    Require(memory.WriteData(overwrite, sizeof(overwrite)),
        "memory stream overwrites existing bytes");
    Require(memory.GetSize(&memorySize) && memorySize == sizeof(memoryPayload),
        "overwrite ending before logical end does not truncate the stream");

    spMemoryStream exactCapacity;
    Require(exactCapacity.Open("exact capacity"),
        "boundary memory stream opens");
    std::vector<std::uint8_t> boundary(
        spMemoryStream::DefaultGrowthQuantum, 0xA5);
    Require(exactCapacity.WriteData(boundary.data(),
            static_cast<std::uint32_t>(boundary.size())),
        "write ending exactly at capacity follows the native reallocation path");
    Require(exactCapacity.GetCapacity() == spMemoryStream::DefaultGrowthQuantum,
        "exact-capacity reallocation keeps the same capacity");
    const std::uint8_t growByte = 0x5A;
    Require(exactCapacity.WriteData(&growByte, 1),
        "write beyond capacity grows the memory buffer");
    Require(exactCapacity.GetCapacity()
            == spMemoryStream::DefaultGrowthQuantum * 2,
        "memory buffer grows by fixed 5000-byte quanta");

    spMemoryStream staged;
    Require(staged.ResizeAndSetSize(4),
        "exact-size helper allocates without Open");
    Require(staged.GetCapacity() == 4
            && staged.GetSize(&memorySize) && memorySize == 4,
        "exact-size helper makes the full allocation logical data");
    auto* stagedBytes = static_cast<std::uint8_t*>(staged.GetBuffer());
    Require(stagedBytes != nullptr, "exact-size helper exposes its buffer");
    stagedBytes[0] = 9;
    stagedBytes[1] = 8;
    stagedBytes[2] = 7;
    stagedBytes[3] = 6;
    std::uint8_t stagedRead[4]{};
    Require(staged.ReadData(stagedRead, sizeof(stagedRead))
            && std::memcmp(stagedRead, stagedBytes, sizeof(stagedRead)) == 0,
        "exact-size helper rewinds for an immediate full read");
    Require(staged.Reset(), "memory Reset succeeds");
    Require(staged.GetSize(&memorySize) && memorySize == 0
            && staged.GetCapacity() == 4,
        "memory Reset preserves allocation while clearing size");

    TestStream shortSource;
    Require(shortSource.Open("short source"), "short source opens");
    Require(shortSource.WriteData(&growByte, 1)
            && shortSource.Seek(spStream::SeekSource::essStart, 0),
        "short source stages one byte");
    spMemoryStream failedCopy;
    Require(failedCopy.Open("failed copy"), "failed-copy destination opens");
    Require(!failedCopy.vfunc_WriteFromStream(&shortSource, 4),
        "stream-to-stream write propagates the source read failure");
    Require(failedCopy.GetSize(&memorySize) && memorySize == 4,
        "failed source read retains preflight's enlarged logical size");
    Require(failedCopy.GetCurrentPosition(memoryPosition)
            && memoryPosition == 0,
        "failed source read does not advance destination position");

    spMemoryStream released;
    Require(released.ResizeAndSetSize(4),
        "release test prepares an exact buffer");
    auto* releasedBytes = released.ReleaseBuffer();
    Require(releasedBytes != nullptr && released.GetBuffer() == releasedBytes,
        "ReleaseBuffer transfers ownership but retains the visible pointer");
    Require(!released.OwnsBuffer() && !released.IsResizeEnabled(),
        "ReleaseBuffer clears both native flags");
    Require(!released.WriteData(memoryPayload, 4),
        "released exact-capacity buffer cannot take the equality resize path");
    Require(released.Close(), "released memory stream closes without freeing");
    delete[] releasedBytes;

    spMemoryStream cloneSource;
    cloneSource.SetName("shared object name");
    Require(cloneSource.Open("diagnostic name"), "clone source opens");
    Require(cloneSource.WriteData(memoryPayload, sizeof(memoryPayload)),
        "clone source receives data");
    auto memoryCloneBase = cloneSource.Clone();
    auto* memoryClone = dynamic_cast<spMemoryStream*>(memoryCloneBase.get());
    Require(memoryClone != nullptr, "memory clone keeps its concrete type");
    Require(memoryClone->GetName() != nullptr
            && std::strcmp(memoryClone->GetName(), "shared object name") == 0,
        "memory clone copies inherited spNamedObject state");
    Require(memoryClone->GetStreamName() == nullptr
            && memoryClone->GetBuffer() == nullptr,
        "native memory clone does not copy stream name or buffer state");

    Require(memory.Close() && exactCapacity.Close() && staged.Close()
            && failedCopy.Close() && shortSource.Close() && cloneSource.Close(),
        "memory-stream test resources close");

    TestFileStream fileLeaf;
    spFileStream& fileBase = fileLeaf;
    Require(fileBase.Open("default-read.smo"),
        "spFileStream one-argument Open dispatches to its platform overload");
    Require(fileLeaf.lastMode == 1 && fileLeaf.lastName == "default-read.smo",
        "spFileStream supplies the exact native read-mode bit");
    fileLeaf.openResult = false;
    Require(!fileBase.Open("missing.smo"),
        "spFileStream propagates the platform Open result");
    Require(fileBase.IsExactly(spFileStream::ClassID)
            && fileBase.IsKindOf(spStream::ClassID),
        "spFileStream exposes its native RTTI position");
    Require(fileBase.Clone() == nullptr,
        "spFileStream keeps the native null-clone behavior");
    Require(fileBase.GetBuffer() == nullptr,
        "spFileStream inherits the null buffer slot");

#if defined(_WIN32)
    const std::filesystem::path pcFilePath = "spPCFileStream-test.bin";
    const std::filesystem::path missingPCFilePath =
        "spPCFileStream-definitely-missing.bin";
    (void)std::filesystem::remove(pcFilePath);
    (void)std::filesystem::remove(missingPCFilePath);

    spPCFileStream pcFile;
    Require(!pcFile.IsOpen() && !pcFile.Close(),
        "default PC file stream is closed");
    Require(pcFile.Open(0x02, pcFilePath.string().c_str()),
        "PC write mode creates or truncates a file");
    Require(pcFile.IsOpen(), "PC file stream exposes a valid open handle");
    const std::uint8_t filePayload[] = {10, 20, 30, 40};
    Require(pcFile.WriteData(filePayload, sizeof(filePayload)),
        "PC raw write reaches WriteFile");
    Require(pcFile.WriteData(nullptr, 0),
        "PC zero-byte WriteFile success is preserved");
    std::uint32_t filePosition = 0;
    Require(pcFile.GetCurrentPosition(filePosition)
            && filePosition == sizeof(filePayload),
        "PC current position follows successful writes");
    Require(!pcFile.Open(0x01, pcFilePath.string().c_str()),
        "PC Open refuses to replace a nonzero handle");
    Require(pcFile.Close() && !pcFile.Close(),
        "PC Close clears a handle and rejects the already-closed state");

    bool pcAsyncCallbackRan = false;
    spMemoryStream pcAsyncDestination;
    {
        spPCAsyncFileStreamManager pcAsyncManager;
        auto completion = [](void* context)
        {
            *static_cast<bool*>(context) = true;
        };
        Require(pcAsyncManager.vfunc_Request(
                    pcFilePath.string().c_str(),
                    &pcAsyncDestination,
                    completion,
                    &pcAsyncCallbackRan)
                && pcAsyncCallbackRan,
            "PC async request completes synchronously");
        Require(spAsyncFileStreamManager::GetInstance() == &pcAsyncManager,
            "PC async leaf occupies the common singleton slot");
    }
    std::uint32_t pcAsyncSize = 0;
    Require(pcAsyncDestination.GetSize(&pcAsyncSize)
            && pcAsyncSize == sizeof(filePayload),
        "PC async request sizes its destination memory stream");
    Require(pcAsyncDestination.GetCurrentPosition(filePosition)
            && filePosition == sizeof(filePayload),
        "PC async request fills the complete destination");
    const auto* pcAsyncBytes = static_cast<const std::uint8_t*>(
        pcAsyncDestination.GetBuffer());
    Require(pcAsyncBytes != nullptr
            && std::memcmp(pcAsyncBytes, filePayload, sizeof(filePayload)) == 0,
        "PC async request copies exact file bytes");
    Require(pcAsyncDestination.Close(),
        "PC async destination releases its allocation");

    spFileStream& pcFileBase = pcFile;
    Require(pcFileBase.Open(pcFilePath.string().c_str()),
        "spFileStream default Open selects PC read mode");
    std::uint32_t fileSize = 0;
    Require(pcFile.GetSize(&fileSize) && fileSize == sizeof(filePayload),
        "PC GetSize returns the low 32-bit file size");
    std::uint8_t firstFileBytes[2]{};
    Require(pcFile.ReadData(firstFileBytes, sizeof(firstFileBytes))
            && firstFileBytes[0] == 10 && firstFileBytes[1] == 20,
        "PC ReadFile returns requested data");
    std::uint8_t shortFileRead[8]{};
    Require(pcFile.ReadData(shortFileRead, sizeof(shortFileRead))
            && shortFileRead[0] == 30 && shortFileRead[1] == 40,
        "PC file stream accepts a short nonzero EOF read as success");
    Require(!pcFile.ReadData(shortFileRead, 0),
        "PC successful zero-byte read is reported as EOF failure");
    Require(pcFile.Seek(spStream::SeekSource::essStart, 1)
            && pcFile.GetCurrentPosition(filePosition) && filePosition == 1,
        "PC start-relative seek and current-position query agree");
    pcFile.SetName("clone-visible-name");
    auto pcCloneBase = pcFile.Clone();
    auto* pcClone = dynamic_cast<spPCFileStream*>(pcCloneBase.get());
    Require(pcClone != nullptr && !pcClone->IsOpen(),
        "PC file clone is a fresh closed platform stream");
    Require(pcClone->GetName() != nullptr
            && std::strcmp(pcClone->GetName(), "clone-visible-name") == 0,
        "PC file clone copies only inherited object state");
    Require(pcClone->GetStreamName() == nullptr,
        "PC file clone does not copy diagnostic stream state");
    Require(pcFile.Close(), "PC read handle closes");

    Require(pcFile.Open(0x02 | 0x08, pcFilePath.string().c_str()),
        "PC append mode opens an existing file");
    Require(pcFile.GetCurrentPosition(filePosition)
            && filePosition == sizeof(filePayload),
        "PC append mode seeks to the physical end");
    const std::uint8_t appended[] = {50, 60};
    Require(pcFile.WriteData(appended, sizeof(appended)) && pcFile.Close(),
        "PC append write succeeds and closes");

    Require(!pcFile.Open(0x01, missingPCFilePath.string().c_str())
            && !pcFile.IsOpen(),
        "failed PC Open retains native INVALID_HANDLE_VALUE state");
    Require(pcFile.Close() && !pcFile.Close(),
        "PC Close consumes the nonzero invalid-handle failure state");

    spMemoryStream directBufferSource;
    Require(directBufferSource.Open("direct buffer source")
            && directBufferSource.WriteData(filePayload, sizeof(filePayload))
            && directBufferSource.Seek(spStream::SeekSource::essStart, 1),
        "direct-buffer source is staged at a nonzero position");
    Require(pcFile.Open(0x02, pcFilePath.string().c_str())
            && pcFile.vfunc_WriteFromStream(&directBufferSource, 2),
        "PC stream-to-stream write accepts a direct memory buffer");
    Require(directBufferSource.GetCurrentPosition(filePosition)
            && filePosition == 3,
        "direct-buffer source advances by count before destination write");
    Require(pcFile.Close() && directBufferSource.Close(),
        "PC stream-copy resources close");
    Require(pcFileBase.Open(pcFilePath.string().c_str()),
        "PC stream-copy output reopens for verification");
    std::uint8_t copiedFromStart[2]{};
    Require(pcFile.ReadData(copiedFromStart, sizeof(copiedFromStart))
            && copiedFromStart[0] == 10 && copiedFromStart[1] == 20,
        "PC direct-buffer copy writes from buffer start, not current position");
    Require(pcFile.Close(), "PC verification handle closes");
    (void)std::filesystem::remove(pcFilePath);
#endif

    Require(sizeof(sparkplug::evidence::ps2::spBaseObjectLayout) == 0x10,
        "PS2 spBaseObject evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spNamedObjectLayout) == 0x14,
        "PS2 spNamedObject evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spCrossPlatformLayout) == 0x14,
        "PS2 spCrossPlatform adds no instance storage");
    Require(sizeof(sparkplug::evidence::ps2::spAppLayout) == 0x20
            && offsetof(sparkplug::evidence::ps2::spAppLayout, ownedText) == 0x1C,
        "PS2 spApp layout and owned text offset are exact");
    Require(sizeof(sparkplug::evidence::ps2::spPS2AppLayout) == 0x20,
        "PS2 platform app adds no observed storage to spApp");
    Require(sizeof(sparkplug::evidence::ps2::spStreamLayout) == 0x1C,
        "PS2 spStream evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spMemoryStreamLayout) == 0x38,
        "PS2 spMemoryStream evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spFileStreamLayout) == 0x1C,
        "PS2 spFileStream adds no storage");
    Require(sizeof(sparkplug::evidence::ps2::spPS2FileBufferLayout) == 0x10,
        "PS2 file-buffer descriptor evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPS2FileStreamLayout) == 0x114,
        "PS2 spPS2FileStream evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spAsyncFileStreamManagerLayout) == 0x18,
        "PS2 spAsyncFileStreamManager evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPS2AsyncFileRequestLayout) == 0x11C,
        "PS2 async request record evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPS2AsyncFileStreamManagerLayout)
            == 0x37A0,
        "PS2 async-manager leaf evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPCKFileRecordLayout) == 0x14
            && sizeof(sparkplug::evidence::ps2::spPCKPackageRecordLayout) == 0x1C,
        "PS2 PCK file and package record sizes are exact");
    Require(sizeof(sparkplug::evidence::ps2::spPCKManagerLayout) == 0x34
            && offsetof(sparkplug::evidence::ps2::spPCKManagerLayout,
                packageCount) == 0x18,
        "PS2 PCK manager layout and package count offset are exact");
    Require(sizeof(sparkplug::evidence::ps2::spErrorLayout) == 0x28
            && offsetof(sparkplug::evidence::ps2::spErrorLayout,
                nextError) == 0x24,
        "PS2 error layout and chain-link offset are exact");
    Require(sizeof(sparkplug::evidence::ps2::spErrorManagerLayout) == 0x424
            && offsetof(sparkplug::evidence::ps2::spErrorManagerLayout,
                dataUsed) == 0x414
            && offsetof(sparkplug::evidence::ps2::spErrorManagerLayout,
                handlerResolved) == 0x420,
        "PS2 error-manager fixed stack and trailing state are exact");
    Require(sizeof(sparkplug::evidence::ps2::spPS2ErrorManagerLayout) == 0x424,
        "PS2 error-manager leaf adds no native storage");
    Require(sizeof(sparkplug::evidence::ps2::spSubscriptionManagerLayout) == 0x20
            && offsetof(
                sparkplug::evidence::ps2::spSubscriptionManagerLayout,
                subscriptionTree) == 0x14,
        "PS2 subscription manager has an exact 0x0c-byte tree tail");
    Require(sizeof(sparkplug::evidence::ps2::spPS2HelperLayout) == 0x11C
            && offsetof(sparkplug::evidence::ps2::spPS2HelperLayout,
                pathPrefix) == 0x1C,
        "PS2 helper layout and inline path buffer are exact");
    Require(sizeof(sparkplug::evidence::ps2::spPS2IOPModuleManagerLayout)
                == 0x28
            && offsetof(
                sparkplug::evidence::ps2::spPS2IOPModuleManagerLayout,
                moduleCount) == 0x1C
            && sizeof(
                sparkplug::evidence::ps2::spPS2IOPModuleListNodeLayout)
                == 0x0C,
        "PS2 IOP manager and owned module-list node layouts are exact");
    Require(offsetof(sparkplug::evidence::ps2::spPS2AsyncFileStreamManagerLayout,
            requests) == 0x1C
        && offsetof(sparkplug::evidence::ps2::spPS2AsyncFileStreamManagerLayout,
            queuedCount) == 0x3794,
        "PS2 async-manager queue occupies fifty exact records");
    Require(sparkplug::evidence::ps2::PS2AsyncDestinationCapacity(0) == 0x1000
        && sparkplug::evidence::ps2::PS2AsyncDestinationCapacity(0x800) == 0x1800
        && sparkplug::evidence::ps2::PS2AsyncDestinationCapacity(0x801) == 0x1800,
        "PS2 async destination uses floor-to-sector plus 0x1000");
    Require(sparkplug::evidence::ps2::PS2AsyncDiscSectorCount(0) == 1
        && sparkplug::evidence::ps2::PS2AsyncDiscSectorCount(0x800) == 2
        && sparkplug::evidence::ps2::PS2AsyncDiscSectorCount(0x801) == 2,
        "PS2 async read requests floor-sector-count plus one");
    Require(offsetof(sparkplug::evidence::ps2::spPS2FileStreamLayout,
            shellFile) == 0x3C,
        "PS2 file stream embedded ShellFile state offset");
    Require(offsetof(sparkplug::evidence::ps2::spPS2FileStreamLayout,
            buffers) == 0xD8,
        "PS2 file stream double-buffer descriptors offset");
    Require(offsetof(sparkplug::evidence::ps2::spPS2FileStreamLayout,
            physicalPath) == 0x110,
        "PS2 file stream final owned path offset");

    using sparkplug::evidence::ps2::ResolveNormalFileSeek;
    auto ps2Seek = ResolveNormalFileSeek(40, 100, 300, 1, 25);
    Require(ps2Seek.accepted && ps2Seek.absolutePosition == 125,
        "PS2 start-relative seek includes the logical origin");
    ps2Seek = ResolveNormalFileSeek(40, 100, 120, 1, 25);
    Require(!ps2Seek.accepted && ps2Seek.absolutePosition == 40,
        "PS2 start-relative seek rejects positions past a known size");
    ps2Seek = ResolveNormalFileSeek(40, 100, 0, 1, 25);
    Require(ps2Seek.accepted && ps2Seek.absolutePosition == 125,
        "PS2 zero physical size disables the start upper bound");
    ps2Seek = ResolveNormalFileSeek(40, 0, 100, 4, -41);
    Require(!ps2Seek.accepted && ps2Seek.absolutePosition == 40,
        "PS2 current-relative seek rejects a negative wrapped result");
    ps2Seek = ResolveNormalFileSeek(40, 0, 100, 4, 60);
    Require(ps2Seek.accepted && ps2Seek.absolutePosition == 100,
        "PS2 current-relative seek permits the exact end position");
    ps2Seek = ResolveNormalFileSeek(40, 0, 100, 2, -25);
    Require(ps2Seek.accepted && ps2Seek.absolutePosition == 125,
        "PS2 end-relative seek performs no bounds validation");
    ps2Seek = ResolveNormalFileSeek(40, 0, 100, 99, 25);
    Require(ps2Seek.accepted && ps2Seek.absolutePosition == 40,
        "PS2 unknown seek source preserves position and reports success");

    std::uint32_t ps2ReportedValue = 123;
    Require(!sparkplug::evidence::ps2::ReadNormalFilePosition(
                false, 300, 100, ps2ReportedValue)
            && ps2ReportedValue == 0,
        "PS2 closed position query clears its output");
    Require(sparkplug::evidence::ps2::ReadNormalFilePosition(
                true, 300, 100, ps2ReportedValue)
            && ps2ReportedValue == 200,
        "PS2 open position query subtracts logical origin");
    Require(sparkplug::evidence::ps2::ReadNormalFileSize(
                true, 900, 120, ps2ReportedValue)
            && ps2ReportedValue == 120,
        "PS2 logical PCK size overrides physical backing size");
    Require(sparkplug::evidence::ps2::ReadNormalFileSize(
                true, 900, 0, ps2ReportedValue)
            && ps2ReportedValue == 900,
        "PS2 zero logical override reports physical size");
    Require(!sparkplug::evidence::ps2::ReadNormalFileSize(
                false, 900, 120, ps2ReportedValue)
            && ps2ReportedValue == 0,
        "PS2 closed size query clears its output");
    Require(offsetof(sparkplug::evidence::ps2::spMemoryStreamLayout, buffer) == 0x30,
        "PS2 spMemoryStream buffer offset");
    Require(offsetof(sparkplug::evidence::ps2::spMemoryStreamLayout, ownsBuffer) == 0x34,
        "PS2 spMemoryStream ownership offset");
    Require(offsetof(sparkplug::evidence::ps2::spStreamLayout, logicalOrigin) == 0x14,
        "PS2 spStream logical-origin offset");
    Require(offsetof(sparkplug::evidence::ps2::spStreamLayout, ownedStreamName) == 0x18,
        "PS2 spStream name offset");
    Require(sizeof(sparkplug::evidence::ps2::spReverseReferenceListLayout) == 0x0C,
        "PS2 reverse-reference list evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spReverseReferenceNodeLayout) == 0x0C,
        "PS2 reverse-reference node evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spReverseReferenceNotificationLayout) == 0x20,
        "PS2 reverse-reference notification evidence size");
    Require(offsetof(sparkplug::evidence::ps2::spReverseReferenceNotificationLayout,
            sourceObject) == 0x10,
        "PS2 reverse-reference notification source offset");
    Require(offsetof(sparkplug::evidence::ps2::spReverseReferenceNotificationLayout,
            referringObject) == 0x14,
        "PS2 reverse-reference notification owner offset");
    Require(sizeof(sparkplug::evidence::ps2::spRTTIRegistrationLayout) == 0x60,
        "PS2 registration evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPropertyGroupLayout) == 0x0C,
        "PS2 property-group evidence size");
    Require(sizeof(sparkplug::evidence::ps2::SingletonSupportSubobjectLayout) == 0x04,
        "PS2 singleton-support subobject evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spCloneManagerLayout) == 0x18,
        "PS2 spCloneManager evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spRTTIManagerLayout) == 0x24,
        "PS2 spRTTIManager evidence size");
    Require(sizeof(sparkplug::evidence::ps2::spPropertySystemLayout) == 0x20,
        "PS2 spPropertySystem evidence size");
    Require(sizeof(sparkplug::evidence::ps2::Ps2MemberFunctionDescriptorLayout) == 0x0C,
        "PS2 member-function descriptor evidence size");
    Require(sizeof(sparkplug::evidence::ps2::PropertyRecord58Layout) == 0x58,
        "PS2 property-record evidence size");
    Require(offsetof(sparkplug::evidence::ps2::PropertyRecord58Layout, propertyName) == 0x04,
        "PS2 property-record name offset");
    Require(offsetof(sparkplug::evidence::ps2::PropertyRecord58Layout, propertyType) == 0x08,
        "PS2 property-record type offset");
    Require(offsetof(sparkplug::evidence::ps2::PropertyRecord58Layout, getter) == 0x18,
        "PS2 property-record getter offset");
    Require(offsetof(sparkplug::evidence::ps2::PropertyRecord58Layout, setter) == 0x24,
        "PS2 property-record setter offset");
    Require(offsetof(sparkplug::evidence::ps2::PropertyRecord58Layout,
            typeSpecificPayload) == 0x50,
        "PS2 property-record type-specific payload offset");
    Require(spCloneManager::NativeClassID
            == sparkplug::evidence::ps2::spCloneManagerClassID,
        "portable clone facade preserves the native class ID");
    Require(spRTTIManager::NativeClassID
            == sparkplug::evidence::ps2::spRTTIManagerClassID,
        "portable RTTI facade preserves the native class ID");
    Require(sizeof(sparkplug::evidence::pc::spBaseObjectLayout) == 0x10,
        "PC spBaseObject evidence size");
    Require(sizeof(sparkplug::evidence::pc::spAppLayout) == 0x20
            && offsetof(sparkplug::evidence::pc::spAppLayout, ownedText) == 0x1C,
        "PC spApp layout agrees with PS2");
    Require(sizeof(sparkplug::evidence::pc::spPCAppObservedPrefixLayout) == 0x84
            && offsetof(sparkplug::evidence::pc::spPCAppObservedPrefixLayout,
                windowWidth) == 0x60
            && offsetof(sparkplug::evidence::pc::spPCAppObservedPrefixLayout,
                windowStyle) == 0x78,
        "PC platform app preserves its exact observed Win32-state prefix");
    Require(sizeof(sparkplug::evidence::pc::spStreamLayout) == 0x1C,
        "PC spStream evidence size");
    Require(sizeof(sparkplug::evidence::pc::spMemoryStreamLayout) == 0x38,
        "PC spMemoryStream evidence size");
    Require(sizeof(sparkplug::evidence::pc::spFileStreamLayout) == 0x1C,
        "PC spFileStream adds no storage");
    Require(sizeof(sparkplug::evidence::pc::spPCFileStreamLayout) == 0x20,
        "PC spPCFileStream adds exactly one native handle");
    Require(sizeof(sparkplug::evidence::pc::spAsyncFileStreamManagerLayout) == 0x18,
        "PC spAsyncFileStreamManager evidence size");
    Require(sizeof(sparkplug::evidence::pc::spPCAsyncFileStreamManagerLayout) == 0x18,
        "PC async-manager leaf adds no instance storage");
    Require(sizeof(sparkplug::evidence::pc::spPCKFileRecordLayout) == 0x14
            && sizeof(sparkplug::evidence::pc::spPCKPackageRecordLayout) == 0x1C,
        "PC PCK record layouts agree with PS2");
    Require(sizeof(sparkplug::evidence::pc::spPCKManagerObservedLayout) == 0x3C
            && offsetof(sparkplug::evidence::pc::spPCKManagerObservedLayout,
                trackOpenedNames) == 0x28,
        "PC PCK manager observed extent reflects its compiler-specific vectors");
    Require(sizeof(sparkplug::evidence::pc::spErrorObservedPrefixLayout) == 0x28
            && offsetof(sparkplug::evidence::pc::spErrorObservedPrefixLayout,
                nextError) == 0x24,
        "PC error prefix agrees with the exact PS2 payload layout");
    Require(sizeof(sparkplug::evidence::pc::spErrorManagerLayout) == 0x1024
            && offsetof(sparkplug::evidence::pc::spErrorManagerLayout,
                dataUsed) == 0x1014
            && offsetof(sparkplug::evidence::pc::spErrorManagerLayout,
                handlerResolved) == 0x1020,
        "PC error manager preserves its larger fixed stack layout");
    Require(sizeof(sparkplug::evidence::pc::spPCErrorManagerLayout) == 0x1024,
        "PC error-manager leaf adds no native storage");
    Require(sizeof(sparkplug::evidence::pc::spSubscriptionManagerLayout) == 0x20
            && offsetof(
                sparkplug::evidence::pc::spSubscriptionManagerLayout,
                subscriptionTree) == 0x14,
        "PC subscription manager agrees on its 0x0c-byte tree tail");
    Require(offsetof(sparkplug::evidence::pc::spMemoryStreamLayout, buffer) == 0x30,
        "PC spMemoryStream buffer offset");
    Require(offsetof(sparkplug::evidence::pc::spBaseObjectLayout, padding0A) == 0x0A,
        "PC spBaseObject alignment padding offset");
    Require(sparkplug::evidence::pc::spBaseObjectClassID
            == sparkplug::evidence::ps2::spBaseObjectClassID,
        "PC and PS2 spBaseObject class IDs agree");
    Require(sparkplug::evidence::pc::spCrossPlatformClassID
            == sparkplug::evidence::ps2::spCrossPlatformClassID,
        "PC and PS2 spCrossPlatform class IDs agree");
    Require(spApp::ClassID == sparkplug::evidence::pc::spAppClassID
            && spApp::ClassID == sparkplug::evidence::ps2::spAppClassID,
        "portable, PC and PS2 spApp class IDs agree");
#if defined(_WIN32)
    Require(spPCApp::ClassID == sparkplug::evidence::pc::spPCAppClassID,
        "portable PC app preserves its native class ID");
#endif
    Require(spPS2App::ClassID == sparkplug::evidence::ps2::spPS2AppClassID,
        "portable PS2 app preserves its native class ID");
    Require(sparkplug::evidence::pc::spStreamClassID
            == sparkplug::evidence::ps2::spStreamClassID,
        "PC and PS2 spStream class IDs agree");
    Require(spStream::ClassID == sparkplug::evidence::ps2::spStreamClassID,
        "portable spStream preserves the native class ID");
    Require(sparkplug::evidence::pc::spMemoryStreamClassID
            == sparkplug::evidence::ps2::spMemoryStreamClassID,
        "PC and PS2 spMemoryStream class IDs agree");
    Require(sparkplug::evidence::pc::spFileStreamClassID
            == sparkplug::evidence::ps2::spFileStreamClassID,
        "PC and PS2 spFileStream class IDs agree");
    Require(spAsyncFileStreamManager::ClassID
            == sparkplug::evidence::pc::spAsyncFileStreamManagerClassID
        && spAsyncFileStreamManager::ClassID
            == sparkplug::evidence::ps2::spAsyncFileStreamManagerClassID,
        "portable and native async-manager class IDs agree");
    Require(spPCKManager::ClassID == sparkplug::evidence::pc::spPCKManagerClassID
            && spPCKManager::ClassID
                == sparkplug::evidence::ps2::spPCKManagerClassID,
        "portable, PC and PS2 PCK manager class IDs agree");
    Require(spError::ClassID == sparkplug::evidence::pc::spErrorClassID
            && spError::ClassID == sparkplug::evidence::ps2::spErrorClassID
            && spErrorManager::ClassID
                == sparkplug::evidence::pc::spErrorManagerClassID
            && spErrorManager::ClassID
                == sparkplug::evidence::ps2::spErrorManagerClassID,
        "portable, PC and PS2 error class IDs agree");
    Require(spPS2ErrorManager::ClassID
            == sparkplug::evidence::ps2::spPS2ErrorManagerClassID,
        "portable PS2 error manager preserves its native class ID");
    Require(spSubscriptionManager::ClassID
            == sparkplug::evidence::pc::spSubscriptionManagerClassID
            && spSubscriptionManager::ClassID
                == sparkplug::evidence::ps2::spSubscriptionManagerClassID,
        "portable, PC and PS2 subscription-manager class IDs agree");
#if defined(_WIN32)
    Require(spPCErrorManager::ClassID
            == sparkplug::evidence::pc::spPCErrorManagerClassID,
        "portable PC error manager preserves its native class ID");
#endif
    Require(spPS2Helper::ClassID
            == sparkplug::evidence::ps2::spPS2HelperClassID,
        "portable PS2 helper preserves the native class ID");
    Require(spPS2IOPModuleManager::ClassID
            == sparkplug::evidence::ps2::spPS2IOPModuleManagerClassID,
        "portable PS2 IOP module manager preserves the native class ID");
    Require(sparkplug::evidence::pc::spAsyncFileStreamManagerRequestSlot == 0x1C
        && sparkplug::evidence::pc::spAsyncFileStreamManagerUpdateSlot == 0x20,
        "PC async-manager request/update slots follow the primary vtable");
    Require(sparkplug::evidence::ps2::spAsyncFileStreamManagerRequestSlot == 0x30
        && sparkplug::evidence::ps2::spAsyncFileStreamManagerUpdateSlot == 0x34,
        "PS2 async-manager slots follow the interposed support vtable");
    Require(sparkplug::evidence::pc::spStreamOpenRead
            == sparkplug::evidence::ps2::spStreamOpenRead
        && sparkplug::evidence::pc::spStreamOpenWrite
            == sparkplug::evidence::ps2::spStreamOpenWrite
        && sparkplug::evidence::pc::spStreamOpenReadWrite
            == sparkplug::evidence::ps2::spStreamOpenReadWrite
        && sparkplug::evidence::pc::spStreamOpenAppend
            == sparkplug::evidence::ps2::spStreamOpenAppend,
        "PC and PS2 stream open-mode bits agree");
    Require((sparkplug::evidence::pc::spStreamLastPureSlot
                - sparkplug::evidence::pc::spStreamFirstPureSlot) / 4 + 1 == 9,
        "PC spStream has nine consecutive pure slots");
    Require((sparkplug::evidence::ps2::spStreamLastPureSlot
                - sparkplug::evidence::ps2::spStreamFirstPureSlot) / 4 + 1 == 9,
        "PS2 spStream has nine consecutive pure slots");
    Require(sparkplug::evidence::ps2::spStreamDefaultBufferSlot
            == sparkplug::evidence::pc::spStreamDefaultBufferSlot + 8,
        "PS2 stream slot offsets include two leading ABI words");
    Require(sparkplug::evidence::pc::spStreamWriteFromStreamSlot
            < sparkplug::evidence::pc::spStreamWriteDataSlot,
        "PC places stream-to-stream write before raw WriteData");
    Require(sparkplug::evidence::ps2::spStreamWriteDataSlot
            < sparkplug::evidence::ps2::spStreamWriteFromStreamSlot,
        "PS2 places raw WriteData before stream-to-stream write");

    std::cout << "SparkBase reconstruction tests passed\n";
    return EXIT_SUCCESS;
}
