# PC decoded Fog → renderer identity/device cache (CP78)

2026-09-07; pinned pristine PC EXE
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Actual43B910 читает три Fog payload;4AD390 вызывается с secondary
receiver(renderer+18). Поэтому current identity находится в baseCA20,
fallback — baseE4A0, не в CA08 geometry cache.

NULL input выбирает fallback. Равный current pointer возвращает1 до чтения
type/payload. Иначе pointer публикуется до проверки type.0 отключает state28;
1/2 включают28, задают35=type,38=raw density и34=ARGB;3 задаёт28/35,36=raw
start,37=raw end,34=ARGB. Каждый шаг проходит actual4B0A90: равное значение
подавляет device call, изменённое кэшируется после вызова независимо от
HRESULT. Callback уже видит новый Fog pointer, но старое cached state word.

Неизвестный type4 возвращает0 после сохранения pointer без device calls.
Повтор того же pointer возвращает1. Изменения color/start того же выбранного
объекта не читаются до переключения identity. Disabled оставляет остальные
state slots прежними. Raw negative zero, NaN payload и infinite word не
нормализуются; ARGB12345678 сохраняется без перестановки каналов.

**9 exact captures,160 native assertions**, source default **123/123**.
disabled/exp/exp2/linear/unknown/raw-linear/raw-density/failed-device/cache;
max164 instructions per renderer call,62640-byte monotonic arena, all tracked
owners freed. Source ApplyFogForAnalysis переносит identity/fallback/raw-state
поведение. NULL transition без fallback guarded до native dereference;
missing device callback — прежний явный cache helper guard. Initial zero
current/fallback даёт успешный no-op. Никакого GPU forward или cap increase.

```powershell
python research/native_workbench.py run pc-renderer-fog --deadline-utc 2026-09-07T16:00:00Z
```
