#include "Code/Sparkplug/spMessageQueue.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Analysis/PC/spMessageQueueAbi.h"
#include "Analysis/PS2/spMessageQueueAbi.h"
#include <array>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace sparkplug::reconstruction;
namespace
{
    using Result = spMessageQueueTickResultForAnalysis;
    using Numbers = std::vector<std::uint64_t>;
    using Events = std::vector<Numbers>;
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }

    struct Receiver final : spBaseObject
    {
        unsigned id = 0;
        unsigned* destructions = nullptr;
        std::function<void(unsigned, const spNotificationForAnalysis&)> notify;
        ~Receiver() override { if (destructions) ++*destructions; }
        void vfunc_0C(const void* message) noexcept override
        {
            if (notify) notify(id, *static_cast<const spNotificationForAnalysis*>(message));
        }
    };

    struct Fixture final : spMessageQueueHost
    {
        spTaskTimer clock;
        std::unique_ptr<spSubscriptionManager> manager;
        std::array<Receiver, 3> receivers;
        spMessageQueue queue;
        Events events;
        std::function<void(const spNotificationForAnalysis&)> action;
        unsigned managerCreations = 0;
        std::size_t pendingAtCreation = 999, liveAtCreation = 999;
        bool nestedCompleted = true;

        Fixture() : queue(clock, this)
        {
            for (unsigned i = 0; i < receivers.size(); ++i)
            {
                receivers[i].id = i + 1;
                receivers[i].notify = [this](unsigned id, const auto& packet) {
                    Numbers event{0, HeadCode(), queue.LiveMessageCountForAnalysis(), id};
                    const auto words = Words(packet); event.insert(event.end(), words.begin(), words.end());
                    events.push_back(std::move(event));
                    if (action) action(packet);
                };
            }
        }
        spSubscriptionManager& CreateSubscriptionManagerForAnalysis() override
        {
            ++managerCreations;
            pendingAtCreation = queue.PendingCountForAnalysis();
            liveAtCreation = queue.LiveMessageCountForAnalysis();
            manager = std::make_unique<spSubscriptionManager>();
            return *manager;
        }
        void Now(std::uint32_t now)
        {
            auto state = clock.GetStateForAnalysis(); state.currentMilliseconds = now;
            clock.SetStateForAnalysis(state);
        }
        spNotificationForAnalysis Packet(std::uint32_t code, std::uint32_t token = 1,
            std::uint32_t key = 0, std::uint32_t payload = 0x7FA12345, std::uint32_t tail = 0xFF800000)
        {
            spNotificationForAnalysis packet;
            packet.code = code; packet.word04 = 0xAAAAAAAA; packet.word08 = 0x13579BDF;
            packet.word0C = key; packet.sender = &clock; packet.word14 = token;
            packet.payload = payload; packet.tail = tail;
            return packet;
        }
        void Enqueue(spNotificationForAnalysis& packet, std::uint32_t delay = 0)
        {
            spMessageQueueReceiverBindingForAnalysis binding;
            if (packet.word0C == 0 && packet.word14 >= 1 && packet.word14 <= receivers.size())
                binding = {packet.word14, &receivers[packet.word14 - 1]};
            queue.EnqueueForAnalysis(packet, delay, binding);
        }
        std::uint32_t HeadCode() const
        {
            const auto packets = queue.PendingMessagesForAnalysis();
            return packets.empty() ? 0 : packets.front().code;
        }
        Numbers Words(const spNotificationForAnalysis& packet) const
        {
            return {packet.code, packet.word04, packet.word08, packet.word0C,
                packet.sender == &clock ? 0xE0000001 : reinterpret_cast<std::uintptr_t>(packet.sender),
                packet.word14, packet.payload, packet.tail};
        }
        Numbers Snapshot() const
        {
            const auto packets = queue.PendingMessagesForAnalysis();
            Numbers result{packets.size(), queue.LiveMessageCountForAnalysis(), clock.GetStateForAnalysis().currentMilliseconds};
            for (const auto& packet : packets)
            {
                const auto words = Words(packet); result.insert(result.end(), words.begin(), words.end());
            }
            return result;
        }
        void Post(unsigned kind)
        {
            events.push_back({kind, HeadCode(), queue.LiveMessageCountForAnalysis()});
        }
        std::vector<std::uint32_t> DeliveredCodes() const
        {
            std::vector<std::uint32_t> result;
            for (const auto& event : events) if (event[0] == 0) result.push_back(static_cast<std::uint32_t>(event[4]));
            return result;
        }
    };

    void UnitTests()
    {
        {
            Fixture f;
            Check(f.queue.IsExactly(spMessageQueue::ClassID) && f.queue.IsKindOf(spBaseObject::ClassID), "Queue RTTI");
            Check(spMessageQueue::StaticRTTI().factory == nullptr && !f.queue.Clone(), "No invented factory or clone");
            Check(f.queue.TickForAnalysis() == Result::Completed && f.queue.PendingCountForAnalysis() == 0, "Empty Tick");
            auto first = f.Packet(1), second = f.Packet(2), third = f.Packet(3);
            f.Now(0xFFFFFFF0); f.Enqueue(first, 32); f.Enqueue(second, 16); f.Enqueue(third, 32);
            Check(first.word04 == 16 && second.word04 == 0 && third.word04 == 16, "Caller due mutation and uint32 wrap");
            const auto pending = f.queue.PendingMessagesForAnalysis();
            Check(pending[0].code == 2 && pending[1].code == 1 && pending[2].code == 3, "Unsigned order and stable ties");
            Check(pending[1].payload == 0x7FA12345 && pending[1].tail == 0xFF800000, "Raw payload words preserved");
            Check(f.queue.TickForAnalysis() == Result::Completed && f.DeliveredCodes() == std::vector<std::uint32_t>({2, 1, 3}), "Unsigned due comparison");
            Check(f.queue.LiveMessageCountForAnalysis() == 0, "Release after normal callbacks");
        }
        {
            Fixture f; f.Now(1000);
            auto a = f.Packet(10), b = f.Packet(11), c = f.Packet(12);
            f.Enqueue(a); f.Enqueue(b); f.Enqueue(c);
            bool held = false;
            f.action = [&](const auto& current) {
                if (current.code != 10) return;
                f.queue.ClearForAnalysis();
                held = current.code == 10 && f.queue.PendingCountForAnalysis() == 0 && f.queue.LiveMessageCountForAnalysis() == 1;
            };
            Check(f.queue.TickForAnalysis() == Result::Completed && held, "Clear callback preserves current borrowed message");
            Check(f.DeliveredCodes() == std::vector<std::uint32_t>({10}) && f.queue.LiveMessageCountForAnalysis() == 0, "Cleared nodes never dispatch");
        }
        {
            Fixture f; f.Now(1000);
            auto a = f.Packet(10), b = f.Packet(11), d = f.Packet(13);
            f.Enqueue(a); f.Enqueue(b, 1500); bool changed = false;
            f.action = [&](const auto& packet) {
                if (packet.code == 10 && !changed)
                {
                    changed = true; f.Now(2000); f.Enqueue(d);
                    f.nestedCompleted = f.queue.TickForAnalysis() == Result::Completed;
                }
            };
            Check(f.queue.TickForAnalysis() == Result::Completed && f.nestedCompleted, "Native-style nested Tick");
            Check(f.DeliveredCodes() == std::vector<std::uint32_t>({10, 13}) && f.queue.PendingCountForAnalysis() == 1, "Nested clock and outer captured now");
        }
        {
            spTaskTimer clock; Receiver receiver; spMessageQueue q(clock);
            auto packet = spNotificationForAnalysis{}; packet.word14 = 1;
            q.EnqueueForAnalysis(packet, 0, {2, &receiver});
            Check(q.TickForAnalysis() == Result::UnqualifiedReceiverBinding && q.PendingCountForAnalysis() == 1, "Typed token mismatch guard is analytical");
            q.ClearForAnalysis(); packet.word14 = 0;
            q.EnqueueForAnalysis(packet, 0, {0, &receiver});
            Check(q.TickForAnalysis() == Result::UnqualifiedReceiverBinding, "Native null address never mapped to invented receiver");
            q.ClearForAnalysis(); packet.word0C = 1;
            q.EnqueueForAnalysis(packet, 0);
            Check(q.TickForAnalysis() == Result::UnqualifiedManagerProvider && q.LiveMessageCountForAnalysis() == 1, "Missing provider guard preserves queued work");
        }
        {
            Fixture f; f.Now(1000); auto packet = f.Packet(0x2717, 0xDEADBEEF, 0x80000000);
            f.Enqueue(packet);
            Check(f.queue.TickForAnalysis() == Result::Completed && f.managerCreations == 1, "Actual lazy shared manager");
            Check(f.pendingAtCreation == 0 && f.liveAtCreation == 1, "Lazy creation follows unlink and precedes release");
            f.Enqueue(packet);
            Check(f.queue.TickForAnalysis() == Result::Completed && f.managerCreations == 1, "Existing real singleton reused");
        }
        {
            Fixture f; f.manager = std::make_unique<spSubscriptionManager>();
            for (unsigned i = 3; i; --i) Check(f.manager->SubscribeForAnalysis(0xFFFFFFFF, f.receivers[i - 1]), "Real subscription insertion");
            auto packet = f.Packet(0x2717, 0xDEADBEEF, 0xFFFFFFFF); f.Enqueue(packet);
            Check(f.queue.TickForAnalysis() == Result::Completed && f.events.size() == 3, "High-bit key broadcasts instead of dereferencing14");
            Check(f.events[0][3] == 1 && f.events[1][3] == 2 && f.events[2][3] == 3, "Real manager pointer ordering");
        }
        {
            unsigned destroyed = 0; Receiver receiver; receiver.destructions = &destroyed;
            spTaskTimer clock;
            {
                spMessageQueue first(clock), destination(clock); spNotificationForAnalysis p; p.word14 = 7;
                first.EnqueueForAnalysis(p, 0, {7, &receiver}); destination.EnqueueForAnalysis(p, 0, {7, &receiver});
                spCloneManager clones;
                Check(first.vfunc_14(destination, clones) && first.PendingCountForAnalysis() == 1 && destination.PendingCountForAnalysis() == 1, "Inherited Copy does not clear/copy queue");
            }
            Check(destroyed == 0, "Queue destructor never owns receiver or packet pointers");
        }
    }

    void Emit(const Numbers& values) { for (auto value : values) std::cout << ' ' << value; }
    void Emit(const Events& events)
    {
        std::cout << ' ' << events.size();
        for (const auto& event : events) Emit(event);
    }
    void Replay(std::istream& input)
    {
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty()) continue;
            std::istringstream in(line); char command; in >> command; Fixture f;
            if (command == 'o')
            {
                std::uint32_t now; unsigned count; in >> now >> count;
                if (count > 64) throw std::runtime_error("Replay order count");
                f.Now(now);
                for (unsigned i = 0; i < count; ++i)
                {
                    std::uint32_t delay; in >> delay;
                    auto packet = f.Packet(0x1000 + i, i % 3 + 1, 0, 0x7FA12345 + i);
                    f.Enqueue(packet, delay);
                }
                const auto before = f.Snapshot(); f.events.clear();
                if (f.queue.TickForAnalysis() != Result::Completed) throw std::runtime_error("Order Tick guard");
                std::cout << 'o'; Emit(before); Emit(f.Snapshot()); Emit(f.events); std::cout << '\n';
            }
            else if (command == 's')
            {
                std::uint32_t key; unsigned count, lazy; in >> key >> count >> lazy;
                if (count > 3) throw std::runtime_error("Replay subscriber count");
                f.Now(1000);
                if (!lazy)
                {
                    f.manager = std::make_unique<spSubscriptionManager>();
                    for (unsigned i = count; i; --i) (void)f.manager->SubscribeForAnalysis(key, f.receivers[i - 1]);
                }
                auto packet = f.Packet(0x2717, 0xDEADBEEF, key); f.Enqueue(packet); f.events.clear();
                if (f.queue.TickForAnalysis() != Result::Completed) throw std::runtime_error("Subscription Tick guard");
                std::cout << 's'; Emit(f.Snapshot()); Emit(f.events); std::cout << '\n';
            }
            else if (command == 'm')
            {
                unsigned mutation; in >> mutation; if (mutation > 7) throw std::runtime_error("Replay mutation");
                f.Now(1000); std::vector<std::uint32_t> delays;
                if (mutation == 6) delays = {0, 500, 1000, 1001};
                else if (mutation == 0 || mutation == 7) delays = {0, 1, 2};
                else delays = {0, 0, 0};
                for (unsigned i = 0; i < delays.size(); ++i) { auto packet = f.Packet(10 + i); f.Enqueue(packet, delays[i]); }
                const auto before = f.Snapshot();
                auto extra = f.Packet(13, 1, 0, 0x7FC12345, 0x80000000); bool acted = false;
                f.action = [&](const auto& packet) {
                    if (packet.code != 10 || acted) return; acted = true;
                    if (mutation == 0 || mutation == 1) f.Now(mutation == 0 ? 2000 : 0);
                    else if (mutation == 2) { f.queue.ClearForAnalysis(); f.Post(1); }
                    else if (mutation <= 5) { f.Now(mutation == 3 ? 1000 : mutation == 4 ? 999 : 2000); f.Enqueue(extra); f.Post(2); }
                    else { f.Now(mutation == 6 ? 2000 : 0); f.nestedCompleted = f.queue.TickForAnalysis() == Result::Completed; f.Post(3); }
                };
                f.events.clear();
                if (f.queue.TickForAnalysis() != Result::Completed || !f.nestedCompleted) throw std::runtime_error("Mutation Tick guard");
                const auto after = f.Snapshot(); const auto firstEvents = f.events;
                std::cout << 'm'; Emit(before); Emit(after); Emit(firstEvents);
                const bool second = mutation == 0 || mutation == 5; std::cout << ' ' << second;
                if (second)
                {
                    f.events.clear();
                    if (f.queue.TickForAnalysis() != Result::Completed) throw std::runtime_error("Second Tick guard");
                    Emit(f.Snapshot()); Emit(f.events);
                }
                std::cout << '\n';
            }
            else throw std::runtime_error("Unknown replay command");
            if (!in) throw std::runtime_error("Malformed replay row");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") { Replay(std::cin); return 0; }
        UnitTests(); std::cout << "PASS " << checks << " Queue checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
