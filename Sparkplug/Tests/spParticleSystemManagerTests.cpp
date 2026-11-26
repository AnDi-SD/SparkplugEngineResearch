#include "Code/Sparkplug/spParticleSystemManager.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using Manager = sparkplug::reconstruction::spParticleSystemManager;
using Address = Manager::Address;
using Node = Manager::ListNodeForAnalysis;
using List = Manager::ListForAnalysis;
using Result = Manager::UpdateResultForAnalysis;
using Call = std::pair<Address, std::uint32_t>;

void Require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

struct Trace
{
    Address core = 0;
    std::uint32_t delta = 0;
    std::string order;
    std::vector<Call> calls;
    bool changeCoreAndDeltaAfterFirst = false;

    Manager::DispatchForAnalysis Dispatch()
    {
        return {ReadCore, ReadDelta, Update, this};
    }

    static Address ReadCore(void* context)
    {
        auto& self = *static_cast<Trace*>(context);
        self.order += 'c';
        return self.core;
    }

    static std::uint32_t ReadDelta(void* context, Address core)
    {
        auto& self = *static_cast<Trace*>(context);
        Require(core == self.core, "delta read used a stale EngineCore address");
        self.order += 'd';
        return self.delta;
    }

    static void Update(void* context, Address particle, std::uint32_t bits)
    {
        auto& self = *static_cast<Trace*>(context);
        self.order += 'u';
        self.calls.emplace_back(particle, bits);
        if (self.changeCoreAndDeltaAfterFirst && self.calls.size() == 1)
        {
            self.core += 4;
            self.delta = 0x80000000;
        }
    }
};

std::uint32_t Parse(std::string_view text, int base)
{
    std::uint32_t value{};
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value, base);
    Require(parsed.ec == std::errc{} && parsed.ptr == end, "invalid replay number");
    return value;
}

std::uint32_t ReadWord(std::istringstream& fields)
{
    std::string text;
    Require(bool(fields >> text) && text.size() == 8, "expected eight hexadecimal digits");
    return Parse(text, 16);
}

void LinkRing(Node& head, std::vector<Node>& nodes)
{
    head.next = nodes.empty() ? &head : &nodes.front();
    for (std::size_t index = 0; index < nodes.size(); ++index)
        nodes[index].next = index + 1 == nodes.size() ? &head : &nodes[index + 1];
}

int Replay()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream fields(line);
        Trace trace;
        trace.core = ReadWord(fields);
        trace.delta = ReadWord(fields);
        std::string text;
        Require(bool(fields >> text), "expected a node count");
        const auto count = Parse(text, 10);
        std::vector<Node> nodes;
        while (fields >> text)
        {
            Require(text.size() == 8, "expected eight hexadecimal digits for the particle address");
            nodes.push_back({nullptr, Parse(text, 16)});
        }
        Require(nodes.size() == count, "particle addresses do not match the node count");
        Node head;
        LinkRing(head, nodes);
        Require(Manager::UpdateExistingForAnalysis({Manager::CapturedPrimary, &head}, trace.Dispatch())
            == Result::Completed, "manager refused the replay case");
        std::size_t coreReads = 0, deltaReads = 0;
        for (char step : trace.order)
        {
            coreReads += step == 'c';
            deltaReads += step == 'd';
        }
        std::cout << std::dec << coreReads << ' ' << deltaReads << ' ' << trace.calls.size();
        for (const auto& [particle, bits] : trace.calls)
            std::cout << ' ' << std::hex << std::setfill('0') << std::setw(8) << particle
                << ' ' << std::setw(8) << bits;
        std::cout << '\n';
    }
    Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
    return 0;
}

void CheckDispatch()
{
    Node head;
    head.next = &head;
    const List list{Manager::CapturedPrimary, &head};
    Trace trace;
    Require(Manager::UpdateExistingForAnalysis(list, trace.Dispatch()) == Result::Completed
        && trace.order.empty(), "empty ring consulted EngineCore");
    Require(Manager::UpdateExistingForAnalysis(list, {}) == Result::Completed,
        "empty ring requires no provider");
    Require(Manager::UpdateExistingForAnalysis({}, trace.Dispatch()) == Result::UnqualifiedPrimary,
        "untyped list was accepted");

    const Node last{&head, 0x31000100};
    const Node first{&last, 0x31000100};
    head.next = &first;
    Require(Manager::UpdateExistingForAnalysis(list, trace.Dispatch()) == Result::MissingExistingEngineCore
        && trace.order == "c" && trace.calls.empty(), "missing core was used");

    trace.order.clear();
    trace.core = 0x31000200;
    trace.delta = 0x3E800000;
    trace.changeCoreAndDeltaAfterFirst = true;
    Require(Manager::UpdateExistingForAnalysis(list, trace.Dispatch()) == Result::Completed,
        "qualified ring refused");
    Require(trace.order == "cducdu", "dispatch order or provider reload changed");
    Require(trace.calls == std::vector<Call>{{first.particle, 0x3E800000}, {last.particle, 0x80000000}},
        "duplicate borrowed payload or exact delta bits changed");
    Require(head.next == &first && first.next == &last && last.next == &head,
        "dispatcher changed borrowed links");
}

void CheckLongCoherentRing()
{
    Node head;
    std::vector<Node> nodes(4097, Node{nullptr, 0x31000100});
    LinkRing(head, nodes);
    Trace trace;
    trace.core = 0x31000200;
    trace.delta = 0x3E800000;
    Require(Manager::UpdateExistingForAnalysis({Manager::CapturedPrimary, &head}, trace.Dispatch())
        == Result::Completed, "long finite sentinel-ended ring was refused");
    Require(trace.calls.size() == nodes.size(), "coherent ring did not reach the final node");
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            return Replay();
        if (argc != 1)
            return 2;
        CheckDispatch();
        CheckLongCoherentRing();
        std::cout << "ParticleSystemManager dispatch checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
