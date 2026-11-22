#pragma once
#include "GuestIsolationCatalog.h"

namespace winxcoop::adapter::guest_isolation
{
    // These are producer-region gates, not replacements for the stock input
    // device. The five action-region gates admit Skip only for the guest's
    // exact Playing state and device context 3. Menu and pause actions must
    // use the original path. The four ordinary producers have ESI=device;
    // the keyboard tail has EBP=device and ESI=command. The two cheat gates
    // are unconditional for the guest, including in menus.
    //
    // Each action-region prefix consists of complete instructions without a
    // relative CALL/JMP/Jcc. Its pass path replays that prefix then continues
    // at address+size, NOT at next: next is the withheld region's destination.
    // Cheat gates have no pass path; in particular, the relative Jcc in the
    // cheat-body prefix must never be copied as an unrelocated trampoline.
    // The skipped regions have no outstanding stack arguments at either end,
    // so their skip stackBytes is 0. On the pass path the original subsequent
    // query consumes the PUSH in each five-byte MOV/PUSH prefix.
    inline constexpr Site InputSites[] = {
        // UserInput has already restored ESI and its 0C-byte frame. A consumer
        // may normalize the command, then perform the original plain RET.
        // next names the following padding; jumping there is not RET replay.
        {0x4E2D3E, 0x4E2D3F, 0, 1,
            {0xC3}, Action::InputTail, "input-tail"},

        // The stock prologue has saved EBX/EBP/ESI/EDI. EBP is the device.
        // Skip the cheat prefix unconditionally, then execute the original
        // MOV ESI,[ESP+14] to obtain the command at 6044AA.
        {0x60437B, 0x6044AA, 0, 5,
            {0x8D, 0x50, 0x01, 0x8B, 0xFF}, Action::Skip, "input-cheat-prefix"},

        // JE 604519; MOV ECX,[EBP+38]. Complete instructions, but the JE is
        // relative. This gate is ALWAYS a skip; do not replay this prefix.
        // The original logical 6 Escape tail and four-register epilogue stay.
        {0x6044AE, 0x604AAA, 0, 5,
            {0x74, 0x69, 0x8B, 0x4D, 0x38}, Action::Skip, "input-cheat-body"},

        // MOV ECX,[EBP+38]; PUSH 0C. Escape was already produced at604AB8.
        // Withhold direct message27F2 before either query or temporary message
        // arguments. EBP is the device here; ESI is the command, not a device.
        // The destination is the four-POP/RET4 epilogue, after CALL40EC00.
        {0x604ABC, 0x604AF8, 0, 5,
            {0x8B, 0x4D, 0x38, 0x6A, 0x0C}, Action::Skip, "keyboard-tail-message-27f2"},

        // MOV ECX,[ESI+34]; PUSH 15. Withhold targeted message 2716 before
        // either logical 15 query or its temporary message arguments exist.
        {0x603F8A, 0x603FDE, 0, 5,
            {0x8B, 0x4E, 0x34, 0x6A, 0x15}, Action::Skip, "keyboard-targeted-action"},

        // MOV ECX,[ESI+34]; PUSH 11. Withhold focus/target cycling and 2802.
        // This also skips the original device+B6=0 store at 6040FB; the
        // consumer must not infer a reset of that private latch from a skip.
        {0x6040F6, 0x604322, 0, 5,
            {0x8B, 0x4E, 0x34, 0x6A, 0x11}, Action::Skip, "keyboard-focus-action"},

        // MOV AL,[ESI+B6]. Withhold pad focus/target cycling and 2802;
        // jump, crouch, ordinary command production and Escape remain outside
        // this span and are handled by the stock producer/input-tail policy.
        {0x604BF4, 0x604D0D, 0, 6,
            {0x8A, 0x86, 0xB6, 0x00, 0x00, 0x00}, Action::Skip, "pad-focus-action"},

        // MOV ECX,[ESI+3C]; PUSH 15. Logical 6 Escape was already produced at
        // 604D97..604DA5; withhold only the following targeted message 2716.
        {0x604DA8, 0x604DFC, 0, 5,
            {0x8B, 0x4E, 0x3C, 0x6A, 0x15}, Action::Skip, "pad-targeted-action"},
    };

    struct InputDestination final
    {
        std::uint32_t address, size;
        std::array<std::uint8_t, 16> bytes;
        const char* name;
    };

    // Validate these original/live destination prefixes before patching any
    // producer. They end at complete instructions and contain no relocated
    // relative control flow. They are pins, not additional patch sites.
    inline constexpr InputDestination InputDestinations[] = {
        {0x6044AA, 4, {0x8B, 0x74, 0x24, 0x14}, "input-after-cheat-prefix"},
        {0x604AAA, 5, {0x8B, 0x4D, 0x38, 0x6A, 0x06}, "input-after-cheat-body"},
        {0x604AF8, 7, {0x5F, 0x5E, 0x5D, 0x5B, 0xC2, 0x04, 0x00}, "input-after-keyboard-tail-message"},
        {0x603FDE, 5, {0x8B, 0x4E, 0x38, 0x6A, 0x07}, "keyboard-after-targeted-action"},
        {0x604322, 5, {0x8B, 0x4E, 0x38, 0x6A, 0x23}, "keyboard-after-focus-action"},
        {0x604D0D, 5, {0x8B, 0x4E, 0x3C, 0x6A, 0x23}, "pad-after-focus-action"},
        {0x604DFC, 6, {0x8A, 0x86, 0xB6, 0x00, 0x00, 0x00}, "pad-after-targeted-action"},
    };
}
