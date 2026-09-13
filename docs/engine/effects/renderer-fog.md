# PC decoded Fog → renderer identity/device cache

Неизвестный type4 возвращает0 после сохранения pointer без device calls.
Повтор того же pointer возвращает1. Изменения color/start того же выбранного
объекта не читаются до переключения identity. Disabled оставляет остальные
state slots прежними. Raw negative zero, NaN payload и infinite word не
нормализуются; ARGB12345678 сохраняется без перестановки каналов.
