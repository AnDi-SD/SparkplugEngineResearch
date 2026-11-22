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
#include "Networking/WindowsTcpTransport.h"
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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
        enum Control : int { Port=101, Address, Lobbies, Host, Find, Join, Start, Leave, Status, GameRoot, BrowseGame, OpenLogs, PlainGame };
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
        std::wstring WideError(const char* text)
        {
            if(!text) return L"Неизвестная ошибка";
            const auto size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);
            if(size<=0 || size>2048) return L"Ошибка приложения";
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
            ~LobbyWindow(){Close();}
            int Run(HINSTANCE instance,const int show)
            {
                instance_=instance;
                WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance; wc.lpfnWndProc=&Procedure;
                wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1); wc.lpszClassName=WindowClass;
                if(!RegisterClassExW(&wc)) throw std::runtime_error("Cannot register lobby window");
                window_=CreateWindowExW(0,WindowClass,L"WinxCoop — штатный запуск и локальное лобби",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,860,660,nullptr,nullptr,instance,this);
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
            HWND window_=nullptr,port_=nullptr,address_=nullptr,list_=nullptr,status_=nullptr,gameRoot_=nullptr;
            HANDLE report_=INVALID_HANDLE_VALUE;
            HANDLE log_=INVALID_HANDLE_VALUE;
            std::wstring localDirectory_,lastStatus_;
            bool launchAfterQualification_=false;
            LobbyDiscovery discovery_;
            std::unique_ptr<WindowsTcpTransport> transport_;
            std::unique_ptr<PrototypeProfileCheck> profileCheck_;
            std::unique_ptr<ReplicationCoordinator> coordinator_;
            std::wstring lastCoordinatorStatus_;
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
            void SetStatus(const std::wstring& text){if(lastStatus_==text)return;lastStatus_=text;Log("status",text);if(status_)SetWindowTextW(status_,text.c_str());}
            void BeginProfileCheck(const bool launch)
            {
                if(transport_ || (coordinator_ && (coordinator_->GameActive() || !coordinator_->CleanupConfirmed())) || (profileCheck_ && !profileCheck_->Finished()))return;
                const auto selected=Text(gameRoot_);
                if(selected.empty()){SetStatus(L"Выберите папку установленной игры.");return;}
                options_.paths.gameRoot=ResolvePrototypePath(selected);options_.paths.media=options_.paths.gameRoot+L"\\Media";
                coordinator_.reset();profileCheck_.reset();profileHandled_=false;launchAfterQualification_=launch;
                profileCheck_=std::make_unique<PrototypeProfileCheck>(options_.paths);
                Log("game-selected",options_.paths.gameRoot);
                WritePrivateProfileStringW(L"Paths",L"gameRoot",options_.paths.gameRoot.c_str(),(localDirectory_+L"\\launcher-settings.ini").c_str());
                SetStatus(L"Проверяем пути игры и компоненты адаптера. Настройки игры сохраняются.");UpdateButtons();
            }
            void BrowseInstalledGame()
            {
                wchar_t selected[MAX_PATH]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
                dialog.lpstrFilter=L"Winx Club (WinxClub.exe)\0WinxClub.exe\0";dialog.lpstrFile=selected;dialog.nMaxFile=MAX_PATH;
                dialog.lpstrTitle=L"Выберите WinxClub.exe установленной игры";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
                if(!GetOpenFileNameW(&dialog))return;
                std::wstring path=selected;path=path.substr(0,path.find_last_of(L"\\/"));SetWindowTextW(gameRoot_,path.c_str());
                try{BeginProfileCheck(false);}catch(const std::exception& error){SetStatus(WideError(error.what()));Log("selection-failed",WideError(error.what()));}
            }
            HWND Child(const wchar_t* type,const wchar_t* text,const DWORD style,const int id,const int x,const int y,const int width,const int height)
            {
                const auto child=CreateWindowExW(lstrcmpW(type,L"EDIT")==0?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
                if(!child) throw std::runtime_error("Cannot create lobby control");
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return child;
            }
            void CreateControls()
            {
                Child(L"STATIC",L"Установленная игра",0,0,16,18,150,22);
                gameRoot_=Child(L"EDIT",options_.paths.gameRoot.c_str(),ES_AUTOHSCROLL|WS_TABSTOP,GameRoot,170,14,485,25);
                SendMessageW(gameRoot_,EM_SETLIMITTEXT,MAX_PATH-1,0);
                Child(L"BUTTON",L"Выбрать…",WS_TABSTOP,BrowseGame,668,13,150,28);
                Child(L"STATIC",L"Запуск через обычное меню. Игра и INI не копируются и не изменяются.",0,0,16,54,800,22);
                Child(L"STATIC",L"TCP-порт сервера",0,0,16,98,150,22);
                port_=Child(L"EDIT",std::to_wstring(options_.port).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,Port,170,94,100,25);
                SendMessageW(port_,EM_SETLIMITTEXT,5,0);
                Child(L"BUTTON",L"Создать лобби",WS_TABSTOP,Host,290,93,160,28);
                Child(L"BUTTON",L"Найти в LAN",WS_TABSTOP,Find,466,93,160,28);
                Child(L"STATIC",L"Адрес IPv4:порт",0,0,16,140,150,22);
                address_=Child(L"EDIT",EndpointText({0x7f000001u,options_.port}).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,Address,170,135,280,25);
                SendMessageW(address_,EM_SETLIMITTEXT,63,0);
                Child(L"BUTTON",L"Подключиться",WS_TABSTOP,Join,466,134,160,28);
                Child(L"STATIC",L"Найденные лобби (двойной щелчок — подключиться)",0,0,16,183,600,22);
                list_=Child(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP,Lobbies,16,210,810,230);
                Child(L"BUTTON",L"Запустить игру",WS_TABSTOP,Start,16,469,160,30);
                Child(L"BUTTON",L"Покинуть лобби",WS_TABSTOP,Leave,190,469,180,30);
                Child(L"BUTTON",L"Папка журналов",WS_TABSTOP,OpenLogs,384,469,160,30);
                Child(L"BUTTON",L"Без адаптера",WS_TABSTOP,PlainGame,558,469,160,30);
                status_=Child(L"STATIC",L"Выберите установленную игру. Затем можно открыть штатное меню.",SS_LEFT,Status,16,519,810,70);
                UpdateButtons();
                if(!SetTimer(window_,PumpTimer,20,nullptr)) throw std::runtime_error("Cannot start network pump");
            }
            void UpdateButtons()
            {
                const auto active=transport_!=nullptr;
                const bool selectedMatches=Text(gameRoot_)==options_.paths.gameRoot;
                EnableWindow(GetDlgItem(window_,Host),!closing_ && !active && selectedMatches && coordinator_ && coordinator_->CanAttach(SessionRole::Host));
                EnableWindow(GetDlgItem(window_,Join),!closing_ && !active && selectedMatches && coordinator_ && coordinator_->CanAttach(SessionRole::Guest));
                EnableWindow(port_,!closing_ && !active);
                EnableWindow(GetDlgItem(window_,Leave),!closing_ && active);
                const bool editable=!closing_ && !active && (!coordinator_ || (!coordinator_->GameActive() && coordinator_->CleanupConfirmed())) && (!profileCheck_ || profileCheck_->Finished());
                EnableWindow(gameRoot_,editable);EnableWindow(GetDlgItem(window_,BrowseGame),editable);
                EnableWindow(GetDlgItem(window_,Start),!closing_ && ((coordinator_ && coordinator_->CanStartHostGame()) || (!coordinator_ && editable)));
            }
            std::wstring Text(HWND control) const
            { std::array<wchar_t,MAX_PATH> value{}; GetWindowTextW(control,value.data(),static_cast<int>(value.size())); return value.data(); }
            void HostLobby()
            {
                if(Text(gameRoot_)!=options_.paths.gameRoot){BeginProfileCheck(false);return;}
                if(transport_ || !coordinator_ || !coordinator_->CanAttach(SessionRole::Host)) return;
                const auto port=Number(Text(port_),65535);
                if(!port || !*port) { SetStatus(L"Введите TCP-порт от 1 до 65535."); return; }
                TcpTransportConfig config; config.role=SessionRole::Host; config.localBind={0,static_cast<std::uint16_t>(*port)};
                auto candidate=std::make_unique<WindowsTcpTransport>(config); const auto now=GetTickCount64();
                if(!candidate->Start(now)) { Record(11,candidate->LastSocketError()); SetStatus(L"Не удалось открыть TCP-порт. Код Winsock: "+std::to_wstring(candidate->LastSocketError())); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                const auto token=static_cast<std::uint16_t>((GetTickCount()^GetCurrentProcessId())|1u);
                if(!candidate->HostStart(token,now)) { Record(12); SetStatus(L"Не удалось подготовить сетевой канал."); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                if(!coordinator_->Attach(*candidate,SessionRole::Host)){candidate->Close();Accumulate(candidate->Metrics());return;}
                transport_=std::move(candidate); isHost_=true; lastState_=TcpTransportState::Idle; lastPhase_=SessionPhase::Idle; lastConnection_=0;
                const bool advertised=discovery_.StartHosting(transport_->BoundEndpoint().port,L"WinxCoop");
                discoveryUnavailable_=!advertised;
                Record(10,advertised?0:discovery_.LastSocketError()); UpdateButtons();
                SetStatus(advertised?L"Лобби создано. Можно открыть обычное меню кнопкой «Запустить игру».":L"Лобби создано, но LAN-поиск недоступен. Гость может подключиться по IPv4:порту.");
            }
            void JoinLobby()
            {
                if(Text(gameRoot_)!=options_.paths.gameRoot){BeginProfileCheck(false);return;}
                if(transport_ || !coordinator_ || !coordinator_->CanAttach(SessionRole::Guest)) return;
                const auto server=ParseEndpoint(Text(address_));
                if(!server) { SetStatus(L"Введите адрес в формате IPv4:порт, например 192.168.1.10:27025."); return; }
                TcpTransportConfig config; config.role=SessionRole::Guest; config.localBind={0,0}; config.server=*server;
                auto candidate=std::make_unique<WindowsTcpTransport>(config);
                if(!candidate->Start(GetTickCount64())) { Record(21,candidate->LastSocketError()); SetStatus(L"Не удалось подключиться. Код Winsock: "+std::to_wstring(candidate->LastSocketError())); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                if(!coordinator_->Attach(*candidate,SessionRole::Guest)){candidate->Close();Accumulate(candidate->Metrics());return;}
                transport_=std::move(candidate); isHost_=false; lastState_=TcpTransportState::Idle; lastPhase_=SessionPhase::Idle; lastConnection_=0;
                Record(20); UpdateButtons(); SetStatus(L"Подключение к серверу…");
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
                SetStatus(coordinator_ && coordinator_->GameActive()?L"Вы вышли из лобби. Удалённая модель удаляется; игра сервера может продолжаться.":L"Вы вышли из лобби.");
            }
            void FindLobbies()
            {
                const bool began=discovery_.BeginSearch(GetTickCount64()); Record(40,began?0:discovery_.LastSocketError());
                SetStatus(began?L"Ищем лобби в локальной сети…":L"Не удалось начать LAN-поиск. Доступно ручное подключение."); RefreshList();
            }
            void RefreshList()
            {
                const auto selected=SendMessageW(list_,LB_GETCURSEL,0,0);
                SendMessageW(list_,LB_RESETCONTENT,0,0);
                for(const auto& lobby:discovery_.Lobbies())
                {
                    const auto text=lobby.name+L" — "+EndpointText(lobby.endpoint)+(lobby.occupied?L" (занято)":L" (свободно)");
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
                if(join) { if(lobby.occupied) SetStatus(L"В лобби уже два участника."); else JoinLobby(); }
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
                        SetStatus(L"Игра выбрана. Запуск откроет обычное меню; лобби можно создать отдельно.");
                        if(launchAfterQualification_){launchAfterQualification_=false;coordinator_->StartHostGame(now);SetStatus(coordinator_->Status());}
                    }
                    else if(!closing_)
                    {launchAfterQualification_=false;Record(81);SetStatus(L"Адаптер не готов: "+WideError(profileCheck_->Failure().c_str())+L". Проверьте выбранную папку игры и компоненты мода.");}
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
                if(wasSearching && !discovery_.Searching()) SetStatus(discovery_.Lobbies().empty()?L"Лобби не найдены. Можно подключиться вручную по IPv4:порту.":L"Поиск завершён. Выберите лобби или введите адрес вручную.");
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
                            SetStatus(L"Сетевой канал готов. Игры открываются через штатное меню; локация определяется после загрузки.");
                        else if(state==TcpTransportState::Listening)
                            SetStatus(discoveryUnavailable_?L"Лобби открыто. LAN-поиск недоступен; гость подключается вручную.":L"Лобби открыто. Ожидаем гостя.");
                        else if(state==TcpTransportState::Closed || state==TcpTransportState::Failed)
                            SetStatus(L"Соединение завершено. Для нового подключения нажмите «Покинуть лобби». Код Winsock: "+std::to_wstring(transport_->LastSocketError()));
                    }
                }
                if(coordinator_)
                {
                    coordinator_->Pump(now);
                    if(lastCoordinatorStatus_!=coordinator_->Status())
                    {lastCoordinatorStatus_=coordinator_->Status();SetStatus(lastCoordinatorStatus_);}
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
                const int controls[]{Host,Find,Join,Start,Leave,Port,Address,Lobbies};
                for(const auto id:controls)EnableWindow(GetDlgItem(window_,id),FALSE);
                if(status_)SetWindowTextW(status_,L"Закрываем сеанс. Ожидаем завершения принадлежащих ему процессов…");
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
                        number("commandsPublished",r.commandsPublished);number("commandsAcknowledged",r.commandsAcknowledged);number("stopReason",static_cast<std::uint32_t>(r.stopReason));
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
                case WM_COMMAND:
                    if(closing_)return 0;
                    switch(LOWORD(w))
                    {
                    case GameRoot:if(HIWORD(w)==EN_CHANGE)UpdateButtons();break;
                    case BrowseGame:if(HIWORD(w)==BN_CLICKED)BrowseInstalledGame();break;
                    case OpenLogs:if(HIWORD(w)==BN_CLICKED)ShellExecuteW(window_,L"open",localDirectory_.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
                    case PlainGame:
                        if(HIWORD(w)==BN_CLICKED)
                        {const auto root=Text(gameRoot_);Log("plain-game-launch",root);ShellExecuteW(window_,L"open",(root+L"\\WinxClub.exe").c_str(),nullptr,root.c_str(),SW_SHOWNORMAL);}
                        break;
                    case Host: if(HIWORD(w)==BN_CLICKED) HostLobby(); break;
                    case Find: if(HIWORD(w)==BN_CLICKED) FindLobbies(); break;
                    case Join: if(HIWORD(w)==BN_CLICKED) JoinLobby(); break;
                    case Leave: if(HIWORD(w)==BN_CLICKED) LeaveLobby(); break;
                    case Start:
                        if(HIWORD(w)==BN_CLICKED)
                        {if(coordinator_ && Text(gameRoot_)==options_.paths.gameRoot){const auto began=coordinator_->StartHostGame(GetTickCount64());Record(began?82:83);SetStatus(coordinator_->Status());}else BeginProfileCheck(true);UpdateButtons();}
                        break;
                    case Lobbies: if(HIWORD(w)==LBN_SELCHANGE || HIWORD(w)==LBN_DBLCLK) SelectLobby(HIWORD(w)==LBN_DBLCLK); break;
                    } return 0;
                case WM_TIMER: if(w==PumpTimer) Pump(); return 0;
                case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={840,650}; return 0;
                case WM_SIZE:
                    if(list_) { const int width=LOWORD(l),height=HIWORD(l); MoveWindow(list_,16,210,width-32,height-360,TRUE); MoveWindow(GetDlgItem(window_,Start),16,height-130,160,30,TRUE); MoveWindow(GetDlgItem(window_,Leave),190,height-130,180,30,TRUE); MoveWindow(GetDlgItem(window_,OpenLogs),384,height-130,160,30,TRUE);MoveWindow(GetDlgItem(window_,PlainGame),558,height-130,160,30,TRUE);MoveWindow(status_,16,height-80,width-32,70,TRUE); } return 0;
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
    try { winxcoop::launcher::LobbyWindow app(winxcoop::launcher::ParseOptions()); return app.Run(instance,show); }
    catch(const std::exception& error) { const auto text=winxcoop::launcher::WideError(error.what()); MessageBoxW(nullptr,text.c_str(),L"WinxCoop",MB_ICONERROR|MB_OK); return 2; }
}
