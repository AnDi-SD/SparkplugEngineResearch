# Winx Remix: sampled weighted vertex inputs — 13 сентября 2026

Закрытый запуск `play-rtx-20260913-084907-707` подтвердил **179/179** sampled
пакетов weighted vertex inputs: CPU source bytes, записанный upload, общий layout
и текущие transport/owner границы совпали. Во всех samples — четыре влияния и
16 костей. **Weighted geometry не подавалась в Remix**, деформация и внешний вид
персонажей этим результатом не проверяются.

Игра PID **19572**: 08:49:08.961–08:58:18.807 МСК; server PID **22256** завершил
cleanup в 08:58:15.620. Выполнены переходы **27 → 4 → 27**. Это observer-only
проверка, **не A/B**. Автор checkpoint игру и GPU не запускал; журналы прочитаны
после проверки закрытия обоих PID. [Manifest](../../research/winx-remix-native-skin-vertices-checkpoint-2026-09-13.json)
фиксирует 254 артефакта, включая неизменные snapshots и отдельные регрессии.

## Точная граница данных

Новый собственный `winx_native_skin_vertex_source.h` вызывается только после
успешного численного сравнения Skin palette, на каждом 300-м кадре либо при
существующем диагностическом trigger. Исходный Skin producer и mesh call по-прежнему
исполняются. Новых игровых записей, COM-вызовов, деформации и API submission нет.

Пакет требует exact native mesh/VB/IB/declaration identities, общий captured
generation, действующий transport witness, отсутствие writable locks и полное
побитовое равенство известных CPU-source ranges с записанным D3D upload.
`native_mesh_source::Layout` должен совпасть с фактической declaration.
После чтения и анализа повторяются headers/identity/transport/frame/mutation
проверки. Это заимствование на время операции, не постоянное владение native объектами.

Исследуются только вершины, на которые ссылается uint16 index range; повторный
index внутри sample считается один раз. Допускаются native triangle list/strip,
до 65 536 вершин, 32 768 примитивов, 1…4 влияния и 1…256 костей. Проверяются
конечные позиции/веса/индексы, целочисленность и диапазон palette indices.
Весовые слова наблюдаются без изменения, нормализации или замены остаточным весом.
Общий [mapper исходных bytes](winx-remix-native-weighted-vertex-source-2026-09-13.md)
остаётся единственной реализацией packed-byte → float4 преобразования.

Negative weights и сумма весов вне `1±1e-5` — **диагностика**, а не автоматический
отказ observer или доказательство ошибки игры. CPU fixture и анализатор отдельно
проверяют допустимый sample с такими признаками. Нормали, tangents, окончательная
деформация и shader treatment не выводятся из правильной суммы весов.

## Закрытые samples и связь с палитрой

| Показатель | Результат |
|---|---:|
| Завершённые sampled attempts / accepted / rejected | 179 / 179 / 0 |
| Sampled frame rows | 13, диапазон0…3600 |
| Сумма уникальных referenced vertices по samples | 104 512 |
| Negative-weight vertices / nonunit-sum vertices | 0 / 0 |
| Min / max weight | 0 / 1 |
| Максимальная ошибка суммы весов | 8,940696716308594×10⁻⁸ |
| Совпавшие joins с native palette records | 179 / 179 |

104 512 — сумма с повторным учётом между samples, **не число уникальных вершин
игры**. В начале Алфеи записано по11 samples в трёх кадрах, в Домино по12 в семи,
после возврата по31 в двух; frame0 пустой. На момент sampled callback оригинальный
palette record имеет тот же `(frame, modelCall, submission)`, scene/Skin/mesh и
boneCount. Все 179 joins проверены по закрытым файлам; такая связь не распространяет
выборку на кадры без sampling.

Полный palette observer: 78 559 исходных вызовов, 656 AL-false и 656 без собственного
mesh; 77 903 попытки = **53 445 matched + 24 458 scene rejects**. Сравнены 855 120
матриц костей, численных и побитовых расхождений нет. Возвращённая Алфея сохраняет
отдельный набор **31 matched из39 calls**, включая один AL-false и семь scene rejects.
Это не 100% Skin coverage; причины составного scene guard не объявляются UI.

Light analyzer: **35 132 matched и used из35 132 попыток**, mismatch/dirty/invalid
и нарушений подписи comparison нет. Все эти кадры одного режима, поэтому новый
A/B-результат здесь не заявляется. Mesh server audit: **2 309 580 API SUCCESS**,
errors/invalid handles=0, `queue_exit_normal`; SUCCESS не доказывает GPU completion
или работу weighted API. Во всех четырёх журналах предел16MiB не достигнут;
vertex/palette незакрытого sample suffix нет.

Переходы: Домино loaded/presenting за20,07 с и ещё59 кадров; возвращённая Алфея
за10,01 с и ещё24 кадра. Сохранены `alfea-vertices.png`, `domino-vertices.png`,
`alfea-return-vertices.png`; screenshot не заменяет проверку геометрии.
Сообщение OMM memory budget сохранено. Изменение физического света не заявляется.

## Проверки и воспроизводимость

Новый [строгий анализатор](../../research/rtx-remix/analyze_native_skin_vertices.py)
переиспользует PID/immutable-file/JSON/uint helpers palette analyzer. Проверяет
schema, конечность и границы, монотонные frame/call/submission, reason0 для accepted
и1…7 для rejected, generation и **точное равенство frame counters записанным samples**.
Rejection6 может содержать готовый Summary после успешного Inspect и отказа final
fence; exception7 допускается до либо после Inspect. Частичный suffix, cap prefix
и diagnostic values учитываются отдельно.

- **95 checks / 9 CPU tests**, 0,340 с: corrupted rows, счётчики, finite/unsigned,
  sequences, cap, suffix, failure history 64, immutable/PID gates. Среди них17
  настоящих sample rows CPU writer проверены по отдельности с собственными
  synthetic init/frame, без изменения payload. Это не искусственный continuous run:
  повторные fixture sequence909 в полном наборе правильно отклоняются.
- Production CPU fixture **122 PASS**, включая89 parser и31 integration checks;
  без game/native instructions/COM/GPU. Все **75** hashes исходного CPU checkpoint
  проверены повторно; source header **E073A628…8042**.
- Отдельные исправленные wrappers: SurfaceResources **9 379 PASS**, remaining0;
  NativeCamera **165 PASS** на system D3D9 HAL; MaterialChannels **697 PASS**
  с recording Remix API. Последние два — не runtime bridge и не игровые skin tests.
  Первые sandbox HAL attempts отказали после успешной компиляции; оригинальные
  run/build/stderr сохранены. Fresh v2 прошли. Исправление двух `Copy-Item` вместо
  array `Join-Path ChildPath` не изменяло установленную DLL.

Adapter **6CCBE15208923494CCF9F4FBA63A8B4E99D3FCC1A29D868BE3B279F64A2DB5BA**
собран из `vertices-v1/source`; **42** frozen source hashes совпали с install receipt.
Client **4BB7BB4F…783E**, server **A99FEF75…E4F**, stock renderer **F7C31082…09F**,
игровой EXE **C27EA9DB…62CDB** неизменны. Полные hashes и команды сохранены.
Watcher восстановил bridge config в08:58:20.109; семь ini/user/bridge/saves файлов
побитно совпали с before. Новые последующие исследования renderer и текущие
исходники к этому frozen runtime не приписываются.

Следующая проверка деформации остаётся необходимой: отдельный
[парный GPU fixture](winx-remix-skin-api-paired-checkpoint-2026-09-13.md) ранее
подтвердил ошибочный B2–B4 путь stock renderer. Успешный исходный weighted packet
его не исправляет и не подтверждает правильность прямой подачи персонажей.
