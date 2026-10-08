#pragma once
// Our borrowed IOP/kernel/FPU boundary. Native addresses are recorded call
// arguments and data identities, never host pointers or direct OS calls.
#include <array>
#include <cstdint>
#include <optional>
#include "../PS2/spPS2MouseExactIntegerMath.h"

namespace sparkplug::analysis::host
{
    struct spPS2MouseSemaphoreDescriptorForAnalysis final
    {
        // Only these caller bytes were written. The first word and any
        // additional descriptor storage stay unknown; no SDK field names.
        std::optional<std::uint32_t> word00;
        std::uint32_t word04 = 1, word08 = 1;
        std::optional<std::uint32_t> word0C, word10, word14;
    };
    struct spPS2MouseRpcRequestForAnalysis final
    {
        std::uint32_t client, function, mode, send, sendBytes, receive,
            receiveBytes, completionAddress, completionArgument;
    };
    class spPS2MouseBoundaryForAnalysis
    {
    public:
        virtual ~spPS2MouseBoundaryForAnalysis() = default;
        [[nodiscard]] virtual std::optional<std::uint8_t> ReadGpByte(std::int32_t offset) = 0;
        [[nodiscard]] virtual std::optional<std::uint32_t> ReadGpWord(std::int32_t offset) = 0;
        virtual void WriteGpWord(std::int32_t offset, std::uint32_t value) = 0;
        [[nodiscard]] virtual std::optional<std::uint8_t> ReadByte(std::uint32_t address) = 0;
        [[nodiscard]] virtual std::optional<std::uint32_t> ReadWord(std::uint32_t address) = 0;
        virtual void WriteWord(std::uint32_t address, std::uint32_t value) = 0;
        [[nodiscard]] virtual std::optional<std::int32_t> CreateSemaphore(
            const spPS2MouseSemaphoreDescriptorForAnalysis&) = 0;
        virtual void WaitSemaphore(std::uint32_t identifier) = 0;
        virtual void SignalSemaphore(std::uint32_t identifier) = 0;
        virtual void SignalSemaphoreFromInterrupt(std::uint32_t identifier) = 0;
        virtual void InitializeRpc(std::uint32_t mode) = 0;
        [[nodiscard]] virtual std::optional<std::int32_t> BindRpc(std::uint32_t client,
            std::uint32_t identifier, std::uint32_t mode) = 0;
        [[nodiscard]] virtual std::optional<std::int32_t> CallRpc(
            const spPS2MouseRpcRequestForAnalysis&) = 0;
        virtual void Report(std::uint32_t originalMessageAddress,
            std::optional<std::int32_t> originalArgument) = 0;
        // Shared default qualifies exact integral math and normal finite
        // comparisons/CVT.W.S. A backend can
        // supply independently established EE COP1 semantics for other inputs.
        // Ordinary host IEEE arithmetic is never a hidden full R5900 substitute.
        [[nodiscard]] virtual std::optional<std::uint32_t> AddSignedByte(
            std::uint32_t floatBits, std::int8_t offset)
        { return evidence::ps2::mouse_exact_integer_math::AddSignedByte(floatBits,offset); }
        [[nodiscard]] virtual std::optional<std::uint32_t> SubtractSingle(
            std::uint32_t firstBits, std::uint32_t secondBits)
        { return evidence::ps2::mouse_exact_integer_math::SubtractSingle(firstBits,secondBits); }
        [[nodiscard]] virtual std::optional<std::int32_t> ConvertSingleToWord(
            std::uint32_t floatBits)
        { return evidence::ps2::mouse_exact_integer_math::ConvertSingleToWord(floatBits); }
        [[nodiscard]] virtual std::optional<bool> CompareSingleLess(
            std::uint32_t firstBits, std::uint32_t secondBits)
        { return evidence::ps2::mouse_exact_integer_math::CompareLess(firstBits,secondBits); }
        [[nodiscard]] virtual std::optional<bool> CompareSingleLessEqual(
            std::uint32_t firstBits, std::uint32_t secondBits)
        { return evidence::ps2::mouse_exact_integer_math::CompareLessEqual(firstBits,secondBits); }
    };
}
