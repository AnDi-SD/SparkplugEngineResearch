#include "Code/SparkBase/spStreamError.h"
#include "Analysis/PC/spDerivedErrorAbi.h"
#include "Analysis/PS2/spStreamErrorAbi.h"
#if defined(_WIN32)
#include "Code/SparkBasePC/spWindowsError.h"
#endif
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool condition, const char* description)
    {
        if (!condition) throw std::runtime_error(description);
    }
    struct DispatchManager final : spErrorManager
    {
        mutable std::string delivered;
        static void Receive(const AnalysisDispatch& dispatch, void* context)
        {
            static_cast<DispatchManager*>(context)->delivered = dispatch.text;
        }
        AnalysisHandlerBinding ResolveHandlerForAnalysis() const noexcept override
        {
            return {&Receive, const_cast<DispatchManager*>(this)};
        }
    };
    template<class Error> void Lifecycle(const spClassID expected)
    {
        auto& registry = spRTTIManager::Instance();
        Require(registry.Register(spBaseObject::StaticRTTI()), "base registration");
        Require(registry.Register(spError::StaticRTTI()), "error registration");
        Require(registry.Register(Error::StaticRTTI()), "leaf registration");
        auto created = registry.Create(expected);
        Require(dynamic_cast<Error*>(created.get()) != nullptr, "factory result type");
        Error source{spErrorSeverity::Fatal, 7, "source.cpp", 123};
        source.SetMessageForAnalysis("original payload");
        Require(source.IsExactly(expected) && source.IsKindOf(spError::ClassID)
            && source.IsKindOf(spBaseObject::ClassID), "physical RTTI chain");
        spCloneManager manager;
        auto copy = source.vfunc_10(manager);
        auto* leaf = dynamic_cast<Error*>(copy.get());
        Require(leaf != nullptr && manager.FindClone(source) == leaf, "clone mapping");
        Require(leaf->GetCodeForAnalysis() == 0 && leaf->GetSeverityForAnalysis()
            == spErrorSeverity::Information && leaf->GetMessageForAnalysis() == nullptr
            && leaf->GetSourceFileForAnalysis() == nullptr
            && leaf->GetSourceLineForAnalysis() == 0, "clone uses fresh defaults");
        Error destination{spErrorSeverity::Warning, 6, "keep.cpp", 42};
        destination.SetMessageForAnalysis("keep");
        Require(source.vfunc_14(destination, manager), "empty copy result");
        Require(destination.GetCodeForAnalysis() == 6
            && std::string(destination.GetMessageForAnalysis()) == "keep"
            && destination.GetSourceLineForAnalysis() == 42, "empty copy preserves destination");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 5 && std::string(argv[1]) == "--case")
        {
            const auto code = static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10));
            const int length = std::atoi(argv[4]);
            const std::string message(length < 0 ? 0 : length, 'x');
            if (std::string(argv[2]) == "stream")
            {
                spStreamError error{spErrorSeverity::Information, code, nullptr, 0};
                error.SetMessageForAnalysis(length < 0 ? nullptr : message.c_str());
                std::cout << error.DescribeForAnalysis();
            }
#if defined(_WIN32)
            else if (std::string(argv[2]) == "windows")
            {
                spWindowsError error{spErrorSeverity::Fatal, code, nullptr, 0};
                error.SetMessageForAnalysis(length < 0 ? nullptr : message.c_str());
                std::cout << error.DescribeForAnalysis();
            }
#endif
            else throw std::runtime_error("unknown leaf");
            return 0;
        }
        Lifecycle<spStreamError>(spStreamError::ClassID);
        spStreamError missing{spErrorSeverity::Warning, 4, "test.cpp", 12};
        Require(missing.DescribeForAnalysis() == "Can't open stream (NULL)", "null open diagnostic");
        missing.SetMessageForAnalysis("file.smo");
        Require(missing.DescribeSeverityForAnalysis() == "WARNING: Can't open stream (file.smo)", "virtual severity dispatch");
        Require(missing.DescribeSourceForAnalysis() == "WARNING: Can't open stream (file.smo)\n - test.cpp(12)", "virtual source dispatch");
        spStreamError eof{spErrorSeverity::Error, 6, nullptr, 0};
        Require(eof.DescribeForAnalysis() == "Stream reached EOF", "empty EOF diagnostic");
#if defined(_WIN32)
        Lifecycle<spWindowsError>(spWindowsError::ClassID);
        spWindowsError windows{spErrorSeverity::Information, 0xFFFFFFFF, nullptr, 0};
        Require(windows.DescribeForAnalysis().empty(), "windows ignores code with no message");
        windows.SetMessageForAnalysis("OS diagnostic");
        Require(windows.DescribeForAnalysis() == "(OS diagnostic)", "windows wraps supplied message");
        DispatchManager dispatch;
        Require(dispatch.StoreMessageForAnalysis(missing, "file.smo")
            && dispatch.StoreMessageForAnalysis(windows, "denied"), "shared manager stores leaf diagnostics");
        dispatch.LinkErrorForAnalysis(missing);
        dispatch.LinkErrorForAnalysis(windows);
        Require(dispatch.HandleForAnalysis(spErrorSeverity::Information), "shared manager handles derived chain");
        Require(dispatch.delivered == "(denied)\nWARNING: Can't open stream (file.smo)\n - test.cpp(12)", "derived virtual formatter routes through shared manager");
        Require(dispatch.FormatChainForAnalysis(true).empty(), "handling clears derived chain");
#endif
        std::cout << "Derived error lifecycle and formatter checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
