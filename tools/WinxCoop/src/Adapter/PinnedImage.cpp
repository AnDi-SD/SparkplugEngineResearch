#include "PinnedImage.h"
#include <bcrypt.h>

namespace winxcoop::adapter
{
    namespace
    {
        const BYTE OriginalSha256[32] = {
            0x3f,0x02,0x24,0x80,0xbf,0x55,0x04,0x5d,0xa4,0xbf,0x69,0x2e,0x4b,0xc8,0x86,0x2e,
            0xd3,0x8f,0xc0,0x24,0xe8,0xa9,0x64,0xa5,0x58,0xfb,0xdf,0xdf,0x64,0x6d,0xfc,0x4f};
        bool Same(const BYTE* a, const BYTE* b, SIZE_T count) noexcept
        { for (SIZE_T i=0; i<count; ++i) if (a[i]!=b[i]) return false; return true; }
    }
    bool PinnedImage::Open() noexcept
    {
        if (view_) return true;
        wchar_t path[MAX_PATH]{};
        const auto length=GetModuleFileNameW(nullptr,path,MAX_PATH);
        if (!length || length>=MAX_PATH) return false;
        file_=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (file_==INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_,&size) || size.QuadPart<4096 || size.QuadPart>64*1024*1024) {Close();return false;}
        size_=static_cast<DWORD>(size.QuadPart);
        mapping_=CreateFileMappingW(file_,nullptr,PAGE_READONLY,0,0,nullptr);
        if (mapping_) view_=static_cast<const BYTE*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,0));
        if (!view_) {Close();return false;}
        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        BYTE digest[32]{};
        const bool hashed=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
            BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0 &&
            BCryptHashData(hash,const_cast<BYTE*>(view_),size_,0)>=0 &&
            BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
        if (!hashed || !Same(digest,OriginalSha256,32)) {Close();return false;}
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(view_);
        if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || static_cast<DWORD>(dos->e_lfanew)>size_-sizeof(IMAGE_NT_HEADERS32)) {Close();return false;}
        pe_=reinterpret_cast<const IMAGE_NT_HEADERS32*>(view_+dos->e_lfanew);
        if (pe_->Signature!=IMAGE_NT_SIGNATURE || pe_->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
            pe_->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC || pe_->OptionalHeader.ImageBase!=0x400000 ||
            pe_->FileHeader.NumberOfSections>96 || pe_->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER32)) {Close();return false;}
        const auto sectionEnd=reinterpret_cast<const BYTE*>(IMAGE_FIRST_SECTION(pe_)+pe_->FileHeader.NumberOfSections);
        if (sectionEnd>view_+size_) {Close();return false;}
        return true;
    }
    bool PinnedImage::Verify(const std::uint32_t address, const std::size_t bytes) const noexcept
    {
        if (!pe_ || address<0x401000 || bytes==0 || bytes>32) return false;
        const DWORD rva=address-0x400000;
        const auto* sections=IMAGE_FIRST_SECTION(pe_);
        for (WORD i=0; i<pe_->FileHeader.NumberOfSections; ++i)
        {
            const auto& section=sections[i];
            if (!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE) || rva<section.VirtualAddress) continue;
            const DWORD relative=rva-section.VirtualAddress;
            if (relative>section.SizeOfRawData || bytes>section.SizeOfRawData-relative) continue;
            const std::uint64_t offset=static_cast<std::uint64_t>(section.PointerToRawData)+relative;
            if (offset>size_ || bytes>size_-offset) return false;
            BYTE live[32]{}; SIZE_T got{};
            return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),live,bytes,&got) && got==bytes &&
                Same(live,view_+static_cast<SIZE_T>(offset),bytes);
        }
        return false;
    }
    void PinnedImage::Close() noexcept
    {
        pe_=nullptr;
        if(view_) UnmapViewOfFile(view_);
        if(mapping_) CloseHandle(mapping_);
        if(file_!=INVALID_HANDLE_VALUE) CloseHandle(file_);
        view_=nullptr; mapping_=nullptr; file_=INVALID_HANDLE_VALUE; size_=0;
    }
}
