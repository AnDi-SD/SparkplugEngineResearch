// Native Win32 lobby shell. Game launch/IPC integration is intentionally not
// claimed by a successful TCP handshake. The Start control stays disabled
// until a qualified game backend is connected by the application coordinator.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "LobbyDiscovery.h"
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
        enum Control : int { Port=101, Address, Lobbies, Host, Find, Join, Start, Leave, Status };
        struct Options { std::uint16_t port=27025; std::wstring report; };
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
            Options options; bool gotPort=false,gotReport=false;
            for(int i=1;i<count;++i)
            {
                const std::wstring key=arguments[i];
                if(i+1==count) throw std::invalid_argument("Expected --port PORT or --report NEW_PATH");
                const std::wstring value=arguments[++i];
                if(key==L"--port" && !gotPort)
                { const auto port=Number(value,65535); if(!port || !*port) throw std::invalid_argument("Invalid port"); options.port=static_cast<std::uint16_t>(*port); gotPort=true; }
                else if(key==L"--report" && !gotReport && !value.empty() && value.size()<MAX_PATH)
                { options.report=value; gotReport=true; }
                else throw std::invalid_argument("Unknown or repeated argument");
            }
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
                window_=CreateWindowExW(0,WindowClass,L"WinxCoop — локальное лобби",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,840,570,nullptr,nullptr,instance,this);
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
            HWND window_=nullptr,port_=nullptr,address_=nullptr,list_=nullptr,status_=nullptr;
            HANDLE report_=INVALID_HANDLE_VALUE;
            LobbyDiscovery discovery_;
            std::unique_ptr<WindowsTcpTransport> transport_;
            bool isHost_=false,closed_=false,fatal_=false,discoveryUnavailable_=false,cleanupConfirmed_=false;
            std::uint64_t started_=GetTickCount64(),discoveryRevision_=0,lastConnection_=0;
            TcpTransportState lastState_=TcpTransportState::Idle;
            SessionPhase lastPhase_=SessionPhase::Idle;
            std::array<Event,64> events_{};
            std::size_t eventCount_=0,droppedEvents_=0;
            TcpTransportMetrics totals_{};

            void Record(const unsigned code,const int detail=0,const std::uint64_t connection=0) noexcept
            {
                if(eventCount_==events_.size()) { ++droppedEvents_; return; }
                events_[eventCount_++]={GetTickCount64()-started_,connection,code,detail};
            }
            void SetStatus(const std::wstring& text){if(status_) SetWindowTextW(status_,text.c_str());}
            HWND Child(const wchar_t* type,const wchar_t* text,const DWORD style,const int id,const int x,const int y,const int width,const int height)
            {
                const auto child=CreateWindowExW(lstrcmpW(type,L"EDIT")==0?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);
                if(!child) throw std::runtime_error("Cannot create lobby control");
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return child;
            }
            void CreateControls()
            {
                Child(L"STATIC",L"TCP-порт сервера",0,0,16,18,150,22);
                port_=Child(L"EDIT",std::to_wstring(options_.port).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,Port,170,14,100,25);
                SendMessageW(port_,EM_SETLIMITTEXT,5,0);
                Child(L"BUTTON",L"Создать лобби",WS_TABSTOP,Host,290,13,160,28);
                Child(L"BUTTON",L"Найти в LAN",WS_TABSTOP,Find,466,13,160,28);
                Child(L"STATIC",L"Адрес IPv4:порт",0,0,16,60,150,22);
                address_=Child(L"EDIT",EndpointText({0x7f000001u,options_.port}).c_str(),ES_AUTOHSCROLL|WS_TABSTOP,Address,170,55,280,25);
                SendMessageW(address_,EM_SETLIMITTEXT,63,0);
                Child(L"BUTTON",L"Подключиться",WS_TABSTOP,Join,466,54,160,28);
                Child(L"STATIC",L"Найденные лобби (двойной щелчок — подключиться)",0,0,16,103,600,22);
                list_=Child(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP,Lobbies,16,130,790,230);
                Child(L"BUTTON",L"Начать игру",WS_TABSTOP,Start,16,379,160,30);
                Child(L"BUTTON",L"Покинуть лобби",WS_TABSTOP,Leave,190,379,180,30);
                status_=Child(L"STATIC",L"Создайте лобби или подключитесь к серверу. Игровой адаптер ещё не подключён.",SS_LEFT,Status,16,429,790,70);
                UpdateButtons();
                if(!SetTimer(window_,PumpTimer,20,nullptr)) throw std::runtime_error("Cannot start network pump");
            }
            void UpdateButtons()
            {
                const auto active=transport_!=nullptr;
                EnableWindow(GetDlgItem(window_,Host),!active);
                EnableWindow(GetDlgItem(window_,Join),!active);
                EnableWindow(port_,!active);
                EnableWindow(GetDlgItem(window_,Leave),active);
                EnableWindow(GetDlgItem(window_,Start),FALSE); // No qualified runtime backend yet.
            }
            std::wstring Text(HWND control) const
            { std::array<wchar_t,128> value{}; GetWindowTextW(control,value.data(),static_cast<int>(value.size())); return value.data(); }
            void HostLobby()
            {
                if(transport_) return;
                const auto port=Number(Text(port_),65535);
                if(!port || !*port) { SetStatus(L"Введите TCP-порт от 1 до 65535."); return; }
                TcpTransportConfig config; config.role=SessionRole::Host; config.localBind={0,static_cast<std::uint16_t>(*port)};
                auto candidate=std::make_unique<WindowsTcpTransport>(config); const auto now=GetTickCount64();
                if(!candidate->Start(now)) { Record(11,candidate->LastSocketError()); SetStatus(L"Не удалось открыть TCP-порт. Код Winsock: "+std::to_wstring(candidate->LastSocketError())); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                const auto token=static_cast<std::uint16_t>((GetTickCount()^GetCurrentProcessId())|1u);
                if(!candidate->HostStart(token,now)) { Record(12); SetStatus(L"Не удалось подготовить сетевой канал."); candidate->Close(); Accumulate(candidate->Metrics()); return; }
                transport_=std::move(candidate); isHost_=true; lastState_=TcpTransportState::Idle; lastPhase_=SessionPhase::Idle; lastConnection_=0;
                const bool advertised=discovery_.StartHosting(transport_->BoundEndpoint().port,L"WinxCoop");
                discoveryUnavailable_=!advertised;
                Record(10,advertised?0:discovery_.LastSocketError()); UpdateButtons();
                SetStatus(advertised?L"Лобби создано. Ожидаем гостя. Запуск игры станет доступен после подключения адаптера.":L"TCP-лобби создано, но LAN-поиск недоступен. Подключитесь вручную по IPv4:порту.");
            }
            void JoinLobby()
            {
                if(transport_) return;
                const auto server=ParseEndpoint(Text(address_));
                if(!server) { SetStatus(L"Введите адрес в формате IPv4:порт, например 192.168.1.10:27025."); return; }
                TcpTransportConfig config; config.role=SessionRole::Guest; config.localBind={0,0}; config.server=*server;
                auto candidate=std::make_unique<WindowsTcpTransport>(config);
                if(!candidate->Start(GetTickCount64())) { Record(21,candidate->LastSocketError()); SetStatus(L"Не удалось подключиться. Код Winsock: "+std::to_wstring(candidate->LastSocketError())); candidate->Close(); Accumulate(candidate->Metrics()); return; }
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
                if(transport_) { transport_->Close(); Accumulate(transport_->Metrics()); transport_.reset(); }
                lastConnection_=0; isHost_=false; discoveryUnavailable_=false; UpdateButtons(); Record(30); SetStatus(L"Вы вышли из лобби.");
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
                const auto now=GetTickCount64(); const bool wasSearching=discovery_.Searching();
                discovery_.Pump(now);
                if(discoveryRevision_!=discovery_.Revision()) RefreshList();
                if(wasSearching && !discovery_.Searching()) SetStatus(discovery_.Lobbies().empty()?L"Лобби не найдены. Можно подключиться вручную по IPv4:порту.":L"Поиск завершён. Выберите лобби или введите адрес вручную.");
                if(!transport_) return;
                transport_->Pump(now); discovery_.SetOccupied(transport_->ActiveConnectionId()!=0);
                while(const auto event=transport_->TakeSessionEvent()) Record(50+static_cast<unsigned>(event->sessionPhase),static_cast<int>(event->sessionError),event->connectionId);
                // The shell has no compatibility hashes or game backend yet.
                // Do not silently consume or apply future gameplay commands.
                for(std::size_t messages=0;messages<WindowsTcpTransport::MaximumDeliveredMessages;++messages)
                {
                    const auto unexpected=transport_->TakeMessage(); if(!unexpected) break;
                    if(!transport_->ActiveConnectionId() || unexpected->connectionId!=transport_->ActiveConnectionId())
                    { Record(71,0,unexpected->connectionId); continue; }
                    Record(70,0,unexpected->connectionId); transport_->ClosePeer();
                    SetStatus(L"Получены игровые команды, но адаптер ещё не подключён. Сеанс закрыт.");
                    break;
                }
                const auto state=transport_->State(); const auto* session=transport_->Session(); const auto phase=session?session->Phase():SessionPhase::Idle;
                const auto connection=transport_->ActiveConnectionId();
                if(state!=lastState_ || phase!=lastPhase_ || connection!=lastConnection_)
                {
                    lastState_=state; lastPhase_=phase; lastConnection_=connection;
                    if(state==TcpTransportState::Connected && phase==SessionPhase::Running)
                        SetStatus(isHost_?L"Гость подключён к лобби. Игровой адаптер ещё не готов; запуск игры недоступен.":L"Вы в лобби. Ожидаем сервер и подключение игрового адаптера.");
                    else if(state==TcpTransportState::Listening)
                        SetStatus(discoveryUnavailable_?L"TCP-лобби открыто. LAN-поиск недоступен; используйте ручной адрес. Игровой адаптер ещё не подключён.":L"Лобби открыто. Ожидаем гостя. Игровой адаптер ещё не подключён.");
                    else if(state==TcpTransportState::Closed || state==TcpTransportState::Failed)
                        SetStatus(L"Соединение завершено. Нажмите «Покинуть лобби», чтобы подключиться снова. Код Winsock: "+std::to_wstring(transport_->LastSocketError()));
                }
            }
            void WriteReport() noexcept
            {
                if(report_==INVALID_HANDLE_VALUE) return;
                try
                {
                    const auto& d=discovery_.Metrics();
                    std::string json="{\"kind\":\"WinxCoop-native-lobby-shell\",\"protocol\":1,\"pid\":"+std::to_string(GetCurrentProcessId())+",\"gameplayStarted\":false,\"adapterReady\":false,\"cleanupConfirmed\":"+(cleanupConfirmed_?"true":"false")+",\"fatal\":"+(fatal_?"true":"false")+",\"durationMs\":"+std::to_string(GetTickCount64()-started_);
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
                if(transport_) { transport_->Close(); Accumulate(transport_->Metrics()); transport_.reset(); }
                const auto& d=discovery_.Metrics();
                cleanupConfirmed_=totals_.socketsOpened==totals_.socketsClosed && !totals_.socketCloseErrors && totals_.wsaStarts==totals_.wsaCleanups && !totals_.wsaCleanupErrors &&
                    d.socketsOpened==d.socketsClosed && !d.closeErrors && d.wsaStarts==d.wsaCleanups && !d.wsaCleanupErrors;
                if(!cleanupConfirmed_) fatal_=true;
                WriteReport();
            }
            void FailClosed() noexcept
            {
                fatal_=true; Record(98); Close();
                // Keep the error visible, but leave no timer, transport or UI
                // action capable of starting another session after failure.
                const int controls[]{Host,Find,Join,Start,Leave,Port,Address,Lobbies};
                for(const auto id:controls) EnableWindow(GetDlgItem(window_,id),FALSE);
                if(status_) SetWindowTextW(status_,L"Ошибка лобби. Сеанс закрыт. Закройте окно и запустите программу снова.");
            }
            LRESULT Dispatch(const UINT message,const WPARAM w,const LPARAM l)
            {
                // KillTimer does not remove an already queued WM_TIMER.
                if(closed_ && (message==WM_TIMER || message==WM_COMMAND)) return 0;
                switch(message)
                {
                case WM_CREATE: CreateControls(); return 0;
                case WM_COMMAND:
                    switch(LOWORD(w))
                    {
                    case Host: if(HIWORD(w)==BN_CLICKED) HostLobby(); break;
                    case Find: if(HIWORD(w)==BN_CLICKED) FindLobbies(); break;
                    case Join: if(HIWORD(w)==BN_CLICKED) JoinLobby(); break;
                    case Leave: if(HIWORD(w)==BN_CLICKED) LeaveLobby(); break;
                    case Start: SetStatus(L"Запуск недоступен: игровой адаптер ещё не подключён."); break;
                    case Lobbies: if(HIWORD(w)==LBN_SELCHANGE || HIWORD(w)==LBN_DBLCLK) SelectLobby(HIWORD(w)==LBN_DBLCLK); break;
                    } return 0;
                case WM_TIMER: if(w==PumpTimer) Pump(); return 0;
                case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={680,520}; return 0;
                case WM_SIZE:
                    if(list_) { const int width=LOWORD(l),height=HIWORD(l); MoveWindow(list_,16,130,width-32,height-280,TRUE); MoveWindow(GetDlgItem(window_,Start),16,height-130,160,30,TRUE); MoveWindow(GetDlgItem(window_,Leave),190,height-130,180,30,TRUE); MoveWindow(status_,16,height-80,width-32,70,TRUE); } return 0;
                case WM_CLOSE: DestroyWindow(window_); return 0;
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
