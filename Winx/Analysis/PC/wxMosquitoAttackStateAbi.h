#pragma once
#include "wxCharacterStateAbi.h"
namespace winx::evidence::pc
{
    using wxMosquitoAttackStateLayout=wxCharacterStateLayout;
    static_assert(sizeof(wxMosquitoAttackStateLayout)==0x3c);
    inline constexpr Address32 MosquitoAttackTable=0x006f9a78;
    inline constexpr Address32 MosquitoAttackFactory=0x00404720;
    inline constexpr Address32 MosquitoAttackClone=0x0040a4c0;
    inline constexpr Address32 MosquitoAttackEntry=0x005210f0;
    inline constexpr Address32 MosquitoAttackUpdate=0x00521150;
    inline constexpr Address32 MosquitoAttackPermission=0x00523770;
    inline constexpr Address32 MosquitoAttackTag=0x005210b0;
}
