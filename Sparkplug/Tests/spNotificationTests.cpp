#include "Code/SparkBase/spBroadcastNotification.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool value, const char* message)
    {
        if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }
    struct Event { unsigned receiver; spNotificationForAnalysis packet; bool senderPresent; };
    struct Receiver final : spBaseObject
    {
        unsigned id = 0;
        bool mutate = false;
        std::vector<Event>* events = nullptr;
        void vfunc_0C(const void* value) noexcept override
        {
            auto& packet = *static_cast<const spNotificationForAnalysis*>(value);
            events->push_back({id, packet, packet.sender != nullptr});
            // Keep only the observed sender role beyond this synchronous call.
            events->back().packet.sender = nullptr;
            if (mutate)
            {
                auto& writable = const_cast<spNotificationForAnalysis&>(packet);
                writable.code = 0xFE123456;
                writable.word04 = 0xAABBCCDD;
                writable.word08 = 42;
                writable.payload = 0x12345678;
                writable.tail = 0x87654321;
            }
        }
    };
    struct Input
    {
        std::uint32_t code = 0, key = 0, payload = 0, tail = 0;
        unsigned sender = 0, subscribers = 0, lazy = 0, mutate = 0;
    };
    struct Result
    {
        spBroadcastNotificationResultForAnalysis status;
        unsigned factories;
        std::vector<Event> events;
    };
    Result Run(const Input& input)
    {
        Require(input.subscribers <= 3 && (!input.lazy || !input.subscribers), "bounded application inputs");
        Require(!spSubscriptionManager::GetInstance(), "fresh source manager context");
        Result result{spBroadcastNotificationResultForAnalysis::UnqualifiedManagerProvider, 0, {}};
        std::array<Receiver, 3> receivers;
        for (unsigned i = 0; i < receivers.size(); ++i)
        {
            receivers[i].id = i;
            receivers[i].events = &result.events;
            receivers[i].mutate = input.mutate && i == 0;
        }
        std::unique_ptr<spSubscriptionManager> owner;
        if (!input.lazy)
        {
            owner = std::make_unique<spSubscriptionManager>();
            for (unsigned i = input.subscribers; i; --i)
                Require(owner->SubscribeForAnalysis(input.key, receivers[i - 1]), "actual shared source Subscribe");
        }
        spSubscriptionManagerFactoryForAnalysis create = [&]() -> spSubscriptionManager&
        {
            ++result.factories;
            owner = std::make_unique<spSubscriptionManager>();
            return *owner;
        };
        spBaseObject sender;
        result.status = SendBroadcastNotificationForAnalysis(input.sender ? &sender : nullptr,
            input.code, input.key, input.payload, input.tail, create);
        Require(spSubscriptionManager::GetInstance() == owner.get(), "application retains actual source manager");
        return result;
    }
    void Print(const Result& result)
    {
        std::cout << static_cast<unsigned>(result.status) << ' ' << result.factories << ' ' << result.events.size();
        for (const auto& event : result.events)
        {
            const auto& p = event.packet;
            std::cout << ' ' << event.receiver << ' ' << p.code << ' ' << p.word04 << ' ' << p.word08
                << ' ' << p.word0C << ' ' << (event.senderPresent ? 1 : 0) << ' ' << p.word14
                << ' ' << p.payload << ' ' << p.tail;
        }
        std::cout << '\n';
    }
    void Contracts()
    {
        auto result = Run({0x27D1, 3, 1, 0, 1, 3, 0, 1});
        Require(result.status == spBroadcastNotificationResultForAnalysis::Completed
            && result.factories == 0 && result.events.size() == 3, "source manager dispatch completes");
        for (unsigned i = 0; i < 3; ++i) Require(result.events[i].receiver == i, "ordered application receivers");
        Require(result.events[0].packet.code == 0x27D1 && result.events[0].packet.word14 == 0
            && result.events[0].packet.payload == 1, "initial frame observed before mutation");
        Require(result.events[1].packet.code == 0xFE123456 && result.events[2].packet.word04 == 0xAABBCCDD
            && result.events[2].packet.payload == 0x12345678 && result.events[2].packet.tail == 0x87654321,
            "subsequent receivers observe the same borrowed frame");
        result = Run({0, 0, 0x80000000, 0xFFFFFFFF, 0, 0, 1, 0});
        Require(result.status == spBroadcastNotificationResultForAnalysis::Completed
            && result.factories == 1 && result.events.empty(), "real source lazy manager is application owned");
        Require(!spSubscriptionManager::GetInstance(), "application manager lifetime ended");
        Require(SendBroadcastNotificationForAnalysis(nullptr, 0, 0, 0, 0)
            == spBroadcastNotificationResultForAnalysis::UnqualifiedManagerProvider,
            "missing provider is an analytical refusal, never a native false claim");
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--replay")
    {
        Input input;
        while (std::cin >> input.code)
        {
            if (!(std::cin >> input.key >> input.sender >> input.payload >> input.tail
                >> input.subscribers >> input.lazy >> input.mutate)) return EXIT_FAILURE;
            Print(Run(input));
        }
        return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (argc != 1) return EXIT_FAILURE;
    Contracts();
    std::cout << "Notification contracts passed\n";
    return EXIT_SUCCESS;
}
