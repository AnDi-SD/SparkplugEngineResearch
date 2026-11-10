// Real owned Windows timer/period lifecycle checks. Cadence values are observed
// diagnostics, not a tight scheduling guarantee or network/gameplay acceptance.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/Networking/WindowsPumpWait.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using winxcoop::networking::WindowsPumpCadence;
using winxcoop::networking::WindowsPumpWait;
namespace
{
    unsigned checks = 0;
    void Check(bool condition, const char* text)
    { ++checks; if (!condition) throw std::runtime_error(text); }
    std::string Field(const std::string& json, const char* name)
    {
        const std::string key = std::string("\"") + name + "\":";
        const auto at = json.find(key);
        Check(at != std::string::npos, "diagnostic field exists");
        const auto start = at + key.size(), end = json.find_first_of(",}", start);
        return json.substr(start, end == std::string::npos ? end : end - start);
    }
    void ExerciseOwnedWait(bool highResolutionPreferred)
    {
        WindowsPumpWait wait(highResolutionPreferred);
        WindowsPumpCadence cadence;
        Check(!wait.Balanced(), "live timer remains an owned handle until close");
        Check(Field(wait.Json(), "timerHandlesCreated") == "1" &&
            Field(wait.Json(), "timerHandlesClosed") == "0", "exact live timer accounting");
        if (!highResolutionPreferred)
        {
            Check(wait.Json().find("scoped-period-normal-timer") != std::string::npos,
                "explicit normal branch uses original scoped fallback policy");
            Check(Field(wait.Json(), "periodBeginCount") == "1" &&
                Field(wait.Json(), "periodEndCount") == "0", "normal branch owns one resolution request");
        }
        Check(cadence.Observe(), "initial QPC observation");
        for (unsigned i = 0; i < 8; ++i)
        {
            Check(wait.WaitOneMillisecond(), "real relative one-shot wait succeeds");
            Check(cadence.Observe(), "following QPC observation is monotonic");
        }
        const auto fields = cadence.JsonFields();
        Check(Field(fields, "pumpIntervals") == "8", "QPC cadence has one interval per wait");
        const auto average = std::stod(Field(fields, "pumpAverageMicroseconds"));
        const auto maximum = std::stod(Field(fields, "pumpMaximumMicroseconds"));
        Check(std::isfinite(average) && average > 0 && std::isfinite(maximum) && maximum >= average - 1,
            "observed waits have finite nonzero cadence without a latency promise");
        Check(Field(wait.Json(), "waitCalls") == "8" &&
            Field(wait.Json(), "waitFailures") == "0", "successful waits counted exactly");
        wait.Close();
        Check(wait.Balanced(), "timer and any period request close in balance");
        const auto closed = wait.Json();
        Check(Field(closed, "timerHandlesClosed") == "1" &&
            Field(closed, "closeError") == "0", "native handle close confirmed");
        Check(Field(closed, "periodBeginCount") == Field(closed, "periodEndCount"),
            "every resolution request has its paired release");
        wait.Close();
        Check(wait.Json() == closed, "second close has no side effects");
        Check(!wait.WaitOneMillisecond(), "closed wait rejects instead of spinning or reopening");
        Check(Field(wait.Json(), "waitFailures") == "1" && wait.Balanced(),
            "closed-call failure is separate from balanced resource ownership");
    }
}

int main()
{
    try
    {
        ExerciseOwnedWait(true);
        ExerciseOwnedWait(false);
        std::cout << "WindowsPumpWait checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "WindowsPumpWait failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
