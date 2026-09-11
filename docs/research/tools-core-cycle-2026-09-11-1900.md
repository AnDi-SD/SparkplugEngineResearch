# Цикл ядер tools 11 сентября, до 19:00 МСК

Начало: 11 сентября 07:50 МСК. Общий ориентир готовности ядер — 14–15 сентября.
Пользователь возобновил разработку и разрешил самостоятельно применять
ускоряющие технические решения. Полный разбор EXE остаётся долгосрочной целью.

Исходный срез: [семь ядер на 10 сентября](tools-core-migration-status-2026-09-10.md).
Последующий цикл EXE дополнил доказательства, но не изменил production C++.
Повторно считать уже перенесённые readers/inspectors отсутствующими нельзя.

Первый общий пакет — один host encoder FFPS/FAT вместо отдельных writers
Editing/Importer/LVLcreator (TextureTool использует общий Editing). Прямой
original whole-file writer не установлен; сохраняем это ограничение. Новое
разрешение пользователя позволяет реализовать ранее описанную техническую
замену без отдельного ожидания. Цель — точное сохранение существующих outputs,
raw names, ID и opaque payload, последовательная запись без полной лишней копии.
Проверка — реальные операции старых потребителей, общий reader и адресная
совместимость с оригинальным чтением; время и allocations измеряются отдельно.

Следующая очередь уточняется по результату первого пакета: общая sort policy
и Occlusion, перенос Particle Init, свет/multipass, остатки операций импорта.
Ядро считается готовым по законченным необходимым операциям, не по числу тестов
или экспертному проценту EXE. UI и исторические спорные случаи учитываются явно.

## Блок 1 — общий envelope encoder

[Результат и границы](tool-shared-envelope-2026-09-11.md): единый host FFPS/FAT
encoder подключён к трём прежним writers, leaf replacement и общему native
graph producer. Подтверждены 23 побайтных сравнения операций, 17 boundary checks,
три actual PC whole-reader, native suites и настоящий TextureTool (2 PNG/6 замен).
Новые баллы изученности EXE не начисляются: это внедрение и согласованная
техническая замена. Устойчивое ускорение runtime не подтверждено, duplicate
encoder убраны; полный дополнительный data buffer для Stream не вводился.

Следующий пакет — принадлежащий runtime сцены LightManager и чтение actual
RenderNode light cache. Подготовка уже описана в
[досье Icy](tool-viewer-light-cache-boundary-2026-09-10.md); новые алгоритмы выбора
света не требуются. UI redesign не нужен.

## Блок 2 — свет загруженной сцены

[LightManager подключён](tool-viewer-scene-lighting-2026-09-11.md) к общему native
runtime, managed API возвращает cache по конкретному RenderNode. 56 адресных checks
на настоящем Icy, native RenderNode/SkinRender 2/2 и сборка Viewer прошли. Игра и
reconstructed selector не изменены; активация и окончательное обновление после позы —
явная host preview policy. GPU использование выбранного света остаётся следующим шагом.

Далее — общая версия CRT sort для geometry/Alpha и подключение Occlusion Init.
Ожидание выбора из прежнего proposal снято новым разрешением пользователя. Историческая
версия игровой CRT неизвестна; выбранная версия и её доказательства будут названы явно.

## Блоки 3–4 — общий sort и Occlusion runtime

[Версионная CRT policy](tool-shared-sort-policy-2026-09-11.md) реализована один раз
для geometry и Alpha. Десять original cases дали точное совпадение перестановок
и 222 comparator calls. [Occlusion Init/reader/world](tool-occlusion-core-2026-09-11.md)
перенесены и подключены к общему PC loader: race_02 теперь загружается целиком
(7513 objects/319 Node/2 Occlusion), создаёт сцену и LightManager за 0,208 s.
Пять original fresh objects и их world updates сравниваются по 1330 словам полного
поддерживаемого состояния. Ошибка harness cleanup исправлена; свежий пакет прошёл
с освобождением всех tracked allocations. Старые runtime lifetime границы сохранены.

## Блок 5 — начальный CPU-пул частиц

[Общий Particle Init](tool-particle-cpu-init-2026-09-11.md) подключён к reader
и доступен через C ABI/C#. Настоящий PC2 `Menus/bg.smo` теперь загружается:
539 первоначальных записей совпадают с игрой побитно. Десять native cases
покрывают 17818 original words; шесть новых cases собраны одним guest за 1,273 s.
Старые 10 managed fixtures/67 checks не нарушены. Исправлены найденные сравнением
ошибки новой ветки вставки в кольцо и подготовки world input в тесте.
Frame simulation/render, capacity выше 1024 и PS2 ещё не закрыты; пределы явные.

## Блок 6 — адресация размещений в LVLcreator

[Команды редактора](tool-lvlcreator-occurrence-commands-2026-09-11.md) теперь
используют actual container/member slot. Menu открывается с 207 placements,
Alfea02 сохраняет 1008. Movement/undo/redo/selection export/save job/reload
прошли: меняется один настоящий Node, его повторные slots сохраняются,
остальные размещения не двигаются. GUI получает только необходимую передачу
ключа; CoreTests/GUI builds прошли без warnings/errors. Исправлен устаревший
test picking без GPU provider; 63 прежние editor assertions прошли.

## Блок 7 — общая подготовка материала к рисованию

[Native material submission](tool-material-submission-2026-09-11.md) передаёт
современному backend все проходы, actual translated states, textures и UV.
Четыре SMO дали 2184 материала/2236 проходов; native suites 3/3 и C ABI lifetime
checks прошли. Исправлены ожидание отключённых UV и Python byte-buffer slice
в новых тестах; игровые классы не менялись. Подключение этих результатов к
OpenGL — следующий самостоятельный блок, полный вывод света пока не закрыт.

## Блок 8 — OpenGL passes, texture stages и подключение LVLcreator

[Материальные проходы подключены](tool-opengl-material-passes-2026-09-11.md) ко
всем потребителям shared renderer через общий scene snapshot; Viewer обновляет
анимированные материалы на своём native graph. 31 pixel checks на RTX3070,
две реальные сцены (1008/1141 placements), skin/picking и 43 Viewer poses прошли.
Память level test около 249 MiB; кадры небольшого 192×192 target — 4,2–7,7 ms.
Исправлены оставшиеся GPU occurrence keys LVLcreator: actual window прошёл
833 checks. Ошибки новых тестов и границы сохранены в досье. Game lighting
shader, authored mip chain и original Alpha sorting остаются следующими шагами.

## Блок 9 — выбранный свет в вершинном шейдере персонажей

[Общий shader-lighting consumer](tool-shader-lighting-2026-09-11.md) передаёт
actual RenderNode cache через оригинальные PC constant producers в OpenGL.
61 native projection assertions, 156 checks Icy/SAN, 21 GPU numeric checks,
31 прежняя material check и 43 Viewer poses прошли. Icy показывает собственный
vertex color: его ambient product имеет нулевой RGB, что подтверждено отдельно.
Новые тестовые ошибки порядка DTO цветов/округления/ожидаемого вклада света
исправлены; оригинальные алгоритмы не менялись. Rigid fixed-function lighting,
custom shaders и полный игровой frame остаются отдельными границами.

## Блок 10 — игровой alpha gate и порядок прозрачности

[Общий native alpha расчёт](tool-alpha-ordering-2026-09-11.md) заменил C# sort
по центру геометрии. Учитываются настоящий material/pass gate, sphere,
unsigned priority, выбранная CRT и полная occurrence identity. Alfea02:
20 queued units из 160 raw AlphaSort; Icy: 6 из 12. 89 C ABI checks с пятью
старыми original captures, 20 GPU integration checks, 43 Viewer poses и 207
размещений реального окна LVLcreator прошли. Полный original queue/frame не
заявлен: современный batch и правила камеры названы явно.

Следующая найденная зависимость — `spTextNode/52E86EFE`: отдельный файл
`Media/Menus/menu.smo` пока блокируется на object ID26. Это реальный открытый
тип loader, обнаруженный при неверном выборе контрольного меню; правильный
`igmenu_opt_pc.smo` прошёл. Приоритет — восстановить нужный класс и reader,
чтобы остальные ресурсы документа стали доступны ядрам.

## Блок 11 — Text CPU runtime и Font atlas ownership

[Общие Text классы](tool-text-runtime-2026-09-11.md) теперь выполняют настоящий
byte-string measure/layout и derived TextNode attach/detach/clear. 246 original
words совпали; native regression672 checks прошёл. Исправлена доказанная ошибка
Font: атлас — runtime `spTexture`, а не `spTextureData`. Fresh original reader
и destructor подтвердили владение настоящим DXTexture. Icy graph не нарушен.

Меню дошло до legacy atlas ID30 и пока отклоняется strict texture reader.
Следующий шаг — явная совместимая загрузка сохранённых legacy pixels в host
слое с диагностикой. Она разрешена новой автономией11 сентября, но не будет
объявлена восстановленным compatibility path оригинальной игры.

## Блок12 — legacy pixels и доступ к настоящим Text/Font из C#

[Явный host adapter](tool-legacy-texture-and-text-graph-2026-09-11.md) использует
общий pixel reader и сообщает адаптированные IDs. Старое меню целиком загружено:
238 объектов/63 Nodes, десять Text/Font цепочек и один atlas. 52 native checks,
531 C ABI checks на трёх SMO, managed projections и Book GPU прошли. Strict
loader сохранён. Source-selection старой игры всё ещё не подтверждён.

Следующая конкретная зависимость:41 Mesh меню отклонён из-за индексов вне VB;
это выявлено GPU preflight и сохранено в final managed report. Text GPU также
ещё не подключён. Полный вывод меню пока не заявлен.

## Блок13 — геометрия старого меню

[Общая strip projection](tool-legacy-strip-projection-2026-09-11.md) устранила
41 host rejection: original reader действительно сохраняет два`CDCD` в конце
strip, но они попадают только в вырожденные окна. Raw indices неизменны;
проверяются реально выводимые triangles. Обе C# conversion routines удалены.
17 original,358 native и373 C ABI checks прошли; menu41 placements/1202
triangles теперь выводится на RTX3070. Icy и Alfea02 также прошли. Следующий
связанный пробел — GPU Text для оставшихся десяти occurrences меню.

## Блок14 — оригинальная Text geometry и настоящие материалы FontManager

[CPU generator](tool-text-geometry-2026-09-11.md) совпал с2484 original output
bytes в11 случаях;324 native checks и affected EngineCore прошли. Установлено,
что owning34/38 — материалы, а не Fonts; PS2 static path подтверждает ту же
структуру30/34. Старые fake-success startup flags устранены. Доказанная
граница: original long-word wrap зацикливается; host возвращает явную ошибку
по iteration bound. Следующий блок — общий GPU consumer этого генератора.

## Блок 15 — настоящий Text в Viewer и LVLcreator

[Общий GPU consumer](tool-text-gpu-2026-09-11.md) выводит десять Text меню:
41 Mesh + 10 Text, 51 passes и один atlas upload. Самостоятельный Text material
и замена atlas подтверждены свежим original run; unknown Font default power
сохраняется явно. Пустой Text не требует rendering dependencies. 471 C ABI,
175 managed, 634 native и 47 actual-window checks прошли; GPU toggle меняет
1592 components и точно восстанавливает изображение. Icy control сохранён.
В LVLcreator осталось 41 editable Mesh placement: generated Text не выдаётся
за новый сериализованный Mesh. Полный Text frame, writer и UI ещё не закрыты.

## Блок 16 — точные runtime mip-уровни на GPU

[Транспорт полной цепочки](tool-runtime-texture-mips-2026-09-11.md) заменяет
повторную OpenGL генерацию для настоящих runtime textures. Свежий original
loading.smo совпал со всеми 1364 GPU bytes; menu atlas совпал на всех десяти
уровнях, тогда как прежняя генерация отличалась на 20 636 components.
521 C ABI, 179 managed, 99 GPU и 1271 native checks прошли. Убрана дополнительная
копия upload arrays; проверена поздняя замена base preview с освобождением GPU
handle. Transient preview сохраняет явную backend генерацию. Изученность EXE
не повышается за интеграционный транспорт.

## Блок 17 — SkyBox в общем GPU backend

[Отдельный проход](tool-skybox-gpu-2026-09-11.md) использует camera-parent
world из общих Node/SkyBox и отдельную общую alpha queue. Шесть fresh original
worlds подтвердили также retained world position без inheritance. Четыре
реальные сцены, включая два SkyBox и Sky alpha, прошли GPU проверку;
Viewer/LVLcreator подключили 97 placements в настоящих скрытых окнах.
Мировая правка SkyBox без обновлённой local pose пока явно диагностируется
как SKY_EDIT_POSE; полный manager/visibility/Fog lifetime не заявлен.
Подбор файлов ускорен двухэтапным indexed SQLite запросом, без corpus scan.

## Блок 18 — перенос native SMO без угадывания ссылок

[Importer forest relocation](tool-native-visual-transfer-2026-09-11.md)
перешёл на общую reader trace. Найдена и исправлена реальная ошибка: старый
StellaX → Icy результат пропускал 29 ссылок AnimTexController и не загружался
общим reader. Новый файл отличается только этими ID sites. Материалы обоих
направлений, 38 animation keys и все runtime mip levels сохранены; 73 remap
и 26 material checks, две проверки target nodes прошли. Ветки удаляются batch,
выдача ID больше не сканирует каталоги. Промежуточный forward-reference сбой
исправлен порядком наблюдения исходного графа; ошибочная fixture и CLI timeout
описаны отдельно. Flat material import/preview guard пока остаётся.
