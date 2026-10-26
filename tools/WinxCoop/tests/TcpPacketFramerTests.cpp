#include "../src/Networking/TcpPacketFramer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace winxcoop::networking;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(const bool condition, const char* description)
    {
        if(!condition)
            throw std::runtime_error(description);
    }

    std::vector<std::uint8_t> Payload(const std::size_t size)
    {
        std::vector<std::uint8_t> data(size);
        for(std::size_t i = 0; i < size; ++i)
            data[i] = static_cast<std::uint8_t>((i * 37u + 11u) & 255u);
        return data;
    }

    std::vector<std::uint8_t> Wire(const std::size_t payloadSize, const std::uint8_t tcp = 255)
    {
        const auto data = Payload(payloadSize);
        spNetworkPacket packet;
        Check(packet.FillForAnalysis(0x1357, 0x2468, 0x9abc, tcp, 0x11223344, 0x89abcdef,
            static_cast<std::uint16_t>(payloadSize), data.empty() ? nullptr : data.data()), "shared Packet Fill");
        spMemoryStream stream;
        Check(stream.Open("WinxCoop framer test"), "shared MemoryStream Open");
        Check(packet.WriteForAnalysis(stream), "shared Packet Write");
        std::uint32_t size = 0;
        Check(stream.GetSize(&size) && size == TcpPacketFramer::HeaderSize + payloadSize, "encoded native frame size");
        const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return {bytes, bytes + size};
    }

    void Packet(const spNetworkPacket& packet, const std::size_t payloadSize, const std::uint8_t tcp = 255)
    {
        const auto& h = packet.HeaderForAnalysis();
        Check(h.source == 0x1357 && h.destination == 0x2468 && h.packetType == 0x9abc
            && h.useTcp == tcp && h.dataField1 == 0x11223344 && h.dataField2 == 0x89abcdef
            && h.size == payloadSize, "shared Packet decoder preserves every header scalar");
        const auto expected = Payload(payloadSize);
        Check(packet.KnownPayloadPrefixForAnalysis() == payloadSize, "actual decoded known payload length");
        const auto* data = packet.KnownPayloadForAnalysis(payloadSize);
        Check(std::equal(expected.begin(), expected.end(), data), "actual decoded payload bytes");
    }

    void Receiving(const TcpPacketFramer& framer, const std::size_t pending)
    {
        Check(framer.GetState() == StreamState::Receiving && framer.GetError() == FramingError::None,
            "receiving has no terminal error");
        Check(framer.PendingByteCount() == pending && pending <= framer.PendingCapacity(), "fixed bounded pending storage");
    }

    void HeaderAndPayloadSplits()
    {
        const auto small = Wire(5);
        for(std::size_t cut = 0; cut <= TcpPacketFramer::HeaderSize; ++cut)
        {
            TcpPacketFramer framer;
            auto prefix = framer.Feed(small.data(), cut);
            Check(prefix.consumed == cut && !prefix.packet, "incomplete header yields no packet");
            Receiving(framer, cut);
            auto tail = framer.Feed(small.data() + cut, small.size() - cut);
            Check(tail.consumed == small.size() - cut && tail.packet, "every header split is reassembled");
            Packet(*tail.packet, 5);
            Receiving(framer, 0);
        }
        const auto maximum = Wire(256);
        for(std::size_t payloadPrefix = 0; payloadPrefix < 256; ++payloadPrefix)
        {
            const auto cut = TcpPacketFramer::HeaderSize + payloadPrefix;
            TcpPacketFramer framer;
            auto prefix = framer.Feed(maximum.data(), cut);
            Check(prefix.consumed == cut && !prefix.packet, "incomplete payload yields no packet");
            Receiving(framer, cut);
            Check(!framer.Feed(nullptr, 0).packet, "empty input is not EOF");
            Receiving(framer, cut);
            auto tail = framer.Feed(maximum.data() + cut, maximum.size() - cut);
            Check(tail.consumed == maximum.size() - cut && tail.packet, "every maximum payload split is reassembled");
            Packet(*tail.packet, 256);
            Receiving(framer, 0);
        }
        TcpPacketFramer bytes;
        std::unique_ptr<spNetworkPacket> packet;
        for(std::size_t i = 0; i < maximum.size(); ++i)
        {
            auto result = bytes.Feed(maximum.data() + i, 1);
            Check(result.consumed == 1 && bool(result.packet) == (i + 1 == maximum.size()), "bytewise streaming completes only at exact end");
            if(result.packet)
                packet = std::move(result.packet);
        }
        Check(packet != nullptr, "bytewise frame exists");
        Packet(*packet, 256);
    }

    void CoalescedFramesAndOwnership()
    {
        std::vector<std::uint8_t> input;
        const std::size_t sizes[] = {0, 1, 256};
        const std::uint8_t tcp[] = {0, 1, 255};
        for(unsigned i = 0; i < 3; ++i)
        {
            const auto frame = Wire(sizes[i], tcp[i]);
            input.insert(input.end(), frame.begin(), frame.end());
        }
        std::vector<std::unique_ptr<spNetworkPacket>> owned;
        std::size_t offset = 0;
        {
            TcpPacketFramer framer;
            for(unsigned i = 0; i < 3; ++i)
            {
                auto result = framer.Feed(input.data() + offset, input.size() - offset);
                Check(result.packet && result.consumed == TcpPacketFramer::HeaderSize + sizes[i], "coalescence yields one exact frame without swallowing the next");
                Packet(*result.packet, sizes[i], tcp[i]);
                offset += result.consumed;
                owned.push_back(std::move(result.packet));
                Receiving(framer, 0);
            }
            Check(offset == input.size(), "coalesced input fully consumed by caller loop");
            Check(framer.EndOfInput().state == StreamState::Ended, "clean EOF after packets");
            framer.Reset();
        }
        for(unsigned i = 0; i < 3; ++i)
            Packet(*owned[i], sizes[i], tcp[i]);

        const auto empty = Wire(0);
        std::vector<std::uint8_t> many;
        for(unsigned i = 0; i < 1024; ++i)
            many.insert(many.end(), empty.begin(), empty.end());
        TcpPacketFramer bounded;
        offset = 0;
        unsigned count = 0;
        while(offset < many.size())
        {
            auto result = bounded.Feed(many.data() + offset, many.size() - offset);
            Check(result.packet && result.consumed == empty.size(), "large coalescence makes bounded one-packet progress");
            offset += result.consumed;
            ++count;
            Receiving(bounded, 0);
        }
        Check(count == 1024 && bounded.PendingCapacity() == 273, "input length does not grow internal storage or queue");
    }

    void OversizeAndCompletePrefix()
    {
        const auto valid = Wire(1);
        auto invalid = Wire(0);
        invalid[TcpPacketFramer::HeaderSize - 2] = 1;
        invalid[TcpPacketFramer::HeaderSize - 1] = 1; // Corrupt a shared-codec header to Size257.
        invalid.insert(invalid.end(), 700, 0xaa);
        std::vector<std::uint8_t> input = valid;
        input.insert(input.end(), invalid.begin(), invalid.end());
        TcpPacketFramer framer;
        auto complete = framer.Feed(input.data(), input.size());
        Check(complete.packet && complete.consumed == valid.size(), "complete prefix delivered before later bad frame");
        auto failure = framer.Feed(input.data() + complete.consumed, input.size() - complete.consumed);
        Check(!failure.packet && failure.consumed == TcpPacketFramer::HeaderSize
            && failure.state == StreamState::Failed && failure.error == FramingError::OversizePayload,
            "oversize is rejected after header before copying payload or entering Packet Read");
        Check(framer.PendingByteCount() == 17, "oversize retains only bounded header");
        Check(framer.EndOfInput().error == FramingError::OversizePayload
            && framer.FailTransport().error == FramingError::OversizePayload, "first failure is retained");
        auto stopped = framer.Feed(valid.data(), valid.size());
        Check(stopped.consumed == 0 && !stopped.packet && stopped.error == FramingError::OversizePayload,
            "failed stream does not silently consume more data");
        framer.Reset();
        auto fresh = framer.Feed(valid.data(), valid.size());
        Check(fresh.packet != nullptr, "new connection works after explicit reset");
        Packet(*fresh.packet, 1);
        Packet(*complete.packet, 1);

        for(const std::uint16_t size : {std::uint16_t(257), std::uint16_t(0x8000), std::uint16_t(0xffff)})
        {
            auto header = Wire(0);
            header[TcpPacketFramer::HeaderSize - 2] = static_cast<std::uint8_t>(size);
            header[TcpPacketFramer::HeaderSize - 1] = static_cast<std::uint8_t>(size >> 8);
            header.insert(header.end(), 700, 0xbb);
            TcpPacketFramer split;
            auto prefix = split.Feed(header.data(), 16);
            Check(prefix.consumed == 16 && !prefix.packet, "size is not guessed before complete header");
            Receiving(split, 16);
            auto rejected = split.Feed(header.data() + 16, header.size() - 16);
            Check(rejected.consumed == 1 && rejected.error == FramingError::OversizePayload
                && split.PendingByteCount() == 17, "full uint16 oversized lengths reject before payload copy");
        }
    }

    void EndAndFailureStates()
    {
        const auto frame = Wire(256);
        for(std::size_t cut = 1; cut < frame.size(); ++cut)
        {
            TcpPacketFramer framer;
            auto partial = framer.Feed(frame.data(), cut);
            Check(!partial.packet && partial.consumed == cut, "incomplete frame pending before EOF");
            const auto ended = framer.EndOfInput();
            Check(ended.state == StreamState::Failed && ended.error == FramingError::TruncatedFrame
                && framer.PendingByteCount() == cut, "EOF distinguishes every truncated header/payload boundary");
            auto late = framer.Feed(frame.data() + cut, frame.size() - cut);
            Check(late.consumed == 0 && !late.packet && late.error == FramingError::TruncatedFrame,
                "EOF is terminal until reset");
        }
        TcpPacketFramer empty;
        Check(empty.EndOfInput().state == StreamState::Ended && empty.GetError() == FramingError::None, "empty EOF is clean");
        Check(empty.FailTransport().state == StreamState::Ended, "first terminal EOF is retained");
        Check(empty.Feed(frame.data(), frame.size()).consumed == 0, "ended stream consumes no bytes");
        empty.Reset();
        Receiving(empty, 0);
        auto complete = empty.Feed(frame.data(), frame.size());
        Check(complete.packet != nullptr, "complete packet before transport failure");
        auto partial = empty.Feed(frame.data(), 23);
        Check(partial.consumed == 23 && !partial.packet, "partial next frame before transport failure");
        Check(empty.FailTransport().error == FramingError::TransportFailure && empty.PendingByteCount() == 23,
            "transport error preserves incomplete prefix for diagnostics");
        Packet(*complete.packet, 256);
        Check(empty.EndOfInput().error == FramingError::TransportFailure, "EOF cannot erase transport error");
        empty.Reset();
        auto zero = Wire(0, 0);
        auto newConnection = empty.Feed(zero.data(), zero.size());
        Check(newConnection.packet != nullptr, "reset discards old partial connection bytes");
        Packet(*newConnection.packet, 0, 0);
        Packet(*complete.packet, 256);
    }

    void InvalidCallerInput()
    {
        const auto frame = Wire(5);
        TcpPacketFramer framer;
        auto prefix = framer.Feed(frame.data(), 10);
        Check(prefix.consumed == 10 && !prefix.packet, "prefix before invalid input");
        bool threw = false;
        try { (void)framer.Feed(nullptr, 1); }
        catch(const std::invalid_argument&) { threw = true; }
        Check(threw, "null nonzero caller input is rejected");
        Receiving(framer, 10);
        auto tail = framer.Feed(frame.data() + 10, frame.size() - 10);
        Check(tail.packet != nullptr, "invalid caller input did not mutate pending bytes");
        Packet(*tail.packet, 5);
    }
}

int main()
{
    try
    {
        HeaderAndPayloadSplits();
        CoalescedFramesAndOwnership();
        OversizeAndCompletePrefix();
        EndAndFailureStates();
        InvalidCallerInput();
        std::cout << "PASS WinxCoop bounded TCP framing over shared Packet/MemoryStream codec\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
