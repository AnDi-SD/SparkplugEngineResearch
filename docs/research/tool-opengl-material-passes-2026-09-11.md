# OpenGL использует общие проходы материала

Блок 8 цикла 11 сентября. Новый backend потребляет
[общий native material submission](tool-material-submission-2026-09-11.md).
Игра и восстановленные классы не изменены. Это внедрение в программы;
EXE scores не увеличиваются.

## Подключение и работа

`SmoLoadedResources` сохраняет frame1 preview draws после immutable inspection,
до освобождения уже загруженного graph. Используются тот же material view и
те же texture DTO: второго graph load или второго полного pixel buffer нет.
Файловые bytes и ранее скопированные inspection DTO не изменяются. Ошибка
подготовки одного материала имеет отдельную диагностику, не уничтожает graph.
`SmoSceneBuilder` передаёт snapshots всем существующим OpenGL-потребителям.

`SmoGpuSceneRenderer` выполняет все passes и до восьми texture stages. Blend,
depth test/write/function, alpha test/reference/function, cull и fill берутся
из actual mapped states. У raw game state нет второй таблицы перевода в C#.
Поддержаны все определённые color operations из имеющегося native mapper;
неопределённые alpha operations и неизвестные inputs явно отклоняются.
Составление pixels и вызовы GL — наш платформенный код.

Vertex diffuse передаётся как authored stream; прежний поиск «похожего на
placeholder» цвета не применяется. Scene использует actual Model/Skin AlphaSort,
а старые corpus-tuple/effect approximations больше не задают raster states и
не порождают ложные preview warnings для подключённого common draw.

UV0/UV1 и native matrix layout проверены на GPU. Для weighted shader источник
всех UV stages — UV0, как в original `Fixed.rfx`; для rigid fixed-function
контракта действует mapped coordinate index. Sampler objects разделяют настройки
одной texture между stages/materials. Обрабатываются только активные stages:
однотекстурный материал не отправляет восемь наборов uniforms/sampler calls.

Viewer заимствует material runtime своего actual scene graph. Только материалы
с controller references вычисляются повторно; статичные snapshots используются
готовыми. Все возможные texture-animation frames регистрируются заранее.
Elapsed clock и draw order — явно host preview schedule, не game AnimationManager.
Оригинальные controller update/frame gates/last-bound owner сохраняются.
UI layout не изменён; Viewer и LVLcreator показывают ошибки material backend.

Контракт texture operations/аргументов сопоставлен с официальной документацией
[D3DTEXTUREOP](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
и [texture blending](https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-blending).
Это описание входной границы, Direct3D/COM не загружаются.

## Исправление LVLcreator

В его подключении GPU ещё терялся OccurrenceKey, хотя core-команды блока 6 уже
сохраняли actual support/member slot. Загрузчик render items, обновление world,
highlight/hide и picking теперь передают одинаковый полный ключ. Обновление
больше не группирует все slots по одному SceneObjectIndex. Выбор/дизайн UI прежние.
Synthetic pending placements ищутся по их фактическим render meshes.

Actual скрытое окно на `igmenu_opt_pc.smo`: **207 placements, 833 checks**.
Проверены GPU identities, highlight/hide, реальная core-команда и Changed handler,
Undo и совпадение picking keys. Полный интерактивный UI сценарий не заявляется.

## Проверка и измерения

RTX3070, OpenGL3.3. **31 pixel checks** на RGBA8 target: authored color,
modulate, два stages, два draw passes, alpha discard/equality, UV1, UV matrix,
разные sampler states одной texture, depth-never. 1,450 s для стенда целиком.

| Сцена | Размещения / проходы | Textures | Live frame samples, ms | Peak process |
|---|---:|---:|---|---:|
| Alfea02 | 1008 / 1021 | 85 | 5,063; 4,556; 4,230 | 249,6 MiB |
| Alfea01 | 1141 / 1180 | 63 | 7,715; 6,150; 5,791 | 248,8 MiB |

Измерены 192×192, MSAA off, три frames после warmup, включая GL.Finish;
сохранены PNG и readback counts. Это не fullscreen performance и не устойчивый
процент ускорения. На тех же scenes material backend errors отсутствуют.
Alfea02 сохраняет diagnostic неподдержанного renderable [410]; 5 и 4 Model
без material имеют прежний NULL_MATERIAL_RENDER_CONTEXT и host preview fallback.
Отсутствующий game default/current context не объявлен восстановленным.

Прежний GPU skinning: 11 numeric checks; GPU picking readback: 27 checks.
Actual Viewer handlers: 7 clips/43 poses, **13124 checks**, 4,737 s.
GuiTests и LVLcreator Release собираются без warnings/errors; это не выпуск.

Новые тесты исправлены по результатам запусков: неактуальная сборка после
ошибки PixelFormat alias была отклонена и остановлена 30s harness; исправлен
контекст вставки reset-кода. Level test сначала ошибочно требовал материал у
каждого Model, затем сохранил явные NULL diagnostics. LVL test держал старые
render items после штатного rebuild и передал placement DTO вместо scene mesh;
обе ошибки стенда исправлены. GLWpf foreground thread без показанного Dispatcher
требует явного завершения принадлежащего тесту процесса после cleanup.

## Оставшиеся границы

Scene-light shader consumer пока не подключён: Lit ещё использует явно
обозначенный editor light. Полный native lit/color/shader parity не заявлен.
Не закрыты shader-generated UV кроме подтверждённого weighted UV0, UV channels
выше1/automatic coordinate generation, flat shade и другие явно отказанные
состояния. Mip chain по-прежнему генерирует GL из подготовленного BGRA level0;
authored lower mips не подключены. Прозрачные объекты пока сортируются host
distance ordering; actual visibility/partition/Alpha priority sequence не заявлены.
LVLcreator использует initial material snapshot; его timeline не добавлялся.

Следующий приоритет — настоящий LightManager cache → common shader parameters
→ современный vertex shader. Подготовка common light cache уже проверена блоком 2.
