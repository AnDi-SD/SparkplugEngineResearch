#pragma once

#include <string>

namespace winxcoop::launcher
{
    enum class UiLanguage { English, Russian };

    // One table keeps the two translations adjacent and requires a row for
    // every key. String views, paths and diagnostics from other layers are data.
#define WINXCOOP_LAUNCH_TEXT(X) \
    X(None, L"", L"") \
    X(Diagnostic, L"Error: {0}", L"Ошибка: {0}") \
    X(WindowTitle, L"WinxCoop — passive companion", L"WinxCoop — пассивный спутник") \
    X(AppTitle, L"WinxCoop", L"WinxCoop") \
    X(AppSubtitle, L"Two Blooms together. The server leads the game.", L"Две Блум рядом. Прохождение ведёт сервер.") \
    X(LanguageLabel, L"Language", L"Язык") \
    X(LanguageSystem, L"System", L"Язык системы") \
    X(LanguageRussian, L"Русский", L"Русский") \
    X(LanguageEnglish, L"English", L"English") \
    X(CommonGroup, L"General settings", L"Общие настройки") \
    X(GamePathLabel, L"Installed game", L"Установленная игра") \
    X(BrowseGame, L"Browse…", L"Выбрать…") \
    X(OpenLogs, L"Open logs", L"Папка журналов") \
    X(PlainGame, L"Without adapter", L"Без адаптера") \
    X(GameHint, L"Select your installed Winx Club to host or join a game.", L"Выберите установленную Winx Club, чтобы создать лобби или подключиться.") \
    X(ServerGroup, L"Server", L"Сервер") \
    X(ServerHint, L"Play through the original game. The client follows your location.", L"Проходите обычную игру. Клиент следует за вашей локацией.") \
    X(PortLabel, L"TCP port", L"TCP-порт") \
    X(CreateLobby, L"Create lobby", L"Создать лобби") \
    X(StartGame, L"Start game", L"Запустить игру") \
    X(CloseLobby, L"Close lobby", L"Закрыть лобби") \
    X(ClientGroup, L"Client", L"Клиент") \
    X(ClientHint, L"The client game opens once the server has loaded a location.", L"Игра клиента откроется, когда сервер загрузит локацию.") \
    X(AddressLabel, L"IPv4:port address", L"Адрес IPv4:порт") \
    X(JoinLobby, L"Connect", L"Подключиться") \
    X(Disconnect, L"Disconnect", L"Отключиться") \
    X(FindLobbies, L"Find on LAN", L"Найти в LAN") \
    X(LobbyListLabel, L"Local network lobbies", L"Лобби в локальной сети") \
    X(LobbyListHint, L"Double-click an available lobby to connect.", L"Дважды щёлкните по свободному лобби, чтобы подключиться.") \
    X(StatusGroup, L"Status", L"Состояние") \
    X(Occupied, L" (occupied)", L" (занято)") \
    X(Available, L" (available)", L" (свободно)") \
    X(SelectGameDialog, L"Select WinxClub.exe from your installed game", L"Выберите WinxClub.exe установленной игры") \
    X(SelectGame, L"Select the installed game folder.", L"Выберите папку установленной игры.") \
    X(CheckingGame, L"Checking the game path and adapter files…", L"Проверяем папку игры и файлы адаптера…") \
    X(InitialStatus, L"Select the installed game to get started.", L"Для начала выберите установленную игру.") \
    X(InvalidPort, L"Enter a TCP port from 1 to 65535.", L"Введите TCP-порт от 1 до 65535.") \
    X(TcpOpenFailed, L"Could not open the TCP port. Winsock error: {0}", L"Не удалось открыть TCP-порт. Код Winsock: {0}") \
    X(ChannelPrepareFailed, L"Could not prepare the connection.", L"Не удалось подготовить сетевой канал.") \
    X(LobbyCreated, L"Lobby created. Select Start game to open the original game menu.", L"Лобби создано. Нажмите «Запустить игру», чтобы открыть обычное меню.") \
    X(LobbyCreatedManual, L"Lobby created. LAN discovery is unavailable. The client can connect by IPv4 address and port.", L"Лобби создано. LAN-поиск недоступен. Клиент может подключиться по адресу IPv4 и порту.") \
    X(InvalidAddress, L"Enter an IPv4 address and port, for example 192.168.1.10:27025.", L"Введите адрес IPv4 и порт, например 192.168.1.10:27025.") \
    X(ConnectFailed, L"Could not connect. Winsock error: {0}", L"Не удалось подключиться. Код Winsock: {0}") \
    X(Connecting, L"Connecting to the server…", L"Подключаемся к серверу…") \
    X(LeftLobbyRunning, L"Disconnected. Removing the remote character. The game can continue.", L"Соединение закрыто. Убираем удалённого персонажа. Игра может продолжаться.") \
    X(LeftLobby, L"Lobby connection closed.", L"Соединение с лобби закрыто.") \
    X(Searching, L"Looking for lobbies on the local network…", L"Ищем лобби в локальной сети…") \
    X(SearchFailed, L"Could not start LAN discovery. You can enter the address manually.", L"Не удалось начать LAN-поиск. Можно ввести адрес вручную.") \
    X(LobbyFull, L"This lobby already has two players.", L"В этом лобби уже два игрока.") \
    X(AdapterNotReady, L"The adapter is unavailable. Check the game folder and the mod files. Details: {0}", L"Адаптер не готов. Проверьте папку игры и файлы мода. Подробности: {0}") \
    X(NoLobbies, L"No lobbies found. Enter an address and port to connect manually.", L"Лобби не найдены. Введите адрес и порт для ручного подключения.") \
    X(SearchComplete, L"Search complete. Choose a lobby or enter its address manually.", L"Поиск завершён. Выберите лобби или введите адрес вручную.") \
    X(ChannelReady, L"Connected. The client waits for a ready server location.", L"Соединение установлено. Клиент ждёт готовой локации сервера.") \
    X(LobbyListeningManual, L"Lobby open. LAN discovery is unavailable. The client can connect manually.", L"Лобби открыто. LAN-поиск недоступен. Клиент может подключиться вручную.") \
    X(LobbyListening, L"Lobby open. Waiting for a client.", L"Лобби открыто. Ждём клиента.") \
    X(ConnectionClosed, L"Connection closed. You can connect again. Winsock code: {0}", L"Соединение закрыто. Можно подключиться снова. Код Winsock: {0}") \
    X(Closing, L"Closing the session and waiting for its processes to exit…", L"Закрываем сеанс и ждём завершения процессов…") \
    X(UnknownError, L"Unknown error", L"Неизвестная ошибка") \
    X(ApplicationError, L"Application error", L"Ошибка приложения") \
    X(GameSelected, L"Game selected. Start it from the server panel or connect as a client.", L"Игра выбрана. Запустите её в разделе сервера или подключитесь как клиент.") \
    X(SessionClosedError, L"Network session closed: {0}", L"Сетевой сеанс закрыт: {0}") \
    X(SceneEndedReconnect, L"The location has ended. The game is still open. Exit and restart it to connect again.", L"Локация завершена. Игра остаётся открытой. Для повторного подключения завершите и запустите её заново.") \
    X(DisconnectedGameOpen, L"Disconnected. The game stays open while the remote character is removed. You can reconnect to the same location.", L"Соединение закрыто. Игра остаётся открытой, удалённый персонаж убирается. Можно снова подключиться к той же локации.") \
    X(GuestLeftRemoving, L"The client disconnected. The server game remains open while the remote character is removed.", L"Клиент отключился. Серверная игра остаётся открытой, удалённый персонаж убирается.") \
    X(DisconnectedCleanupUnknown, L"Disconnected. Game cleanup could not be confirmed. Keep the local logs.", L"Соединение закрыто. Очистка игры не подтверждена. Сохраните локальные журналы.") \
    X(Disconnected, L"Disconnected. You can connect again.", L"Соединение закрыто. Можно подключиться снова.") \
    X(StartingClient, L"Starting the client in server location {0}.", L"Запускаем клиент в локации сервера {0}.") \
    X(StartingHost, L"Opening the original game menu. Choose a location in the game.", L"Открываем обычное меню игры. Локация выбирается в самой игре.") \
    X(HostLobbyOpen, L"Lobby open. You can start the server game before the client joins.", L"Лобби открыто. Игру на сервере можно запустить до подключения клиента.") \
    X(WaitingServerAssignment, L"Connecting. The client is waiting for a ready location from the server.", L"Подключаемся. Клиент ждёт, пока сервер загрузит локацию.") \
    X(GameCleanupUnknownDetail, L"The game exited, but cleanup could not be confirmed. Keep the local logs. Details: {0}", L"Игра завершилась, но очистка не подтверждена. Сохраните локальные журналы. Подробности: {0}") \
    X(GameExited, L"The game exited. Its result and cleanup are in the local logs.", L"Игра завершилась. Результат и очистка записаны в локальные журналы.") \
    X(GameNotStarted, L"The game did not start. MediaPath was not changed. You can try again.", L"Игра не запустилась. MediaPath не менялся. Можно повторить запуск.") \
    X(GameExitedMediaRestored, L"The game exited. The remote character was removed and MediaPath was restored.", L"Игра завершилась. Удалённый персонаж убран, MediaPath восстановлен.") \
    X(SceneUnloaded, L"The previous location has unloaded. The game stays open while the next server location loads.", L"Предыдущая локация выгружена. Игра остаётся открытой и ждёт готовности новой локации сервера.") \
    X(AdapterError, L"Adapter error. Waiting for its processes to exit. Details: {0}", L"Ошибка адаптера. Ждём завершения его процессов. Подробности: {0}") \
    X(LocationLoaded, L"Location {0} loaded. Waiting for both players to be in the same location.", L"Загружена локация {0}. Ждём, пока оба игрока окажутся в одной локации.") \
    X(StoppingGame, L"Closing the game and waiting for its processes to exit…", L"Закрываем игру и ждём завершения её процессов…") \
    X(StoppingGameMedia, L"Closing the game and waiting for MediaPath to be restored…", L"Закрываем игру и ждём восстановления MediaPath…") \
    X(FollowingLocation, L"The client is loading location {0} in the same game window.", L"Клиент загружает локацию {0} в том же окне игры.") \
    X(PrototypeTimeLimit, L"Time limit reached. Removing the remote character before closing the game…", L"Лимит времени достигнут. Убираем удалённого персонажа перед закрытием игры…") \
    X(SameLocationReconnect, L"Reconnecting to the same location. Waiting for the old character to be removed and both games to be ready.", L"Переподключаемся к той же локации. Ждём удаления прежнего персонажа и готовности обеих игр.") \
    X(NewSceneAssigned, L"The server assigned a new location. Removing the old character before the client can follow.", L"Сервер назначил новую локацию. Убираем прежнего персонажа перед переходом клиента.") \
    X(GuestSceneUnavailable, L"The client cannot follow from its current location. Waiting for its game to exit.", L"Клиент не может перейти из текущей локации. Ждём завершения его игры.") \
    X(ServerUnloadingScene, L"The server is leaving the previous location. Removing remote characters and waiting for the next location.", L"Сервер выгружает предыдущую локацию. Убираем удалённых персонажей и ждём следующую локацию.") \
    X(PlayersTogether, L"Both players are in the same location. Showing the second Bloom.", L"Оба игрока в одной локации. Показываем вторую Блум.") \
    X(ConnectionClosedGameOpen, L"Connection closed. The game stays open while the remote character is removed.", L"Соединение закрыто. Игра остаётся открытой, удалённый персонаж убирается.") \
    X(GuestLeftWaiting, L"The client disconnected. The server game remains open and waits for a new connection.", L"Клиент отключился. Серверная игра остаётся открытой и ждёт нового подключения.") \
    X(ServerLeftClosingGuest, L"The server disconnected. Closing the client game and removing the remote character.", L"Сервер отключился. Закрываем клиентскую игру и убираем удалённого персонажа.") \
    X(ClosingLobby, L"The game has exited. Closing the lobby.", L"Игра завершилась. Закрываем лобби.") \
    X(CoopError, L"WinxCoop error. Waiting for cleanup. Details: {0}", L"Ошибка WinxCoop. Ждём завершения очистки. Подробности: {0}")

    enum class TextId
    {
#define WINXCOOP_TEXT_ID(id, en, ru) id,
        WINXCOOP_LAUNCH_TEXT(WINXCOOP_TEXT_ID)
#undef WINXCOOP_TEXT_ID
    };

    inline constexpr const wchar_t* Text(TextId id, UiLanguage language) noexcept
    {
        switch(id)
        {
#define WINXCOOP_TEXT_CASE(id, en, ru) case TextId::id: return language == UiLanguage::Russian ? ru : en;
            WINXCOOP_LAUNCH_TEXT(WINXCOOP_TEXT_CASE)
#undef WINXCOOP_TEXT_CASE
        }
        return L"";
    }
#undef WINXCOOP_LAUNCH_TEXT

    struct UiStatus
    {
        TextId id=TextId::None;
        std::wstring detail{};

        bool operator==(const UiStatus& other) const noexcept
        {
            return id==other.id && detail==other.detail;
        }
        bool operator!=(const UiStatus& other) const noexcept { return !(*this==other); }
    };

    inline std::wstring Render(const UiStatus& message, UiLanguage language)
    {
        std::wstring result=Text(message.id,language);
        const auto placeholder=result.find(L"{0}");
        if(placeholder!=std::wstring::npos)
            result.replace(placeholder,3,message.detail);
        return result;
    }
}
