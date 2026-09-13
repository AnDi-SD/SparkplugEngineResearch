# Общий PC material-state mapping без renderer ownership

13 сентября 2026. Существующие CPU-преобразования material states выделены в
[`spPCMaterialStateMapping.h`](../../Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h).
Теперь native reader может передать собственные local caches и recording sink,
не подключая весь `spDXRenderer` и не создавая вторую таблицу в Remix adapter.
Пять прежних методов класса делегируют общему helper; публичные типы и signatures
в `spDXRenderer.h` сохранены. Это перенос существующей реализации, без изменения
игрового поведения или новой функциональной оценки.

| Общий helper | Подтверждённая native граница |
|---|---|
| `ApplyPCRenderStateCacheEntryForAnalysis` | `4B0A90`: equality gate, device call, затем cache write; HRESULT игнорируется |
| `ApplyPCMaterialRenderStateForAnalysis` | `4B0AD0`: raw write до dispatch/guard, mapping states1..10, state8 передаётся lighting |
| `ApplyPCMaterialColorSourceForAnalysis` | `4BDDB0/4BDE00`: raw source cache; 10/11/12 → device0/1/2 |
| `ApplyPCMaterialLightingForAnalysis` | `4BDB10`: mode/power/color/source mutation и порядок команд |
| `ApplyPCMaterialStateSetForAnalysis` | `4BB890`: lighting raw cache255, indices1..10 по порядку, per-state source selection |

Основание — [CP30](native-pc-material-state-map.md),
[CP32](native-pc-material-lighting.md), [CP33](native-pc-material-install.md)
и поздняя [проверка mutable global black](native-pc-skin-lights-render.md).
Старые native captures не пересчитывались. `globalBlackARGB`, packed color,
installed colors/power и source caches остаются явными входами: helper не читает
native globals и не создаёт «правильных» renderer defaults. ARGB conversion
использует прежний общий `spColorMath.h` с подтверждённым float32 coefficient.

Lighting и batch — templates для caller-owned структур. Batch принимает
`array<uint32_t,11>` raw/material, `const Overrides*`, `Lighting&`, callback
`int32_t(*)(void*,uint32_t,uint32_t) noexcept` и context. Overrides имеет прежние
`sources/count/selectors`; при отсутствии overrides нужен типизированный null
pointer. Mapping dispatch и нижний device cache остаются разными границами:
recording sink может записывать все ожидаемые значения либо вызывать общий
cache helper. Исходные host guards для NULL и unsafe table indices сохранены;
они не объявляются безопасными native branches.

`InstallMaterialForAnalysis` и его **copy-before-color-update** порядок,
producer/init/pass logic не изменялись. В `spDXRenderer.cpp` изменены только
include и пять wrapper bodies. Точная проверка **5/5 тел, 3341 символ** допустила
лишь замену имён внутренних вызовов на имена общих helpers. Весь остальной cpp,
публичный header, color helper и исходники двух tests совпали с before.

До пересборки сохранены старые EXE и **19 captures / 355 записей / 77 151 байт**:
6 material-map, 7 lighting и 6 installation/batch сценариев. После сборки только
`SparkplugDXRenderStateTests` и `SparkplugMaterialApplyTests` все captures
**побайтно совпали**. Обычные CPU tests: **1727/1727 и 162/162 PASS**.
Старый MaterialApply EXE выполнял160 checks; две проверки ObserveUnknown уже
находились в неизменённом source, но ещё не вошли в старый EXE. Их diagnostic
labels отсутствуют в before binary и присутствуют в after; увеличение счётчика
не является новым тестом mapping, написанным в этом блоке.

Отдельный **x86 header-only consumer** с собственными Lighting/Overrides и
recording/cache sink собран и слинкован без renderer header/library. Этот EXE
не запускался. Существующие CPU tests исполнялись на host; игра, native functions,
эмулятор и GPU в этом этапе не запускались. Первый вызов build.cmd с relative
forward-slash path не дошёл до компилятора; повторён с абсолютным Windows path,
первоначальный log сохранён. Ошибок компиляции или capture comparison не было.

Все source/EXE/capture hashes и команды — в
[манифесте](../../research/winx-remix-shared-material-state-mapping-2026-09-13.json),
frozen evidence — `local-data/rtx-remix/material-state-tests/extraction-v1`.
SHA256 общего header:
`7C6C2372CC2893C1BB88EB870AC6972BB06F996BF8FD91F6888E4BB763485DE7`.
Свежую same-input native эмуляцию этот refactor не заменяет и не заявляет:
оригинальное поведение подтверждено прежними CP, здесь проверена эквивалентность
переноса. Подключение native raw sources, lifetime и самостоятельная API-подача
остаются отдельными adapter этапами.
