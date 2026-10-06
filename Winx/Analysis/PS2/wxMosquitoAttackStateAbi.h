#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::ps2
{
    using wxMosquitoAttackStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxMosquitoAttackStateLayout)==0x3c);
    inline constexpr Address32 MosquitoAttackTable=0x004995e0;
    inline constexpr Address32 MosquitoAttackFactory=0x003f11a0;
    inline constexpr Address32 MosquitoAttackConstructor=0x002f51f0;
    inline constexpr Address32 MosquitoAttackEntry=0x002f4fb0;
    inline constexpr Address32 MosquitoAttackUpdate=0x002f4f50;
    inline constexpr Address32 MosquitoAttackPermission=0x002f5180;
    inline constexpr Address32 MosquitoAttackTag=0x002f5110;
}
