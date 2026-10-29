#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include "PrototypeProfile.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <limits>
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
            std::uint32_t value{};Require(!text.empty() && text.size()<5,"Prototype seconds must be 5..3600");
            for(const auto c:text){Require(c>=L'0' && c<=L'9',"Invalid prototype seconds");value=value*10+static_cast<unsigned>(c-L'0');}
            Require(value>=5 && value<=3600,"Prototype seconds must be 5..3600");return value;
        }
        void Ordinary(const std::wstring& path,const bool directory)
        {
            const auto attr=GetFileAttributesW(path.c_str());
            Require(attr!=INVALID_FILE_ATTRIBUTES && bool(attr&FILE_ATTRIBUTE_DIRECTORY)==directory && !(attr&FILE_ATTRIBUTE_REPARSE_POINT),
                "Prepared game, media or helper path is missing or is a reparse point");
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
            void Integer(std::uint64_t value,const unsigned bytes)
            {std::array<std::uint8_t,8> wire{};for(unsigned i=0;i<bytes;++i){wire[i]=static_cast<std::uint8_t>(value);value>>=8;}Add(wire.data(),bytes);}
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
        std::string LowerAscii(std::string value)
        {for(auto& c:value)if(c>='A' && c<='Z')c=static_cast<char>(c+('a'-'A'));return value;}
        std::wstring LowerAscii(std::wstring value)
        {for(auto& c:value)if(c>=L'A' && c<=L'Z')c=static_cast<wchar_t>(c+(L'a'-L'A'));return value;}
        std::string Utf8(const std::wstring& value)
        {
            const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
            Require(count>0,"Invalid Media filename encoding");std::string result(static_cast<std::size_t>(count),'\0');
            Require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr)==count,"Media filename encoding failed");return result;
        }
        bool Volatile(const std::wstring& name,const bool directory)
        {
            const auto lower=LowerAscii(name);
            if(directory)return lower==L"saved" || lower==L"logs";
            const auto dot=lower.find_last_of(L'.');const auto extension=dot==std::wstring::npos?L"":lower.substr(dot);
            return extension==L".log" || extension==L".tmp" || extension==L".dmp";
        }
        struct MediaFile final {std::wstring path;std::string name;};
        void Inventory(const std::wstring& root,const std::wstring& relative,std::vector<MediaFile>& files,const std::atomic<bool>& cancel,const unsigned depth)
        {
            NotCancelled(cancel);Require(depth<=64,"Media tree is too deep");
            const auto folder=relative.empty()?root:root+L'\\'+relative;Ordinary(folder,true);
            WIN32_FIND_DATAW entry{};const auto search=FindFirstFileW((folder+L"\\*").c_str(),&entry);
            if(search==INVALID_HANDLE_VALUE){Require(GetLastError()==ERROR_FILE_NOT_FOUND,"Media inventory failed");return;}
            struct Search final {HANDLE handle;~Search(){FindClose(handle);}} close{search};
            for(;;)
            {
                NotCancelled(cancel);const std::wstring name(entry.cFileName);const bool directory=(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
                if(name!=L"." && name!=L".." && !Volatile(name,directory))
                {
                    Require(!(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"Media reparse points are not qualified");
                    const auto next=relative.empty()?name:relative+L'\\'+name;
                    if(directory)Inventory(root,next,files,cancel,depth+1);
                    else
                    {
                        Require(files.size()<200000,"Media inventory exceeds 200000 files");
                        auto key=Utf8(next);for(auto& c:key)if(c=='\\')c='/';
                        files.push_back({root+L'\\'+next,LowerAscii(std::move(key))});
                    }
                }
                if(!FindNextFileW(search,&entry)){Require(GetLastError()==ERROR_NO_MORE_FILES,"Media inventory was interrupted");break;}
            }
        }
        gameplay::Digest256 MediaHash(const std::wstring& root,const std::atomic<bool>& cancel,std::uint64_t& count,std::uint64_t& bytes)
        {
            std::vector<MediaFile> files;Inventory(root,L"",files,cancel,0);Require(!files.empty(),"Prepared Media tree is empty");
            std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.name<b.name;});
            Sha256 hash;constexpr char domain[]="WinxCoop.MediaTree.v1\n";hash.Add(domain,sizeof(domain)-1);
            hash.Integer(files.size(),8);std::string previous;
            for(const auto& file:files)
            {
                Require(file.name!=previous,"Media filenames alias after ASCII case normalization");previous=file.name;
                std::uint64_t size{};const auto digest=FileHash(file.path,cancel,&size);
                Require(size<=std::numeric_limits<std::uint64_t>::max()-bytes,"Media byte count overflow");bytes+=size;
                hash.Integer(file.name.size(),4);hash.Add(file.name.data(),file.name.size());hash.Integer(size,8);hash.Add(digest.data(),digest.size());
            }
            count=files.size();return hash.Finish();
        }
        gameplay::Digest256 ExpectedExecutable()
        {
            constexpr char hex[]="3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F";
            const auto digit=[](char c){return c<='9'?c-'0':c-'A'+10;};gameplay::Digest256 result{};
            for(std::size_t i=0;i<result.size();++i)result[i]=static_cast<std::uint8_t>((digit(hex[i*2])<<4)|digit(hex[i*2+1]));return result;
        }
        std::string Trim(std::string text)
        {const auto begin=text.find_first_not_of(" \t\r");if(begin==std::string::npos)return {};return text.substr(begin,text.find_last_not_of(" \t\r")-begin+1);}
        void CheckStartLevel(const std::wstring& path)
        {
            Ordinary(path,false);Handle file;file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
            Require(file.value!=INVALID_HANDLE_VALUE,"Prepared winx.ini unavailable");LARGE_INTEGER size{};
            Require(GetFileSizeEx(file.value,&size) && size.QuadPart>0 && size.QuadPart<=65536,"Prepared winx.ini size is invalid");
            std::string text(static_cast<std::size_t>(size.QuadPart),'\0');DWORD got{};
            Require(ReadFile(file.value,text.data(),static_cast<DWORD>(text.size()),&got,nullptr) && got==text.size(),"Prepared winx.ini read failed");
            if(text.compare(0,3,"\xEF\xBB\xBF")==0)text.erase(0,3);
            bool section=false,found=false;std::size_t begin{};
            while(begin<text.size())
            {
                const auto end=text.find('\n',begin);auto line=Trim(text.substr(begin,end==std::string::npos?end:end-begin));begin=end==std::string::npos?text.size():end+1;
                if(line.empty() || line[0]==';' || line[0]=='#')continue;
                if(line[0]=='['){section=true;continue;}
                const auto equals=line.find('=');if(equals==std::string::npos)continue;
                if(LowerAscii(Trim(line.substr(0,equals)))!="startlevel")continue;
                Require(!section && !found,"startLevel must occur exactly once before any section in winx.ini");
                auto value=Trim(line.substr(equals+1));const auto comment=value.find_first_of(";#");if(comment!=std::string::npos)value=Trim(value.substr(0,comment));
                Require(value=="28","Stage P requires an already prepared sectionless startLevel=28");found=true;
            }
            Require(found,"Stage P requires an already prepared sectionless startLevel=28; the launcher does not edit winx.ini");
        }
        PrototypeProfile Qualify(PrototypePaths paths,const std::atomic<bool>& cancel)
        {
            for(auto* path:{&paths.gameRoot,&paths.media,&paths.adapter,&paths.helper,&paths.outer,&paths.caseRoot})*path=Absolute(*path);
            Require(paths.seconds>=5 && paths.seconds<=3600 && paths.caseRoot.size()<MAX_PATH-80,"Prototype duration or caseRoot exceeds the bounded contract");
            Ordinary(paths.gameRoot,true);Ordinary(paths.media,true);Ordinary(paths.helper,false);Ordinary(paths.outer,false);
            PrototypeProfile profile;profile.paths=std::move(paths);profile.game=profile.paths.gameRoot+L"\\WinxClub.exe";profile.ini=profile.paths.gameRoot+L"\\winx.ini";
            const auto pins=std::make_shared<PrototypeFilePins>();profile.filePins=pins;
            CheckStartLevel(profile.ini);profile.compatibility.executable=FileHash(profile.game,cancel,nullptr,&pins->executable);
            Require(profile.compatibility.executable==ExpectedExecutable(),"Unsupported original WinxClub.exe SHA256");
            profile.compatibility.adapter=FileHash(profile.paths.adapter,cancel,nullptr,&pins->adapter);
            profile.level.levelId=28;profile.level.entryId=0;
            profile.level.resources=MediaHash(profile.paths.media,cancel,profile.mediaFiles,profile.mediaBytes);
            // Explicit situation contract. This digest is independent of paths,
            // timestamps and PC identity; assets and full DLL have their own SHA.
            constexpr char scenario[]="WinxCoop.StageP.Alfea28.entry0.v3\nstartLevel=28\nstock-local-movement\nhost-observation-decisions\npassive-remote-root\nno-level-transitions\n";
            Sha256 hash;hash.Add(scenario,sizeof(scenario)-1);profile.level.scenario=hash.Finish();NotCancelled(cancel);return profile;
        }
    }
    std::wstring ResolvePrototypePath(const std::wstring& path){return Absolute(path);}
    PrototypePaths ReadPrototypePaths()
    {
        const auto base=ModuleDirectory();const auto ini=base+L"\\WinxCoopLauncher.ini";PrototypePaths result;
        result.gameRoot=Absolute(Ini(ini,L"Paths",L"gameRoot",L"PreparedGame"));
        result.media=Absolute(Ini(ini,L"Paths",L"media",result.gameRoot+L"\\Media"));
        result.adapter=Absolute(Ini(ini,L"Paths",L"adapter",L"WinxCoop.NativeAdapter.dll"));
        result.helper=Absolute(Ini(ini,L"Paths",L"helper",L"WinxCoopGameHost.exe"));
        result.outer=Absolute(Ini(ini,L"Paths",L"outer",L"XpGameProbe.exe"));
        result.caseRoot=Absolute(Ini(ini,L"Paths",L"caseRoot",L"Cases"));
        result.seconds=Seconds(Ini(ini,L"Prototype",L"seconds",L"900"));return result;
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
