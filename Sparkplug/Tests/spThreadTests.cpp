#include "Code/SparkplugPC/spPCThread.h"
#include "Analysis/PC/spThreadAbi.h"
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Thread = spPCThread;
    using Host = Thread::HostForAnalysis;
    int checks = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Fixture
    {
        Thread thread;
        Host host;
        std::vector<std::vector<std::uint64_t>> calls;
        std::uint32_t answer = 1, exitCode = 259;
        bool apiSuccess = true;
        Fixture()
        {
            thread.BindHostForAnalysis(host);
            host.create = [this](const Host::CreateRequest& request) {
                Check(request.parameter == &thread, "Create receives full object, not body parameter");
                calls.push_back({0, request.entryToken, thread.GetStateForAnalysis().bodyParameter,
                    request.flags, request.stackBytes});
                return Host::Handle(answer);
            };
            host.wait = [this](Host::Handle handle, std::uint32_t duration) {
                calls.push_back({1, handle, duration});
                return answer;
            };
            host.exitCode = [this](Host::Handle handle) {
                calls.push_back({2, handle});
                return Host::ExitCodeObservation{apiSuccess, exitCode};
            };
            host.resume = [this](Host::Handle handle) {
                calls.push_back({3, handle, thread.GetStateForAnalysis().opaqueByte});
                return answer;
            };
            host.suspend = [this](Host::Handle handle) {
                calls.push_back({4, handle});
                return answer;
            };
            host.sleep = [this](std::uint32_t duration) { calls.push_back({5, duration}); };
            host.terminate = [this](Host::Handle handle, std::uint32_t code) {
                calls.push_back({6, handle, code});
                // Native BOOL result is tested for equality1, not nonzero.
                std::int32_t result;
                const auto bits = answer;
                std::memcpy(&result, &bits, sizeof(result));
                return result;
            };
        }
    };
    void Case(const std::string& line)
    {
        std::istringstream in(line);
        std::uint32_t operation, handle, body, opaque, argument, parameter, answer, exitCode, success;
        if (!(in >> operation >> handle >> body >> opaque >> argument >> parameter >> answer >> exitCode >> success))
            throw std::runtime_error("Missing thread fixture values");
        Fixture f;
        f.thread.SetStateForAnalysis({body, std::uint8_t(opaque), handle});
        f.answer = answer;
        f.exitCode = exitCode;
        f.apiSuccess = success != 0;
        bool result = false;
        switch (operation)
        {
        case 0: result = f.thread.CreateForAnalysis(argument, parameter); break;
        case 1: result = f.thread.WaitForAnalysis(argument); break;
        case 2: result = f.thread.IsRunningForAnalysis(); break;
        case 3: result = f.thread.ResumeForAnalysis(); break;
        case 4: result = f.thread.SuspendForAnalysis(); break;
        case 5: f.thread.SleepForAnalysis(argument); break; // void, result absent
        case 6: result = f.thread.TerminateForAnalysis(argument); break;
        default: throw std::runtime_error("Unknown original thread operation");
        }
        const auto state = f.thread.GetStateForAnalysis();
        std::cout << "{\"result\":";
        if (operation == 5)
            std::cout << "null";
        else
            std::cout << int(result);
        std::cout << ",\"state\":[" << state.bodyParameter << ',' << unsigned(state.opaqueByte) <<
            ',' << state.handle << "],\"calls\":[";
        bool separator = false;
        for (const auto& call : f.calls)
        {
            if (separator)
                std::cout << ',';
            separator = true;
            std::cout << '[';
            for (std::size_t index = 0; index < call.size(); ++index)
                std::cout << (index ? "," : "") << call[index];
            std::cout << ']';
        }
        std::cout << "]}\n";
    }
    void MainChecks()
    {
        static_assert(std::is_abstract_v<spThread>);
        Check(spThread::ClassID == 0x3DFE3B16 && Thread::ClassID == 0x438758EA,
            "exact registered class identities");
        Thread unbound;
        Check(unbound.WaitForAnalysis(0xffffffff) && !unbound.IsRunningForAnalysis() &&
            !unbound.ResumeForAnalysis() && !unbound.SuspendForAnalysis() &&
            !unbound.TerminateForAnalysis(0), "null handle original paths need no OS service");
        bool failed = false;
        try { (void)unbound.CreateForAnalysis(1, 2); } catch (const std::runtime_error&) { failed = true; }
        Check(failed && unbound.GetStateForAnalysis().bodyParameter == 0,
            "missing Create host is rejected by explicit host preflight");
        Fixture f;
        f.thread.SetName("thread-name");
        f.thread.SetStateForAnalysis({123, 255, 5});
        f.answer = 0;
        Check(!f.thread.CreateForAnalysis(0x12345678, 42) &&
            f.thread.GetStateForAnalysis().bodyParameter == 42 &&
            f.thread.GetStateForAnalysis().handle == 0 &&
            f.thread.GetStateForAnalysis().opaqueByte == 255,
            "failed Create overwrites body/handle and preserves opaque byte");
        f.thread.SetStateForAnalysis({42, 255, 7});
        f.answer = 0xffffffff;
        Check(f.thread.WaitForAnalysis(50), "WAIT_FAILED is true in original");
        f.answer = 0x102;
        Check(!f.thread.WaitForAnalysis(50), "only WAIT_TIMEOUT is false");
        f.answer = 0xffffffff;
        Check(f.thread.ResumeForAnalysis() && f.thread.GetStateForAnalysis().opaqueByte == 0,
            "Resume clears byte before OS call, ignores OS result");
        Check(f.thread.SuspendForAnalysis(), "Suspend ignores OS result");
        Check(!f.thread.TerminateForAnalysis(9), "Terminate requires exact OS1");
        f.answer = 1;
        Check(f.thread.TerminateForAnalysis(9) && f.thread.GetStateForAnalysis().handle == 7,
            "successful Terminate preserves stored handle");
        f.apiSuccess = false;
        f.exitCode = 259;
        Check(f.thread.IsRunningForAnalysis(), "exit-code result259 wins even OS failure");
        f.exitCode = 0;
        Check(!f.thread.IsRunningForAnalysis() && f.thread.GetStateForAnalysis().handle == 0,
            "non259 clears handle without closing it");
        f.thread.SetStateForAnalysis({42, 255, 7});
        f.host.exitCode = [](Host::Handle) { return Host::ExitCodeObservation{}; };
        failed = false;
        try { (void)f.thread.IsRunningForAnalysis(); } catch (const std::runtime_error&) { failed = true; }
        Check(failed && f.thread.GetStateForAnalysis().handle == 7,
            "unobserved failed-API residue cannot become fake exitcode0");
        spCloneManager clones;
        auto clone = clones.Clone(f.thread);
        auto* typed = dynamic_cast<Thread*>(clone.get());
        Check(typed && std::string(typed->GetName()) == "thread-name" &&
            typed->GetStateForAnalysis().bodyParameter == 0 &&
            typed->GetStateForAnalysis().opaqueByte == 0 && typed->GetStateForAnalysis().handle == 0,
            "clone copies only name and resets runtime payload");
        Check(f.thread.vfunc_18().IsKindOf(spThread::ClassID) &&
            f.thread.vfunc_18().IsKindOf(spCrossPlatform::ClassID), "original RTTI ancestry");
        std::cout << "PASS " << checks << "/" << checks << ": thread own policy and OS boundary\n";
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
        {
            std::string line;
            while (std::getline(std::cin, line))
                if (!line.empty())
                    Case(line);
        }
        else
            MainChecks();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
