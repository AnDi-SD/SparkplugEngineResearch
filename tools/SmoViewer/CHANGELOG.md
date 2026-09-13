# Changelog

## Unreleased

## 0.5.0 — 2026-08-24

- панель «Слои» помещена в вертикальный `ScrollViewer`: все настройки, список
  костей и служебные объекты доступны без разворачивания окна на полный экран;
- добавлен динамический выбор OpenGL-сглаживания `Выкл. / MSAA 2× / 4× / 8×`.
  По умолчанию используется MSAA 4×; собственный multisample framebuffer
  пересоздаётся при смене режима или размера viewport и разрешается в framebuffer
  `GLWpfControl` без перезагрузки сцены;
- удалён устаревший слой просмотра «Сделать модель полупрозрачной»;
- princess-style `FinalBlendOp=2` теперь получает прозрачный проход только когда
  UV-покрытие содержит одновременно полностью прозрачные и полупрозрачные texels.
  Это сохраняет graduated alpha тиары `[83]`, но возвращает sibling chunks
  `[85]`, `[87]`, `[89]` в opaque pass: они затрагивают лишь partial-alpha края
  atlas и не покрывают ни одного texel с alpha 0;
- убрана перекрывающая сцену красная плашка `NATIVE-RISK DIAGNOSTIC`: эти
  исследовательские предупреждения остаются в журнале и подробностях ресурса;
- OpenGL projection теперь трактует WPF `PerspectiveCamera.FieldOfView` как
  горизонтальный угол и переводит его в корректный vertical scale с учётом
  aspect ratio. Skeleton, attachment, collision, control-rig и marker overlays
  снова пиксельно совпадают с моделью; framing и скорость pan используют тот же
  вертикальный угол обзора;
- весь видимый SMO viewport теперь использует единый OpenGL 3.3 renderer:
  статические и анимируемые rigid mesh, skinned mesh с GPU palette skinning,
  texture sequences, UV1 и подтверждённые base/effect layers. GUI-сцены получают
  orthographic projection, SAN playback обновляет bone/model matrices прямо на
  GPU. WPF оставлен только для невидимой hit-test geometry и аварийного fallback,
  если OpenGL context недоступен;
- vertex layout `0x1900` со stride 32 теперь декодируется как
  `XYZ + diffuse ARGB + UV0 + UV1`. Из-за отсутствия этого layout меш `[4085]`
  `WallA-000` в `Alfea_broken_01.smo` терял запечённые vertex RGB и отображался
  голубым служебным цветом Viewer; теперь используется записанная в SMO тёплая
  окраска. Материал стены не содержит texture object, поэтому текстура ему не
  подставляется;
- textureless rigid lights с `FinalBlendOp=2`, companion state `2` и mixed-RGB
  vertex alpha от нуля к видимому значению теперь передают authored alpha в GPU
  и рисуются после opaque geometry. Это восстанавливает прозрачные
  `Ray_light_D01/E03/F04` `[1005]/[1018]/[1022]` в `Alfea_broken_01.smo`, не
  превращая в прозрачные обычные level-полы с небольшими служебными alpha;
- диагностика выбранного GPU mesh теперь называет фактическую texture или явно
  сообщает, что material является vertex-only. Для `[1078] poutreW06-000`
  подтверждена `marble2 [43]`, а контрастные линии принадлежат отдельному
  парному mesh `[1082]` с `linegen00`;
- исправлены две регрессии direct GPU viewport: native UV0 больше не получает
  лишний `1-V`, из-за которого картины и таблички отражались по вертикали;
  невидимая WPF-копия GPU mesh сохраняет прозрачный hit-test material, поэтому
  ЛКМ снова выбирает объект в сцене и раскрывает его в дереве без возврата
  CPU texture baking;
- основной viewport переведён с WPF texture baking на
  прямой OpenGL 3.3 path: исходные positions/indices, UV0, texture и vertex
  diffuse загружаются в видеокарту, а shader выполняет штатную модуляцию
  `texture × interpolated diffuse`. CPU triangle-atlas для таких mesh больше не
  строится; shared placements используют один VBO/EBO и различаются только
  model matrix;
- прозрачная геометрия сохраняет отдельный отсортированный проход, а сетка пола
  теперь рисуется в том же depth buffer и не просвечивает поверх стен;
- контрольный запуск pristine `Alfea03.smo`: decode 1,56 с, подготовка сцены
  0,20 с, первый GPU upload 0,085 с; 509 unique mesh buffers и 63 texture
  обслуживают 1 263 placements. До удаления CPU-atlas подготовка той же сцены
  занимала 32,17 с, а working set составлял около 934 МиБ; прямой path использует
  около 281 МиБ (замеры одной локальной debug-сборки, не универсальный benchmark);
- Viewer принимает пути `.smo` из командной строки и пишет отдельные времена
  decode, подготовки сцены и первого GPU upload в журнал;
- разрешение private triangle-atlas теперь определяется одинаково для любого
  textured mesh: из максимального UV-размаха его треугольников в texels исходной
  texture, с крайним отсчётом и защитными полями. Название, размер и назначение
  объекта в выборе качества не участвуют; `[2756] plaque02` из `Alfea03.smo`
  получает ячейку 133×133 для сохранения 128 texel-интервалов вместо прежней
  32×32. Единый предел памяти WPF применяется как техническое ограничение и не
  выдаётся за свойство ресурса; при выборе mesh журнал отдельно показывает
  требуемую и фактически выделенную ячейку и явно отмечает срабатывание лимита;
- `Alfea03.smo` теперь открывается без 11 ложных texture/render issues: добавлен
  строгий decode mipmapped BGRA `0x0EE3`, статический двухслойный material
  `crystal2(UV0) + cryst_hl(UV1)` и подтверждён второй rigid effect tuple для
  book glow/spark;
- штатные `FinalBlendOp 0x4/0x5` WPF-приближения остаются доступны в подробностях
  материала, но больше не попадают в журнал как проблемы загрузки;
- то же универсальное правило даёт геометрии `Garch_B01` из `Alfea01.smo`
  ячейки 69×69: 64 texel-интервала `noise03b`, один конечный отсчёт и по два
  защитных пикселя с каждой стороны;
- добавлено точное разрешение shared material references из level-`spModel`:
  поле type `0` с `{materialObjectId, 0}` теперь восстанавливает ранее
  сериализованный `spMaterialData`; `[3795] centerhallL01-001` получает настоящую
  `caro00` вместо размазанной служебной vertex-colour карты (50 таких ссылок в
  `Alfea01.smo`, 17 в `Alfea02.smo`, 19 в `Alfea03.smo`);
- восстановлена имитация отражающей плитки `[3785] centerfloorR-000` из
  `Alfea01.smo`: shared `floor02` умножается на равномерную vertex alpha `0xD8`,
  рисуется после непрозрачной `RF`-геометрии под полом и больше не скрывает
  сериализованный дубликат «отражения» полностью;
- private triangle-atlas теперь поддерживает до 2 048 треугольников: дверь
  `[381] H_DoorB07` из `Alfea01.smo` корректно сочетает почти белую `noise03b`
  с 199 authored vertex colors вместо их конфликтного запекания в общую 64×64
  texture; то же исправление применяется к однотипным `[390]`, `[399]` и `[408]`;
- добавлено распознавание reference-only `spModel`, ссылающихся на общую
  `spMeshData`: `Alfea03.smo` содержит 510 физических mesh и 757 ссылочных
  размещений 156 источников; `[2674] casierD16` теперь виден во всех 22 authored
  размещениях вдоль стен;
- shared mesh instances явно помечаются в дереве как
  `Mesh instance → [источник]`, отдельно считаются в корне, статусе и журнале, а
  выбор физического mesh подсвечивает все его размещения в собранной сцене;
- SmoExporter 0.5.0 сохраняет shared-instance семантику в GLB/FBX и предлагает
  явный выбор между ссылками и запеканием; OBJ разворачивает размещения с
  предупреждением из-за ограничений формата;
- исправлено отображение повторяющихся UV и vertex-diffuse tint в WPF: текстурные
  меши `Alfea02.smo` `[176]`, `[2355]`, `[2479]` и `[2522]` больше не выглядят
  белыми; нетекстурированный `[2705]` использует сохранённый вершинный RGB/alpha
  вместо белого fallback;
- WPF-материалы сохраняют абсолютные координаты исходного texture atlas вместо
  растягивания UV-области меша на всю картинку; исправлено натяжение и окраска
  кровати `[1741]`, подушек `[1753]` и ковра `[2059]` из `Alfea02.smo`;
- подтверждён vertex layout `0x0840`: stride 32, normal XYZ по `+12`, UV0 по
  `+24`, без diffuse stream; меш `[284]` (`dseed`) из `Alfea02.smo` теперь
  использует свою коричнево-золотую texture вместо серого fallback;
- для rigid-мешей с разными vertex RGB на общих UV Viewer создаёт внутренний
  triangle-atlas вместо усреднения цветов в одной texture; дверная рама `[681]`
  сохраняет зелёные vertex colors, а `[689]` корректно сочетает `noise04w` с
  голубыми, фиолетовыми, жёлтыми и зелёными деталями;
- textureless materials в больших level-графах сохраняют diffuse и render state;
  одинаковый vertex RGB с меняющейся alpha распознаётся как authored
  alpha-gradient, поэтому `DVlight04` `[2929]` сохраняет прозрачность, но
  рисуется после непрозрачной стены;
- UV-alpha analyzer поддерживает целочисленные repeated tiles; chandelier glow
  `[2863]` с texture `chand` теперь сохраняет partial alpha и рисуется после
  непрозрачного chandelier body `[2859]`, устраняя ложные белые полосы;
- uniform vertex tint у non-skinned level meshes модулирует всю texture;
  листва `[993]` из `Alfea01.smo` теперь сочетает `leaf01` с авторским
  зелёным `0xFF3EAB00` вместо белой alpha-mask;
- добавлено структурное распознавание плоских GUI-сцен и условная панель
  **2D / GUI** с выбором корневого экрана, состояния элемента и отображения
  `GUICollision`;
- GUI-сцены можно кадрировать ортографической камерой, перемещать средней кнопкой
  мыши и масштабировать колесом; скрытые экраны и состояния исключаются из
  hit-testing;
- выбор объекта в дереве синхронизирует GUI-экран, visual state и отображение
  `GUICollision`, после чего подсвечивает объект в контексте всей собранной
  панели; двойной клик кадрирует панель, а не отдельный mesh;
- node-only 2D-layout вроде `gameover.smo` распознаётся без привязки к имени
  файла по сочетанию `spNode`-графа, `text_*` leaves, state-ветвей и transforms;
  runtime-слоты отображаются как явно диагностические карточки, поскольку mesh,
  фактического текста и шрифта в таком SMO нет;
- исправлено накопление transforms в partitioned-уровнях: serializer containment
  больше не превращает `sector/portal` intervals в пространственных родителей,
  а world-матрица `spStaticRenderObject` завершает placement chain; стены и
  static props `Alfea02.smo` больше не получают сумму центров нескольких секторов;
- Inspector выводит число плоских, state- и collision-mesh, наличие text-классов
  и корневые GUI-группы;
- corpus-проверка `igmenu_opt_pc.smo` закрепляет 99 строго декодированных
  плоских mesh, 60 state-mesh, 2 `GUICollision` mesh и 13 mesh ветви `display`.

## 0.4.2 — 2026-08-24

- suite обновлён до SmoExporter `0.4.0` и SmoImporter `0.5.0`;
- общий комплект содержит один `native/`-каталог с `SmoFbxBridge.exe`,
  `libfbxsdk.dll` Autodesk FBX SDK 2020.3.10 и лицензией SDK; Exporter и Importer
  находят его без Blender, Python и пользовательской настройки пути;
- версия Viewer и release metadata обновлены до `0.4.2`; собственный parser,
  рендер и native validator Viewer функционально совместимы с `0.4.1`.

## 0.4.1 — 2026-08-22

- вкладка «Игра» разделена на секции «1. Патчи» и «2. Проверка модели в игре»;
  настройки запуска проверки по умолчанию свёрнуты, а ход нативной загрузки
  входит в общую прокручиваемую страницу и уезжает вместе с ней;
- в секцию «Патчи» добавлена кнопка «Отключить волосы Блум»,
  запускающая отдельный Winx Hair Patcher; найденный Viewer путь к `WinxClub.exe`
  передаётся отдельным аргументом и автоматически подставляется в патчере;
- полный suite-пакет теперь содержит `tools/WinxHairPatcher/WinxHairPatcher.Gui.exe`
  версии 0.2.0 и его документацию;
- окно «О программе» показывает фактически найденную версию Winx Hair Patcher
  вместе с Viewer, Exporter и Importer;
- отсутствие или ошибка запуска патчера обрабатываются сообщением без завершения Viewer.
- общий resolver наследует между повторными `spSkin` цельный material
  binding: texture, diffuse ARGB, alpha-blend state и layered/animation metadata;
- family/primary fallback выбирает ближайший согласованный binding без
  смешивания alpha-blend флагов разных материалов.
- исправлена ошибочная трактовка `FinalBlendOp` как битовой маски: `0x4` и `0x5`
  теперь диагностируются как отдельные effect-операции и получают явное WPF-
  приближение, а не обычный source-alpha material;
- Viewer декодирует полный массив из 11 `MaterialRenderStates` и проверяет
  consumer-specific состояние для `FinalBlendOp = 0x6`. Подтверждённая обычная
  skinned-поверхность использует точный tuple
  `[0,0,1,2,1,1,3,0,4,0,6]` и `spSkin.AlphaSortEnable = 1`; rigid- и skinned-
  effect-проходы распознаются по своим отдельным точным tuple. Несовпадение
  показывается как предупреждение вместо ложного «всё в порядке»;
- добавлена отдельная corpus-backed классификация tiara из
  `bloom_princess.smo`: `FinalBlendOp=0x2` считается princess-style прозрачной
  поверхностью только при точном tuple, `spSkin A0/P1`, чёрном vertex diffuse и
  фактически покрытом UV0 промежуточном alpha. Произвольный `op2` или alpha где-то
  в atlas по-прежнему не включают прозрачный проход;
- добавлен второй exact bound `FinalBlendOp=0x2` профиль
  `SkinnedTransparentSurfaceFinalBlend2`, подтверждённый Minautor mesh `[13]`:
  tuple `[0,0,1,0,1,1,3,0,4,0,6]`, `spSkin A1/P1`, black vertex diffuse и
  UV-covered partial alpha. Он использует transparent ordering без emissive;
- сочетание princess `RS[5]=0` tuple с `AlphaSortEnable=1` теперь не исчезает как
  opaque, а получает отдельное предупреждение
  `UNCONFIRMED_FINAL_BLEND_2_HYBRID` и явно приблизительный authored-alpha preview;
- крупные partial-alpha `RS[5]=0` surfaces получают
  `LARGE_ALPHA_RS5_ZERO_ORDERING_UNCONFIRMED` и viewport-баннер: изолированный
  WPF Viewer не может доказать порядок крыльев относительно foliage/level draws;
- добавлена проверка `imp_o_x_*` против retained body vertex diffuse. Сочетание
  `FFFFFFFF`/`FF000000` показывает `IMPORTED_FACE_VERTEX_DIFFUSE_MISMATCH` и
  баннер **MIXED FACE VERTEX DIFFUSE**; исправленный white-face fixture даёт
  ноль таких предупреждений;
- в «Слои просмотра» добавлен безопасный turntable
  **«Нативный свет: вращать модель»**: model root вращается относительно
  фиксированных WPF lights для поиска angle-dependent normals/diffuse различий;
  интерфейс прямо отмечает, что это не native lighting parity;
- добавлена структурная диагностика `NATIVE_ALPHA_RUN_GRANULARITY_UNCONFIRMED`:
  подтверждённый princess-style alpha-run с несколькими пространственно удалёнными
  компонентами и разными deform-targets больше не считается безопасным только потому,
  что WPF-кадр выглядит правильно. Viewer рекомендует сохранить исходные границы
  material/renderable runs и отдельно подчёркивает необходимость игрового теста;
- добавлена узкая диагностика `NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED` для созданных
  importer near-coplanar face overlays. Подозрительные runs глаз и рта скрываются в
  режиме **SIMULATED FACE-OVERLAY LOSS**, чтобы показать лежащую под ними opaque-
  поверхность и тот класс дефекта, который WPF source-alpha ранее маскировал. Баннер
  прямо сообщает, что это диагностическая симуляция, а не эмуляция native blend/depth;
- нативный loader smoke-test и WPF viewport явно отделены от проверки итогового
  кадра: успешная загрузка подтверждает структуру ресурса, но не правильность
  native blend/shading в игровой сцене.

## 0.4.0 — 2026-08-14

- добавлена четвёртая панель «Игра» со встроенным `SmoNativeValidator.Core`: автоматический/ручной поиск `WinxClub.exe`, поиск внутренних loader-вызовов по masked-сигнатурам независимо от SHA-256 и известных патчей, безопасная in-memory подмена запрошенного SMO, FFPS/resource-loader checkpoints, exception/timeout diagnostics и постоянный JSONL-журнал;
- нативная проверка получила два маршрута: рекомендуемый быстрый smoke-test через ранний `Menus\mousecursor.smo` и контекстную загрузку исходного slot с `startLevel`; игра запускается в окне из изолированной временной папки с частными INI и копией `Shaders`, не меняя установку, реестр или пользовательские настройки;
- итог нативной проверки теперь показывается отдельной цветной карточкой с крупным однозначным verdict и коротким указанием последней подтверждённой стадии; верхняя часть вкладки прокручивается и заканчивается над trace/нижним журналом, а после теста сама доводит карточку до видимой области; адреса, регистры и полный trace остаются в JSONL-журнале, а недоказанный фоновый crash не выдаётся за поломку модели;
- native debugger распознаёт обычные и WOW64-коды software breakpoint/single-step (`0x80000003/04` и `0x4000001F/1E`), поэтому собственные checkpoints валидатора больше не классифицируются как crash игры;
- добавлен последовательный `SmoNativeValidator.Cli` с отдельным JSONL на модель, общими `summary.json`/`summary.tsv`, кратким целевым выводом в консоль и tracked-манифестом десяти Bloom-моделей для регрессионного прогона; полный фоновый trace остаётся в JSONL и доступен через `--verbose`;
- `SmoNativeValidator.Core`, CLI и тесты остаются в исходниках Viewer: нативная проверка входит в `SmoViewer.exe`, но самостоятельный пакет или отдельный GitHub Release для Validator не формируется;
- оконные camera shortcuts больше не перехватывают клавиши в полях ввода панели «Игра»; logical-пути с `_`, `+`, `-` и другими редактируемыми символами вводятся штатно;
- почти полностью чёрный двухцветный diffuse-placeholder в character meshes больше не умножается на texture; mesh `[86]` в `Troll.smo` снова показывает встроенный atlas `troll` вместо чёрной поверхности;
- resolver распознаёт точную reference-only связь `spMaterialData` field `10` →
  `spTextureData`; общий атлас `stella_x` корректно назначается материалам головы,
  тела и крыльев после безопасной перестановки visual branches;
- в постоянные благодарности и окно «О программе» добавлен `kotwys`, автор независимого [STX GIMP plugin](https://github.com/kotwys/stx-gimp-plugin), за предоставленные исследование и исходный код и разрешение изучать и использовать полезные выводы.

## 0.3.1 — 2026-08-13

- на этом историческом этапе `FinalBlendOp` ещё ошибочно считался набором render
  flags, а `0x4` — общим alpha-bit. Эта интерпретация исправлена в 0.4.1:
  `0x4`, `0x5` и `0x6` являются разными операциями и требуют полного
  consumer-specific состояния;
- в разделе «Слои» добавлены выбор цвета фона, выбор цвета и переключатель адаптивной напольной сетки; оформление применяется к сцене сразу;
- однотонный vertex color у rigid-мешей без текстуры теперь отображается как их материал; очки mesh `[6]` у `knut.smo` и `knutAdventure.smo` восстановлены в записанном в модели чёрном цвете без ошибочного атласа персонажа;
- вложенные rigid-крылья `WingL`/`WingR` теперь наследуют атлас внешнего render-node; восстановлена текстура mesh `[29]` (и симметричного `[25]`) у `bloomx.smo`;
- двухслойные материалы с отдельной основой и анимированной additive-текстурой теперь распознаются и смешиваются по второму UV-каналу; чёрный фон кадров эффекта больше не перекрывает основу mesh `[103]` и `[105]` у `bloomx.smo`;
- rigid-аксессуары под анимируемыми `spRenderNode` теперь следуют своим SAN-трекам; исправлены отстающие очки mesh `[6]` у `Knut.smo`;
- исправлена анимация моделей с vertex layout `0x093E`: Viewer теперь декодирует их упакованные blend weights и palette indices вместо отображения статичной геометрии;
- в правую часть верхней панели добавлена кнопка «О программе» с версиями Viewer, Exporter и Importer, датой сборки, сведениями о создателе, ссылками на репозитории, описанием проекта и благодарностями;
- постоянная строка с описанием управления камерой заменена компактной кнопкой `?` рядом с выбором режима;
- подсказка при наведении показывает актуальное управление для режимов Blender и «Полёт», включая текущую скорость полёта.

## 0.3.0 — 2026-08-12

- дерево ресурсов, слои просмотра и анимации объединены в три переключаемые
  вкладки левой колонки; панели больше не перекрывают viewport;
- плавающая панель skeleton/palette без копирования стороннего интерфейса;
- bind-pose skeleton, отдельные attachment points и полупрозрачный режим модели;
- глобальный список костей с отображением локальных `spSkin` palette slots;
- подсветка mesh по фактическим ненулевым vertex weights выбранной кости;
- панель переименована в «Слои просмотра» и получила независимые слои collision
  volumes, control/IK rig и служебных markers;
- выбранный collision volume выделяется утолщённым красным каркасом, выбор
  синхронизирован между деревом и списком служебных объектов;
- единая подсветка и кадрирование по двойному клику для mesh, skeleton/attachment,
  collision, control/IK nodes и пространственных markers;
- отдельное меню анимаций с автопоиском рядом с SMO, добавлением SAN/ANM-файлов
  и выбором произвольной папки;
- checkbox-фильтр по подтверждённым ANM-наборам (`Bloom`, `AdvBloom`, `Bird` и
  другие), включая множественную принадлежность SAN и группу файлов без ANM;
- список анимаций очищается при смене модели; при отсутствии SAN/ANM рядом с SMO
  автоматически подключается `Media/Characters/Bloom` текущего дерева игры;
- timeline с ползунком, play/pause и покадровым переходом;
- декодирование PC SAN `0x56EE563A`, интерполяция position/rotation/scale tracks
  и CPU skinning позиций/normals через inverse-bind matrices;
- animation world pose учитывает промежуточные control nodes: `C-lowerRoot` →
  `Pelvis` и `C-upperRoot` → `Spine_01`, устраняя складывание модели пополам;
- animation path не передаёт rigid `0x0940` meshes без blend arrays в vertex skinning и
  перехватывает ошибки применения позы; `Dragon.smo + dnaf.san` больше не
  приводит к закрытию viewer;
- position-only render nodes используют identity rotation; это исправляет
  transform `Horns_03`/mesh `[49]` в `Dragon.smo`, а rigid детали теперь следуют
  за ближайшей анимированной bind-костью без vertex skinning;
- geometry анимируемых rigid attachments не замораживается WPF `Freeze()`, поэтому
  обновление `Positions` не вызывает `InvalidOperationException`;
- инспектор проверяет SAN: каталог Bloom прошёл 168/168 клипов без ошибок;
- объекты `85`, `88`, `91`, `92`, `95`, `119`, `120` из `bloom_jeans.smo`
  классифицированы и отображаются визуально либо в информационном списке;
- предметные названия и сортировка ресурсов в дереве (`Palette`, `Mesh`, `Bone`,
  `Attachment`, `Material`, `Texture`);
- аудит прототипа Butermix: учтены batch scene export, UV Editor direction и
  исправление SharpGLTF `ToGltf2`; авторство закреплено в `ACKNOWLEDGEMENTS.md`.
- кнопки «Экспорт…» и «Импорт…» запускают вложенные release-приложения с текущей
  моделью; каталог подключённых анимаций передаётся экспортеру с точными ANM-подписями
  и группами без повторного поиска.

## 0.2.0 — 2026-08-10

- декодирование `spSkin`: иерархия костей, palette, inverse-bind matrices и rigid placement деталей;
- новые PC vertex layouts и texture formats для character assets;
- корректное связывание нескольких равновеликих atlas по object order;
- UV1 и двухстадийные материалы с аддитивными texture sequences;
- mesh-local vertex-color modulation с gutter вокруг UV-островов;
- поддержка GUI mesh layout `0x0100`, дополнительные class ID и fallback-цвета;
- расширенные corpus checks и документация формата.

SAN/ANM-анимации, динамическая skin deformation и полный gameplay graph пока не реализованы.

## 0.1.0 — 2026-08-08

Первый исследовательский релиз SmoViewer.

- строгий разбор контейнера FFPS/SMO и object graph;
- PC mesh layouts, triangle list/strip и диагностика неподдерживаемых данных;
- декодирование встроенных ABGR/BGRA текстур и UV0;
- vertex diffuse и material colors;
- node и `spStaticRenderObject` transforms для составных уровней;
- преобразование left-handed Sparkplug world в WPF right-handed world;
- режимы камеры Blender и свободный полёт;
- ленивое дерево объектов, связи, 3D picking и кадрирование выбранного объекта;
- многострочный журнал загрузки и диагностики;
- corpus regression tests на выбранных игровых SMO.

Известное ограничение: часть root-level `spRenderNode` в больших уровнях остаётся
неотличимой от неинстанцированных prototype assets. Полное восстановление scene graph,
skin/skeleton, animations и SPT/SPL gameplay layer продолжается.
