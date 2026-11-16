#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include "PrototypeProfile.h"
#include <array>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace winxcoop::launcher
{
    struct PrototypeFilePins final
    {
        HANDLE executable=INVALID_HANDLE_VALUE,adapter=INVALID_HANDLE_VALUE;
        ~PrototypeFilePins(){if(executable!=INVALID_HANDLE_VALUE)CloseHandle(executable);if(adapter!=INVALID_HANDLE_VALUE)CloseHandle(adapter);}
    };
    namespace
    {
        void Require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
        struct Handle final
        {
            HANDLE value=INVALID_HANDLE_VALUE;
            ~Handle() { if(value!=INVALID_HANDLE_VALUE && value) CloseHandle(value); }
        };
        std::wstring ModuleDirectory()
        {
            wchar_t path[MAX_PATH]{};const auto count=GetModuleFileNameW(nullptr,path,MAX_PATH);
            Require(count>0 && count<MAX_PATH,"Launcher path exceeds MAX_PATH");
            const std::wstring result(path,count);return result.substr(0,result.find_last_of(L"\\/"));
        }
        std::wstring Absolute(const std::wstring& value)
        {
            Require(!value.empty(),"Empty prototype path");
            const auto input=value.size()>2 && value[1]==L':'?value:ModuleDirectory()+L'\\'+value;
            wchar_t path[MAX_PATH]{};const auto count=GetFullPathNameW(input.c_str(),MAX_PATH,path,nullptr);
            Require(count>3 && count<MAX_PATH,"Prototype path exceeds MAX_PATH");
            std::wstring result(path,count);
            Require(result[1]==L':' && result[2]==L'\\',"Absolute drive paths required");
            for(const auto c:result) Require(c>=32 && c<=126 && c!=L'"',"Prototype helpers require ASCII paths without quotes");
            while(result.size()>3 && result.back()==L'\\')result.pop_back();return result;
        }
        std::wstring Ini(const std::wstring& path,const wchar_t* section,const wchar_t* key,const std::wstring& fallback)
        {
            wchar_t result[MAX_PATH]{};const auto count=GetPrivateProfileStringW(section,key,fallback.c_str(),result,MAX_PATH,path.c_str());
            Require(count<MAX_PATH-1,"Launcher INI value exceeds MAX_PATH");return std::wstring(result,count);
        }
        std::uint32_t Seconds(const std::wstring& text)
        {
            std::uint32_t value{};Require(!text.empty() && text.size()<5,"Prototype seconds must be 0 or 5..3600");
            for(const auto c:text){Require(c>=L'0' && c<=L'9',"Invalid prototype seconds");value=value*10+static_cast<unsigned>(c-L'0');}
            Require(value==0 || (value>=5 && value<=3600),"Prototype seconds must be 0 or 5..3600");return value;
        }
        void Ordinary(const std::wstring& path,const bool directory)
        {
            const auto attr=GetFileAttributesW(path.c_str());
            Require(attr!=INVALID_FILE_ATTRIBUTES && bool(attr&FILE_ATTRIBUTE_DIRECTORY)==directory && !(attr&FILE_ATTRIBUTE_REPARSE_POINT),
                "Installed game, media or helper path is missing or is a reparse point");
        }
        class Sha256 final
        {
        public:
            Sha256()
            {
                Require(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider unavailable");
                DWORD bytes{},got{};
                if(BCryptGetProperty(algorithm_,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&bytes),sizeof(bytes),&got,0)<0 || !bytes || bytes>65536)
                {BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("SHA256 object size invalid");}
                try{storage_.resize(bytes);}catch(...){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw;}
                if(BCryptCreateHash(algorithm_,&hash_,storage_.data(),bytes,nullptr,0,0)<0)
                {BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("SHA256 object unavailable");}
            }
            ~Sha256(){if(hash_)BCryptDestroyHash(hash_);if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);}
            void Add(const void* data,const std::size_t size)
            {Require(size<=MAXDWORD && BCryptHashData(hash_,static_cast<PUCHAR>(const_cast<void*>(data)),static_cast<ULONG>(size),0)>=0,"SHA256 update failed");}
            gameplay::Digest256 Finish()
            {gameplay::Digest256 result{};Require(BCryptFinishHash(hash_,result.data(),static_cast<ULONG>(result.size()),0)>=0,"SHA256 finish failed");return result;}
        private:
            BCRYPT_ALG_HANDLE algorithm_{};BCRYPT_HASH_HANDLE hash_{};std::vector<BYTE> storage_;
        };
        void NotCancelled(const std::atomic<bool>& cancel)
        {Require(!cancel.load(std::memory_order_relaxed),"Prototype qualification cancelled");}
        gameplay::Digest256 FileHash(const std::wstring& path,const std::atomic<bool>& cancel,std::uint64_t* fileBytes=nullptr,HANDLE* retainReadPin=nullptr)
        {
            NotCancelled(cancel);Ordinary(path,false);Handle file;
            file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
            Require(file.value!=INVALID_HANDLE_VALUE,"Cannot open qualification input for a stable read");
            LARGE_INTEGER size{};Require(GetFileSizeEx(file.value,&size) && size.QuadPart>=0,"Cannot read qualification input size");
            Sha256 hash;std::array<BYTE,65536> buffer{};std::uint64_t total{};
            for(;;)
            {
                NotCancelled(cancel);DWORD got{};Require(ReadFile(file.value,buffer.data(),static_cast<DWORD>(buffer.size()),&got,nullptr)!=0,"Qualification input read failed");
                if(!got)break;hash.Add(buffer.data(),got);total+=got;
            }
            Require(total==static_cast<std::uint64_t>(size.QuadPart),"Qualification input changed size during hashing");
            const auto digest=hash.Finish();if(fileBytes)*fileBytes=total;
            if(retainReadPin){*retainReadPin=file.value;file.value=INVALID_HANDLE_VALUE;}return digest;
        }
        gameplay::Digest256 OwnProtocolTag(const char* text,const std::size_t bytes)
        { Sha256 hash;hash.Add(text,bytes);return hash.Finish(); }
        HANDLE PinGameImage(const std::wstring& path,const std::atomic<bool>& cancel)
        {
            NotCancelled(cancel);Ordinary(path,false);Handle file;
            file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            Require(file.value!=INVALID_HANDLE_VALUE,"Cannot retain installed game source");
            LARGE_INTEGER size{};
            Require(GetFileSizeEx(file.value,&size) && size.QuadPart>=4096 && size.QuadPart<=64*1024*1024,"Installed game PE size exceeds reader bound");
            IMAGE_DOS_HEADER dos{};DWORD got{};
            Require(ReadFile(file.value,&dos,static_cast<DWORD>(sizeof(dos)),&got,nullptr) && got==sizeof(dos) &&
                dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>=0 &&
                static_cast<ULONGLONG>(dos.e_lfanew)+sizeof(IMAGE_NT_HEADERS32)<=static_cast<ULONGLONG>(size.QuadPart),"Installed game DOS/PE header bounds differ");
            LARGE_INTEGER at{};at.QuadPart=dos.e_lfanew;IMAGE_NT_HEADERS32 pe{};
            Require(SetFilePointerEx(file.value,at,nullptr,FILE_BEGIN) && ReadFile(file.value,&pe,static_cast<DWORD>(sizeof(pe)),&got,nullptr) && got==sizeof(pe) &&
                pe.Signature==IMAGE_NT_SIGNATURE && pe.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
                pe.FileHeader.SizeOfOptionalHeader==sizeof(IMAGE_OPTIONAL_HEADER32) &&
                pe.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC && pe.OptionalHeader.ImageBase==0x400000 &&
                pe.FileHeader.NumberOfSections>0 && pe.FileHeader.NumberOfSections<=96,
                "Installed game requires the native x86 PE32/base layout");
            NotCancelled(cancel);const auto result=file.value;file.value=INVALID_HANDLE_VALUE;return result;
        }
        PrototypeProfile Qualify(PrototypePaths paths,const std::atomic<bool>& cancel)
        {
            Require(!paths.gameRoot.empty(),"Select the installed game directory first");
            paths.gameRoot=Absolute(paths.gameRoot);
            if(paths.media.empty())paths.media=paths.gameRoot+L"\\Media";
            for(auto* path:{&paths.media,&paths.adapter,&paths.helper,&paths.caseRoot})*path=Absolute(*path);
            if(!paths.outer.empty())paths.outer=Absolute(paths.outer);
            Require((paths.menuFirst ? paths.seconds==0 || (paths.seconds>=5 && paths.seconds<=3600) :
                (paths.seconds>=5 && paths.seconds<=3600)) && paths.caseRoot.size()<MAX_PATH-80,
                "Prototype service/legacy duration or caseRoot exceeds its contract");
            Ordinary(paths.gameRoot,true);Ordinary(paths.media,true);Ordinary(paths.gameRoot+L"\\Shaders",true);
            Ordinary(paths.helper,false);
            if(!paths.menuFirst)Ordinary(paths.outer,false);
            PrototypeProfile profile;profile.paths=std::move(paths);profile.game=profile.paths.gameRoot+L"\\WinxClub.exe";
            const auto pins=std::make_shared<PrototypeFilePins>();profile.filePins=pins;
            pins->executable=PinGameImage(profile.game,cancel);
            profile.compatibility.adapter=FileHash(profile.paths.adapter,cancel,nullptr,&pins->adapter);
            // Kept wire fields describe our protocol/capabilities, not a digest
            // of the selected game EXE or resources. Typed native feature pins
            // are checked separately before the adapter uses them.
            constexpr char protocol[]="WinxCoop.PassiveCompanion.PoseProtocol.V3.v1\n";
            constexpr char visual[]="WinxCoop.PassiveCompanion.VisualGraph.BloomHairColor.IdleWalkJump.v1\n";
            constexpr char scenario[]="WinxCoop.PassiveCompanion.StockLocalPhysics.PassiveRemotePose.v1\n";
            profile.compatibility.executable=OwnProtocolTag(protocol,sizeof(protocol)-1);
            profile.level.levelId=0;profile.level.entryId=0; // No scene has been selected/observed yet.
            profile.level.resources=OwnProtocolTag(visual,sizeof(visual)-1);
            profile.level.scenario=OwnProtocolTag(scenario,sizeof(scenario)-1);
            NotCancelled(cancel);return profile;
        }
    }
    std::wstring ResolvePrototypePath(const std::wstring& path){return Absolute(path);}
    std::wstring LocalPrototypeDataDirectory()
    {
        wchar_t value[MAX_PATH]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",value,MAX_PATH);
        Require(count>0 && count<MAX_PATH,"Local application data is unavailable or exceeds MAX_PATH");
#if defined(_WIN64)
        constexpr wchar_t program[]=L"Windows-x64";
#else
        constexpr wchar_t program[]=L"Win7-x86";
#endif
        const auto directory=Absolute(std::wstring(value,count)+L"\\WinxCoop\\"+program);
        const auto drive=directory.substr(0,3);const auto type=GetDriveTypeW(drive.c_str());
        Require(type!=DRIVE_REMOTE && type!=DRIVE_UNKNOWN && type!=DRIVE_NO_ROOT_DIR,"Program data requires an available local drive");
        return directory;
    }
    PrototypePaths ReadPrototypePaths()
    {
        const auto base=ModuleDirectory();const auto ini=base+L"\\WinxCoopLauncher.ini";PrototypePaths result;
        result.gameRoot=Ini(ini,L"Paths",L"gameRoot",L"");
        if(!result.gameRoot.empty())result.gameRoot=Absolute(result.gameRoot);
        result.media=Ini(ini,L"Paths",L"media",result.gameRoot.empty()?L"":result.gameRoot+L"\\Media");
        if(!result.media.empty())result.media=Absolute(result.media);
        result.adapter=Absolute(Ini(ini,L"Paths",L"adapter",L"WinxCoop.NativeAdapter.dll"));
        result.helper=Absolute(Ini(ini,L"Paths",L"helper",L"WinxCoopGameHost.exe"));
        result.outer=Absolute(Ini(ini,L"Paths",L"outer",L"XpGameProbe.exe"));
        result.caseRoot=Absolute(Ini(ini,L"Paths",L"caseRoot",LocalPrototypeDataDirectory()+L"\\Sessions"));
        result.seconds=Seconds(Ini(ini,L"Prototype",L"seconds",L"0"));
        const auto menu=Ini(ini,L"Prototype",L"menuFirst",L"true");
        Require(menu==L"true" || menu==L"1" || menu==L"false" || menu==L"0","menuFirst must be true or false");
        result.menuFirst=menu==L"true" || menu==L"1";return result;
    }
    struct PrototypeProfileCheck::Impl final
    {
        std::atomic<bool> cancel{},done{};PrototypeProfile profile;std::string failure;bool successful{};
        PrototypePaths input;
        HANDLE worker=nullptr;
        explicit Impl(PrototypePaths paths):input(std::move(paths))
        {
            // The installed STL thread runtime imports a Windows 8 clock API.
            // Keep this read-only worker on the Windows 7 thread API instead.
            worker=CreateThread(nullptr,0,&ThreadEntry,this,0,nullptr);
            if(!worker)
            {
                const auto error=GetLastError();
                throw std::runtime_error("Cannot create profile worker: "+std::to_string(error));
            }
        }
        static DWORD WINAPI ThreadEntry(void* context) noexcept
        {
            static_cast<Impl*>(context)->Run();return 0;
        }
        void Run() noexcept
        {
            try{profile=Qualify(std::move(input),cancel);successful=true;}
            catch(const std::exception& error){failure=error.what();}
            catch(...){failure="Unknown profile qualification error";}
            done.store(true,std::memory_order_release);
        }
        ~Impl() noexcept
        {
            cancel.store(true,std::memory_order_relaxed);
            if(worker)
            {
                // Never release paths, errors or pinned profile handles while
                // the worker can still access them. An invalid own handle is
                // a fatal ownership failure, as a failed std::thread join was.
                if(WaitForSingleObject(worker,INFINITE)!=WAIT_OBJECT_0)std::terminate();
                if(!CloseHandle(worker))std::terminate();
            }
        }
    };
    PrototypeProfileCheck::PrototypeProfileCheck(PrototypePaths paths):implementation_(std::make_unique<Impl>(std::move(paths))){}
    PrototypeProfileCheck::~PrototypeProfileCheck()=default;
    void PrototypeProfileCheck::Cancel() noexcept{implementation_->cancel.store(true,std::memory_order_relaxed);}
    bool PrototypeProfileCheck::Finished() const noexcept{return implementation_->done.load(std::memory_order_acquire);}
    bool PrototypeProfileCheck::Successful() const noexcept{return Finished() && implementation_->successful;}
    const PrototypeProfile& PrototypeProfileCheck::Profile() const{Require(Successful(),"Profile not qualified");return implementation_->profile;}
    const std::string& PrototypeProfileCheck::Failure() const{Require(Finished(),"Profile check still running");return implementation_->failure;}
}
