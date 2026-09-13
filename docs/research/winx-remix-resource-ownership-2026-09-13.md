# Владение API-ресурсами Remix при отказах выделения памяти

13 сентября 2026. В общем собственном backend устранено окно между успешным
`CreateMaterial` / `CreateMesh` и выделением узла кэша: теперь узел создаётся
**до API-вызова**. CPU-проверка владения прошла **302 проверки**, прежняя проверка
pressure/TTL — **9379 проверок с 773 намеренными отказами**, остаток ресурсов —
**0**. Последующая проверка root на system D3D9 с recording Remix API прошла
**697 проверок**. Это дополнение к историческому
[resource-pressure checkpoint](winx-remix-resource-pressure-2026-09-13.md);
его результаты и файлы не переписаны.

Изменён собственный адаптер `research/rtx-remix/winx_surface_submit.h`, общий
для прежнего D3D пути и нового источника геометрии. Игровая логика, native ABI,
исходники Sparkplug/Winx и runtime bridge этой задачей не менялись. Это не
восстановленное поведение игры. Замороженный SHA-256 header:
`725E1C0CD2080D1C5D8080608F38E7B26F11995EE6DA041F420C3FA2544781C5`.
Он совпал в CPU snapshot, последующем system D3D9 snapshot и рабочем header
при подготовке отчёта. [JSON-манифест](../../research/winx-remix-resource-ownership-2026-09-13.json)
содержит точные хеши, источники, результаты и сохранённые ограничения.

Порядок владения теперь следующий:

1. Материал резервирует узел кэша с `handle=nullptr, usable=false`. Mesh сначала
   выделяет последовательные indices, затем такой же pending-узел и учёт bytes.
   `bad_alloc` на этих шагах возвращает `nullptr` до вызова API. Оба материальных
   entry point используют один `CreateSurfaceMaterialOwned`.
2. Любой ненулевой handle, записанный `Create`, получает владельца в уже готовом
   узле, включая результат с кодом ошибки или `std::bad_alloc` после записи
   handle. После API для этого больше не требуется выделять память. Выдать
   handle можно только при `SUCCESS`, неизменных retirement serial и frame.
3. Ошибочный либо устаревший output немедленно передаётся в `Destroy`. Если
   удаление отказало, узел остаётся в **quarantine**: `usable=false`, handle,
   bytes и ссылка mesh → material сохранены. Следующий запрос с тем же ключом
   не выдаёт этот handle и не создаёт второго владельца. Повторная очистка
   удаляет mesh перед его материалом. Отсутствие API удаления также не позволяет
   начать создание соответствующего ресурса.

Счётчики `surface*Creates` означают число полученных ненулевых handles, включая
ошибочные outputs; счётчики успешного удаления не включают отказавшие попытки.
В тестовом JSON `materialCreates` / `meshCreates` — число входов в recording
Create, включая случаи с нулевым output. Эти две метрики различаются.

`SurfaceResourceOperation` держит существующий recursive mutex. Повторный вход
в создание или pressure во время занятой операции отклоняется. Итераторы и
ссылки на узлы кэша не живут через COM/API-вызов: удаление копирует descriptor,
затем заново находит узел по key и handle. Запрос retirement увеличивает serial
и ownership epoch даже при пустом кэше. Если операция занята, запрос сохраняется
как pending и ограниченно обрабатывается на внешней границе. При непрерывном
reentry и отказах удаления pending может остаться; следующая операция делает
одну попытку обработки и отклоняется, если запрос ещё не завершён. Немедленная
успешная очистка при недоступном API не обещается.

Прежние пределы **512 meshes / 256 materials / 64 MiB**, TTL 300 кадров,
периодический обход раз в 30 кадров и forced retirement сохранены. Pressure
использует ограниченные массивы без heap scratch, защищает ресурсы текущего
кадра и входящий material. Отказавшее pressure-удаление того же ресурса не
повторяется в этом кадре; periodic/forced cleanup остаётся отдельным механизмом.

`SurfaceResourceEpoch()` и `SurfaceMeshCurrent(handle, material)` не вызывают
API и не выделяют память. Первый возвращает serial собственного resource cache,
отдельный от прежнего surface-role epoch и от native object generation. Второй
проверяет оба владельца и отвергает busy, pending retirement, quarantine,
отсутствующий mesh/material и переданный несовпадающий material. Это проверка
текущего состояния, а не долговременная аренда handle.

Оба материальных entry point также ловят `bad_alloc` при подготовке descriptor
и asset. `SubmitSurfaceOverlay` сохраняет флаг успешного `DrawInstance`: ошибка
выделения до него возвращает `false`, после него — `true`, чтобы исходный D3D
draw не повторился. Это свойство catch подтверждено чтением исходника;
отдельный end-to-end отказ диагностической аллокации после draw данным CPU
fixture не воспроизводился.

Проверки сохранены раздельно:

| Проверка | Результат | Граница |
|---|---:|---|
| `resource-ownership-tests/ownership-v2` — новый fixture | 302, PASS; 23 material и 10 mesh Create calls; 9 reentry rejects; remaining 0 | MSVC x86, recording API, реальные отказы `operator new`, без COM device, игры и GPU |
| Тот же snapshot — прежний `test_surface_resources.cpp` | 9379, PASS; 773 injected failures; remaining 0 | Реальные пределы кэша, pressure/TTL, dependencies, frame wrap, failed Destroy |
| `material-channel-tests/independent-submit-ownership-v1` | 697, PASS; native source 171, material 137, transport 53, texture 83 | Настоящий system D3D9 HAL device с production hooks и **recording** Remix API; не runtime bridge |

Новый CPU fixture проверяет отказ выделения material node, sequential indices
и mesh node до API; отсутствие новых аллокаций после успешного Create; null,
error и throwing outputs; quarantine и повторное освобождение; повторные ключи;
вложенные Create/pressure/retirement; retirement до появления первого узла;
смену frame во время Create; ограниченный повтор отказавшей очистки; защиту
подготовленной группы текущего кадра и noAPI fences. Оба реальных material entry
point вызываются с null device и заранее созданными asset-existence sentinels:
это подтверждает путь управления ресурсами, **не** экспорт или декодирование
DDS. Legacy entry point может загрузить системную `d3dx9_43.dll` для поиска
export; сам export в таком тесте не вызывается.

CPU wrapper `Test-ResourceOwnership.ps1 -Name <fresh-name>` сохраняет полный
source snapshot, stdout, stderr и exit/timeout/PID каждого процесса. Проверены
все **52 хеша** его source inventory. PID 19848 и 12752 завершились с кодом 0,
timeout отсутствует; каждый запуск ограничен 30 секундами. Первая попытка
`ownership-v1` сохранена как **compile failure**: fixture ошибочно обращался
к `position.x/y`, хотя API объявляет `float[3]`; исправлен только fixture.
Нативный код и GPU в той попытке не запускались. Число checks включает
повторные проверки владельцев recording API, это не число уникальных сценариев
или процент покрытия игры.

System D3D9 regression выполнен root после заморозки header: 35 submitted,
5 rejected, 10 mesh Creates, 9 material Creates, 2968 asset bytes. Проверен
сохранённый `result.json`, EXE и source snapshot с тем же header hash. Fixture
имеет 30-секундный watchdog. Отдельных build/stderr/process логов в этом каталоге
нет; stdout сохранён как `result.json`. Утверждения об успешном runtime bridge,
игровом запуске или исправленной видимости из этой проверки не следуют.

Полное время жизни renderer resources, device-loss recovery, произвольный
foreign callback/concurrency и все сцены остаются открытыми. Fixture считает,
как и прежний backend, что отказавший `Destroy` оставляет handle живым. Ошибки
транспорта и частичный перенос группы support через несколько `DrawInstance`
проверяются отдельными этапами root; этот checkpoint закрывает владение
ресурсами, но не объявляет independent scene submission завершённым.
