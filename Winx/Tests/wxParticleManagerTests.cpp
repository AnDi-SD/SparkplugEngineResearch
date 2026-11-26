#include "Code/wxParticleManager.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using Manager = wxParticleManager;
    using Address = Manager::Address;
    using Link = Manager::ListNodeForAnalysis;
    using Result = Manager::UpdateResultForAnalysis;
    constexpr auto Profile = Manager::ProfileForAnalysis::PcExistingOwnersFiniteNearest64;
    constexpr Address Primary = 0x00703ED0;

    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct Member
    {
        Address address = 0x31000100;
        Manager::ParticleForAnalysis particle{0x31000200, 0x3F000000, 0xBF800000, 0x40000000};
        Manager::RenderNodeForAnalysis node{0x200, {}, 0x31000300};
    };

    struct Effect
    {
        char kind;
        Address address;
        std::uint32_t bits;
    };

    struct Case
    {
        Manager::PositionBits position{};
        Address core = 0x31000400;
        std::uint32_t delta = 0x3E800000;
        Address game = 0x31000500;
        std::uint32_t state = 0;
        Address scene = 0x31000300;
        std::vector<Member> members;
        std::string order;
        std::vector<Effect> effects;

        static Address Core(void* context)
        {
            auto& self = *static_cast<Case*>(context);
            self.order += 'c';
            return self.core;
        }

        static std::uint32_t Delta(void* context, Address core)
        {
            auto& self = *static_cast<Case*>(context);
            Require(core == self.core, "delta used another core");
            self.order += 'd';
            return self.delta;
        }

        static Address Game(void* context)
        {
            auto& self = *static_cast<Case*>(context);
            self.order += 'g';
            return self.game;
        }

        static std::uint32_t State(void* context, Address game)
        {
            auto& self = *static_cast<Case*>(context);
            Require(game == self.game, "state used another game");
            self.order += 's';
            return self.state;
        }

        Member& FindParticle(Address address)
        {
            for (auto& member : members)
            {
                if (member.address == address)
                    return member;
            }
            throw std::runtime_error("unknown borrowed particle address");
        }

        static const Manager::ParticleForAnalysis* Particle(void* context, Address address)
        {
            return &static_cast<Case*>(context)->FindParticle(address).particle;
        }

        static const Manager::RenderNodeForAnalysis* Node(void* context, Address address)
        {
            const auto& self = *static_cast<Case*>(context);
            for (const auto& member : self.members)
            {
                if (member.particle.node == address)
                    return &member.node;
            }
            throw std::runtime_error("unknown borrowed node address");
        }

        static Address Scene(void* context, Address core)
        {
            auto& self = *static_cast<Case*>(context);
            Require(core == self.core, "scene used another core");
            self.order += 'r';
            return self.scene;
        }

        static void WriteClock(void* context, Address particle, std::uint32_t bits)
        {
            auto& self = *static_cast<Case*>(context);
            self.order += 'k';
            self.effects.push_back({'K', particle, bits});
            self.FindParticle(particle).particle.clockBits = bits;
        }

        static void Update(void* context, Address particle, std::uint32_t bits)
        {
            auto& self = *static_cast<Case*>(context);
            self.order += 'u';
            self.effects.push_back({'U', particle, bits});
        }

        Result Run()
        {
            std::vector<Link> links(members.size());
            Link head;
            head.next = links.empty() ? &head : &links.front();
            for (std::size_t i = 0; i < links.size(); ++i)
            {
                links[i].particle = members[i].address;
                links[i].next = i + 1 == links.size() ? &head : &links[i + 1];
            }
            const Manager::DispatchForAnalysis dispatch{Core, Delta, Game, State, Particle, Node,
                Scene, WriteClock, Update, this};
            return Manager::UpdateExistingForAnalysis({Primary, &head, position}, dispatch, Profile);
        }
    };

    std::uint32_t ParseNumber(std::string_view text, int base)
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
        return ParseNumber(text, 16);
    }

    Manager::PositionBits ReadPosition(std::istringstream& fields)
    {
        Manager::PositionBits value{};
        for (auto& bits : value)
            bits = ReadWord(fields);
        return value;
    }

    int Replay()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream fields(line);
            Case input;
            input.position = ReadPosition(fields);
            input.core = ReadWord(fields);
            input.delta = ReadWord(fields);
            input.game = ReadWord(fields);
            input.state = ReadWord(fields);
            input.scene = ReadWord(fields);
            std::string text;
            Require(bool(fields >> text), "expected a decimal node count");
            const auto count = ParseNumber(text, 10);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Member member;
                member.address = ReadWord(fields);
                member.particle.node = ReadWord(fields);
                member.node.flags = ReadWord(fields);
                member.particle.clockBits = ReadWord(fields);
                member.particle.durationBits = ReadWord(fields);
                member.particle.lifetimeBits = ReadWord(fields);
                member.node.worldPosition = ReadPosition(fields);
                member.node.scene = ReadWord(fields);
                input.members.push_back(member);
            }
            Require(!(fields >> text), "extra replay fields");
            Require(input.Run() == Result::Completed, "manager refused a qualified original replay");
            std::cout << std::dec;
            for (char field : {'c', 'd', 'g', 's', 'r'})
                std::cout << std::count(input.order.begin(), input.order.end(), field) << ' ';
            std::cout << input.effects.size();
            for (const auto& effect : input.effects)
                std::cout << ' ' << effect.kind << ' ' << std::hex << std::setfill('0') << std::setw(8)
                    << effect.address << ' ' << std::setw(8) << effect.bits;
            std::cout << '\n';
        }
        Require(std::cin.eof() && bool(std::cout), "replay input or output failed");
        return 0;
    }

    void CheckOne(Case input, char kind, std::uint32_t bits, std::string_view order)
    {
        Require(input.Run() == Result::Completed && input.order == order, "manager provider order changed");
        Require(input.effects.size() == 1 && input.effects[0].kind == kind
            && input.effects[0].address == input.members[0].address && input.effects[0].bits == bits,
            "manager effect or raw float bits changed");
    }

    void CheckRoutes()
    {
        Case empty;
        Require(empty.Run() == Result::Completed && empty.order == "cdgs" && empty.effects.empty(),
            "empty list lost the original provider reads");
        Case withNull;
        withNull.members = {Member{}};
        withNull.members[0].address = 0;
        Require(withNull.Run() == Result::Completed && withNull.order == "cdgs" && withNull.effects.empty(),
            "zero payload invoked a child");

        Case detached;
        detached.members = {Member{}};
        detached.members[0].particle.node = 0;
        detached.members[0].particle.durationBits = 0x7FC00000; // unread on this route
        CheckOne(detached, 'K', 0x3F400000, "cdgsk");
        detached.delta = 0x80000000;
        detached.members[0].particle.clockBits = 0x80000000;
        CheckOne(detached, 'K', 0x80000000, "cdgsk");

        Case predicate;
        predicate.members = {Member{}};
        predicate.members[0].node.flags = 0;
        Require(predicate.Run() == Result::UnqualifiedNodePredicate && predicate.order == "cdgs"
            && predicate.effects.empty(), "unqualified virtual predicate was modeled");

        Case duration;
        duration.members = {Member{}};
        auto& particle = duration.members[0].particle;
        particle.clockBits = 0x40400000;
        particle.durationBits = 0x3F800000;
        CheckOne(duration, 'K', 0x40500000, "cdgsk");
        particle.clockBits = 0x3F800000;
        particle.durationBits = 0x33000000;
        particle.lifetimeBits = 0x3F800000;
        CheckOne(duration, 'U', duration.delta, "cdgsu");

        Case distance;
        distance.members = {Member{}};
        distance.position = {0x451C3FFF, 0x3F800000, 0};
        CheckOne(distance, 'U', distance.delta, "cdgsu");
        distance.position = {0x451C4000, 0, 0};
        CheckOne(distance, 'K', 0x3F400000, "cdgscrk");
        distance.members[0].node.scene += 4;
        CheckOne(distance, 'U', distance.delta, "cdgscru");
        distance.members[0].node.scene = distance.scene;
        distance.state = 0x21;
        CheckOne(distance, 'U', distance.delta, "cdgsu");
        distance.position = {0x45BB8000, 0x45160000, 0x44480000}; // 6000, 2400, 800
        CheckOne(distance, 'K', 0x3F400000, "cdgscrk");

        Case origin;
        origin.members = {Member{}};
        origin.members[0].node.worldPosition[0] = 0x461C4000; // 10000
        origin.position[0] = 0x3A83126F;
        CheckOne(origin, 'U', origin.delta, "cdgsu");
        origin.position[0] = 0x3A831270;
        CheckOne(origin, 'K', 0x3F400000, "cdgscrk");
    }

    void CheckArithmeticDomain()
    {
        const auto verifyRefusal = [](Case input)
        {
            Require(input.Run() == Result::UnqualifiedArithmetic && input.order == "cdgs" && input.effects.empty(),
                "out-of-profile arithmetic reached a clock write or particle update");
        };

        Case origin;
        origin.members = {Member{}};
        origin.members[0].node.worldPosition[0] = 0x7FC00000;
        // The original reads the three node coordinates before the origin branch discards the differences.
        verifyRefusal(origin);

        Case detached;
        detached.members = {Member{}};
        auto& particle = detached.members[0].particle;
        particle.node = 0;
        particle.clockBits = 0x7F7FFFFF;
        detached.delta = 0x7F7FFFFF;
        verifyRefusal(detached); // the clock-only binary32 store would overflow

        particle.clockBits = 0x00800001;
        detached.delta = 0x80800000;
        verifyRefusal(detached); // cancellation leaves a subnormal binary32 result
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
        CheckRoutes();
        CheckArithmeticDomain();
        std::cout << "ParticleManager dispatch checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
