# Text в общем OpenGL renderer, 11 сентября 2026

Блок 15 цикла до 19:00. Viewer и LVLcreator получают настоящую геометрию
Text из общих `spTextRenderable/spFontManager`, с материалами и атласом SMO.
В `Media/Menus/menu.smo` доступны 41 Mesh и 10 Text: 51 GPU occurrence,
51 pass, один atlas upload. Редактор сохраняет 41 редактируемое Mesh placement.

## Оригинальный выбор материала

PC 437D6D–437D9C сохраняет прежний primary Material FontManager, назначает
материал Text и выбирает Font/color. PC 41F3F8–41F44B берёт primary, при NULL —
fallback; требует первый StdLayer первого pass и заменяет его owning texture
на image выбранного Font. В конце успешного Text draw 437E5E–437E61
восстанавливается прежний primary. Atlas в material при этом не откатывается.
NULL/empty строка возвращается из FontManager до зависимостей и GPU setup.

Общие `SelectPCDrawResourcesForAnalysis` и `BindPCTextAtlasForAnalysis`
выполняют подтверждённые участки. Visibility, callbacks и полный Render
не объявляются восстановленными. Host владеет manager своей draw scope и
передаёт его явно; singleton не выбирает другой активный документ.

В menu Text использует собственные материалы: например, Text ID27 → Material
ID28, Font ID29 → Atlas ID30. VertexAlpha=0 и address U/V=1 сохраняются;
defaults FontManager (vertexAlpha=1, U/V=3) не подставляются. Перед каждым
live Text draw общий метод снова назначает atlas, затем работает существующий
MaterialSubmission и общий controller/pass pipeline.

Fresh original `custom-original-run5`: 54 checks, четыре alignment cases,
1,200 s, arena 58144 bytes. Две настоящие DXTexture, два material producer,
Font/Text readers, выбор собственного material, замена atlas и teardown
выполнены оригинальными инструкциями. Все tracked allocations освобождены,
две borrowed device references возвращены. Setup/acquire/draw и COM refcount —
объявленные fixture leaves, не оригинальный GPU frame.

FTM1 fixture: 4794 bytes, SHA256
`D5AB7EC0239EED85CCB17500E1478757199A31413F7D297188731121DD1D0F83`.
Новые 864 geometry bytes совпали с оригиналом; вместе с прежним FTG1 suite
проверяет 3348 original bytes и 411 assertions.

## Неназначенный specular power

PC Font default material оставляет +B8 неназначенным. Original
`power-original-run1` подтвердил: 4BE180 всё равно копирует все 17 слов в
renderer +E4A4 и сохраняет borrowed owner +E47C (5 checks). Allocator CC —
не игровой default. Явный режим `ObserveUnknown` сохраняет 16 color floats,
переносит power как NaN и presence=false. Стандартный вызов по-прежнему
отклоняет неизвестный power. Узкий Font fallback consumer разрешает его
только в unlit color mode 2. C# использует nullable power, без выдуманного нуля.

## Подключение и проверки

Native TextView владеет graph lifetime, использует общий generator и общий
MaterialSubmission. C ABI проверяет точные extents и публикует output атомарно.
Пустой Text не требует Font/atlas и не создаёт draw pass. Для непустого Text
неизвестный default Font, неподдержанный первый layer и nonfinite upload
сообщаются явно. Уже выполненные original mutations при ошибке не откатываются.

`SmoLoadedText/SmoTextGeometry` — транспорт состояния, без C# layout/sampler.
`SmoPreparedScene.Texts` сохраняет identity container/member slot. OpenGL
помещает vertices в transient graphics buffer, который не попадает в Mesh
catalog или editable/export placements. Используются исходные ARGB/UV,
настоящая alpha sphere и общий native alpha ordering. Viewer передаёт текущие
Node worlds своего runtime; LVLcreator использует загруженную prepared scene.
Минимальное подключение Viewer направляет menu с Text через общий GPU путь,
включая соседние Mesh. Полный UI и редактор текста не переработаны.

Raw: `local-data/results/tools-core-cycle-20260911-1900/text-gpu/`.
Manifest: `research/tools-core-text-gpu-2026-09-11.json`.

- Native batch: TextGeometry 411, MaterialApply 162, ShaderLighting 61;
  три suites прошли за 0,21 s в последнем запуске.
- C ABI: 471 checks, включая ten Text, lifetime после освобождения внешнего
  graph handle, live draw, short/null output и empty Text. Empty fixture
  меняет только байт существующего literal в памяти, source не переписывается.
- Managed: 175 checks; общие Font/atlas DTO, материалы, geometry и отдельные
  Text occurrences. Source bytes неизменны.
- RTX 3070: 51 placements, один atlas; выключение Text меняет 1592 framebuffer
  components, включение восстанавливает изображение побайтно. Это 192×192
  asset preview, не сравнение с original game framebuffer. Live timings
  записаны в report; повторный atlas upload не требуется.
- Реальные окна Viewer/LVLcreator: 47 checks, по 51 renderer item,
  41 editable Mesh placement. Это проверка подключения, не полный UI regression.
- Icy — контрольный случай без Text: native graph и GPU shader-lighting.

Первый ABI fixture ошибочно ожидал default material меню. Original custom
probe потребовал восстановить выбранный singleton после создания второго
manager и освободить лениво созданный ResourceManager 75DB78. Эти fixture
ошибки исправлены в новых запусках; faulted guest не продолжался. Empty fixture
сначала ожидал длинный field header, затем ограничился единственным counted
literal внутри Text extent. Промежуточные captures и ошибки сохранены.

Открыты: системный Font producer, Text writer/clone, полный игровой frame и
callbacks, original PS2 geometry execution, текстовые editing/UI операции.
PS2 material structure из блока 14 остаётся отдельным static подтверждением;
PC GPU consumer не выдаётся за PS2 runtime. EXE score не меняется.
