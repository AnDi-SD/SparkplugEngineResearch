// Native Win32 Stage P launcher. A carrier handshake does not launch a guest:
// only a compatible Welcome naming the host's actual ready scene may do so.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include "LobbyDiscovery.h"
#include "PrototypeProfile.h"
#include "ReplicationCoordinator.h"
#include "LaunchText.h"
#include "Networking/WindowsTcpTransport.h"
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <algorithm>

namespace winxcoop::launcher
{
    using namespace networking;
    namespace
    {
        constexpr wchar_t WindowClass[]=L"WinxCoop.LobbyShell.v1";
        constexpr UINT_PTR PumpTimer=1;
        std::string JsonString(const std::string& value)
        {
            constexpr char hex[]="0123456789abcdef";
            std::string result="\"";
            for(const unsigned char c:value)
            {
                if(c=='\"' || c=='\\'){result+='\\';result+=static_cast<char>(c);}
                else if(c<0x20){result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
                else result+=static_cast<char>(c);
            }
            result+='\"';return result;
        }
        enum Control : int { Port=101, Address, Lobbies, Host, Find, Join, Start, Leave, Status, GameRoot, BrowseGame, OpenLogs, PlainGame,
            Disconnect, Language, Title=200, Subtitle, LanguageLabel, CommonGroup, GamePathLabel, GameHint,
            ServerGroup, ServerHint, PortLabel, ClientGroup, ClientHint, AddressLabel, LobbyListLabel, LobbyListHint, StatusGroup };
        enum class LanguagePreference { System, Russian, English };
        constexpr UiLanguage ResolveLanguage(const LanguagePreference preference,const LANGID systemLanguage) noexcept
        {
            return preference==LanguagePreference::Russian?UiLanguage::Russian:
                preference==LanguagePreference::English?UiLanguage::English:
                (systemLanguage&0x3ff)==LANG_RUSSIAN?UiLanguage::Russian:UiLanguage::English;
        }
        static_assert(ResolveLanguage(LanguagePreference::System,0x0419)==UiLanguage::Russian);
        static_assert(ResolveLanguage(LanguagePreference::System,0x0409)==UiLanguage::English);
        static_assert(ResolveLanguage(LanguagePreference::System,0x0411)==UiLanguage::English);
        static_assert(ResolveLanguage(LanguagePreference::English,0x0419)==UiLanguage::English);
        static_assert(ResolveLanguage(LanguagePreference::Russian,0x0409)==UiLanguage::Russian);
        struct Options { std::uint16_t port=27025; std::wstring report; PrototypePaths paths; };
        std::optional<std::uint32_t> Number(const std::wstring& text,const std::uint32_t maximum)
        {
            if(text.empty()) return std::nullopt;
            std::uint32_t value=0;
            for(const auto c:text)
            {
                if(c<L'0' || c>L'9') return std::nullopt;
                const auto digit=static_cast<std::uint32_t>(c-L'0');
                if(digit>maximum || value>(maximum-digit)/10) return std::nullopt;
                value=value*10+digit;
            }
            return value;
        }
        std::optional<Endpoint> ParseEndpoint(const std::wstring& text)
        {
            const auto colon=text.find(L':'); if(colon==std::wstring::npos) return std::nullopt;
            const auto ip=text.substr(0,colon); std::uint32_t address=0; std::size_t begin=0;
            for(unsigned i=0;i<4;++i)
            {
                const auto end=i==3?ip.size():ip.find(L'.',begin);
                if(end==std::wstring::npos || end<begin) return std::nullopt;
                const auto octet=Number(ip.substr(begin,end-begin),255); if(!octet) return std::nullopt;
                address=(address<<8)|*octet; begin=end+1;
            }
            const auto port=Number(text.substr(colon+1),65535);
            if(!port || !*port || !address || address==0xffffffffu) return std::nullopt;
            return Endpoint{address,static_cast<std::uint16_t>(*port)};
        }
        std::wstring EndpointText(const Endpoint e)
        { return std::to_wstring(e.address>>24)+L'.'+std::to_wstring((e.address>>16)&255)+L'.'+std::to_wstring((e.address>>8)&255)+L'.'+std::to_wstring(e.address&255)+L':'+std::to_wstring(e.port); }
        Options ParseOptions()
        {
            int count=0; wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);
            if(!arguments) throw std::runtime_error("Command line unavailable");
            struct FreeArgs { wchar_t** p; ~FreeArgs(){LocalFree(p);} } release{arguments};
            Options options;options.paths=ReadPrototypePaths();bool gotPort=false,gotReport=false,gotRoot=false,gotMedia=false;
            const auto settings=LocalPrototypeDataDirectory()+L"\\launcher-settings.ini";
            wchar_t remembered[MAX_PATH]{};
            if(GetPrivateProfileStringW(L"Paths",L"gameRoot",L"",remembered,MAX_PATH,settings.c_str())>0)
            {options.paths.gameRoot=ResolvePrototypePath(remembered);options.paths.media=options.paths.gameRoot+L"\\Media";}
            std::array<bool,6> gotPath{};bool gotSeconds=false;
            for(int i=1;i<count;++i)
            {
                const std::wstring key=arguments[i];
                if(i+1==count) throw std::invalid_argument("Expected an option followed by its value");
                const std::wstring value=arguments[++i];
                if(key==L"--port" && !gotPort)
                { const auto port=Number(value,65535); if(!port || !*port) throw std::invalid_argument("Invalid port"); options.port=static_cast<std::uint16_t>(*port); gotPort=true; }
                else if(key==L"--report" && !gotReport && !value.empty() && value.size()<MAX_PATH)
                { options.report=value; gotReport=true; }
                else if(key==L"--seconds" && !gotSeconds)
                {const auto seconds=Number(value,3600);if(!seconds || (*seconds && *seconds<5))throw std::invalid_argument("Seconds must be0 (normal) or5..3600 (diagnostic)");options.paths.seconds=*seconds;options.paths.menuFirst=*seconds==0;gotSeconds=true;}
                else
                {
                    const std::array<const wchar_t*,6> names{{L"--game-root",L"--media",L"--adapter",L"--helper",L"--outer",L"--case-root"}};
                    const std::array<std::wstring*,6> paths{{&options.paths.gameRoot,&options.paths.media,&options.paths.adapter,&options.paths.helper,&options.paths.outer,&options.paths.caseRoot}};
                    bool found=false;
                    for(std::size_t p=0;p<names.size();++p)if(key==names[p])
                    {if(gotPath[p])throw std::invalid_argument("Repeated path option");*paths[p]=ResolvePrototypePath(value);gotPath[p]=true;found=true;if(p==0)gotRoot=true;if(p==1)gotMedia=true;break;}
                    if(!found)throw std::invalid_argument("Unknown or repeated argument");
                }
            }
            if(gotRoot && !gotMedia)options.paths.media=options.paths.gameRoot+L"\\Media";
            return options;
        }
        std::wstring WideError(const char* text,const UiLanguage language=UiLanguage::English)
        {
            if(!text) return Text(TextId::UnknownError,language);
            const auto size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);
            if(size<=0 || size>2048) return Text(TextId::ApplicationError,language);
            std::wstring result(static_cast<std::size_t>(size),L'\0');
            MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,result.data(),size); result.pop_back(); return result;
        }
        struct Event { std::uint64_t time=0,connection=0; unsigned code=0; int detail=0; };
        class LobbyWindow final
        {
        public:
            explicit LobbyWindow(Options options):options_(std::move(options))
            {
                localDirectory_=LocalPrototypeDataDirectory();
                wchar_t rememberedLanguage[16]{};
                GetPrivateProfileStringW(L"Interface",L"language",L"system",rememberedLanguage,16,(localDirectory_+L"\\launcher-settings.ini").c_str());
                languagePreference_=lstrcmpW(rememberedLanguage,L"ru")==0?LanguagePreference::Russian:
                    lstrcmpW(rememberedLanguage,L"en")==0?LanguagePreference::English:LanguagePreference::System;
                language_=ResolveLanguage(languagePreference_,GetUserDefaultUILanguage());
                const auto made=SHCreateDirectoryExW(nullptr,(localDirectory_+L"\\Sessions").c_str(),nullptr);
                if(made!=ERROR_SUCCESS && made!=ERROR_ALREADY_EXISTS && made!=ERROR_FILE_EXISTS)throw std::runtime_error("Cannot create local WinxCoop logs/sessions");
                const auto logPath=localDirectory_+L"\\launcher.log";
                log_=CreateFileW(logPath.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(log_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open local WinxCoop launcher.log");
                Log("launcher-opened",options_.paths.gameRoot);
                if(!options_.paths.gameRoot.empty())profileCheck_=std::make_unique<PrototypeProfileCheck>(options_.paths);
                if(!options_.report.empty())
                {
                    report_=CreateFileW(options_.report.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                    if(report_==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create a fresh report. Choose a new --report path.");
                }
            }
            ~LobbyWindow(){Close();if(font_)DeleteObject(font_);if(titleFont_)DeleteObject(titleFont_);}
            int Run(HINSTANCE instance,const int show)
            {
                instance_=instance;
                const auto screen=GetDC(nullptr);dpi_=screen?GetDeviceCaps(screen,LOGPIXELSX):96;if(screen)ReleaseDC(nullptr,screen);
                font_=CreateFontW(-MulDiv(9,dpi_,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
                titleFont_=CreateFontW(-MulDiv(16,dpi_,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
                WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance; wc.lpfnWndProc=&Procedure;
                wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1); wc.lpszClassName=WindowClass;
                if(!RegisterClassExW(&wc)) throw std::runtime_error("Cannot register lobby window");
                RECT initial{0,0,Scale(930),Scale(700)};AdjustWindowRectEx(&initial,WS_OVERLAPPEDWINDOW,FALSE,0);
                window_=CreateWindowExW(0,WindowClass,Text(TextId::WindowTitle,language_),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                    CW_USEDEFAULT,CW_USEDEFAULT,initial.right-initial.left,initial.bottom-initial.top,nullptr,nullptr,instance,this);
                if(!window_) throw std::runtime_error("Cannot create lobby window");
                ShowWindow(window_,show); UpdateWindow(window_);
                MSG message{}; int result=0;
                while((result=GetMessageW(&message,nullptr,0,0))>0)
                { if(!IsDialogMessageW(window_,&message)) { TranslateMessage(&message); DispatchMessageW(&message); } }
                if(result<0) { fatal_=true; Record(99,static_cast<int>(GetLastError())); }
                Close(); // Finalize cleanup before choosing the process exit code.
                return fatal_?1:0;
            }
        private:
            Options options_;
            HINSTANCE instance_=nullptr;
            HWND window_=nullptr,port_=nullptr,address_=nullptr,list_=nullptr,status_=nullptr,gameRoot_=nullptr,languageBox_=nullptr;
            HFONT font_=nullptr,titleFont_=nullptr;
            int dpi_=96;
            LanguagePreference languagePreference_=LanguagePreference::System;
            UiLanguage language_=UiLanguage::English;
            HANDLE report_=INVALID_HANDLE_VALUE;
            HANDLE log_=INVALID_HANDLE_VALUE;
            std::wstring localDirectory_;
            UiStatus lastStatus_{TextId::InitialStatus};
            bool launchAfterQualification_=false;
            LobbyDiscovery discovery_;
            std::unique_ptr<WindowsTcpTransport> transport_;
            std::unique_ptr<PrototypeProfileCheck> profileCheck_;
            std::unique_ptr<ReplicationCoordinator> coordinator_;
            UiStatus lastCoordinatorStatus_{};
            bool profileHandled_=false,closing_=false,isHost_=false,closed_=false,fatal_=false,discoveryUnavailable_=false,cleanupConfirmed_=false;
            std::uint64_t started_=GetTickCount64(),discoveryRevision_=0,lastConnection_=0;
            TcpTransportState lastState_=TcpTransportState::Idle;
            SessionPhase lastPhase_=SessionPhase::Idle;
            std::array<Event,64> events_{};
            std::size_t eventCount_=0,droppedEvents_=0;
            TcpTransportMetrics totals_{};

            void Record(const unsigned code,const int detail=0,const std::uint64_t connection=0) noexcept
            {
                Log("event-"+std::to_string(code),std::to_wstring(detail)+L" / "+std::to_wstring(connection));
                if(eventCount_==events_.size()) { ++droppedEvents_; return; }
                events_[eventCount_++]={GetTickCount64()-started_,connection,code,detail};
            }
            void OpenDefaultReport()
            {
                if(report_!=INVALID_HANDLE_VALUE)return;
                const auto& root=profileCheck_->Profile().paths.caseRoot;const auto attr=GetFileAttributesW(root.c_str());
                if(attr==INVALID_FILE_ATTRIBUTES)
                {if(!CreateDirectoryW(root.c_str(),nullptr))throw std::runtime_error("Cannot create launcher caseRoot for the report");}
                else if(!(attr&FILE_ATTRIBUTE_DIRECTORY) || (attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Launcher caseRoot is not an ordinary directory");
                const auto path=root+L"\\launcher-"+std::to_wstring(GetCurrentProcessId())+L'-'+std::to_wstring(GetTickCount64())+L".json";
                report_=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(report_==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create a fresh launcher metrics report");
            }
            void Log(const std::string& event,const std::wstring& detail=L"") noexcept
            {
                if(log_==INVALID_HANDLE_VALUE)return;
                try
                {
                    LARGE_INTEGER length{};
                    if(GetFileSizeEx(log_,&length) && length.QuadPart>16*1024*1024)
                    {
                        CloseHandle(log_);log_=INVALID_HANDLE_VALUE;
                        const auto path=localDirectory_+L"\\launcher.log";
                        MoveFileW(path.c_str(),(localDirectory_+L"\\launcher-"+std::to_wstring(GetCurrentProcessId())+L'-'+std::to_wstring(GetTickCount64())+L".log").c_str());
                        log_=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                        if(log_==INVALID_HANDLE_VALUE)return;
                    }
                    const auto size=WideCharToMultiByte(CP_UTF8,0,detail.data(),static_cast<int>(detail.size()),nullptr,0,nullptr,nullptr);
                    std::string utf8(size,'\0');if(size)WideCharToMultiByte(CP_UTF8,0,detail.data(),static_cast<int>(detail.size()),utf8.data(),size,nullptr,nullptr);
                    const auto line="{\"timeMs\":"+std::to_string(GetTickCount64())+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"event\":"+JsonString(event)+",\"detail\":"+JsonString(utf8)+"}\n";
                    DWORD written{};WriteFile(log_,line.data(),static_cast<DWORD>(line.size()),&written,nullptr);
                }catch(...){}
            }
            void SetStatus(const UiStatus& text)
            {
                if(lastStatus_==text)return;lastStatus_=text;
                // Log stable English diagnostics; switching language only re-renders the UI.
                Log("status",Render(text,UiLanguage::English));
                if(status_)SetWindowTextW(status_,Render(text,language_).c_str());
            }
            void SetStatus(const TextId key){SetStatus(UiStatus{key});}
            int Scale(const int value) const noexcept {return MulDiv(value,dpi_,96);}
            void ApplyLanguage()
            {
                SetWindowTextW(window_,Text(TextId::WindowTitle,language_));
                const std::pair<int,TextId> labels[]{
                    {Title,TextId::AppTitle},{Subtitle,TextId::AppSubtitle},{LanguageLabel,TextId::LanguageLabel},
                    {CommonGroup,TextId::CommonGroup},{GamePathLabel,TextId::GamePathLabel},{BrowseGame,TextId::BrowseGame},
                    {OpenLogs,TextId::OpenLogs},{PlainGame,TextId::PlainGame},{GameHint,TextId::GameHint},
                    {ServerGroup,TextId::ServerGroup},{ServerHint,TextId::ServerHint},{PortLabel,TextId::PortLabel},
                    {Host,TextId::CreateLobby},{Start,TextId::StartGame},{Leave,TextId::CloseLobby},
                    {ClientGroup,TextId::ClientGroup},{ClientHint,TextId::ClientHint},{AddressLabel,TextId::AddressLabel},
                    {Join,TextId::JoinLobby},{Disconnect,TextId::Disconnect},{Find,TextId::FindLobbies},
                    {LobbyListLabel,TextId::LobbyListLabel},{LobbyListHint,TextId::LobbyListHint},{StatusGroup,TextId::StatusGroup}};
                for(const auto& label:labels)SetWindowTextW(GetDlgItem(window_,label.first),Text(label.second,language_));
                SendMessageW(languageBox_,CB_RESETCONTENT,0,0);
                for(const auto key:{TextId::LanguageSystem,TextId::LanguageRussian,TextId::LanguageEnglish})
                    SendMessageW(languageBox_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(Text(key,language_)));
                SendMessageW(languageBox_,CB_SETCURSEL,static_cast<WPARAM>(languagePreference_),0);
                SetWindowTextW(status_,Render(lastStatus_,language_).c_str());
                RefreshList();
            }
            void ChangeLanguage()
            {
                const auto selected=SendMessageW(languageBox_,CB_GETCURSEL,0,0);
                if(selected<0 || selected>2)return;
                languagePreference_=static_cast<LanguagePreference>(selected);
                language_=ResolveLanguage(languagePreference_,GetUserDefaultUILanguage());
                const auto preference=languagePreference_==LanguagePreference::Russian?L"ru":
                    languagePreference_==LanguagePreference::English?L"en":L"system";
                if(!WritePrivateProfileStringW(L"Interface",L"language",preference,(localDirectory_+L"\\launcher-settings.ini").c_str()))
                    Log("language-save-failed",std::to_wstring(GetLastError()));
                Log("language-selected",preference);ApplyLanguage();
            }
            void BeginProfileCheck(const bool launch)
            {
                if(transport_ || (coordinator_ && (coordinator_->GameActive() || !coordinator_->CleanupConfirmed())) || (profileCheck_ && !profileCheck_->Finished()))return;
                const auto selected=ControlText(gameRoot_);
                if(selected.empty()){SetStatus(TextId::SelectGame);return;}
                options_.paths.gameRoot=ResolvePrototypePath(selected);options_.paths.media=options_.paths.gameRoot+L"\\Media";
                coordinator_.reset();profileCheck_.reset();profileHandled_=false;launchAfterQualification_=launch;
                profileCheck_=std::make_unique<PrototypeProfileCheck>(options_.paths);
                Log("game-selected",options_.paths.gameRoot);
                WritePrivateProfileStringW(L"Paths",L"gameRoot",options_.paths.gameRoot.c_str(),(localDirectory_+L"\\launcher-settings.ini").c_str());
                SetStatus(TextId::CheckingGame);UpdateButtons();
            }
            void BrowseInstalledGame()
            {
                wchar_t selected[MAX_PATH]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
                dialog.lpstrFilter=L"Winx Club (WinxClub.exe)\0WinxClub.exe\0";dialog.lpstrFile=selected;dialog.nMaxFile=MAX_PATH;
                dialog.lpstrTitle=Text(TextId::SelectGameDialog,language_);dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
                if(!GetOpenFileNameW(&dialog))return;
                std::wstring path=selected;path=path.substr(0,path.find_last_of(L"\\/"));SetWindowTextW(gameRoot_,path.c_str());
                try{BeginProfileCheck(false);}catch(const std::exception& error){SetStatus({TextId::Diagnostic,WideError(error.what(),language_)});Log("selection-failed",WideError(error.what()));}
            }
            HWND Child(const wchar_t* type,const wchar_t* text,const DWORD style,const int id,const int x,const int y,const int width,const int height)
            {
                const auto child=CreateWindowExW(lstrcmpW(type,L"EDIT")==0?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
                if(!child) throw std::runtime_error("Cannot create lobby control");
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font_?font_:GetStockObject(DEFAULT_GUI_FONT)),TRUE); return child;
            }
            void CreateControls()
            {
                // Logical positions are assigned together by LayoutControls; creation order is tab order.
                Child(L"STATIC",L"",0,Title,0,0,0,0);
                SendMessageW(GetDlgItem(window_,Title),WM_SETFONT,reinterpret_cast<WPARAM>(titleFont_?titleFont_:font_),TRUE);
                Child(L"STATIC",L"",0,Subtitle,0,0,0,0);
                Child(L"STATIC",L"",0,LanguageLabel,0,0,0,0);
                languageBox_=Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Language,0,0,0,0);
                Child(L"BUTTON",L"",BS_GROUPBOX,CommonGroup,0,0,0,0);
                Child(L"STATIC",L"",0,GamePathLabel,0,0,0,0);
                gameRoot_=Child(L"EDIT",options_.paths.gameRoot.c_str(),ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,GameRoot,0,0,0,0);
                SendMessageW(gameRoot_,EM_SETLIMITTEXT,MAX_PATH-1,0);
                Child(L"BUTTON",L"",WS_TABSTOP,BrowseGame,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,OpenLogs,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,PlainGame,0,0,0,0);
                Child(L"STATIC",L"",0,GameHint,0,0,0,0);
                Child(L"BUTTON",L"",BS_GROUPBOX,ServerGroup,0,0,0,0);
                Child(L"STATIC",L"",0,ServerHint,0,0,0,0);
                Child(L"STATIC",L"",0,PortLabel,0,0,0,0);
                port_=Child(L"EDIT",std::to_wstring(options_.port).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,Port,0,0,0,0);
                SendMessageW(port_,EM_SETLIMITTEXT,5,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Host,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Start,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Leave,0,0,0,0);
                Child(L"BUTTON",L"",BS_GROUPBOX,ClientGroup,0,0,0,0);
                Child(L"STATIC",L"",0,ClientHint,0,0,0,0);
                Child(L"STATIC",L"",0,AddressLabel,0,0,0,0);
                address_=Child(L"EDIT",EndpointText({0x7f000001u,options_.port}).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,Address,0,0,0,0);
                SendMessageW(address_,EM_SETLIMITTEXT,63,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Join,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Disconnect,0,0,0,0);
                Child(L"STATIC",L"",0,LobbyListLabel,0,0,0,0);
                Child(L"BUTTON",L"",WS_TABSTOP,Find,0,0,0,0);
                list_=Child(L"LISTBOX",L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_HSCROLL|WS_BORDER|WS_TABSTOP,Lobbies,0,0,0,0);
                Child(L"STATIC",L"",0,LobbyListHint,0,0,0,0);
                Child(L"BUTTON",L"",BS_GROUPBOX,StatusGroup,0,0,0,0);
                status_=Child(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,Status,0,0,0,0);
                ApplyLanguage();LayoutControls();UpdateButtons();
                if(!SetTimer(window_,PumpTimer,20,nullptr)) throw std::runtime_error("Cannot start network pump");
            }
            void LayoutControls()
            {
                if(!list_)return;
                RECT client{};GetClientRect(window_,&client);
                const int width=MulDiv(client.right,96,dpi_),height=MulDiv(client.bottom,96,dpi_);
                const int half=(width-48)/2,right=32+half,roleTop=218,roleHeight=height-roleTop-108;
                const auto move=[&](const int id,const int x,const int y,const int w,const int h)
                {MoveWindow(GetDlgItem(window_,id),Scale(x),Scale(y),Scale((std::max)(1,w)),Scale((std::max)(1,h)),TRUE);};
                move(Title,20,10,width-310,30);move(Subtitle,20,44,width-40,22);
                move(LanguageLabel,width-272,18,76,22);move(Language,width-192,13,172,180);
                move(CommonGroup,16,74,width-32,130);move(GamePathLabel,32,100,142,24);
                move(GameRoot,174,96,width-338,28);move(BrowseGame,width-152,95,120,30);
                move(OpenLogs,32,150,156,30);move(PlainGame,200,150,156,30);
                move(GameHint,374,140,width-408,48);
                move(ServerGroup,16,roleTop,half,roleHeight);move(ClientGroup,right,roleTop,half,roleHeight);
                move(ServerHint,32,roleTop+28,half-32,52);move(ClientHint,right+16,roleTop+28,half-32,52);
                move(PortLabel,32,roleTop+94,130,24);move(Port,174,roleTop+90,100,28);
                move(Host,32,roleTop+138,half-32,34);move(Start,32,roleTop+184,half-32,34);
                move(Leave,32,roleTop+230,half-32,34);
                move(AddressLabel,right+16,roleTop+86,half-32,24);move(Address,right+16,roleTop+110,half-32,28);
                move(Join,right+16,roleTop+148,(half-44)/2,32);move(Disconnect,right+28+(half-44)/2,roleTop+148,(half-44)/2,32);
                move(LobbyListLabel,right+16,roleTop+198,half-178,24);move(Find,right+half-154,roleTop+192,138,32);
                move(Lobbies,right+16,roleTop+234,half-32,roleHeight-282);
                move(LobbyListHint,right+16,roleTop+roleHeight-40,half-32,34);
                move(StatusGroup,16,height-94,width-32,78);move(Status,32,height-68,width-64,48);
            }
            void UpdateButtons()
            {
                const auto active=transport_!=nullptr;
                const bool selectedMatches=ControlText(gameRoot_)==options_.paths.gameRoot;
                EnableWindow(GetDlgItem(window_,Host),!closing_ && !active && selectedMatches && coordinator_ && coordinator_->CanAttach(SessionRole::Host));
                EnableWindow(GetDlgItem(window_,Join),!closing_ && !active && selectedMatches && coordinator_ && coordinator_->CanAttach(SessionRole::Guest));
                EnableWindow(port_,!closing_ && !active && (!coordinator_ || coordinator_->CanAttach(SessionRole::Host)));
                EnableWindow(GetDlgItem(window_,Leave),!closing_ && active && isHost_);
                EnableWindow(GetDlgItem(window_,Disconnect),!closing_ && active && !isHost_);
                const bool canSearch=!closing_ && !active && (!coordinator_ || coordinator_->CanAttach(SessionRole::Guest));
                EnableWindow(address_,canSearch);EnableWindow(list_,canSearch);EnableWindow(GetDlgItem(window_,Find),canSearch);
                const bool editable=!closing_ && !active && (!coordinator_ || (!coordinator_->GameActive() && coordinator_->CleanupConfirmed())) && (!profileCheck_ || profileCheck_->Finished());
                EnableWindow(gameRoot_,editable);EnableWindow(GetDlgItem(window_,BrowseGame),editable);
                EnableWindow(GetDlgItem(window_,PlainGame),editable && !ControlText(gameRoot_).empty());
                EnableWindow(languageBox_,!closing_);EnableWindow(GetDlgItem(window_,OpenLogs),!closing_);
                EnableWindow(GetDlgItem(window_,Start),!closing_ && ((coordinator_ && coordinator_->CanStartHostGame()) || (!coordinator_ && editable)));
            }
            std::wstring ControlText(HWND control) const
            { std::array<wchar_t,MAX_PATH> value{}; GetWindowTextW(control,value.data(),static_cast<int>(value.size())); return value.data(); }
            void HostLobby()
            {
                if(ControlText(gameRoot_)!=options_.paths.gameRoot){BeginProfileCheck(false);return;}
                if(transport_ || !coordinator_ || !coordinator_->CanAttach(SessionRole::Host)) return;
                const auto port=Number(ControlText(port_),65535);
                if(!port || !*port) { SetStatus(TextId::InvalidPort); return; }
                TcpTransportConfig config; config.role=SessionRole::Host; config.localBind={0,static_cast<std::uint16_t>(*port)};
                auto candidate=std::make_unique<WindowsTcpTransport>(config); const auto now=GetTickCount64();
                if(!candidate->Start(now)) { Record(11,candidate->LastSocketError()); SetStatus({TextId::TcpOpenFailed,std::to_wstring(candidate->LastSocketError())}); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                const auto token=static_cast<std::uint16_t>((GetTickCount()^GetCurrentProcessId())|1u);
                if(!candidate->HostStart(token,now)) { Record(12); SetStatus(TextId::ChannelPrepareFailed); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                if(!coordinator_->Attach(*candidate,SessionRole::Host)){candidate->Close();Accumulate(candidate->Metrics());return;}
                transport_=std::move(candidate); isHost_=true; lastState_=TcpTransportState::Idle; lastPhase_=SessionPhase::Idle; lastConnection_=0;
                const bool advertised=discovery_.StartHosting(transport_->BoundEndpoint().port,L"WinxCoop");
                discoveryUnavailable_=!advertised;
                Record(10,advertised?0:discovery_.LastSocketError()); UpdateButtons();
                SetStatus(advertised?TextId::LobbyCreated:TextId::LobbyCreatedManual);
            }
            void JoinLobby()
            {
                if(ControlText(gameRoot_)!=options_.paths.gameRoot){BeginProfileCheck(false);return;}
                if(transport_ || !coordinator_ || !coordinator_->CanAttach(SessionRole::Guest)) return;
                const auto server=ParseEndpoint(ControlText(address_));
                if(!server) { SetStatus(TextId::InvalidAddress); return; }
                TcpTransportConfig config; config.role=SessionRole::Guest; config.localBind={0,0}; config.server=*server;
                auto candidate=std::make_unique<WindowsTcpTransport>(config);
                if(!candidate->Start(GetTickCount64())) { Record(21,candidate->LastSocketError()); SetStatus({TextId::ConnectFailed,std::to_wstring(candidate->LastSocketError())}); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                if(!coordinator_->Attach(*candidate,SessionRole::Guest)){candidate->Close();Accumulate(candidate->Metrics());return;}
                transport_=std::move(candidate); isHost_=false; lastState_=TcpTransportState::Idle; lastPhase_=SessionPhase::Idle; lastConnection_=0;
                Record(20); UpdateButtons(); SetStatus(TextId::Connecting);
            }
            void Accumulate(const TcpTransportMetrics& metrics) noexcept
            {
                totals_.socketsOpened+=metrics.socketsOpened; totals_.socketsClosed+=metrics.socketsClosed;
                totals_.socketCloseErrors+=metrics.socketCloseErrors; totals_.wsaStarts+=metrics.wsaStarts;
                totals_.wsaCleanups+=metrics.wsaCleanups; totals_.wsaCleanupErrors+=metrics.wsaCleanupErrors;
                totals_.accepted+=metrics.accepted; totals_.refused+=metrics.refused;
                totals_.sentBytes+=metrics.sentBytes; totals_.receivedBytes+=metrics.receivedBytes;
                totals_.droppedSessionEvents+=metrics.droppedSessionEvents;
            }
            void LeaveLobby()
            {
                discovery_.StopHosting();
                if(coordinator_)coordinator_->Detach();
                if(transport_) { transport_->Close(); Accumulate(transport_->Metrics()); transport_.reset(); }
                lastConnection_=0; isHost_=false; discoveryUnavailable_=false; UpdateButtons(); Record(30);
                SetStatus(coordinator_ && coordinator_->GameActive()?TextId::LeftLobbyRunning:TextId::LeftLobby);
            }
            void FindLobbies()
            {
                if(closing_ || transport_ || (coordinator_ && !coordinator_->CanAttach(SessionRole::Guest)))return;
                const bool began=discovery_.BeginSearch(GetTickCount64()); Record(40,began?0:discovery_.LastSocketError());
                SetStatus(began?TextId::Searching:TextId::SearchFailed); RefreshList();
            }
            void RefreshList()
            {
                const auto selected=SendMessageW(list_,LB_GETCURSEL,0,0);
                SendMessageW(list_,LB_RESETCONTENT,0,0);
                for(const auto& lobby:discovery_.Lobbies())
                {
                    const auto text=lobby.name+L" — "+EndpointText(lobby.endpoint)+Text(lobby.occupied?TextId::Occupied:TextId::Available,language_);
                    SendMessageW(list_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
                }
                if(selected>=0 && static_cast<std::size_t>(selected)<discovery_.Lobbies().size()) SendMessageW(list_,LB_SETCURSEL,static_cast<WPARAM>(selected),0);
                discoveryRevision_=discovery_.Revision();
            }
            void SelectLobby(const bool join)
            {
                const auto selected=SendMessageW(list_,LB_GETCURSEL,0,0);
                if(selected<0 || static_cast<std::size_t>(selected)>=discovery_.Lobbies().size()) return;
                const auto& lobby=discovery_.Lobbies()[static_cast<std::size_t>(selected)];
                SetWindowTextW(address_,EndpointText(lobby.endpoint).c_str());
                if(join) { if(lobby.occupied) SetStatus(TextId::LobbyFull); else JoinLobby(); }
            }
            void Pump()
            {
                const auto now=GetTickCount64();
                if(profileCheck_ && profileCheck_->Finished() && !profileHandled_)
                {
                    profileHandled_=true;
                    if(!closing_ && profileCheck_->Successful())
                    {
                        OpenDefaultReport();coordinator_=std::make_unique<ReplicationCoordinator>(profileCheck_->Profile());Record(80);
                        SetStatus(TextId::GameSelected);
                        if(launchAfterQualification_){launchAfterQualification_=false;coordinator_->StartHostGame(now);SetStatus(coordinator_->Status());}
                    }
                    else if(!closing_)
                    {launchAfterQualification_=false;Record(81);SetStatus({TextId::AdapterNotReady,WideError(profileCheck_->Failure().c_str(),language_)});}
                    UpdateButtons();
                }
                if(closing_)
                {
                    if(coordinator_)coordinator_->Pump(now);
                    if((!profileCheck_ || profileCheck_->Finished()) && (!coordinator_ || coordinator_->Finished()))DestroyWindow(window_);
                    return;
                }
                const bool wasSearching=discovery_.Searching();
                discovery_.Pump(now);
                if(discoveryRevision_!=discovery_.Revision()) RefreshList();
                if(wasSearching && !discovery_.Searching()) SetStatus(discovery_.Lobbies().empty()?TextId::NoLobbies:TextId::SearchComplete);
                if(transport_)
                {
                    transport_->Pump(now); discovery_.SetOccupied(transport_->ActiveConnectionId()!=0);
                    while(const auto event=transport_->TakeSessionEvent())Record(50+static_cast<unsigned>(event->sessionPhase),static_cast<int>(event->sessionError),event->connectionId);
                    const auto state=transport_->State();const auto* session=transport_->Session();const auto phase=session?session->Phase():SessionPhase::Idle;
                    const auto connection=transport_->ActiveConnectionId();
                    if(state!=lastState_ || phase!=lastPhase_ || connection!=lastConnection_)
                    {
                        lastState_=state;lastPhase_=phase;lastConnection_=connection;
                        if(state==TcpTransportState::Connected && phase==SessionPhase::Running)
                            SetStatus(TextId::ChannelReady);
                        else if(state==TcpTransportState::Listening)
                            SetStatus(discoveryUnavailable_?TextId::LobbyListeningManual:TextId::LobbyListening);
                        else if(state==TcpTransportState::Closed || state==TcpTransportState::Failed)
                            SetStatus({TextId::ConnectionClosed,std::to_wstring(transport_->LastSocketError())});
                    }
                }
                if(coordinator_)
                {
                    coordinator_->Pump(now);
                    if(lastCoordinatorStatus_!=coordinator_->Status())
                    {lastCoordinatorStatus_=coordinator_->Status();SetStatus(lastCoordinatorStatus_);}
                }
                if(transport_ && !isHost_ && (transport_->State()==TcpTransportState::Closed || transport_->State()==TcpTransportState::Failed))
                {
                    // The coordinator drops its borrowed pointer before the
                    // terminal carrier is destroyed. No connection is retried
                    // automatically; this only makes explicit Join available.
                    const auto error=transport_->LastSocketError();
                    if(coordinator_)coordinator_->Detach();
                    transport_->Close();Accumulate(transport_->Metrics());transport_.reset();
                    lastConnection_=0;lastState_=TcpTransportState::Idle;lastPhase_=SessionPhase::Idle;
                    Record(31,error);
                    if(coordinator_){lastCoordinatorStatus_=coordinator_->Status();SetStatus(lastCoordinatorStatus_);}
                }
                UpdateButtons();
            }
            void BeginClose() noexcept
            {
                if(closing_ || closed_)return;closing_=true;
                if(profileCheck_)profileCheck_->Cancel();
                discovery_.Close();
                if(coordinator_){coordinator_->Detach();coordinator_->RequestShutdown();}
                if(transport_){transport_->Close();Accumulate(transport_->Metrics());transport_.reset();}
                const int controls[]{Host,Find,Join,Start,Leave,Disconnect,Port,Address,Lobbies,GameRoot,BrowseGame,OpenLogs,PlainGame,Language};
                for(const auto id:controls)EnableWindow(GetDlgItem(window_,id),FALSE);
                lastStatus_={TextId::Closing};
                if(status_)SetWindowTextW(status_,Text(TextId::Closing,language_));
                // The timer remains alive until facade reports the outer exit.
                // No GUI wait and no premature destruction of live IPC owners.
            }
            void WriteReport() noexcept
            {
                if(report_==INVALID_HANDLE_VALUE) return;
                try
                {
                    const auto& d=discovery_.Metrics();
                    const ReplicationMetrics empty{};const auto& m=coordinator_?coordinator_->Metrics():empty;
                    std::string json="{\"kind\":\"WinxCoop-native-launcher\",\"protocol\":3,\"pid\":"+std::to_string(GetCurrentProcessId())+",\"gameplayStarted\":"+(m.cooperativeStarts?"true":"false")+",\"adapterReady\":"+(m.gameReady?"true":"false")+",\"cleanupConfirmed\":"+(cleanupConfirmed_?"true":"false")+",\"fatal\":"+(fatal_?"true":"false")+",\"durationMs\":"+std::to_string(GetTickCount64()-started_);
                    const auto number=[&](const char* name,const auto value){json+=",\""+std::string(name)+"\":"+std::to_string(value);};
                    const auto boolean=[&](const char* name,const bool value){json+=",\""+std::string(name)+"\":"+(value?"true":"false");};
                    json+=",\"failure\":"+JsonString(coordinator_?coordinator_->Failure():std::string{});
                    json+=",\"replication\":{\"stockLocalPhysics\":true,\"fullHostPhysics\":false";
                    number("gameStarts",m.gameStarts);number("gameReady",m.gameReady);number("nativeFrames",m.nativeFrames);number("sampledStates",m.sampledStates);
                    number("deferredSamples",m.deferredSamples);number("invalidSamples",m.invalidSamples);number("messagesSent",m.messagesSent);number("messagesReceived",m.messagesReceived);
                    number("wrongConnections",m.wrongConnections);number("staleMessages",m.staleMessages);number("protocolFailures",m.protocolFailures);
                    number("observationsSent",m.observationsSent);number("observationsReceived",m.observationsReceived);number("observationsAccepted",m.observationsAccepted);
                    number("observationsRejected",m.observationsRejected);number("acknowledgementsReceived",m.acknowledgementsReceived);number("remoteStates",m.remoteStates);
                    number("visualStatesCoalesced",m.visualStatesCoalesced);number("nativeCommands",m.nativeCommands);number("nativeAcknowledgements",m.nativeAcknowledgements);
                    number("nativeCommandsCancelledOnRetirement",m.nativeCommandsCancelledOnRetirement);
                    number("nativeCreatesAcknowledged",m.nativeCreatesAcknowledged);number("nativeAppliesAcknowledged",m.nativeAppliesAcknowledged);number("nativeRemovesAcknowledged",m.nativeRemovesAcknowledged);
                    number("nativeAckLatencyTotalMs",m.nativeAckLatencyTotalMs);number("nativeAckLatencyMaxMs",m.nativeAckLatencyMaxMs);
                    number("observationRttSamples",m.observationRttSamples);number("observationRttTotalMs",m.observationRttTotalMs);number("observationRttMaxMs",m.observationRttMaxMs);
                    number("nativeAnimationSamples",m.nativeAnimationSamples);number("derivedAnimationSamples",m.derivedAnimationSamples);
                    number("nativeRemoteCreated",m.nativeRemoteCreated);number("nativeRemoteRemoved",m.nativeRemoteRemoved);
                    number("remoteDisplaySamples",m.remoteDisplaySamples);number("remoteDisplayMaxGapMs",m.remoteDisplayMaxGapMs);number("remoteDisplayMaxStep",m.remoteDisplayMaxStep);
                    number("cooperativeStarts",m.cooperativeStarts);number("cooperativeRunningMs",m.cooperativeRunningMs);
                    number("guestSceneReuses",m.guestSceneReuses);number("sceneMismatchWaits",m.sceneMismatchWaits);
                    number("nativeEpochRetirements",m.nativeEpochRetirements);number("visualRemovalWaits",m.visualRemovalWaits);
                    number("hostSceneAssignments",m.hostSceneAssignments);number("sceneSuspensions",m.sceneSuspensions);
                    number("guestSceneFollows",m.guestSceneFollows);number("nativeFollowRequestsAcknowledged",m.nativeFollowRequestsAcknowledged);
                    number("gameSoftLimitStops",m.gameSoftLimitStops);number("gameSoftLimitFallbackStops",m.gameSoftLimitFallbackStops);
                    number("lastGameHelperLimitSeconds",m.lastGameHelperLimitSeconds);number("lastGameSoftLimitSeconds",m.lastGameSoftLimitSeconds);
                    number("gameCompletions",m.gameCompletions);number("gameSuccessfulCompletions",m.gameSuccessfulCompletions);
                    number("gameNativeCleanupCompletions",m.gameNativeCleanupCompletions);number("gameNoOuterLaunchCompletions",m.gameNoOuterLaunchCompletions);json+='}';
                    const auto qualified=profileCheck_ && profileCheck_->Successful();
                    json+=",\"profile\":{\"qualified\":"+std::string(qualified?"true":"false");
                    if(qualified){number("level",profileCheck_->Profile().level.levelId);number("entry",profileCheck_->Profile().level.entryId);number("mediaFiles",profileCheck_->Profile().mediaFiles);number("mediaBytes",profileCheck_->Profile().mediaBytes);}json+='}';
                    json+=",\"lastGame\":{\"present\":"+std::string(coordinator_ && coordinator_->HasGameResult()?"true":"false");
                    if(coordinator_ && coordinator_->HasGameResult())
                    {
                        const auto& r=coordinator_->LastGameResult();boolean("completed",r.completed);boolean("successful",r.successful);boolean("outerStarted",r.outerStarted);
                        boolean("outerExited",r.outerExited);boolean("outerReportValid",r.outerReportValid);boolean("outerExitCodeKnown",r.outerExitCodeKnown);number("outerExitCode",r.outerExitCode);
                        boolean("mediaRestoreAttempted",r.mediaRestoreAttempted);boolean("mediaRestoreVerified",r.mediaRestoreVerified);boolean("mediaLeaseAcquired",r.mediaLeaseAcquired);
                        boolean("mediaLeaseInherited",r.mediaLeaseInherited);boolean("mediaLeaseReleased",r.mediaLeaseReleased);boolean("helperReportValid",r.helperReportValid);boolean("helperCompleted",r.helperCompleted);
                        boolean("gameExitCodeKnown",r.gameExitCodeKnown);number("gameExitCode",r.gameExitCode);boolean("gameExitEventContinued",r.gameExitEventContinued);
                        boolean("gameCleanupConfirmed",r.gameCleanupConfirmed);boolean("gameForcedTermination",r.gameForcedTermination);boolean("commandUnconfirmed",r.commandUnconfirmed);
                        boolean("commandCancelledOnRetirement",r.commandCancelledOnRetirement);
                        number("commandsPublished",r.commandsPublished);number("commandsAcknowledged",r.commandsAcknowledged);
                        number("commandsCompleted",r.commandsCompleted);number("commandsCancelledOnRetirement",r.commandsCancelledOnRetirement);
                        number("sceneGeneration",r.sceneGeneration);number("sceneRetirements",r.sceneRetirements);
                        number("stopReason",static_cast<std::uint32_t>(r.stopReason));
                        boolean("helperLimitReached",r.helperLimitReached);json+=",\"error\":"+JsonString(r.error);
                    }
                    json+='}';
                    json+=",\"tcp\":{\"socketsOpened\":"+std::to_string(totals_.socketsOpened)+",\"socketsClosed\":"+std::to_string(totals_.socketsClosed)+",\"closeErrors\":"+std::to_string(totals_.socketCloseErrors)+",\"wsaStarts\":"+std::to_string(totals_.wsaStarts)+",\"wsaCleanups\":"+std::to_string(totals_.wsaCleanups)+",\"wsaCleanupErrors\":"+std::to_string(totals_.wsaCleanupErrors)+",\"accepted\":"+std::to_string(totals_.accepted)+",\"refused\":"+std::to_string(totals_.refused)+",\"sentBytes\":"+std::to_string(totals_.sentBytes)+",\"receivedBytes\":"+std::to_string(totals_.receivedBytes)+",\"droppedSessionEvents\":"+std::to_string(totals_.droppedSessionEvents)+'}';
                    json+=",\"discovery\":{\"socketsOpened\":"+std::to_string(d.socketsOpened)+",\"socketsClosed\":"+std::to_string(d.socketsClosed)+",\"closeErrors\":"+std::to_string(d.closeErrors)+",\"wsaStarts\":"+std::to_string(d.wsaStarts)+",\"wsaCleanups\":"+std::to_string(d.wsaCleanups)+",\"wsaCleanupErrors\":"+std::to_string(d.wsaCleanupErrors)+",\"probesSent\":"+std::to_string(d.probesSent)+",\"probesReceived\":"+std::to_string(d.probesReceived)+",\"repliesSent\":"+std::to_string(d.repliesSent)+",\"repliesReceived\":"+std::to_string(d.repliesReceived)+",\"invalidDatagrams\":"+std::to_string(d.invalidDatagrams)+",\"sendErrors\":"+std::to_string(d.sendErrors)+",\"receiveErrors\":"+std::to_string(d.receiveErrors)+'}';
                    json+=",\"droppedEvents\":"+std::to_string(droppedEvents_)+",\"events\":[";
                    for(std::size_t i=0;i<eventCount_;++i)
                    { if(i) json+=','; const auto& e=events_[i]; json+="{\"t\":"+std::to_string(e.time)+",\"code\":"+std::to_string(e.code)+",\"detail\":"+std::to_string(e.detail)+",\"connection\":"+std::to_string(e.connection)+'}'; }
                    json+="]}\n";
                    DWORD written=0; if(json.size()>65536 || !WriteFile(report_,json.data(),static_cast<DWORD>(json.size()),&written,nullptr) || written!=json.size()) fatal_=true;
                }
                catch(...) { fatal_=true; }
                if(!CloseHandle(report_)) fatal_=true; report_=INVALID_HANDLE_VALUE;
            }
            void Close() noexcept
            {
                if(closed_) return; closed_=true;
                if(window_) KillTimer(window_,PumpTimer);
                discovery_.Close();
                if(coordinator_){coordinator_->Detach();coordinator_->RequestShutdown();}
                if(transport_) { transport_->Close(); Accumulate(transport_->Metrics()); transport_.reset(); }
                const auto& d=discovery_.Metrics();
                cleanupConfirmed_=totals_.socketsOpened==totals_.socketsClosed && !totals_.socketCloseErrors && totals_.wsaStarts==totals_.wsaCleanups && !totals_.wsaCleanupErrors &&
                    d.socketsOpened==d.socketsClosed && !d.closeErrors && d.wsaStarts==d.wsaCleanups && !d.wsaCleanupErrors &&
                    (!coordinator_ || (coordinator_->Finished() && coordinator_->CleanupConfirmed())) && (!profileCheck_ || profileCheck_->Finished());
                if(!cleanupConfirmed_) fatal_=true;
                WriteReport();
                Log("launcher-closed");if(log_!=INVALID_HANDLE_VALUE){CloseHandle(log_);log_=INVALID_HANDLE_VALUE;}
            }
            void FailClosed() noexcept
            {
                fatal_=true;Record(98);BeginClose();
            }
            LRESULT Dispatch(const UINT message,const WPARAM w,const LPARAM l)
            {
                // KillTimer does not remove an already queued WM_TIMER.
                if(closed_ && (message==WM_TIMER || message==WM_COMMAND)) return 0;
                switch(message)
                {
                case WM_CREATE: CreateControls(); return 0;
                case WM_CTLCOLORSTATIC:
                    // Keep panel text on the same surface as the group box.
                    // Header text and the read-only path retain system defaults;
                    // foreground/disabled colors remain owned by Windows.
                    switch(GetDlgCtrlID(reinterpret_cast<HWND>(l)))
                    {
                    case GamePathLabel:case GameHint:case ServerHint:case PortLabel:
                    case ClientHint:case AddressLabel:case LobbyListLabel:case LobbyListHint:case Status:
                        SetBkColor(reinterpret_cast<HDC>(w),GetSysColor(COLOR_WINDOW));
                        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
                    default:return DefWindowProcW(window_,message,w,l);
                    }
                case WM_COMMAND:
                    if(closing_)return 0;
                    switch(LOWORD(w))
                    {
                    case GameRoot:if(HIWORD(w)==EN_CHANGE)UpdateButtons();break;
                    case BrowseGame:if(HIWORD(w)==BN_CLICKED)BrowseInstalledGame();break;
                    case Language:if(HIWORD(w)==CBN_SELCHANGE)ChangeLanguage();break;
                    case OpenLogs:if(HIWORD(w)==BN_CLICKED)ShellExecuteW(window_,L"open",localDirectory_.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
                    case PlainGame:
                        if(HIWORD(w)==BN_CLICKED && IsWindowEnabled(GetDlgItem(window_,PlainGame)))
                        {const auto root=ControlText(gameRoot_);Log("plain-game-launch",root);ShellExecuteW(window_,L"open",(root+L"\\WinxClub.exe").c_str(),nullptr,root.c_str(),SW_SHOWNORMAL);}
                        break;
                    case Host: if(HIWORD(w)==BN_CLICKED) HostLobby(); break;
                    case Find: if(HIWORD(w)==BN_CLICKED) FindLobbies(); break;
                    case Join: if(HIWORD(w)==BN_CLICKED) JoinLobby(); break;
                    case Leave: if(HIWORD(w)==BN_CLICKED && transport_ && isHost_) LeaveLobby(); break;
                    case Disconnect: if(HIWORD(w)==BN_CLICKED && transport_ && !isHost_) LeaveLobby(); break;
                    case Start:
                        if(HIWORD(w)==BN_CLICKED && IsWindowEnabled(GetDlgItem(window_,Start)))
                        {if(coordinator_ && ControlText(gameRoot_)==options_.paths.gameRoot){const auto began=coordinator_->StartHostGame(GetTickCount64());Record(began?82:83);SetStatus(coordinator_->Status());}else BeginProfileCheck(true);UpdateButtons();}
                        break;
                    case Lobbies: if(HIWORD(w)==LBN_SELCHANGE || HIWORD(w)==LBN_DBLCLK) SelectLobby(HIWORD(w)==LBN_DBLCLK); break;
                    } return 0;
                case WM_TIMER: if(w==PumpTimer) Pump(); return 0;
                case WM_GETMINMAXINFO:
                    {RECT minimum{0,0,Scale(880),Scale(650)};AdjustWindowRectEx(&minimum,WS_OVERLAPPEDWINDOW,FALSE,0);
                    reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={minimum.right-minimum.left,minimum.bottom-minimum.top};} return 0;
                case WM_SIZE:
                    if(w!=SIZE_MINIMIZED)LayoutControls();return 0;
                case WM_CLOSE: BeginClose(); return 0;
                case WM_DESTROY: Close(); PostQuitMessage(fatal_?1:0); return 0;
                default: return DefWindowProcW(window_,message,w,l);
                }
            }
            static LRESULT CALLBACK Procedure(HWND window,const UINT message,const WPARAM w,const LPARAM l) noexcept
            {
                auto* self=reinterpret_cast<LobbyWindow*>(GetWindowLongPtrW(window,GWLP_USERDATA));
                if(message==WM_NCCREATE) { self=static_cast<LobbyWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); self->window_=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
                if(!self) return DefWindowProcW(window,message,w,l);
                try { return self->Dispatch(message,w,l); }
                catch(...) { self->FailClosed(); return message==WM_CREATE?-1:0; }
            }
        };
    }
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,wchar_t*,int show)
{
    SetProcessDPIAware(); // Available on Win7; the launcher uses system-DPI-scaled logical coordinates.
    try { winxcoop::launcher::LobbyWindow app(winxcoop::launcher::ParseOptions()); return app.Run(instance,show); }
    catch(const std::exception& error)
    {
        const auto language=(GetUserDefaultUILanguage()&0x3ff)==LANG_RUSSIAN?winxcoop::launcher::UiLanguage::Russian:winxcoop::launcher::UiLanguage::English;
        const auto text=winxcoop::launcher::Render({winxcoop::launcher::TextId::Diagnostic,winxcoop::launcher::WideError(error.what(),language)},language);
        MessageBoxW(nullptr,text.c_str(),L"WinxCoop",MB_ICONERROR|MB_OK);return 2;
    }
}
