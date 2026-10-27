// Bounded x86 owner of one original-game child. This diagnostic loads our
// adapter on the child's original thread; it never attaches to an existing PID.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include "RuntimeContract.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace winxcoop::adapter;
namespace
{
    static_assert(sizeof(void*) == 4, "AdapterProbe is an x86-only owned debugger");
    static_assert(sizeof(FLOATING_SAVE_AREA) == 112, "Controlled seed uses the qualified x86 legacy layout");
    constexpr DWORD FullContext = 0x1003f, Wx86Breakpoint = 0x4000001f;
    constexpr DWORD WorldReturn = 0x45A802, WorldCaller = 0x41CDF8;
    constexpr SIZE_T PageBytes = 4096, PathOffset = 512, StartOffset = 1536, ResultLoad = 4088, ResultStart = 4092;
    constexpr ULONGLONG BootstrapLimit = 30000, WorldWitnessLimit = 5000, LogLimit = 16ull * 1024 * 1024;
    constexpr std::array<BYTE,16> WorldCallerSignature{{0x8b,0x4e,0x40,0x8b,0x01,0xff,0x50,0x28,0xa1,0x84,0xdb,0x75,0x00,0x85,0xc0,0x5e}};
    constexpr std::array<BYTE,32> GameHash{{0x3f,0x02,0x24,0x80,0xbf,0x55,0x04,0x5d,0xa4,0xbf,0x69,0x2e,0x4b,0xc8,0x86,0x2e,
        0xd3,0x8f,0xc0,0x24,0xe8,0xa9,0x64,0xa5,0x58,0xfb,0xdf,0xdf,0x64,0x6d,0xfc,0x4f}};

    struct Handle
    {
        HANDLE value{};
        Handle() = default;
        explicit Handle(HANDLE h) : value(h) {}
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        ~Handle() { if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        bool Valid() const noexcept { return value && value != INVALID_HANDLE_VALUE; }
    };
    [[noreturn]] void Error(const char* message)
    { throw std::runtime_error(std::string(message) + " (win32=" + std::to_string(GetLastError()) + ')'); }
    void Require(const bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
    std::string Boolean(const bool v) { return v ? "true" : "false"; }
    std::string Hex(const DWORD value)
    {
        const char* digits = "0123456789ABCDEF"; std::string text(10,'0'); text[0]='0'; text[1]='x';
        for(unsigned n=0;n<8;++n) text[9-n]=digits[(value>>(n*4))&15]; return text;
    }
    std::string HexDigest(const std::array<BYTE,32>& digest)
    {
        const char* digits="0123456789abcdef";std::string out;out.reserve(64);
        for(const BYTE value:digest){out+=digits[value>>4];out+=digits[value&15];}return out;
    }
    std::string Quote(const std::string& text)
    {
        std::string out="\"";
        for(const unsigned char c:text)
        {
            if(c=='"' || c=='\\') { out+='\\'; out+=static_cast<char>(c); }
            else if(c=='\n') out+="\\n"; else if(c=='\r') out+="\\r";
            else if(c<32) out+='?'; else out+=static_cast<char>(c);
        }
        return out+'"';
    }
    void WriteAll(HANDLE file, const std::string& text)
    {
        DWORD written{}; Require(text.size()<=std::numeric_limits<DWORD>::max(), "Bounded log write overflow");
        if(!WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) || written!=text.size()) Error("Diagnostic write failed");
    }
    void ReadAt(HANDLE file, const LONGLONG offset, void* output, const DWORD size)
    {
        LARGE_INTEGER at{}; at.QuadPart=offset; DWORD got{};
        if(!SetFilePointerEx(file,at,nullptr,FILE_BEGIN) || !ReadFile(file,output,size,&got,nullptr) || got!=size) Error("Pinned file read failed");
    }
    std::array<BYTE,32> Hash(HANDLE file)
    {
        BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
        struct CloseHash { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h; ~CloseHash(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);} } release{algorithm,hash};
        Require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider unavailable");
        DWORD objectBytes{},got{};
        Require(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectBytes),sizeof(objectBytes),&got,0)>=0 && objectBytes<=65536,"SHA256 object size invalid");
        std::vector<BYTE> object(objectBytes);
        Require(BCryptCreateHash(algorithm,&hash,object.data(),objectBytes,nullptr,0,0)>=0,"SHA256 creation failed");
        LARGE_INTEGER zero{}; if(!SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)) Error("Hash rewind failed");
        std::array<BYTE,65536> chunk{};
        for(;;)
        {
            DWORD amount{}; if(!ReadFile(file,chunk.data(),static_cast<DWORD>(chunk.size()),&amount,nullptr)) Error("Hash read failed");
            if(!amount) break;
            Require(BCryptHashData(hash,chunk.data(),amount,0)>=0,"SHA256 update failed");
        }
        std::array<BYTE,32> digest{};
        Require(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0,"SHA256 finish failed"); return digest;
    }
    IMAGE_NT_HEADERS32 FilePe(HANDLE file)
    {
        IMAGE_DOS_HEADER dos{}; ReadAt(file,0,&dos,sizeof(dos));
        Require(dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>=static_cast<LONG>(sizeof(dos)) && dos.e_lfanew<1024*1024,"Invalid DOS image");
        IMAGE_NT_HEADERS32 pe{}; ReadAt(file,dos.e_lfanew,&pe,sizeof(pe));
        Require(pe.Signature==IMAGE_NT_SIGNATURE && pe.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && pe.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC,"Expected x86 PE32 image");
        return pe;
    }
    void ReadRemote(HANDLE process, const DWORD address, void* output, const SIZE_T size)
    {
        SIZE_T got{};
        if(address<0x10000 || !ReadProcessMemory(process,reinterpret_cast<const void*>(address),output,size,&got) || got!=size) Error("Owned-process memory read failed");
    }
    void WriteRemote(HANDLE process, const DWORD address, const void* input, const SIZE_T size)
    {
        SIZE_T wrote{};
        if(!WriteProcessMemory(process,reinterpret_cast<void*>(address),input,size,&wrote) || wrote!=size) Error("Owned-process memory write failed");
    }
    DWORD Word(HANDLE process, const DWORD address) { DWORD value{}; ReadRemote(process,address,&value,sizeof(value)); return value; }
    IMAGE_NT_HEADERS32 RemotePe(HANDLE process, const DWORD base)
    {
        IMAGE_DOS_HEADER dos{}; ReadRemote(process,base,&dos,sizeof(dos));
        Require(dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>=static_cast<LONG>(sizeof(dos)) && dos.e_lfanew<1024*1024,"Invalid loaded DOS image");
        IMAGE_NT_HEADERS32 pe{}; ReadRemote(process,base+dos.e_lfanew,&pe,sizeof(pe));
        Require(pe.Signature==IMAGE_NT_SIGNATURE && pe.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && pe.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC,"Loaded image is not PE32"); return pe;
    }
    bool SameFile(HANDLE a, HANDLE b)
    {
        BY_HANDLE_FILE_INFORMATION x{},y{};
        return a && b && GetFileInformationByHandle(a,&x) && GetFileInformationByHandle(b,&y) &&
            x.dwVolumeSerialNumber==y.dwVolumeSerialNumber && x.nFileIndexHigh==y.nFileIndexHigh && x.nFileIndexLow==y.nFileIndexLow &&
            x.nFileSizeHigh==y.nFileSizeHigh && x.nFileSizeLow==y.nFileSizeLow;
    }
    MODULEENTRY32W Module(const DWORD ownedPid, const wchar_t* name)
    {
        HANDLE raw=INVALID_HANDLE_VALUE;
        for(unsigned n=0;n<3 && raw==INVALID_HANDLE_VALUE;++n)
        { raw=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,ownedPid); if(raw==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_BAD_LENGTH) break; }
        Handle modules(raw); if(!modules.Valid()) Error("Owned-process module snapshot failed");
        MODULEENTRY32W entry{}; entry.dwSize=sizeof(entry);
        if(Module32FirstW(modules.value,&entry))
            do { if(_wcsicmp(entry.szModule,name)==0) return entry; } while(Module32NextW(modules.value,&entry));
        throw std::runtime_error("Required x86 module absent from owned process");
    }
    DWORD RemoteLoadLibrary(HANDLE process, const DWORD ownedPid)
    {
        const auto local=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
        Require(local!=nullptr,"Local LoadLibraryW unavailable"); HMODULE owner{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(local),&owner)) Error("LoadLibraryW owner unavailable");
        wchar_t path[MAX_PATH]{}; const DWORD length=GetModuleFileNameW(owner,path,MAX_PATH);
        Require(length && length<MAX_PATH,"LoadLibraryW owner path invalid");
        const wchar_t* name=path; for(const wchar_t* p=path;*p;++p) if(*p==L'\\' || *p==L'/') name=p+1;
        const auto remote=Module(ownedPid,name); const DWORD localBase=reinterpret_cast<DWORD>(owner), remoteBase=reinterpret_cast<DWORD>(remote.modBaseAddr);
        const auto lpe=RemotePe(GetCurrentProcess(),localBase), rpe=RemotePe(process,remoteBase);
        Require(lpe.FileHeader.TimeDateStamp==rpe.FileHeader.TimeDateStamp && lpe.OptionalHeader.SizeOfImage==rpe.OptionalHeader.SizeOfImage,"System loader module identity differs");
        const DWORD rva=reinterpret_cast<DWORD>(local)-localBase;
        Require(rpe.OptionalHeader.SizeOfImage>=16 && rva<rpe.OptionalHeader.SizeOfImage-16,"LoadLibraryW RVA outside actual owner");
        std::array<BYTE,16> actual{}; ReadRemote(process,remoteBase+rva,actual.data(),actual.size());
        Require(std::memcmp(actual.data(),reinterpret_cast<const void*>(local),actual.size())==0,"Remote LoadLibraryW prologue differs from local owning image");
        return remoteBase+rva;
    }
    bool SameGprAndControl(const CONTEXT& a, const CONTEXT& b)
    {
        return a.Eax==b.Eax && a.Ebx==b.Ebx && a.Ecx==b.Ecx && a.Edx==b.Edx && a.Esi==b.Esi && a.Edi==b.Edi &&
            a.Ebp==b.Ebp && a.Esp==b.Esp && a.Eip==b.Eip;
    }
    bool SameSegments(const CONTEXT& a, const CONTEXT& b)
    { return a.SegCs==b.SegCs && a.SegSs==b.SegSs && a.SegDs==b.SegDs && a.SegEs==b.SegEs && a.SegFs==b.SegFs && a.SegGs==b.SegGs; }
    bool SameDebugRegisters(const CONTEXT& a, const CONTEXT& b)
    { return a.Dr0==b.Dr0 && a.Dr1==b.Dr1 && a.Dr2==b.Dr2 && a.Dr3==b.Dr3 && a.Dr6==b.Dr6 && a.Dr7==b.Dr7; }
    bool SameContext(const CONTEXT& a, const CONTEXT& b)
    {
        return SameGprAndControl(a,b) && a.EFlags==b.EFlags && SameSegments(a,b) && SameDebugRegisters(a,b) &&
            std::memcmp(&a.FloatSave,&b.FloatSave,sizeof(a.FloatSave))==0 &&
            std::memcmp(a.ExtendedRegisters,b.ExtendedRegisters,sizeof(a.ExtendedRegisters))==0;
    }
    constexpr std::array<BYTE,10> SeedSt0{{0,0,0,0,0,0,0,0xa0,0xff,0x3f}}; // 80-bit +1.25.
    DWORD SeedXmmWord(const unsigned reg, const unsigned lane)
    { return 0x3f800000u+(reg*4+lane)*0x00010000u; } // 32 distinct finite positive floats.
    WORD Fx16(const CONTEXT& context, const SIZE_T offset)
    { WORD value{};CopyMemory(&value,context.ExtendedRegisters+offset,sizeof(value));return value; }
    DWORD Fx32(const CONTEXT& context, const SIZE_T offset)
    { DWORD value{};CopyMemory(&value,context.ExtendedRegisters+offset,sizeof(value));return value; }
    void SetFx16(CONTEXT& context, const SIZE_T offset, const WORD value)
    { CopyMemory(context.ExtendedRegisters+offset,&value,sizeof(value)); }
    void SetFx32(CONTEXT& context, const SIZE_T offset, const DWORD value)
    { CopyMemory(context.ExtendedRegisters+offset,&value,sizeof(value)); }
    CONTEXT FloatingSeed(const CONTEXT& original)
    {
        CONTEXT seed=original;
        // Preserve every reserved byte. Set only the named FSAVE/FXSAVE state;
        // TOP=0 makes the legacy and FX register orders agree unambiguously.
        seed.FloatSave.ControlWord=(seed.FloatSave.ControlWord&0xffff0000u)|0x037fu;
        seed.FloatSave.StatusWord&=0xffff0000u;
        seed.FloatSave.TagWord=(seed.FloatSave.TagWord&0xffff0000u)|0xfffcu;
        seed.FloatSave.ErrorOffset=0;seed.FloatSave.ErrorSelector&=0xf8000000u;
        seed.FloatSave.DataOffset=0;seed.FloatSave.DataSelector&=0xffff0000u;
        ZeroMemory(seed.FloatSave.RegisterArea,sizeof(seed.FloatSave.RegisterArea));
        CopyMemory(seed.FloatSave.RegisterArea,SeedSt0.data(),SeedSt0.size());
        SetFx16(seed,0,0x037f);SetFx16(seed,2,0);seed.ExtendedRegisters[4]=1;
        SetFx16(seed,6,static_cast<WORD>(Fx16(seed,6)&0xf800u));
        SetFx32(seed,8,0);SetFx16(seed,12,0);SetFx32(seed,16,0);SetFx16(seed,20,0);
        SetFx32(seed,24,0x1f80); // All SSE exceptions masked, default rounding.
        for(unsigned reg=0;reg<8;++reg)
        {
            CopyMemory(seed.ExtendedRegisters+32+reg*16,seed.FloatSave.RegisterArea+reg*10,10);
            for(unsigned lane=0;lane<4;++lane)SetFx32(seed,160+reg*16+lane*4,SeedXmmWord(reg,lane));
        }
        return seed;
    }
    bool ValidFloatingSeed(const CONTEXT& context)
    {
        if((context.FloatSave.ControlWord&0xffffu)!=0x037f || (context.FloatSave.StatusWord&0xffffu)!=0 ||
           (context.FloatSave.TagWord&0xffffu)!=0xfffc || context.FloatSave.ErrorOffset!=0 ||
           (context.FloatSave.ErrorSelector&0x07ffffffu)!=0 || context.FloatSave.DataOffset!=0 ||
           (context.FloatSave.DataSelector&0xffffu)!=0 || Fx16(context,0)!=0x037f || Fx16(context,2)!=0 ||
           context.ExtendedRegisters[4]!=1 || (Fx16(context,6)&0x07ffu)!=0 || Fx32(context,8)!=0 ||
           Fx16(context,12)!=0 || Fx32(context,16)!=0 || Fx16(context,20)!=0 || Fx32(context,24)!=0x1f80)return false;
        const DWORD mxcsrMask=Fx32(context,28)?Fx32(context,28):0x0000ffbfu;
        if(Fx32(context,24)&~mxcsrMask)return false;
        for(unsigned reg=0;reg<8;++reg)
        {
            for(unsigned byte=0;byte<10;++byte)
                if(context.FloatSave.RegisterArea[reg*10+byte]!=(reg==0?SeedSt0[byte]:0))return false;
            if(std::memcmp(context.ExtendedRegisters+32+reg*16,context.FloatSave.RegisterArea+reg*10,10)!=0)return false;
            for(unsigned lane=0;lane<4;++lane)
            {
                const DWORD word=Fx32(context,160+reg*16+lane*4);
                if(word!=SeedXmmWord(reg,lane) || (word&0x7f800000u)==0x7f800000u)return false;
            }
        }
        return true;
    }
    struct CpuEncodingField { BYTE reservedMask; const char* name; };
    CpuEncodingField SeedEncodingField(const bool extended, const SIZE_T offset)
    {
        // These are encoding/OS-reserved fields, not executable FP state.
        // They may canonicalize only across the fresh CPU INT3 BEFORE VEH.
        // The eventual VEH witness still compares every FP/SSE byte exactly.
        if(!extended)
        {
            if(offset==2 || offset==3)return {0xff,"legacyControlUpperReserved16"};
            if(offset==6 || offset==7)return {0xff,"legacyStatusUpperReserved16"};
            if(offset==10 || offset==11)return {0xff,"legacyTagUpperReserved16"};
            if(offset==19)return {0xf8,"legacyFopUpperReserved5"};
            if(offset==26 || offset==27)return {0xff,"legacyDataSelectorUpperReserved16"};
            if(offset>=108 && offset<112)return {0xff,"legacySpare0"};
            return {0,"legacyArchitecturalState"};
        }
        if(offset==5)return {0xff,"fxReservedByte5"};
        if(offset==7)return {0xf8,"fxFopUpperReserved5"};
        if(offset==14 || offset==15 || offset==22 || offset==23)return {0xff,"fxSelectorPadding"};
        if(offset>=32 && offset<160 && (offset-32)%16>=10)return {0xff,"fxStSlotPadding"};
        if(offset>=288 && offset<464)return {0xff,"fxReservedTail"};
        if(offset>=464 && offset<512)return {0xff,"fxSoftwareTail"};
        if(offset>=28 && offset<32)return {0,"fxMxcsrMaskUnchanged"};
        return {0,"fxArchitecturalState"};
    }
    bool QualifiedCpuSeedEncoding(const CONTEXT& expected, const CONTEXT& actual)
    {
        const auto* a=reinterpret_cast<const BYTE*>(&expected.FloatSave);
        const auto* b=reinterpret_cast<const BYTE*>(&actual.FloatSave);
        for(SIZE_T i=0;i<sizeof(expected.FloatSave);++i)
            if((a[i]^b[i])&~SeedEncodingField(false,i).reservedMask)return false;
        for(SIZE_T i=0;i<sizeof(expected.ExtendedRegisters);++i)
            if((expected.ExtendedRegisters[i]^actual.ExtendedRegisters[i])&~SeedEncodingField(true,i).reservedMask)return false;
        return ValidFloatingSeed(actual);
    }
    std::string CpuSeedEncodingDifferences(const void* expected, const void* actual, const SIZE_T count, const bool extended)
    {
        const auto* a=static_cast<const BYTE*>(expected);const auto* b=static_cast<const BYTE*>(actual);
        std::string out="[";unsigned differences=0;
        for(SIZE_T i=0;i<count;++i)if(a[i]!=b[i])
        {
            const auto field=SeedEncodingField(extended,i);if(differences++)out+=',';
            out+="{\"offset\":"+std::to_string(i)+",\"field\":"+Quote(field.name)+",\"expected\":"+std::to_string(a[i])+",\"actual\":"+std::to_string(b[i])+
                ",\"allowedReservedBitMask\":"+std::to_string(field.reservedMask)+",\"qualified\":"+Boolean(((a[i]^b[i])&~field.reservedMask)==0)+"}";
        }
        return out+']';
    }
    std::string SeedFields(const CONTEXT& context)
    {
        const char* digits="0123456789abcdef";std::string st0;
        for(unsigned i=0;i<10;++i){const BYTE value=context.FloatSave.RegisterArea[i];st0+=digits[value>>4];st0+=digits[value&15];}
        std::string out="{\"legacyControlWord\":"+Quote(Hex(context.FloatSave.ControlWord))+",\"legacyStatusWord\":"+Quote(Hex(context.FloatSave.StatusWord))+
            ",\"legacyTagWord\":"+Quote(Hex(context.FloatSave.TagWord))+",\"fxControlWord\":"+Quote(Hex(Fx16(context,0)))+
            ",\"fxStatusWord\":"+Quote(Hex(Fx16(context,2)))+",\"fxTagByte\":"+std::to_string(context.ExtendedRegisters[4])+
            ",\"st0Bytes\":"+Quote(st0)+",\"mxcsr\":"+Quote(Hex(Fx32(context,24)))+",\"mxcsrMask\":"+Quote(Hex(Fx32(context,28)))+",\"xmmWords\":[";
        for(unsigned reg=0;reg<8;++reg)
        { if(reg)out+=',';out+='[';for(unsigned lane=0;lane<4;++lane){if(lane)out+=',';out+=Quote(Hex(Fx32(context,160+reg*16+lane*4)));}out+=']'; }
        return out+"]}";
    }
    std::string ContextWords(const CONTEXT& c)
    {
        const DWORD values[]={c.Eax,c.Ebx,c.Ecx,c.Edx,c.Esi,c.Edi,c.Ebp,c.Esp,c.Eip,c.EFlags,
            c.SegCs,c.SegSs,c.SegDs,c.SegEs,c.SegFs,c.SegGs,c.Dr0,c.Dr1,c.Dr2,c.Dr3,c.Dr6,c.Dr7};
        const char* names[]={"eax","ebx","ecx","edx","esi","edi","ebp","esp","eip","eflags",
            "cs","ss","ds","es","fs","gs","dr0","dr1","dr2","dr3","dr6","dr7"};
        std::string out="{";
        for(unsigned n=0;n<sizeof(values)/sizeof(values[0]);++n){if(n)out+=',';out+=Quote(names[n])+':'+Quote(Hex(values[n]));}
        return out+'}';
    }
    std::string ByteDifferences(const void* expected, const void* actual, const SIZE_T count)
    {
        const auto* a=static_cast<const BYTE*>(expected);const auto* b=static_cast<const BYTE*>(actual);
        std::string out="[";unsigned differences=0;
        for(SIZE_T i=0;i<count;++i)if(a[i]!=b[i])
        {if(differences++)out+=',';out+="{\"offset\":"+std::to_string(i)+",\"expected\":"+std::to_string(a[i])+",\"actual\":"+std::to_string(b[i])+"}";}
        return out+']';
    }
    void StackGuard(HANDLE process, const CONTEXT& context)
    {
        Require(context.Esp>=0x10100 && (context.Esp&3)==0 && !(context.EFlags&0x100),"Invalid original stack or pending trap flag");
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQueryEx(process,reinterpret_cast<const void*>(context.Esp-0x100),&memory,sizeof(memory))) Error("Original stack query failed");
        const std::uintptr_t start=reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        const std::uintptr_t end=start+memory.RegionSize;
        const DWORD p=memory.Protect&0xff;
        Require(memory.State==MEM_COMMIT && !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
            (p==PAGE_READWRITE || p==PAGE_WRITECOPY || p==PAGE_EXECUTE_READWRITE || p==PAGE_EXECUTE_WRITECOPY) &&
            start<=context.Esp-0x100 && end>=std::uint64_t(context.Esp)+4,"Original native stack lacks committed read/write call space");
        std::array<BYTE,0x104> bytes{}; ReadRemote(process,context.Esp-0x100,bytes.data(),bytes.size());
    }
    std::array<BYTE,18> Stub(const DWORD function, const DWORD argument, const DWORD result)
    {
        std::array<BYTE,18> code{{0x68,0,0,0,0,0xb8,0,0,0,0,0xff,0xd0,0xa3,0,0,0,0,0xcc}};
        CopyMemory(code.data()+1,&argument,4); CopyMemory(code.data()+6,&function,4); CopyMemory(code.data()+13,&result,4); return code;
    }
    struct Options { std::wstring game,adapter,report,media,mode=L"observe"; DWORD seconds=0; };
    DWORD Number(const std::wstring& text)
    {
        Require(!text.empty(),"Missing observation duration"); DWORD value=0;
        for(const wchar_t c:text)
        { Require(c>=L'0' && c<=L'9',"Duration must be decimal digits"); const DWORD digit=c-L'0'; Require(value<=(600-digit)/10,"Duration exceeds 600 seconds"); value=value*10+digit; }
        Require(value>=5 && value<=600,"Duration must be 5..600 seconds"); return value;
    }
    std::wstring Absolute(const std::wstring& text, const std::size_t limit)
    {
        Require(text.size()>3 && text.size()<limit && text[1]==L':' && (text[2]==L'\\' || text[2]==L'/'),"Use an absolute ASCII drive path");
        for(const wchar_t c:text) Require(c>=32 && c<=126 && c!=L'"',"Paths must be plain ASCII without quotes");
        wchar_t full[MAX_PATH]{}; const DWORD n=GetFullPathNameW(text.c_str(),MAX_PATH,full,nullptr);
        Require(n>0 && n<limit,"Absolute path exceeds bounded path contract"); return full;
    }
    Options Parse()
    {
        int count{}; wchar_t** raw=CommandLineToArgvW(GetCommandLineW(),&count); if(!raw) Error("Command line unavailable");
        struct Args { wchar_t** p; ~Args(){if(p)LocalFree(p);} } args{raw}; std::wstring sidecar;
        if(count==1)
        {
            wchar_t own[MAX_PATH]{}; const DWORD n=GetModuleFileNameW(nullptr,own,MAX_PATH); Require(n && n<MAX_PATH-5,"Probe path invalid");
            Handle file(CreateFileW((std::wstring(own)+L".args").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
            if(!file.Valid()) Error("Adjacent .exe.args unavailable"); LARGE_INTEGER size{};
            Require(GetFileSizeEx(file.value,&size) && size.QuadPart>0 && size.QuadPart<=4096,"Sidecar must contain 1..4096 ASCII bytes");
            std::vector<BYTE> bytes(static_cast<std::size_t>(size.QuadPart)); DWORD got{};
            if(!ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) || got!=bytes.size()) Error("Sidecar read failed");
            sidecar=L"AdapterProbe.exe ";
            for(const BYTE c:bytes) { Require((c>=32 && c<=126) || c=='\r' || c=='\n' || c=='\t',"Sidecar contains non-ASCII/control data"); sidecar+=wchar_t(c); }
            LocalFree(args.p); args.p=nullptr; raw=CommandLineToArgvW(sidecar.c_str(),&count); if(!raw) Error("Sidecar command line invalid"); args.p=raw;
        }
        Options options; bool game=false,adapter=false,report=false,seconds=false,mode=false,media=false;
        for(int i=1;i<count;++i)
        {
            const std::wstring key=raw[i]; Require(i+1<count,"Every option requires one value"); const std::wstring value=raw[++i];
            if(key==L"--game" && !game) { options.game=Absolute(value,MAX_PATH);game=true; }
            else if(key==L"--adapter" && !adapter) { options.adapter=Absolute(value,MAX_PATH);adapter=true; }
            else if(key==L"--report" && !report) { options.report=Absolute(value,MAX_PATH-16);report=true; }
            else if(key==L"--seconds" && !seconds) { options.seconds=Number(value);seconds=true; }
            else if(key==L"--mode" && !mode) { options.mode=value;mode=true; }
            else if(key==L"--media" && !media) { options.media=Absolute(value,MAX_PATH-40);media=true; }
            else throw std::runtime_error("Unknown or repeated option");
        }
        Require(game && adapter && report && seconds,"Expected --game --adapter --report --seconds");
        Require(options.mode==L"observe" || options.mode==L"passive-actor" || options.mode==L"passive-unload","Unknown adapter probe mode");
        Require((options.mode==L"observe")==!media,"Passive modes require --media; observer must not supply it");
        return options;
    }

    // Reused without changing the seed/replay/C3 comparison algorithm. The
    // initial observer admission is retained separately from actor intervals.
    struct WorldWitnessState
    {
        bool worldWitnessStarted=false,worldContextWitnessPassed=false,worldOriginalContextRestored=false;
        bool worldSeedApplied=false,worldSeedValidated=false,worldSeedActuallyChanged=false,worldControlledPatternPassed=false;
        bool worldSeedReplayRequested=false,worldCpuSeedCaptured=false,worldCpuSeedEncodingQualified=false,worldCpuSeedRawEncodingChanged=false;
        bool worldGprPreserved=false,worldSegmentsPreserved=false,worldDebugPreserved=false,worldFpuBytesPreserved=false,worldSseBytesPreserved=false,worldExtendedBytesPreserved=false;
        DWORD worldWitnessCanonicalizedBits=0,worldRestoreCanonicalizedBits=0,worldSeedCanonicalizedBits=0;
        DWORD worldReplaySetCanonicalizedBits=0,worldCpuSeedCanonicalizedBits=0;
        CONTEXT worldBefore{},worldOriginal{};
        ULONGLONG worldWitnessBegan{};
    };
    struct ActorWitnessSummary
    {
        std::uint32_t started{},passed{},nativeCalls{},lastSequence{},lastAckWorldUpdates{};
    };
    struct Probe : WorldWitnessState
    {
        Options options;
        Handle gameFile,adapterFile,report,events,mapping,heldProcess,heldThread;
        RuntimeShared* shared{};
        HMODULE localAdapter{};
        PROCESS_INFORMATION pi{};
        bool processAutoClosed=false,threadAutoClosed=false,exitContinued=false,exitKnown=false,forced=false,timedOut=false;
        bool loadComplete=false,startComplete=false,contextRestored=false,pageFreed=false,cleanupConfirmed=false;
        bool callerSignatureCaptured=false,callerBreakpointArmed=false,callerBreakpointRestored=false,callerProtectionChanged=false;
        DWORD exitCode=STILL_ACTIVE,eventCount=0,fatal=0,readySamples=0,samples=0,remoteAdapter=0,exportRva=0;
        DWORD privatePage=0,trapLoad=0,trapStart=0,adapterSeen=0,originalReturn=0;
        DWORD initialContextCanonicalizedBits=0;
        DWORD callerOriginalProtection=0;
        unsigned bootstrap=0;
        CONTEXT saved{};
        std::array<BYTE,16> callerOriginalBytes{};
        ULONGLONG began=0,bootstrapBegan=0,lastSample=0,logBytes=0;
        RuntimeSnapshot latest{};
        RuntimeCommand pendingCommand{};
        std::uint32_t commandsSent{},commandsAcknowledged{},scriptStep{};
        ULONGLONG commandBegan{},lastCommandCompleted{};
        bool actorLifecycleCompleted{},mediaQualifiedObserved{};
        WorldWitnessState observerWitness{};
        bool observerWitnessSaved{},actorWitnessActive{},actorWitnessContextCompleted{},pendingCommandPublished{};
        RuntimeSnapshot actorWitnessBefore{};
        RuntimeAck actorWitnessAck{};
        ActorWitnessSummary actorWitnesses[3]{};
        std::uint32_t actorWitnessIntervalsStarted{},actorWitnessIntervalsPassed{};
        std::string failure;
        IMAGE_NT_HEADERS32 gamePe{},adapterPe{};
        explicit Probe(Options o):options(std::move(o)){}
        ~Probe()
        {
            if(shared) UnmapViewOfFile(shared);
            if(localAdapter) FreeLibrary(localAdapter);
            if(pi.hThread && !threadAutoClosed) CloseHandle(pi.hThread);
            if(pi.hProcess && !processAutoClosed) CloseHandle(pi.hProcess);
        }
        void Log(const std::string& value)
        {
            Require(logBytes+value.size()+1<=LogLimit,"Event log reached its 16 MiB limit");
            WriteAll(events.value,value+'\n'); logBytes+=value.size()+1;
        }
        void Setup()
        {
            report.value=CreateFileW(options.report.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!report.Valid()) Error("Fresh report unavailable");
            events.value=CreateFileW((options.report+L".events.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!events.Valid()) Error("Fresh event log unavailable");
            gameFile.value=CreateFileW(options.game.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            adapterFile.value=CreateFileW(options.adapter.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(!gameFile.Valid() || !adapterFile.Valid()) Error("Pinned game/adapter file unavailable");
            Require(Hash(gameFile.value)==GameHash,"Original game SHA256 differs from qualified image");
            const auto adapterHash=Hash(adapterFile.value);gamePe=FilePe(gameFile.value); adapterPe=FilePe(adapterFile.value);
            Require(gamePe.OptionalHeader.ImageBase==0x400000,"Original preferred image base mismatch");
            localAdapter=LoadLibraryExW(options.adapter.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
            if(!localAdapter) Error("Own adapter export inspection failed");
            const auto function=GetProcAddress(localAdapter,"WinxCoopStartAdapter"); Require(function!=nullptr,"Own adapter export unavailable");
            exportRva=reinterpret_cast<DWORD>(function)-reinterpret_cast<DWORD>(localAdapter);
            Require(exportRva<adapterPe.OptionalHeader.SizeOfImage,"Own adapter export outside image");
            std::uint64_t nonce{}; Require(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0 && nonce,"IPC nonce unavailable");
            const std::wstring name=L"Local\\WinxCoop.AdapterProbe."+std::to_wstring(GetCurrentProcessId())+L'.'+std::to_wstring(nonce);
            Require(name.size()<127,"IPC name exceeds start contract");
            mapping.value=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(RuntimeShared),name.c_str());
            if(!mapping.Valid()) Error("IPC mapping unavailable"); Require(GetLastError()!=ERROR_ALREADY_EXISTS,"IPC name collision");
            shared=static_cast<RuntimeShared*>(MapViewOfFile(mapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(RuntimeShared)));
            if(!shared) Error("IPC mapping view unavailable"); ZeroMemory(shared,sizeof(*shared));
            shared->magic=IpcMagic;shared->version=IpcVersion;shared->bytes=sizeof(*shared);shared->launcherPid=GetCurrentProcessId();shared->nonce=nonce;
            RuntimeStart start{};start.bytes=sizeof(start);start.version=IpcVersion;start.nonce=nonce;
            if(options.mode!=L"observe")
            {
                start.mode=RuntimeMode::PassiveActor;start.session=nonce;start.levelEpoch=1;
                Require(options.media.size()<sizeof(start.mediaDirectory),"Media path exceeds IPC contract");
                for(std::size_t i=0;i<options.media.size();++i)start.mediaDirectory[i]=static_cast<char>(options.media[i]);
            }
            CopyMemory(start.mappingName,name.c_str(),(name.size()+1)*sizeof(wchar_t)); pendingStart=start;
            Log("{\"event\":\"identity-pinned\",\"originalGameSha256Matched\":true,\"adapterSha256\":"+Quote(HexDigest(adapterHash))+",\"imageMachine\":332,\"ipcVersion\":"+std::to_string(IpcVersion)+",\"runtimeSharedBytes\":"+std::to_string(sizeof(RuntimeShared))+
                ",\"runtimeCommandBytes\":"+std::to_string(sizeof(RuntimeCommand))+",\"runtimeAckBytes\":"+std::to_string(sizeof(RuntimeAck))+",\"mode\":"+Quote(options.mode==L"observe"?"observe":options.mode==L"passive-actor"?"passive-actor":"passive-unload")+"}");
        }
        RuntimeStart pendingStart{};
        void Launch()
        {
            STARTUPINFOW startup{};startup.cb=sizeof(startup);std::wstring command=L'"'+options.game+L'"';
            const auto slash=options.game.find_last_of(L"\\/"); const auto directory=options.game.substr(0,slash);
            if(!CreateProcessW(options.game.c_str(),command.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS|CREATE_NEW_PROCESS_GROUP|CREATE_DEFAULT_ERROR_MODE,nullptr,directory.c_str(),&startup,&pi)) Error("Owned game launch failed");
            if(!DuplicateHandle(GetCurrentProcess(),pi.hProcess,GetCurrentProcess(),&heldProcess.value,0,FALSE,DUPLICATE_SAME_ACCESS) ||
               !DuplicateHandle(GetCurrentProcess(),pi.hThread,GetCurrentProcess(),&heldThread.value,0,FALSE,DUPLICATE_SAME_ACCESS)) Error("Own independent debug handles unavailable");
            if(!DebugSetProcessKillOnExit(TRUE)) Error("Own debugger kill-on-exit policy unavailable");
            pendingStart.gamePid=pi.dwProcessId;pendingStart.mainThreadId=pi.dwThreadId;
            began=GetTickCount64();
            Log("{\"event\":\"owned-game-start\",\"pid\":"+std::to_string(pi.dwProcessId)+",\"mainThread\":"+std::to_string(pi.dwThreadId)+",\"debugOnlyThisProcess\":true}");
        }
        CONTEXT Context()
        { CONTEXT c{};c.ContextFlags=FullContext;if(!GetThreadContext(heldThread.value,&c))Error("Full original x86 context unavailable");Require((c.ContextFlags&FullContext)==FullContext,"Incomplete x86 context");return c; }
        void Redirect(const DWORD address)
        { CONTEXT c=saved;c.ContextFlags=FullContext;c.Eip=address;if(!SetThreadContext(heldThread.value,&c))Error("Owned main-thread redirection failed"); }
        void BeginBootstrap(const DEBUG_EVENT& event)
        {
            saved=Context();const DWORD at=reinterpret_cast<DWORD>(event.u.Exception.ExceptionRecord.ExceptionAddress);
            const auto ntdll=Module(pi.dwProcessId,L"ntdll.dll");const DWORD ntdllBase=reinterpret_cast<DWORD>(ntdll.modBaseAddr);
            const auto pe=RemotePe(heldProcess.value,ntdllBase);BYTE instruction{};ReadRemote(heldProcess.value,at,&instruction,1);
            Require(at>=ntdllBase && std::uint64_t(at)<std::uint64_t(ntdllBase)+pe.OptionalHeader.SizeOfImage && saved.Eip==at+1 && instruction==0xcc,"Initial breakpoint is not the qualified x86 NTDLL loader stop");
            // Establish the OS's no-execution Set/Get representation BEFORE
            // either DLL call. Bit1 of architectural EFLAGS is reserved/fixed;
            // some WOW64 contexts omit it after SetThreadContext. All other
            // fields, including every byte of legacy FPU/SSE, must round-trip.
            const CONTEXT original=saved;
            if(!SetThreadContext(heldThread.value,&original))Error("No-execution context preflight failed");
            const CONTEXT canonical=Context();
            CONTEXT flagsAdjusted=original;flagsAdjusted.EFlags=canonical.EFlags;
            initialContextCanonicalizedBits=original.EFlags^canonical.EFlags;
            Require((initialContextCanonicalizedBits&~2u)==0 && SameContext(flagsAdjusted,canonical),"No-execution context preflight changed unexpected architectural state");
            saved=canonical;
            Log("{\"event\":\"no-execution-context-preflight\",\"rawEflags\":"+Quote(Hex(original.EFlags))+
                ",\"canonicalEflags\":"+Quote(Hex(canonical.EFlags))+",\"canonicalizedReservedBits\":"+std::to_string(initialContextCanonicalizedBits)+
                ",\"gprSegmentsDebugFpuSseExact\":true}");
            StackGuard(heldProcess.value,saved);originalReturn=Word(heldProcess.value,saved.Esp);
            const DWORD loader=RemoteLoadLibrary(heldProcess.value,pi.dwProcessId);
            privatePage=reinterpret_cast<DWORD>(VirtualAllocEx(heldProcess.value,nullptr,PageBytes,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
            if(!privatePage) Error("One private bootstrap page unavailable");
            std::array<BYTE,PageBytes> page{};const auto code=Stub(loader,privatePage+PathOffset,privatePage+ResultLoad);
            CopyMemory(page.data(),code.data(),code.size());CopyMemory(page.data()+PathOffset,options.adapter.c_str(),(options.adapter.size()+1)*sizeof(wchar_t));
            CopyMemory(page.data()+StartOffset,&pendingStart,sizeof(pendingStart));
            WriteRemote(heldProcess.value,privatePage,page.data(),page.size());
            if(!FlushInstructionCache(heldProcess.value,reinterpret_cast<const void*>(privatePage),PageBytes))Error("Bootstrap instruction-cache flush failed");
            std::array<BYTE,PageBytes> readback{};ReadRemote(heldProcess.value,privatePage,readback.data(),readback.size());Require(readback==page,"Bootstrap page readback differs");
            trapLoad=privatePage+static_cast<DWORD>(code.size())-1;bootstrapBegan=GetTickCount64();bootstrap=1;Redirect(privatePage);
            Log("{\"event\":\"bootstrap-load-begin\",\"contextFlags\":65599,\"loaderStop\":"+Quote(Hex(at))+",\"originalEsp\":"+Quote(Hex(saved.Esp))+",\"originalReturn\":"+Quote(Hex(originalReturn))+",\"loadLibrary\":"+Quote(Hex(loader))+"}");
        }
        void CompleteLoad()
        {
            const auto c=Context();Require(c.Eip==trapLoad+1 && c.Esp==saved.Esp,"LoadLibrary completion stack mismatch");
            Require(Word(heldProcess.value,saved.Esp)==originalReturn,"Original caller return changed during LoadLibrary");
            remoteAdapter=Word(heldProcess.value,privatePage+ResultLoad);Require(remoteAdapter && remoteAdapter==adapterSeen,"LoadLibrary did not return the observed pinned adapter image");
            const auto actual=RemotePe(heldProcess.value,remoteAdapter);
            Require(actual.FileHeader.TimeDateStamp==adapterPe.FileHeader.TimeDateStamp && actual.OptionalHeader.SizeOfImage==adapterPe.OptionalHeader.SizeOfImage,"Loaded adapter image differs from pinned file");
            loadComplete=true;const auto code=Stub(remoteAdapter+exportRva,privatePage+StartOffset,privatePage+ResultStart);
            WriteRemote(heldProcess.value,privatePage+64,code.data(),code.size());
            std::array<BYTE,18> readback{};ReadRemote(heldProcess.value,privatePage+64,readback.data(),readback.size());Require(readback==code,"Adapter-init stub readback differs");
            if(!FlushInstructionCache(heldProcess.value,reinterpret_cast<const void*>(privatePage+64),code.size()))Error("Adapter-init instruction-cache flush failed");
            trapStart=privatePage+64+static_cast<DWORD>(code.size())-1;bootstrap=2;Redirect(privatePage+64);
            Log("{\"event\":\"bootstrap-load-complete\",\"adapterBase\":"+Quote(Hex(remoteAdapter))+",\"exportRva\":"+Quote(Hex(exportRva))+"}");
        }
        void CompleteStart()
        {
            const auto c=Context();Require(c.Eip==trapStart+1 && c.Esp==saved.Esp,"Adapter-init completion stack mismatch");
            Require(Word(heldProcess.value,privatePage+ResultStart)==0,"Adapter-init returned a failure code");
            Require(Word(heldProcess.value,saved.Esp)==originalReturn,"Original caller return changed during adapter init");
            saved.ContextFlags=FullContext;if(!SetThreadContext(heldThread.value,&saved))Error("Full original context restoration failed");
            const auto restored=Context();
            if(!SameContext(saved,restored))
                Log("{\"event\":\"context-restore-mismatch\",\"expected\":"+ContextWords(saved)+",\"actual\":"+ContextWords(restored)+
                    ",\"floatSaveByteDifferences\":"+ByteDifferences(&saved.FloatSave,&restored.FloatSave,sizeof(saved.FloatSave))+
                    ",\"extendedByteDifferences\":"+ByteDifferences(saved.ExtendedRegisters,restored.ExtendedRegisters,sizeof(saved.ExtendedRegisters))+"}");
            Require(SameContext(saved,restored),"Original GPR/segments/debug/FPU/SSE context readback differs");contextRestored=true;
            Require(restored.Eip<privatePage || std::uint64_t(restored.Eip)>=std::uint64_t(privatePage)+PageBytes,"Cannot free an executing bootstrap page");
            if(!VirtualFreeEx(heldProcess.value,reinterpret_cast<void*>(privatePage),0,MEM_RELEASE))Error("Retired bootstrap page release failed");
            privatePage=0;pageFreed=startComplete=true;bootstrap=3;
            Log("{\"event\":\"bootstrap-complete\",\"fullContextRestored\":true,\"originalStackReturnUnchanged\":true,\"privatePageFreedAfterEipOutside\":true}");
        }
        void CaptureCallerSignature()
        {
            BYTE originalReturnInstruction{};ReadRemote(heldProcess.value,WorldReturn,&originalReturnInstruction,1);
            Require(originalReturnInstruction==0xC3,"Original world instruction is not the qualified C3");
            ReadRemote(heldProcess.value,WorldCaller,callerOriginalBytes.data(),callerOriginalBytes.size());
            Require(callerOriginalBytes==WorldCallerSignature && callerOriginalBytes[0]!=0xCC,"Original world caller signature differs");
            callerSignatureCaptured=true;
            Log("{\"event\":\"world-caller-signature-captured\",\"atCreateProcessBeforeAdapter\":true,\"caller\":"+Quote(Hex(WorldCaller))+
                ",\"firstByte\":"+std::to_string(callerOriginalBytes[0])+",\"signatureBytes\":16}");
        }
        void WriteCallerByte(const BYTE value)
        {
            Require(callerSignatureCaptured && !callerProtectionChanged,"World caller protection/signature state is incomplete");
            BYTE before{};ReadRemote(heldProcess.value,WorldCaller,&before,1);
            Require(before==callerOriginalBytes[0] || before==0xCC,"World caller instruction changed outside the owned witness");
            if(!VirtualProtectEx(heldProcess.value,reinterpret_cast<void*>(WorldCaller),1,PAGE_EXECUTE_READWRITE,&callerOriginalProtection)) Error("World caller protection change failed");
            callerProtectionChanged=true;
            try
            {
                WriteRemote(heldProcess.value,WorldCaller,&value,1);
                if(!FlushInstructionCache(heldProcess.value,reinterpret_cast<const void*>(WorldCaller),1)) Error("World caller instruction-cache flush failed");
                BYTE actual{};ReadRemote(heldProcess.value,WorldCaller,&actual,1);Require(actual==value,"World caller instruction readback differs");
            }
            catch(...)
            {
                DWORD ignored{};
                if(VirtualProtectEx(heldProcess.value,reinterpret_cast<void*>(WorldCaller),1,callerOriginalProtection,&ignored)) callerProtectionChanged=false;
                throw;
            }
            DWORD ignored{};
            if(!VirtualProtectEx(heldProcess.value,reinterpret_cast<void*>(WorldCaller),1,callerOriginalProtection,&ignored)) Error("World caller protection restoration failed");
            callerProtectionChanged=false;
        }
        void RestoreCallerBreakpointBestEffort(const bool stopped) noexcept
        {
            if(!callerBreakpointArmed || !heldProcess.Valid() || (!stopped && !forced)) return;
            try
            {
                // This is called only at an owned debug stop or after our
                // successful TerminateProcess. Never patch a running game.
                if(callerProtectionChanged)
                {
                    DWORD ignored{};
                    if(!VirtualProtectEx(heldProcess.value,reinterpret_cast<void*>(WorldCaller),1,callerOriginalProtection,&ignored)) return;
                    callerProtectionChanged=false;
                }
                std::array<BYTE,16> actual{};ReadRemote(heldProcess.value,WorldCaller,actual.data(),actual.size());
                const BYTE first=actual[0];actual[0]=callerOriginalBytes[0];
                if(actual!=callerOriginalBytes || (first!=0xCC && first!=callerOriginalBytes[0])) return;
                if(first==0xCC) WriteCallerByte(callerOriginalBytes[0]);
                ReadRemote(heldProcess.value,WorldCaller,actual.data(),actual.size());
                if(actual==callerOriginalBytes) { callerBreakpointArmed=false;callerBreakpointRestored=true; }
            }
            catch(...) { /* The owned terminating process remains the cleanup boundary. */ }
        }
        bool ObserverAdmitted() const noexcept
        {
            const WorldWitnessState& value=observerWitnessSaved?observerWitness:static_cast<const WorldWitnessState&>(*this);
            return value.worldContextWitnessPassed && value.worldOriginalContextRestored && value.worldControlledPatternPassed;
        }
        unsigned ActorOperationIndex() const
        {
            const auto operation=static_cast<unsigned>(pendingCommand.operation);
            Require(operation>=1 && operation<=3,"Actor context witness has an invalid pending operation");return operation-1;
        }
        bool MatchingAck(const RuntimeAck& ack) const noexcept
        {
            return ack.bytes==sizeof(ack) && ack.version==IpcVersion && ack.session==pendingCommand.session &&
                ack.levelEpoch==pendingCommand.levelEpoch && ack.sequence==pendingCommand.sequence && ack.entity==pendingCommand.entity &&
                ack.operation==pendingCommand.operation && ack.status==RuntimeAckStatus::Applied && ack.error==RuntimeError::None && !ack.actorError;
        }
        bool ActorFpQualified() const noexcept
        {
            if(options.mode==L"observe" || !failure.empty() || actorWitnessActive || pendingCommand.sequence ||
                actorWitnessIntervalsStarted!=actorWitnessIntervalsPassed || actorWitnessIntervalsPassed!=commandsAcknowledged)return false;
            for(const auto& value:actorWitnesses)if(!value.passed || value.started!=value.passed || !value.nativeCalls)return false;
            return true;
        }
        void BeginActorWitness()
        {
            Require(options.mode!=L"observe" && ObserverAdmitted() && observerWitnessSaved && pendingCommand.sequence &&
                !pendingCommandPublished && !actorWitnessActive && !actorWitnessContextCompleted && !callerBreakpointArmed &&
                actorWitnessIntervalsStarted==actorWitnessIntervalsPassed && actorWitnessIntervalsStarted<6,"Actor witness cannot start or retry");
            RuntimeSnapshot before{};RuntimeAck ack{};RuntimeCommand previous{};
            Require(TryRead(shared->snapshot,before) && before.phase==RuntimePhase::Ready && before.levelId==28 && before.mediaQualified &&
                !before.epochRetired && !before.actorError && before.gamePid==pi.dwProcessId && before.mainThreadId==pi.dwThreadId &&
                before.session==pendingCommand.session && before.levelEpoch==pendingCommand.levelEpoch && before.lastCommandSequence==commandsAcknowledged,
                "Actor witness source/level/previous native operation is not ready");
            Require(TryRead(shared->acknowledgement,ack) && ack.sequence==commandsAcknowledged &&
                TryRead(shared->command,previous) && previous.sequence==commandsAcknowledged,"Actor witness previous command/ACK changed before publication");
            actorWitnessBefore=before;actorWitnessAck={};actorWitnessActive=true;
            ++actorWitnessIntervalsStarted;++actorWitnesses[ActorOperationIndex()].started;
            static_cast<WorldWitnessState&>(*this)=WorldWitnessState{};
            Log("{\"event\":\"actor-native-call-fp-witness-start\",\"sequence\":"+std::to_string(pendingCommand.sequence)+
                ",\"operation\":"+std::to_string(static_cast<unsigned>(pendingCommand.operation))+",\"nativeCallsBefore\":"+std::to_string(before.nativeCalls)+
                ",\"commandNotPublished\":true,\"seedOccupiedX87Registers\":1,\"nativeEmptyX87Requirement\":\"unqualified\",\"originalCallerInstructionDeferred\":true}");
            BeginWorldWitness();
        }
        void PublishWitnessCommand()
        {
            Require(actorWitnessActive && worldCpuSeedCaptured && worldCpuSeedEncodingQualified && callerBreakpointArmed &&
                pendingCommand.sequence && !pendingCommandPublished && !actorWitnessContextCompleted,"Actor command publication requires the fresh seeded CPU stop");
            RuntimeAck ack{};RuntimeCommand previous{};
            Require(TryRead(shared->acknowledgement,ack) && ack.sequence==commandsAcknowledged &&
                TryRead(shared->command,previous) && previous.sequence==commandsAcknowledged,"Actor command/ACK changed during controlled replay");
            Publish(shared->command,pendingCommand);pendingCommandPublished=true;
            Log("{\"event\":\"runtime-command-sent\",\"sequence\":"+std::to_string(pendingCommand.sequence)+
                ",\"entity\":"+std::to_string(pendingCommand.entity)+",\"operation\":"+std::to_string(static_cast<unsigned>(pendingCommand.operation))+
                ",\"animation\":"+std::to_string(static_cast<unsigned>(pendingCommand.animation))+
                ",\"singleOutstanding\":true,\"diagnosticProducer\":true,\"atFreshCpuSeedWorldStop\":true,\"callerBreakpointArmed\":true}");
        }
        void CompleteActorWitnessAtCaller()
        {
            Require(actorWitnessActive && pendingCommandPublished && pendingCommand.sequence && !actorWitnessContextCompleted &&
                worldContextWitnessPassed && worldControlledPatternPassed && worldOriginalContextRestored && worldGprPreserved &&
                worldSegmentsPreserved && worldDebugPreserved && worldFpuBytesPreserved && worldSseBytesPreserved && worldExtendedBytesPreserved &&
                !callerBreakpointArmed && callerBreakpointRestored,"Actor caller context witness did not complete all exact gates");
            Require(TryRead(shared->acknowledgement,actorWitnessAck) && MatchingAck(actorWitnessAck) &&
                actorWitnessAck.worldUpdates>actorWitnessBefore.worldUpdates,"Native command was not acknowledged inside its seeded world-to-caller interval");
            actorWitnessContextCompleted=true;actorWitnessActive=false;
            Log("{\"event\":\"actor-native-call-fp-witness-caller\",\"sequence\":"+std::to_string(pendingCommand.sequence)+
                ",\"operation\":"+std::to_string(static_cast<unsigned>(pendingCommand.operation))+",\"ackWorldUpdates\":"+std::to_string(actorWitnessAck.worldUpdates)+
                ",\"gprSegmentsDebugExact\":true,\"floatSaveBytesExact\":112,\"extendedBytesExact\":512,\"fpMasksUsed\":false,\"controlledCpuSeedValidated\":true,\"originalPreSeedContextRestored\":true,\"callerInstructionRestoredBeforeExecution\":true}");
            static_cast<WorldWitnessState&>(*this)=observerWitness;
        }
        void FinishActorWitnessAfterSnapshot()
        {
            Require(actorWitnessContextCompleted && !actorWitnessActive && pendingCommandPublished && MatchingAck(actorWitnessAck) &&
                latest.lastCommandSequence==pendingCommand.sequence && latest.worldUpdates>=actorWitnessAck.worldUpdates &&
                latest.nativeCalls>actorWitnessBefore.nativeCalls && !latest.actorError && latest.mediaQualified && !latest.epochRetired,
                "Seeded native interval has no matching fresh native-call snapshot");
            const auto operation=pendingCommand.operation;
            Require(latest.remoteCreated==actorWitnessBefore.remoteCreated+(operation==RuntimeOperation::Create?1u:0u) &&
                latest.remoteRemoved==actorWitnessBefore.remoteRemoved+(operation==RuntimeOperation::Remove?1u:0u) &&
                latest.remoteEntity==(operation==RuntimeOperation::Remove?0u:pendingCommand.entity),"Seeded native operation lifecycle postconditions differ");
            auto& result=actorWitnesses[ActorOperationIndex()];++result.passed;++actorWitnessIntervalsPassed;
            const auto delta=latest.nativeCalls-actorWitnessBefore.nativeCalls;result.nativeCalls+=delta;
            result.lastSequence=pendingCommand.sequence;result.lastAckWorldUpdates=actorWitnessAck.worldUpdates;
            Log("{\"event\":\"actor-native-call-fp-witness-qualified\",\"sequence\":"+std::to_string(pendingCommand.sequence)+
                ",\"operation\":"+std::to_string(static_cast<unsigned>(operation))+",\"nativeCallsInInterval\":"+std::to_string(delta)+
                ",\"matchingFreshSnapshotAndAck\":true,\"controlledFullContextAndOriginalRestore\":true}");
            actorWitnessContextCompleted=false;pendingCommandPublished=false;
        }
        void SeedWorldContext()
        {
            const CONTEXT seed=FloatingSeed(worldOriginal);
            Require(ValidFloatingSeed(seed),"Controlled floating-point seed is not legacy/FX consistent");
            worldSeedActuallyChanged=std::memcmp(&worldOriginal.FloatSave,&seed.FloatSave,sizeof(seed.FloatSave))!=0 ||
                std::memcmp(worldOriginal.ExtendedRegisters,seed.ExtendedRegisters,sizeof(seed.ExtendedRegisters))!=0;
            Require(worldSeedActuallyChanged,"Controlled floating-point seed matches the accidental original baseline");
            // Request only FP/SSE state. In particular, do not rewrite the
            // software-breakpoint EIP representation used for VEH admission.
            CONTEXT request=seed;request.ContextFlags=0x10028;
            if(!SetThreadContext(heldThread.value,&request)) Error("Controlled world FP/SSE seed installation failed");
            worldSeedApplied=true;
            const CONTEXT readback=Context();worldSeedCanonicalizedBits=seed.EFlags^readback.EFlags;
            CONTEXT canonical=seed;canonical.EFlags=readback.EFlags;
            const bool namedFieldsValid=ValidFloatingSeed(readback);
            const bool rawReadbackExact=(worldSeedCanonicalizedBits&~2u)==0 && SameContext(canonical,readback);
            worldSeedValidated=namedFieldsValid && rawReadbackExact;
            Log("{\"event\":\"world-context-controlled-seed\",\"setContextFlags\":65576,\"expected\":"+SeedFields(seed)+",\"actual\":"+SeedFields(readback)+
                ",\"namedFieldsAndFinitePatternsValid\":"+Boolean(namedFieldsValid)+",\"fullRawReadbackExactExceptEflagsBit1\":"+Boolean(rawReadbackExact)+
                ",\"eflagsReservedBitNormalization\":"+std::to_string(worldSeedCanonicalizedBits)+",\"seedDiffersFromOriginal\":"+Boolean(worldSeedActuallyChanged)+
                ",\"gprUnchanged\":"+Boolean(SameGprAndControl(worldOriginal,readback))+",\"segmentsUnchanged\":"+Boolean(SameSegments(worldOriginal,readback))+
                ",\"debugRegistersUnchanged\":"+Boolean(SameDebugRegisters(worldOriginal,readback))+
                ",\"floatSaveByteDifferences\":"+ByteDifferences(&seed.FloatSave,&readback.FloatSave,sizeof(seed.FloatSave))+
                ",\"extendedByteDifferences\":"+ByteDifferences(seed.ExtendedRegisters,readback.ExtendedRegisters,sizeof(seed.ExtendedRegisters))+"}");
            Require(worldSeedValidated,"Controlled world seed readback changed named state or unqualified raw FP/SSE bytes");
            worldBefore=readback;
        }
        void RequestCpuSeedReplay()
        {
            Require(worldWitnessStarted && worldSeedValidated && !worldSeedReplayRequested && !worldCpuSeedCaptured && !callerBreakpointArmed,"Controlled seed replay cannot be retried");
            BYTE instruction{};ReadRemote(heldProcess.value,WorldReturn,&instruction,1);
            Require(instruction==0xCC,"CPU seed replay requires the permanent owned INT3");
            CONTEXT replay=worldBefore;replay.ContextFlags=FullContext;replay.Eip=WorldReturn;
            if(!SetThreadContext(heldThread.value,&replay)) Error("Controlled CPU seed replay redirection failed");
            const CONTEXT readback=Context();worldReplaySetCanonicalizedBits=replay.EFlags^readback.EFlags;
            CONTEXT canonical=replay;canonical.EFlags=readback.EFlags;
            const bool exact=(worldReplaySetCanonicalizedBits&~2u)==0 && SameContext(canonical,readback) && ValidFloatingSeed(readback) && !(readback.EFlags&0x100);
            Log("{\"event\":\"world-seed-replay-requested\",\"expected\":"+ContextWords(replay)+",\"actual\":"+ContextWords(readback)+
                ",\"fullRawReadbackExactExceptEflagsBit1\":"+Boolean(exact)+",\"eflagsReservedBitNormalization\":"+std::to_string(worldReplaySetCanonicalizedBits)+
                ",\"floatSaveByteDifferences\":"+ByteDifferences(&replay.FloatSave,&readback.FloatSave,sizeof(replay.FloatSave))+
                ",\"extendedByteDifferences\":"+ByteDifferences(replay.ExtendedRegisters,readback.ExtendedRegisters,sizeof(replay.ExtendedRegisters))+
                ",\"firstWorldExceptionHandledByDebugger\":true,\"nextMainThreadInstructionIsOwnedInt3\":true,\"originalC3NotExecuted\":true}");
            Require(exact,"Controlled seed replay full-context readback differs");
            worldSeedReplayRequested=true;
        }
        void BeginWorldWitness()
        {
            Require(startComplete && bootstrap==3 && callerSignatureCaptured && !worldWitnessStarted && !callerBreakpointArmed,"World witness cannot be reentered or retried");
            worldOriginal=Context();
            Require(worldOriginal.Eip==WorldReturn+1 && worldOriginal.Esp>=0x10000 && worldOriginal.Esp<=MAXDWORD-4 &&
                !(worldOriginal.Esp&3) && !(worldOriginal.EFlags&0x100),"World witness is not the qualified no-TF main-thread return stop");
            Require(Word(heldProcess.value,worldOriginal.Esp)==WorldCaller,"World witness caller return differs from 0x41CDF8");
            BYTE instruction{};ReadRemote(heldProcess.value,WorldReturn,&instruction,1);
            Require(instruction==0xCC,"Permanent world breakpoint is absent");
            std::array<BYTE,16> actual{};ReadRemote(heldProcess.value,WorldCaller,actual.data(),actual.size());
            Require(actual==callerOriginalBytes,"World caller signature changed after adapter initialization");
            worldWitnessStarted=true;worldWitnessBegan=GetTickCount64();
            SeedWorldContext();
            // Handling the first exception avoids dispatching its pre-seed
            // pending ContextRecord. The CPU next executes only our INT3,
            // creating a new exception whose captured FP state must be seeded.
            RequestCpuSeedReplay();
        }
        void CaptureCpuSeedWitness()
        {
            Require(worldWitnessStarted && worldSeedReplayRequested && !worldCpuSeedCaptured && !callerBreakpointArmed,"Unexpected or repeated CPU seed capture");
            Require(GetTickCount64()-worldWitnessBegan<=WorldWitnessLimit,"CPU seed replay exceeded its five-second deadline");
            const CONTEXT cpu=Context();
            Require(cpu.Eip==WorldReturn+1 && cpu.Esp==worldOriginal.Esp && !(cpu.EFlags&0x100) &&
                Word(heldProcess.value,cpu.Esp)==WorldCaller,"CPU seed replay did not retrap at the same no-TF world return");
            BYTE instruction{};ReadRemote(heldProcess.value,WorldReturn,&instruction,1);
            Require(instruction==0xCC,"CPU seed capture world instruction is no longer the owned INT3");
            worldCpuSeedCanonicalizedBits=worldBefore.EFlags^cpu.EFlags;
            const bool gprSame=SameGprAndControl(worldBefore,cpu),segmentsSame=SameSegments(worldBefore,cpu),debugSame=SameDebugRegisters(worldBefore,cpu);
            const bool namedFieldsValid=ValidFloatingSeed(cpu);
            worldCpuSeedEncodingQualified=QualifiedCpuSeedEncoding(worldBefore,cpu);
            worldCpuSeedRawEncodingChanged=std::memcmp(&worldBefore.FloatSave,&cpu.FloatSave,sizeof(cpu.FloatSave))!=0 ||
                std::memcmp(worldBefore.ExtendedRegisters,cpu.ExtendedRegisters,sizeof(cpu.ExtendedRegisters))!=0;
            worldCpuSeedCaptured=gprSame && segmentsSame && debugSame && (worldCpuSeedCanonicalizedBits&~2u)==0 && namedFieldsValid && worldCpuSeedEncodingQualified;
            Log("{\"event\":\"world-cpu-seed-captured\",\"expected\":"+SeedFields(worldBefore)+",\"actual\":"+SeedFields(cpu)+
                ",\"expectedRegisters\":"+ContextWords(worldBefore)+",\"actualRegisters\":"+ContextWords(cpu)+
                ",\"gprUnchanged\":"+Boolean(gprSame)+",\"segmentsUnchanged\":"+Boolean(segmentsSame)+",\"debugRegistersUnchanged\":"+Boolean(debugSame)+
                ",\"eflagsReservedBitNormalization\":"+std::to_string(worldCpuSeedCanonicalizedBits)+",\"namedFieldsAndFinitePatternsValid\":"+Boolean(namedFieldsValid)+
                ",\"rawEncodingChanged\":"+Boolean(worldCpuSeedRawEncodingChanged)+",\"beforeHandlerEncodingQualified\":"+Boolean(worldCpuSeedEncodingQualified)+
                ",\"floatSaveEncodingDifferences\":"+CpuSeedEncodingDifferences(&worldBefore.FloatSave,&cpu.FloatSave,sizeof(cpu.FloatSave),false)+
                ",\"extendedEncodingDifferences\":"+CpuSeedEncodingDifferences(worldBefore.ExtendedRegisters,cpu.ExtendedRegisters,sizeof(cpu.ExtendedRegisters),true)+
                ",\"cpuSeedCaptured\":"+Boolean(worldCpuSeedCaptured)+",\"originalMainThreadInstructionsExecutedBetweenTraps\":false}");
            Require(worldCpuSeedCaptured,"Fresh CPU exception did not capture the controlled seed or changed unqualified state");
            // This actual CPU-captured representation is the complete baseline
            // for the VEH comparison. No encoding masks apply after the handler.
            worldBefore=cpu;
            std::array<BYTE,16> caller{};ReadRemote(heldProcess.value,WorldCaller,caller.data(),caller.size());
            Require(caller==callerOriginalBytes,"World caller changed during controlled seed replay");
            callerBreakpointArmed=true; // Retain ownership even after a partial write failure.
            callerBreakpointRestored=false;
            WriteCallerByte(0xCC);
            Log("{\"event\":\"world-context-witness-begin\",\"thread\":"+std::to_string(pi.dwThreadId)+",\"world\":"+Quote(Hex(WorldReturn))+
                ",\"caller\":"+Quote(Hex(WorldCaller))+",\"before\":"+ContextWords(worldBefore)+
                ",\"originalBeforeSeed\":"+ContextWords(worldOriginal)+",\"controlledFpSeedValidated\":"+Boolean(worldSeedValidated)+
                ",\"cpuSeedCaptured\":"+Boolean(worldCpuSeedCaptured)+",\"secondWorldExceptionPassedToVeh\":true,\"afterHandlerRawFpSseComparisonUsesNoMasks\":true}");
        }
        void CompleteWorldWitness()
        {
            Require(worldWitnessStarted && callerBreakpointArmed && !worldContextWitnessPassed,"Unexpected world caller witness trap");
            Require(GetTickCount64()-worldWitnessBegan<=WorldWitnessLimit,"World caller witness exceeded its five-second deadline");
            const CONTEXT actual=Context();CONTEXT expected=worldBefore;
            expected.Eip=WorldCaller+1;expected.Esp+=4;
            worldGprPreserved=SameGprAndControl(expected,actual);
            worldSegmentsPreserved=SameSegments(expected,actual);worldDebugPreserved=SameDebugRegisters(expected,actual);
            worldFpuBytesPreserved=std::memcmp(&expected.FloatSave,&actual.FloatSave,sizeof(expected.FloatSave))==0;
            worldSseBytesPreserved=std::memcmp(expected.ExtendedRegisters+24,actual.ExtendedRegisters+24,4)==0 &&
                std::memcmp(expected.ExtendedRegisters+160,actual.ExtendedRegisters+160,128)==0;
            worldExtendedBytesPreserved=std::memcmp(expected.ExtendedRegisters,actual.ExtendedRegisters,sizeof(expected.ExtendedRegisters))==0;
            worldWitnessCanonicalizedBits=expected.EFlags^actual.EFlags;
            CONTEXT comparable=expected;comparable.EFlags=actual.EFlags;
            const bool same=(worldWitnessCanonicalizedBits&~2u)==0 && SameContext(comparable,actual);
            Log("{\"event\":\"world-context-witness-compare\",\"expected\":"+ContextWords(expected)+",\"actual\":"+ContextWords(actual)+
                ",\"eflagsReservedBitNormalization\":"+std::to_string(worldWitnessCanonicalizedBits)+",\"gprPreserved\":"+Boolean(worldGprPreserved)+
                ",\"segmentsPreserved\":"+Boolean(worldSegmentsPreserved)+",\"debugRegistersPreserved\":"+Boolean(worldDebugPreserved)+
                ",\"floatSaveBytesPreserved\":"+Boolean(worldFpuBytesPreserved)+",\"sseMxcsrAndXmmBytesPreserved\":"+Boolean(worldSseBytesPreserved)+
                ",\"allExtendedBytesPreserved\":"+Boolean(worldExtendedBytesPreserved)+",\"floatSaveByteDifferences\":"+ByteDifferences(&expected.FloatSave,&actual.FloatSave,sizeof(expected.FloatSave))+
                ",\"extendedByteDifferences\":"+ByteDifferences(expected.ExtendedRegisters,actual.ExtendedRegisters,sizeof(expected.ExtendedRegisters))+"}");
            Require(same && !(actual.EFlags&0x100),"Adapter VEH changed the world return's architectural or legacy FP/SSE context");
            Require(Word(heldProcess.value,worldBefore.Esp)==WorldCaller,"World return stack DWORD changed during VEH");
            std::array<BYTE,16> patched{};ReadRemote(heldProcess.value,WorldCaller,patched.data(),patched.size());
            Require(patched[0]==0xCC,"Owned caller breakpoint disappeared before completion");patched[0]=callerOriginalBytes[0];
            Require(patched==callerOriginalBytes,"Caller signature changed while the witness was armed");
            WriteCallerByte(callerOriginalBytes[0]);callerBreakpointArmed=false;callerBreakpointRestored=true;
            // The witness must not change game-visible state. Restore the
            // original pre-SEED registers with only the exact C3 EIP/ESP effect,
            // then execute the original caller instruction without trap flag.
            CONTEXT resume=worldOriginal;resume.ContextFlags=FullContext;resume.Eip=WorldCaller;resume.Esp+=4;
            if(!SetThreadContext(heldThread.value,&resume)) Error("Original post-C3 world context restoration failed");
            const CONTEXT restored=Context();worldRestoreCanonicalizedBits=resume.EFlags^restored.EFlags;
            CONTEXT canonical=resume;canonical.EFlags=restored.EFlags;
            Require((worldRestoreCanonicalizedBits&~2u)==0 && SameContext(canonical,restored) && !(restored.EFlags&0x100),"Original post-C3 context readback differs");
            worldOriginalContextRestored=true;worldContextWitnessPassed=true;
            worldControlledPatternPassed=worldSeedApplied && worldSeedValidated && worldSeedActuallyChanged && worldSeedReplayRequested && worldCpuSeedCaptured && worldCpuSeedEncodingQualified;
            Log("{\"event\":\"world-context-witness-complete\",\"originalCallerInstructionRestored\":true,\"originalWorldFpSseRestored\":true,\"originalInstructionDeferred\":true,\"trapFlag\":false,\"resumeEip\":"+Quote(Hex(restored.Eip))+
                ",\"resumeEsp\":"+Quote(Hex(restored.Esp))+",\"restoreReservedBitNormalization\":"+std::to_string(worldRestoreCanonicalizedBits)+
                ",\"controlledFpPattern\":"+Boolean(worldControlledPatternPassed)+",\"originalContextRestoredBeforeGameInstruction\":true}");
        }
        void LogAdapterDebug(const DEBUG_EVENT& event)
        {
            const auto& info=event.u.DebugString;
            if(info.fUnicode || !info.nDebugStringLength || info.nDebugStringLength>512 || !info.lpDebugStringData) return;
            std::array<char,512> text{};SIZE_T got{};
            if(!ReadProcessMemory(heldProcess.value,info.lpDebugStringData,text.data(),info.nDebugStringLength,&got) || got!=info.nDebugStringLength) return;
            std::size_t length=info.nDebugStringLength;if(length && text[length-1]==0)--length;
            constexpr char prefix[]="WinxCoop ";
            if(length<sizeof(prefix)-1 || std::memcmp(text.data(),prefix,sizeof(prefix)-1)!=0) return;
            for(std::size_t i=0;i<length;++i)
            { const auto c=static_cast<unsigned char>(text[i]);if((c<32 && c!='\r' && c!='\n' && c!='\t') || c>126)return; }
            Log("{\"event\":\"adapter-debug\",\"thread\":"+std::to_string(event.dwThreadId)+",\"bytes\":"+std::to_string(length)+",\"text\":"+Quote(std::string(text.data(),length))+"}");
        }
        void Sample(const bool force=false)
        {
            const auto now=GetTickCount64();if(!shared || (!force && now-lastSample<50))return;lastSample=now;
            RuntimeSnapshot value{};if(!TryRead(shared->snapshot,value) || value.phase==RuntimePhase::Empty)return;
            Require(value.gamePid==pi.dwProcessId && value.mainThreadId==pi.dwThreadId && static_cast<unsigned>(value.phase)<=static_cast<unsigned>(RuntimePhase::Failed),"Snapshot identity/version mismatch");
            Require(value.mode==pendingStart.mode && value.session==pendingStart.session && value.levelEpoch==pendingStart.levelEpoch && !value.inputQualified,"Runtime mode/session/input qualification mismatch");
            if(samples && value.worldUpdates==latest.worldUpdates && value.phase==latest.phase)return;
            Require(!samples || value.worldUpdates>=latest.worldUpdates,"Runtime world counter moved backward");
            latest=value;++samples;if(value.phase==RuntimePhase::Ready)++readySamples;
            mediaQualifiedObserved=mediaQualifiedObserved || value.mediaQualified!=0;
            std::string position="[";
            for(unsigned i=0;i<3;++i){if(i)position+=',';position+=std::to_string(value.local.position[i]);}position+=']';
            Log("{\"event\":\"runtime-snapshot\",\"phase\":"+std::to_string(static_cast<unsigned>(value.phase))+",\"error\":"+std::to_string(static_cast<unsigned>(value.error))+",\"worldUpdates\":"+std::to_string(value.worldUpdates)+",\"level\":"+std::to_string(value.levelId)+",\"timeMs\":"+std::to_string(value.monotonicMilliseconds)+",\"localPosition\":"+position+",\"componentCount\":"+std::to_string(value.localComponentCount)+
                ",\"remoteCreated\":"+std::to_string(value.remoteCreated)+",\"remoteRemoved\":"+std::to_string(value.remoteRemoved)+",\"remoteEntity\":"+std::to_string(value.remoteEntity)+
                ",\"actorError\":"+std::to_string(value.actorError)+",\"nativeCalls\":"+std::to_string(value.nativeCalls)+",\"lastCommandSequence\":"+std::to_string(value.lastCommandSequence)+
                ",\"unloadGuardHits\":"+std::to_string(value.unloadGuardHits)+",\"unloadCleanups\":"+std::to_string(value.unloadCleanups)+",\"epochRetired\":"+Boolean(value.epochRetired!=0)+",\"mediaQualified\":"+Boolean(value.mediaQualified!=0)+"}");
            Require(value.phase!=RuntimePhase::Failed,"Native adapter reported a failed runtime gate");
        }
        void SendCommand(const RuntimeOperation operation, const RuntimeAnimation animation)
        {
            Require(!pendingCommand.sequence && commandsSent==commandsAcknowledged && commandsSent<0xFFFFFFFF,"Command overwrite or sequence wrap");
            RuntimeAck ack{};Require(TryRead(shared->acknowledgement,ack) && ack.sequence==commandsAcknowledged,"Previous command ACK unavailable");
            RuntimeCommand command{};command.bytes=sizeof(command);command.version=IpcVersion;command.session=pendingStart.session;
            command.levelEpoch=pendingStart.levelEpoch;command.sequence=commandsSent+1;command.entity=2;command.operation=operation;command.animation=animation;
            if(operation!=RuntimeOperation::Remove)
            {
                command.remote=latest.local;command.remote.position[0]+=animation==RuntimeAnimation::Walk?220.0f:180.0f;
                if(animation==RuntimeAnimation::Jump)command.remote.position[1]+=50.0f;
            }
            // Own diagnostic producer, not network authority or native physics.
            pendingCommand=command;commandBegan=GetTickCount64();++commandsSent;
            Log("{\"event\":\"runtime-command-queued\",\"sequence\":"+std::to_string(command.sequence)+",\"entity\":"+std::to_string(command.entity)+",\"operation\":"+std::to_string(static_cast<unsigned>(operation))+",\"animation\":"+std::to_string(static_cast<unsigned>(animation))+",\"singleOutstanding\":true,\"notPublishedUntilFreshCpuSeedStop\":true,\"diagnosticProducer\":true}");
        }
        void PumpCommands()
        {
            if(options.mode==L"observe" || !startComplete)return;
            const auto now=GetTickCount64();
            if(pendingCommand.sequence)
            {
                Require(now-commandBegan<=10000,"Runtime command exceeded its ten-second ACK/context/snapshot deadline");
                if(actorWitnessActive)return;
                RuntimeAck ack{};
                if(TryRead(shared->acknowledgement,ack) && ack.sequence!=commandsAcknowledged)
                {
                    Require(pendingCommandPublished && actorWitnessContextCompleted && MatchingAck(ack) && ack.worldUpdates==actorWitnessAck.worldUpdates,
                        "Runtime command acknowledgement failed or mismatched identity");
                    if(latest.lastCommandSequence<ack.sequence)return;
                    FinishActorWitnessAfterSnapshot();
                    commandsAcknowledged=ack.sequence;pendingCommand={};lastCommandCompleted=now;++scriptStep;
                    Log("{\"event\":\"runtime-command-acknowledged\",\"sequence\":"+std::to_string(ack.sequence)+",\"operation\":"+std::to_string(static_cast<unsigned>(ack.operation))+
                        ",\"worldUpdates\":"+std::to_string(ack.worldUpdates)+",\"latencyMilliseconds\":"+std::to_string(now-commandBegan)+",\"status\":\"applied\"}");
                }
                else return;
            }
            if(actorLifecycleCompleted)return;
            if(scriptStep==6 || (options.mode==L"passive-unload" && scriptStep==5 && latest.epochRetired))
            {
                if(latest.lastCommandSequence<commandsAcknowledged)return; // ACK may precede the throttled snapshot.
                Require(latest.remoteCreated==2 && latest.remoteRemoved==2 && !latest.remoteEntity && !latest.actorError && mediaQualifiedObserved,
                    "Passive actor lifecycle counters/cleanup mismatch");
                if(options.mode==L"passive-unload")Require(latest.unloadCleanups==1 && latest.epochRetired,"Native pre-unload cleanup was not observed");
                else Require(!latest.epochRetired,"Smoke lifecycle unexpectedly crossed a native unload");
                actorLifecycleCompleted=true;
                Log("{\"event\":\"passive-lifecycle-complete\",\"cycles\":2,\"nativePreUnload\":"+Boolean(options.mode==L"passive-unload")+",\"noLiveRemoteEntity\":true,\"gameplayAccepted\":false}");return;
            }
            if(scriptStep==5 && options.mode==L"passive-unload")return;
            if(latest.phase!=RuntimePhase::Ready || !ObserverAdmitted() || readySamples<15)return;
            Require(latest.levelId==28 && latest.mediaQualified && !latest.epochRetired,"Passive command source/level gate is not ready");
            if(scriptStep==0)SendCommand(RuntimeOperation::Create,RuntimeAnimation::Idle);
            else if(scriptStep==1 && now-lastCommandCompleted>=1500)SendCommand(RuntimeOperation::Apply,RuntimeAnimation::Walk);
            else if(scriptStep==2 && now-lastCommandCompleted>=3000)SendCommand(RuntimeOperation::Apply,RuntimeAnimation::Jump);
            else if(scriptStep==3 && now-lastCommandCompleted>=3000)SendCommand(RuntimeOperation::Remove,RuntimeAnimation::Idle);
            else if(scriptStep==4 && now-lastCommandCompleted>=1000)SendCommand(RuntimeOperation::Create,RuntimeAnimation::Idle);
            else if(scriptStep==5 && now-lastCommandCompleted>=3000)SendCommand(RuntimeOperation::Remove,RuntimeAnimation::Idle);
        }
        DWORD Dispatch(const DEBUG_EVENT& event, const bool draining)
        {
            ++eventCount;Require(event.dwProcessId==pi.dwProcessId,"Foreign debug event in owned-only session");
            switch(event.dwDebugEventCode)
            {
                case CREATE_PROCESS_DEBUG_EVENT:
                    processAutoClosed=event.u.CreateProcessInfo.hProcess==pi.hProcess;threadAutoClosed=event.u.CreateProcessInfo.hThread==pi.hThread;
                    { Handle file(event.u.CreateProcessInfo.hFile);
                      Require(SameFile(gameFile.value,file.value),"Created image file identity differs from pinned original");
                      Require(reinterpret_cast<DWORD>(event.u.CreateProcessInfo.lpBaseOfImage)==0x400000,"Actual original image base mismatch");
                      const auto pe=RemotePe(heldProcess.value,0x400000);Require(pe.FileHeader.TimeDateStamp==gamePe.FileHeader.TimeDateStamp && pe.OptionalHeader.SizeOfImage==gamePe.OptionalHeader.SizeOfImage,"Actual original PE header mismatch");
                      if(!draining) { Require(event.dwThreadId==pi.dwThreadId,"Created process event is not the owned main thread");CaptureCallerSignature(); } }
                    break;
                case LOAD_DLL_DEBUG_EVENT:
                    { Handle file(event.u.LoadDll.hFile);
                      if(SameFile(adapterFile.value,file.value)) adapterSeen=reinterpret_cast<DWORD>(event.u.LoadDll.lpBaseOfDll); }
                    break;
                case OUTPUT_DEBUG_STRING_EVENT:LogAdapterDebug(event);break;
                case EXIT_PROCESS_DEBUG_EVENT:exitCode=event.u.ExitProcess.dwExitCode;exitKnown=true;break;
                case RIP_EVENT:throw std::runtime_error("Owned debugger RIP event");
                case EXCEPTION_DEBUG_EVENT:
                {
                    const auto& exception=event.u.Exception;const DWORD code=exception.ExceptionRecord.ExceptionCode;
                    const DWORD at=reinterpret_cast<DWORD>(exception.ExceptionRecord.ExceptionAddress);
                    if(!draining && actorWitnessActive &&
                        !((code==EXCEPTION_BREAKPOINT || code==Wx86Breakpoint) && (at==WorldReturn || at==WorldCaller)))
                    {
                        const std::string fp=event.dwThreadId==pi.dwThreadId?SeedFields(Context()):"null";
                        Log("{\"event\":\"actor-native-call-fp-witness-exception\",\"sequence\":"+std::to_string(pendingCommand.sequence)+
                            ",\"operation\":"+std::to_string(static_cast<unsigned>(pendingCommand.operation))+",\"thread\":"+std::to_string(event.dwThreadId)+
                            ",\"exceptionCode\":"+Quote(Hex(code))+",\"at\":"+Quote(Hex(at))+",\"firstChance\":"+Boolean(exception.dwFirstChance!=0)+
                            ",\"seedOccupiedX87Registers\":1,\"nativeEmptyX87Requirement\":\"unqualified\",\"actualStoppedFpContext\":"+fp+"}");
                    }
                    if(!draining && exception.dwFirstChance && (code==EXCEPTION_BREAKPOINT || code==Wx86Breakpoint) &&
                       ((bootstrap==1 && at==trapLoad) || (bootstrap==2 && at==trapStart)))
                        Require(event.dwThreadId==pi.dwThreadId,"Private bootstrap completion arrived on a foreign thread");
                    if(!draining && exception.dwFirstChance && event.dwThreadId==pi.dwThreadId && (code==EXCEPTION_BREAKPOINT || code==Wx86Breakpoint))
                    {
                        if(bootstrap==0){BeginBootstrap(event);return DBG_CONTINUE;}
                        if(bootstrap==1 && at==trapLoad){CompleteLoad();return DBG_CONTINUE;}
                        if(bootstrap==2 && at==trapStart){CompleteStart();return DBG_CONTINUE;}
                    }
                    if(!draining && exception.dwFirstChance && (code==EXCEPTION_BREAKPOINT || code==Wx86Breakpoint) && bootstrap==3)
                    {
                        if(callerBreakpointArmed && at==WorldCaller)
                        {
                            Require(event.dwThreadId==pi.dwThreadId,"Owned world caller breakpoint arrived on a foreign thread");
                            CompleteWorldWitness();
                            if(actorWitnessActive)CompleteActorWitnessAtCaller();
                            else
                            {
                                Require(!observerWitnessSaved,"Initial observer admission was repeated");
                                observerWitness=static_cast<const WorldWitnessState&>(*this);observerWitnessSaved=true;
                            }
                            return DBG_CONTINUE;
                        }
                        if(at==WorldReturn && !worldContextWitnessPassed)
                        {
                            Require(event.dwThreadId==pi.dwThreadId,"World witness arrived on a foreign thread");
                            if(!worldWitnessStarted) { BeginWorldWitness();return DBG_CONTINUE; }
                            Require(worldSeedReplayRequested && !worldCpuSeedCaptured && !callerBreakpointArmed,"World return reentered after controlled seed capture");
                            CaptureCpuSeedWitness();if(actorWitnessActive)PublishWitnessCommand();return DBG_EXCEPTION_NOT_HANDLED;
                        }
                        if(at==WorldReturn && pendingCommand.sequence && !pendingCommandPublished)
                        {
                            Require(event.dwThreadId==pi.dwThreadId,"Queued actor witness arrived on a foreign thread");
                            BeginActorWitness();return DBG_CONTINUE;
                        }
                    }
                    if(!exception.dwFirstChance){++fatal;throw std::runtime_error("Owned game second-chance exception "+Hex(code)+" at "+Hex(at));}
                    // The permanent world INT3 belongs to the adapter's VEH.
                    // The debugger must pass first chance to the application.
                    return DBG_EXCEPTION_NOT_HANDLED;
                }
            }
            return DBG_CONTINUE;
        }
        void Continue(const DEBUG_EVENT& event, const DWORD status)
        {
            if(!ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status))Error("ContinueDebugEvent failed");
            if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)exitContinued=true;
        }
        void StopAtEvent(const DEBUG_EVENT& event, const std::string& error)
        {
            if(failure.empty())failure=error;
            if(heldProcess.Valid() && TerminateProcess(heldProcess.value,124))forced=true;
            RestoreCallerBreakpointBestEffort(true);
            // Never deliver our private completion INT3 to game exception code
            // after a partial bootstrap failure. The owned child is terminating.
            DWORD status=DBG_CONTINUE;
            if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT && event.u.Exception.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT && event.u.Exception.ExceptionRecord.ExceptionCode!=Wx86Breakpoint)status=DBG_EXCEPTION_NOT_HANDLED;
            Continue(event,status);
        }
        void Run()
        {
            Setup();Launch();
            while(!exitContinued)
            {
                Sample();PumpCommands();const auto now=GetTickCount64();
                if((bootstrap==1 || bootstrap==2) && now-bootstrapBegan>BootstrapLimit)throw std::runtime_error("Bootstrap exceeded 30 second call deadline");
                if(worldWitnessStarted && !worldContextWitnessPassed && now-worldWitnessBegan>WorldWitnessLimit)throw std::runtime_error("World caller witness exceeded its five-second deadline");
                if(now-began>=std::uint64_t(options.seconds)*1000)
                { timedOut=true;if(!startComplete)failure="Observation limit reached before adapter bootstrap completed";else if(!worldContextWitnessPassed)failure="Observation limit reached before the world context witness completed";
                  else if(options.mode!=L"observe" && (!actorLifecycleCompleted || !ActorFpQualified()))failure="Observation limit reached before passive actor lifecycle/context qualification completed";break; }
                DEBUG_EVENT event{};
                if(!WaitForDebugEvent(&event,50)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;Error("WaitForDebugEvent failed");}
                try{const DWORD status=Dispatch(event,false);Continue(event,status);}
                catch(const std::exception& e){StopAtEvent(event,e.what());break;}
            }
            Sample(true);
            if(exitContinued && exitCode!=0 && failure.empty())
                failure="Owned game exited unexpectedly with code "+std::to_string(exitCode);
        }
        void Cleanup() noexcept
        {
            HANDLE process=heldProcess.Valid()?heldProcess.value:pi.hProcess;
            if(!process){cleanupConfirmed=true;return;}
            if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0 && TerminateProcess(process,failure.empty() && startComplete?0:124))forced=true;
            RestoreCallerBreakpointBestEffort(false);
            const auto until=GetTickCount64()+5000;
            while(!exitContinued && GetTickCount64()<until)
            {
                DEBUG_EVENT event{};
                if(!WaitForDebugEvent(&event,50)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;break;}
                DWORD status=DBG_CONTINUE;
                try{status=Dispatch(event,true);}
                catch(const std::exception& e){if(failure.empty())failure=e.what();}
                if(!ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status)){if(failure.empty())failure="Cleanup ContinueDebugEvent failed";break;}
                if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)exitContinued=true;
            }
            const auto now=GetTickCount64();
            const DWORD remaining=now<until?static_cast<DWORD>(until-now):0;
            cleanupConfirmed=WaitForSingleObject(process,remaining)==WAIT_OBJECT_0;
            if(cleanupConfirmed && GetExitCodeProcess(process,&exitCode))exitKnown=true;
            if(!cleanupConfirmed && failure.empty())failure="Owned process did not signal within cleanup budget";
        }
        void Report()
        {
            if(!report.Valid())return;
            const WorldWitnessState& observer=observerWitnessSaved?observerWitness:static_cast<const WorldWitnessState&>(*this);
            const bool completed=failure.empty() && startComplete && contextRestored && pageFreed && ObserverAdmitted() &&
                (options.mode==L"observe" || (actorLifecycleCompleted && ActorFpQualified())) && cleanupConfirmed && fatal==0;
            const std::string witnessFields=",\"worldContextWitnessStarted\":"+Boolean(observer.worldWitnessStarted)+",\"worldContextWitnessPassed\":"+Boolean(observer.worldContextWitnessPassed)+
                ",\"worldContextWitnessGprPreserved\":"+Boolean(observer.worldGprPreserved)+",\"worldContextWitnessSegmentsPreserved\":"+Boolean(observer.worldSegmentsPreserved)+
                ",\"worldContextWitnessDebugRegistersPreserved\":"+Boolean(observer.worldDebugPreserved)+",\"worldContextWitnessFpuBytesPreserved\":"+Boolean(observer.worldFpuBytesPreserved)+
                ",\"worldContextWitnessSseBytesPreserved\":"+Boolean(observer.worldSseBytesPreserved)+",\"worldContextWitnessAllExtendedBytesPreserved\":"+Boolean(observer.worldExtendedBytesPreserved)+
                ",\"worldContextWitnessOriginalContextRestored\":"+Boolean(observer.worldOriginalContextRestored)+",\"worldContextWitnessReservedBitsNormalized\":"+std::to_string(observer.worldWitnessCanonicalizedBits)+
                ",\"worldContextWitnessRestoreReservedBitsNormalized\":"+std::to_string(observer.worldRestoreCanonicalizedBits)+
                ",\"worldContextWitnessControlledFpPattern\":"+Boolean(observer.worldControlledPatternPassed)+",\"worldContextWitnessSeedApplied\":"+Boolean(observer.worldSeedApplied)+
                ",\"worldContextWitnessSeedValidated\":"+Boolean(observer.worldSeedValidated)+",\"worldContextWitnessSeedDiffersFromOriginal\":"+Boolean(observer.worldSeedActuallyChanged)+
                ",\"worldContextWitnessSeedReservedBitsNormalized\":"+std::to_string(observer.worldSeedCanonicalizedBits)+
                ",\"worldContextWitnessSeedReplayRequested\":"+Boolean(observer.worldSeedReplayRequested)+",\"worldContextWitnessCpuSeedCaptured\":"+Boolean(observer.worldCpuSeedCaptured)+
                ",\"worldContextWitnessCpuSeedEncodingQualified\":"+Boolean(observer.worldCpuSeedEncodingQualified)+",\"worldContextWitnessCpuSeedRawEncodingChanged\":"+Boolean(observer.worldCpuSeedRawEncodingChanged)+
                ",\"worldContextWitnessReplaySetReservedBitsNormalized\":"+std::to_string(observer.worldReplaySetCanonicalizedBits)+",\"worldContextWitnessCpuSeedReservedBitsNormalized\":"+std::to_string(observer.worldCpuSeedCanonicalizedBits)+
                ",\"worldContextWitnessSeedSkipped\":null"+
                ",\"callerSignatureCapturedBeforeAdapter\":"+Boolean(callerSignatureCaptured)+",\"callerBreakpointRestored\":"+Boolean(callerBreakpointRestored)+
                ",\"callerBreakpointReclaimedByProcessExit\":"+Boolean(callerBreakpointArmed && cleanupConfirmed)+",\"callerProtectionChangePending\":"+Boolean(callerProtectionChanged);
            std::string operationWitnesses="[";
            for(unsigned i=0;i<3;++i)
            {
                if(i)operationWitnesses+=',';const auto& value=actorWitnesses[i];
                operationWitnesses+="{\"operation\":"+std::to_string(i+1)+",\"attemptedIntervals\":"+std::to_string(value.started)+
                    ",\"qualifiedIntervals\":"+std::to_string(value.passed)+",\"observedNativeCalls\":"+std::to_string(value.nativeCalls)+
                    ",\"lastSequence\":"+std::to_string(value.lastSequence)+",\"lastAckWorldUpdates\":"+std::to_string(value.lastAckWorldUpdates)+
                    ",\"qualified\":"+Boolean(value.passed>0 && value.started==value.passed && value.nativeCalls>0)+"}";
            }
            operationWitnesses+=']';
            const std::string actorFields=",\"mode\":"+Quote(options.mode==L"observe"?"observe":options.mode==L"passive-actor"?"passive-actor":"passive-unload")+
                ",\"ipcVersion\":"+std::to_string(IpcVersion)+",\"commandsSent\":"+std::to_string(commandsSent)+",\"commandsAcknowledged\":"+std::to_string(commandsAcknowledged)+
                ",\"actorLifecycleCompleted\":"+Boolean(actorLifecycleCompleted)+",\"remoteCreated\":"+std::to_string(latest.remoteCreated)+",\"remoteRemoved\":"+std::to_string(latest.remoteRemoved)+
                ",\"remoteEntity\":"+std::to_string(latest.remoteEntity)+",\"nativeCalls\":"+std::to_string(latest.nativeCalls)+",\"unloadGuardHits\":"+std::to_string(latest.unloadGuardHits)+
                ",\"unloadCleanups\":"+std::to_string(latest.unloadCleanups)+",\"epochRetired\":"+Boolean(latest.epochRetired!=0)+",\"mediaQualified\":"+Boolean(latest.mediaQualified!=0)+",\"mediaQualifiedObserved\":"+Boolean(mediaQualifiedObserved)+
                ",\"actorNativeCallsControlledFpWitness\":"+Boolean(ActorFpQualified())+",\"actorFpWitnessIntervalsStarted\":"+std::to_string(actorWitnessIntervalsStarted)+
                ",\"actorFpWitnessIntervalsPassed\":"+std::to_string(actorWitnessIntervalsPassed)+",\"actorFpWitnessActive\":"+Boolean(actorWitnessActive)+
                ",\"actorFpWitnessContextCompletedAwaitingSnapshot\":"+Boolean(actorWitnessContextCompleted)+",\"actorFpWitnessSeedOccupiedX87Registers\":1"+
                ",\"actorFpWitnessNativeEmptyX87Requirement\":\"unqualified\",\"actorFpWitnessCoverage\":\"command-world-intervals-only\""+
                ",\"preUnloadNativeCallsControlledFpWitness\":false,\"actorFpWitnessOperations\":"+operationWitnesses;
            WriteAll(report.value,"{\"kind\":\"WinxCoop-owned-x86-adapter-probe\",\"probeCompleted\":"+Boolean(completed)+",\"gameplayAccepted\":false,\"pid\":"+std::to_string(pi.dwProcessId)+",\"observationSeconds\":"+std::to_string(options.seconds)+",\"limitReached\":"+Boolean(timedOut)+",\"loadComplete\":"+Boolean(loadComplete)+",\"startComplete\":"+Boolean(startComplete)+",\"fullContextRestored\":"+Boolean(contextRestored)+",\"privatePageFreed\":"+Boolean(pageFreed)+",\"privatePageReclaimedByProcessExit\":"+Boolean(privatePage!=0 && cleanupConfirmed)+witnessFields+actorFields+",\"runtimeReadyObserved\":"+Boolean(readySamples>0)+",\"readySamples\":"+std::to_string(readySamples)+",\"samples\":"+std::to_string(samples)+",\"worldUpdates\":"+std::to_string(latest.worldUpdates)+",\"debugEvents\":"+std::to_string(eventCount)+",\"fatalExceptions\":"+std::to_string(fatal)+",\"exitCodeKnown\":"+Boolean(exitKnown)+",\"exitCode\":"+std::to_string(exitCode)+",\"forcedTermination\":"+Boolean(forced)+",\"exitEventContinued\":"+Boolean(exitContinued)+",\"cleanupConfirmed\":"+Boolean(cleanupConfirmed)+",\"error\":"+(failure.empty()?std::string("null"):Quote(failure))+"}\n");
        }
    };
}
int wmain()
{
    try
    {
        Probe probe(Parse());
        try{probe.Run();}catch(const std::exception& e){if(probe.failure.empty())probe.failure=e.what();}
        probe.Cleanup();probe.Report();return probe.failure.empty() && probe.startComplete && probe.ObserverAdmitted() &&
            (probe.options.mode==L"observe" || (probe.actorLifecycleCompleted && probe.ActorFpQualified())) && probe.cleanupConfirmed?0:4;
    }
    catch(...){return 3;}
}
