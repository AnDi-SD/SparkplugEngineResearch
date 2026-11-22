#pragma once
#include <array>
#include <cstdint>

namespace winxcoop::adapter::guest_isolation
{
    enum class Action { Skip, TypedTick, InputTail, FlowRequest, ReturnFalse };

    struct Site final
    {
        std::uint32_t address, next, stackBytes, size;
        std::array<std::uint8_t, 32> bytes;
        Action action;
        const char* name;
    };

    // Fixed complete instruction spans from the original PC image. Consumers
    // must verify these source and live bytes before installing a guest gate;
    // the catalogue does not admit a different span from the current process.
    // next is the original continuation even for an early-return descriptor.
    // stackBytes excludes the return address: RET4 is 4 and a plain RET is 0.
    // A ReturnFalse consumer returns to [ESP], advances ESP by 4+stackBytes,
    // and clears AL. No original prologue has executed at that point.
    inline constexpr Site Sites[] = {
        // Pre-push typed notification delivery. A withheld delivery has no
        // pending argument to remove; local movement/lifecycle admission is
        // the consumer's responsibility rather than a property of this site.
        {0x573D9D, 0x573DA3, 0, 6,
            {0x8B, 0x01, 0x52, 0xFF, 0x50, 0x04}, Action::TypedTick, "typed-tick"},

        // The writer's second instruction is a protected relative gateway.
        // It is a pin only; it must not be copied as an unrelocated trampoline.
        {0x4E55E0, 0x4E55E8, 4, 8,
            {0x83, 0xEC, 0x1C, 0xE9, 0x7E, 0x10, 0xF6, 0xFF}, Action::ReturnFalse, "save-writer"},
        // Capture precedes file writing and has its own state/notification
        // effects. The wrapper also sends message2760 after the writer call.
        {0x543B10, 0x543B18, 0, 8,
            {0x56, 0x8B, 0xF1, 0xE8, 0x98, 0xC8, 0x04, 0x00}, Action::ReturnFalse, "savepoint-capture"},
        {0x543BA0, 0x543BA5, 0, 5,
            {0xA1, 0xD4, 0x5A, 0x76, 0x00}, Action::ReturnFalse, "savepoint-wrapper"},
        // Both native damage frontends have one message argument. These are
        // early rejection points, before hurt/death/resource side effects.
        {0x4FE750, 0x4FE755, 4, 5,
            {0xA1, 0x98, 0x52, 0x75, 0x00}, Action::ReturnFalse, "player-hit-front"},
        {0x4FCF00, 0x4FCF09, 4, 9,
            {0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x24, 0x01, 0x00, 0x00}, Action::ReturnFalse, "base-damage"},
        // Conditional entry gate for messages27DB/27EA only. Other messages
        // must retain the original path. This is not level-entry setup552540.
        {0x4E4560, 0x4E4566, 4, 6,
            {0x8B, 0x54, 0x24, 0x04, 0x8B, 0x02}, Action::ReturnFalse, "profile-notify-27db-27ea-only"},

        // Cinematic start has no stack arguments and returns with plain RET.
        // Both direct callers (4EB391 and 4EBDB7) discard EAX. Reject before
        // its protected getter/prologue or any start-body side effects: a
        // rejected Flow74 request alone still reaches fullscreen fade593B00
        // and movie-selection59E650. Zero EAX is opaque, not a success result.
        // This is not the earlier request frontend4EB300 or every cinematic
        // producer; those callers may already have changed their own state.
        {0x4EA120, 0x4EA129, 0, 9,
            {0x56, 0x8B, 0xF1, 0xFF, 0x25, 0x98, 0x15, 0x3B, 0x01}, Action::ReturnFalse, "cinematic-start"},

        // Direct AppHelper service calls bypass typed entity notification.
        // They have no stack arguments; their unused EAX result is overwritten
        // by the immediately following original singleton load.
        {0x57642D, 0x576432, 0, 5,
            {0xE8, 0xBE, 0xF8, 0x02, 0x00}, Action::Skip, "ai-manager-update"},
        {0x576479, 0x576480, 0, 7,
            {0x8B, 0x10, 0x8B, 0xC8, 0xFF, 0x52, 0x24}, Action::Skip, "one-liner-timer"},
        {0x576493, 0x57649A, 0, 7,
            {0x8B, 0x10, 0x8B, 0xC8, 0xFF, 0x52, 0x1C}, Action::Skip, "cinematic-manager-update"},
        {0x5764AD, 0x5764B4, 0, 7,
            {0x8B, 0x10, 0x8B, 0xC8, 0xFF, 0x52, 0x1C}, Action::Skip, "side-quest-manager-update"},
        {0x57645F, 0x576466, 0, 7,
            {0x8B, 0x10, 0x8B, 0xC8, 0xFF, 0x52, 0x1C}, Action::Skip, "target-manager-update"},
        {0x5764C9, 0x5764CE, 0, 5,
            {0xE8, 0x52, 0xAD, 0xF7, 0xFF}, Action::Skip, "fairy-star-manager-update"},
    };
}
