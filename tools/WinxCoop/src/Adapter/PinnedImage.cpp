#include "PinnedImage.h"
#include "PassiveImageCompatibility.h"

namespace winxcoop::adapter
{
    namespace
    {
        bool Same(const BYTE* a, const BYTE* b, SIZE_T count) noexcept
        { for (SIZE_T i=0; i<count; ++i) if (a[i]!=b[i]) return false; return true; }
        bool Live(const std::uint32_t address, const BYTE* expected, const SIZE_T count) noexcept
        {
            BYTE bytes[32]{}; SIZE_T read{};
            return count && count<=sizeof(bytes) &&
                ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes,count,&read) &&
                read==count && Same(bytes,expected,count);
        }
    }
    bool PinnedImage::Fail(const char* reason, const std::uint32_t address) const noexcept
    { failure_=reason; failureAddress_=address; return false; }

    bool PinnedImage::Open() noexcept
    {
        if (view_) return true;
        failure_="none"; failureAddress_=0;
        if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))!=0x400000)
            return Fail("loaded game base differs from required native layout");
        wchar_t path[MAX_PATH]{};
        const auto length=GetModuleFileNameW(nullptr,path,MAX_PATH);
        if (!length || length>=MAX_PATH) return Fail("game module path is unavailable");
        file_=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (file_==INVALID_HANDLE_VALUE) return Fail("game source read pin is unavailable");
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_,&size) || size.QuadPart<4096 || size.QuadPart>64*1024*1024)
        { Close(); return Fail("game PE size exceeds the bounded reader"); }
        size_=static_cast<DWORD>(size.QuadPart);
        mapping_=CreateFileMappingW(file_,nullptr,PAGE_READONLY,0,0,nullptr);
        if (mapping_) view_=static_cast<const BYTE*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,0));
        if (!view_) { Close(); return Fail("game source mapping is unavailable"); }
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(view_);
        if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 ||
            static_cast<DWORD>(dos->e_lfanew)>size_-sizeof(IMAGE_NT_HEADERS32))
        { Close(); return Fail("game DOS/PE header bounds differ"); }
        pe_=reinterpret_cast<const IMAGE_NT_HEADERS32*>(view_+dos->e_lfanew);
        if (pe_->Signature!=IMAGE_NT_SIGNATURE || pe_->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
            pe_->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC || pe_->OptionalHeader.ImageBase!=0x400000 ||
            !pe_->FileHeader.NumberOfSections || pe_->FileHeader.NumberOfSections>96 ||
            pe_->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER32) ||
            pe_->OptionalHeader.SizeOfImage<0x366000 || pe_->OptionalHeader.SizeOfImage>64*1024*1024)
        { Close(); return Fail("required x86 PE32/base/image layout differs"); }
        const auto* sections=IMAGE_FIRST_SECTION(pe_);
        const auto* sectionEnd=reinterpret_cast<const BYTE*>(sections+pe_->FileHeader.NumberOfSections);
        if (sectionEnd>view_+size_ || pe_->OptionalHeader.SizeOfHeaders>size_ ||
            sectionEnd>view_+pe_->OptionalHeader.SizeOfHeaders)
        { Close(); return Fail("game PE section table bounds differ"); }
        for (WORD i=0; i<pe_->FileHeader.NumberOfSections; ++i)
        {
            const auto& section=sections[i];
            const auto span=section.Misc.VirtualSize>section.SizeOfRawData?section.Misc.VirtualSize:section.SizeOfRawData;
            if (static_cast<std::uint64_t>(section.VirtualAddress)+span>pe_->OptionalHeader.SizeOfImage ||
                static_cast<std::uint64_t>(section.PointerToRawData)+section.SizeOfRawData>size_)
            { Close(); return Fail("game PE section extent exceeds its image"); }
            for (WORD j=0; j<i; ++j)
            {
                const auto& other=sections[j];
                const auto otherSpan=other.Misc.VirtualSize>other.SizeOfRawData?other.Misc.VirtualSize:other.SizeOfRawData;
                if (span && otherSpan && section.VirtualAddress<static_cast<std::uint64_t>(other.VirtualAddress)+otherSpan &&
                    other.VirtualAddress<static_cast<std::uint64_t>(section.VirtualAddress)+span)
                { Close(); return Fail("game PE section virtual ranges overlap"); }
            }
        }
        // Check only the required feature family. Resources and unrelated code
        // may differ. A debugger-owned WorldCaller INT3 is not read here: its
        // retained owner separately verifies/restores that live instruction.
        for (const auto& pin : image_compatibility::CodePins)
        {
            const auto* source=Source(pin.address,pin.size,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_EXECUTE);
            if (!source || !Same(source,pin.bytes.data(),pin.size))
            { Close(); return Fail(pin.name,pin.address); }
        }
        for (const auto& pin : image_compatibility::LayoutPins)
        {
            const auto* source=Source(pin.address,pin.size,IMAGE_SCN_MEM_READ);
            if (!source || !Same(source,pin.bytes.data(),pin.size) || !Live(pin.address,pin.bytes.data(),pin.size))
            { Close(); return Fail(pin.name,pin.address); }
        }
        for (const auto address : image_compatibility::GlobalSlots)
            if (!Layout(address,sizeof(DWORD),IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE))
            { Close(); return Fail("mutable singleton slot left image data layout",address); }
        return true;
    }
    bool PinnedImage::Layout(const std::uint32_t address, const std::size_t bytes, const DWORD flags) const noexcept
    {
        if (!pe_ || address<0x400000 || !bytes || bytes>32) return false;
        const auto rva=address-0x400000;
        const auto* sections=IMAGE_FIRST_SECTION(pe_);
        for (WORD i=0; i<pe_->FileHeader.NumberOfSections; ++i)
        {
            const auto& section=sections[i];
            const auto span=section.Misc.VirtualSize>section.SizeOfRawData?section.Misc.VirtualSize:section.SizeOfRawData;
            if ((section.Characteristics&flags)!=flags || rva<section.VirtualAddress) continue;
            const auto relative=rva-section.VirtualAddress;
            if (relative<=span && bytes<=span-relative) return true;
        }
        return false;
    }
    const BYTE* PinnedImage::Source(const std::uint32_t address, const std::size_t bytes, const DWORD flags) const noexcept
    {
        if (!Layout(address,bytes,flags)) return nullptr;
        const auto rva=address-0x400000;
        const auto* sections=IMAGE_FIRST_SECTION(pe_);
        for (WORD i=0; i<pe_->FileHeader.NumberOfSections; ++i)
        {
            const auto& section=sections[i];
            if ((section.Characteristics&flags)!=flags || rva<section.VirtualAddress) continue;
            const auto relative=rva-section.VirtualAddress;
            if (relative>section.SizeOfRawData || bytes>section.SizeOfRawData-relative) continue;
            const auto offset=static_cast<std::uint64_t>(section.PointerToRawData)+relative;
            if (offset>size_ || bytes>size_-offset) return nullptr;
            return view_+static_cast<SIZE_T>(offset);
        }
        return nullptr;
    }
    bool PinnedImage::Verify(const std::uint32_t address, const std::size_t bytes) const noexcept
    {
        const auto* pin=image_compatibility::FindCode(address,bytes);
        if (!pin) return Fail("uncatalogued native call/hook",address);
        const auto* source=Source(address,pin->size,IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_EXECUTE);
        return source && Same(source,pin->bytes.data(),pin->size) && Live(address,pin->bytes.data(),pin->size)
            ? true : Fail(pin->name,address);
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
