# Fog: оригинальные состояния и современный GPU backend

11 сентября 2026, блок 19. Viewer/LVLcreator получают Fog настоящего
Renderable через общий graph и выводят его для rigid Mesh и generated Text.
Это подключение существующего `spDXRenderer::ApplyFogForAnalysis`, а не
новый C# reader или изменение восстановленного алгоритма игры.

## От оригинального объекта до GPU

[PC renderer Fog](native-pc-renderer-fog.md), reader `43B910`, submission
`4AD390` и state cache `4B0A90` уже восстановлены. Тип 0 пишет только disable;
типы 1/2 — enable/mode/color/density; тип 3 — enable/mode/color/start/end.
Оригинал сохраняет raw float bits, подавляет повторные состояния и игнорирует
HRESULT устройства. Повторная identity не перечитывает изменённый payload.

Новый C ABI `SpvFogDraw` имеет 28 bytes и маску известных состояний. Snapshot
вызывает общую реализацию на временном cache. Каждый нужный slot начинается
с побитового дополнения его входного значения: универсальный sentinel
не подходит, поскольку любой uint может встретиться в IEEE/ARGB payload.
Caller output изменяется только после успеха. ID 0 явно выбирает новый
disabled `spFog`; это default инструмента, а не восстановление глобального
Fog/SceneManager. Graph и оригинальные references не изменяются.

Managed projection кэширует один snapshot на Fog ID и передаёт его через
Model/Text в общий renderer. Live изменение Fog payload и lifetime cache
этим immutable transport не реализованы. Неизвестный тип сохраняет явную
ошибку snapshot, не блокируя загрузку остальных объектов документа.

OpenGL fragment stage выполняет документированный device contract:
линейный, exponential и exponential-squared фактор; perspective eye depth
из reciprocal fragment W, affine device Z; clamp 0..1; blend RGB без
изменения alpha. Fog применяется после texture stages и alpha test, до
framebuffer blending. Основания: Microsoft
[pixel fog](https://learn.microsoft.com/en-us/windows/win32/direct3d9/pixel-fog),
[формулы](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-formulas),
[цвет](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-color).
Старый D3D9 runtime не используется. Это проверка device contract на RTX3070,
не побайтное сравнение кадров с original D3D9 hardware.

Fog включён независимо от режима освещения; Wireframe и отдельный Sky pass
его пропускают. Каждый draw сначала сбрасывает Fog state. Для нечисловых или
неподдерживаемых **потребляемых** параметров backend выдаёт
`FOG_GPU_UNAVAILABLE`, сохраняя обычную геометрию. Неиспользуемые поля, в том
числе NaN при disabled/другом режиме, не блокируют поддерживаемый draw.

У shipped weighted `Fixed.rfx` нет `oFog` producer. Результат этой комбинации
с оригинальным GPU не установлен: `FOG_SHADER_UNVERIFIED` оставляет weighted
геометрию без Fog. Это открытая граница, а не приписанное игре поведение.
Raw invalid state, общий scene fallback и полный frame также не закрыты.
PS2 runtime в этом блоке не исполнялся; platform scores не повышаются.

## Проверка связанным пакетом

- **10 fresh original captures / 177 original checks**, включая HRESULT
  failure, identity cache, неизвестный тип, NaN/negative zero и направленный
  ARGB/float `A5A5A5A5`. Все tracked Fog/serializer owners освобождены;
  прежние instruction/arena bounds сохранены.
- **152 C ABI checks** сравнили известные raw state words с original cache;
  проверены атомарный отказ, default, отсутствие ID, NULL и повторное чтение.
  Входной FFPS содержит только реально прочитанный original Fog payload.
- **83 GPU checks**: границы линейного диапазона, exp/exp2, perspective и
  orthographic projection, отсутствие radial fog, неизменность alpha,
  alpha-test rejection, state reset, toggle, weighted diagnostic и два
  настоящих SkyBox. `g_crystal.smo`: 1 реальная Mesh placement, Fog ID5,
  start900/end3000/ARGB FF006666; toggle меняет 603 components и восстанавливает
  точные pixels. Peak process working set 185 995 264 bytes; 1,47 s внутри теста.
- **107 связанных GPU control checks**: материал31, Icy34, меню/Text42.
  Native RendererSubmit/SkinRender: 2/2 suites, 464 checks. Managed сборка
  GuiTests с потребителями завершилась без warnings/errors.

Файлы подобраны по SQLite class index (413 PC Fog rows), затем unique
file/object lookup. Найдены 8 маленьких active Fog примеров без перечитывания
всего корпуса; последний query 0,058 s. Первый cold query достиг пятисекундного
лимита; увеличен только бюджет этого read-only индексного поиска до15 s.
Emulator limits и объём всего корпуса не менялись.

Первая GPU fixture оказалась в default Unlit и выявила ненужную связь Fog с
режимом освещения; production gate исправлен, добавлена отдельная проверка.
После успешных 74 проверок сериализация отчёта не приняла тестовый NaN;
JSON теперь явно сохраняет named floating literals. Финальный полный запуск
завершился успешно. Предыдущие raw failures сохранены, не переименованы в pass.
Sentinel comparison сначала ошибочно приравнивал подавленные device calls
к неизвестному состоянию; итог сравнивает original cache и проверяет collision.

[Manifest](../../research/tools-core-fog-gpu-2026-09-11.json) содержит выбранные
source bindings, pristine EXE hash, original captures, binaries и GPU readbacks.
Raw directory: `local-data/results/tools-core-cycle-20260911-1900/fog-gpu/`.
Это подтверждение перечисленных операций, не оценка всего Fog renderer игры.
