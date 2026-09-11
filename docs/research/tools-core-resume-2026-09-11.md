# Короткая точка возобновления ядер после 11 сентября

Этот файл сокращает повторный аудит; полный evidence остаётся в датированных
досье. Сначала прочитать действующие правила, manifesto и
[текущий срез семи ядер](tools-core-migration-status-2026-09-11.md).
Общий срок —14–15 сентября. Прямое разрешение 11 сентября снимает прежнее
ожидание согласования технических замен. Новый timed цикл не назначать самому.

## Не повторять как незакрытое

Общие envelope/field/reference writers, scene LightManager, Occlusion Init,
Particle looping Init, Text CPU/GPU, strips, material passes, weighted shader
lighting, alpha, runtime mips, Sky camera pass, rigid Fog уже подключены.
Native SMO transfer включает 29 ранее пропущенных AnimTexController references;
StellaX/Icy проверены в обе стороны, включая реальное окно Importer.
SAN progress output исправлен для API callers со strict CP1251 stdout.

Последние machine manifests:
[Fog](../../research/tools-core-fog-gpu-2026-09-11.json),
[native window](../../research/tools-core-native-transfer-window-2026-09-11.json),
[пять остальных consumers](../../research/tools-core-final-consumers-2026-09-11.json).
Native DLL SHA256:
`EF92713019BD80B172AE79F9EDECCB972AA705EEB7E2D4C866B618AE66C0F073`.
Это версии прошедших проверку артефактов; после новых изменений не приписывать
им результаты прежних DLL. Key source bindings не являются полным compiler
dependency closure. Current git status проверять перед первой правкой.

## Первый полезный пакет: rigid lighting

- Выбор света уже выполняется через `SceneLighting.h` и настоящий
  `spLightManager`. Не писать новый selector или исключать Directional0:
  common enum — Directional0/Point1/Spot2/Ambient3.
- У `spDXLight` уже есть 26 optional device words и original world refresh
  `4B58D0` → producer `4B53C0`. Untouched поля остаются unknown.
- `spDXRenderer::SubmitLightsForAnalysis` (`4BDE50`) и
  `LightingStateForAnalysis`/InstallMaterial/ApplyMaterialLighting уже
  восстановлены. Нужен transport реально потребляемых fixed-function states
  и modern device consumer. Не использовать weighted constants как замену:
  shader path сохраняет selected disabled lights, fixed-function submission
  фильтрует иначе; NULL и пустой список также имеют разные cache effects.
- Native `ShaderLighting.h` и managed `CaptureShaderLighting` относятся
  именно к shipped `Fixed.rfx`. Его GPU translation готова. Rigid preview
  пока находится в `SmoGpuSceneRenderer.MaterialShader.cs::PreviewLighting`.
- Existing evidence: [renderer lights](native-pc-renderer-lights.md),
  [material lighting](native-pc-material-lighting.md),
  [weighted integration](tool-shader-lighting-2026-09-11.md).
  Перед новым original run переиспользовать эти контракты и короткие captures.

Прямые точки подключения:
[состав и callbacks device light](../../Sparkplug/Code/SparkplugDX/spDXRenderer.h),
[готовый producer/submission](../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp),
[общий shader transport](../../tools/SparkplugViewer.Native/ShaderLighting.h),
[развилка rigid/weighted](../../tools/SmoViewer/SmoViewer.Rendering.Wpf/SmoGpuSceneRenderer.Lighting.cs),
[текущий rigid preview](../../tools/SmoViewer/SmoViewer.Rendering.Wpf/SmoGpuSceneRenderer.MaterialShader.cs).
`BindShaderLighting` сейчас возвращается для `BoneMatrices.Length == 0`;
имя `_fixed*` в uniforms относится к shader `Fixed.rfx`, а не уже готовому
fixed-function consumer. Не начинать разбор света заново из-за этих имён.

Последняя сверка transport: [MaterialSubmission](../../tools/SparkplugViewer.Native/MaterialSubmission.cpp)
уже отдаёт 17 material words и состояния 29/137/145/148; повторный material
DTO не нужен. Однако имена `ambientSource`/`ambient` в common helper вводят
в заблуждение: он отправляет **148 — EMISSIVEMATERIALSOURCE**, тогда как
AMBIENTMATERIALSOURCE имеет номер 147. Число отправки менять нельзя на основании
имени переменной. Сверка: [официальный enum](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype)
и original numeric captures в material lighting dossier. Нового original run
при этой сверке не было. NORMALIZENORMALS=1 уже подтверждён
[original startup tail](tool-pc-renderer-startup-states-2026-09-10.md).
Для остальных потребляемых startup states сначала проверить existing evidence;
defaults документации сами по себе не доказывают сохранение состояния в игре.

В fixed-function GPU consumer держать отдельный specular канал: он добавляется
после texture cascade, до blending ([контракт D3D](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d9types/ne-d3d9types-_d3drenderstatetype)).
Это граница современного backend, не новый восстановленный алгоритм игры.
Первый общий тестовый пакет должен различать modes 2/3/4/5/6, disabled light,
NULL/empty lists, diffuse/emissive sources, specular поверх текстуры и
неравномерный scale normals. Weighted, Text и Fog достаточно оставить
небольшими смежными контролями, а не запускать весь корпус.

Weighted Fog не снимать с guard только из-за наличия table mode:
[Microsoft vertex-fog contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/vertex-fog)
требует vertex Fog при vertex shader, а shipped shader не производит `oFog`.
[vs_1_1 output registers](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-1-1)
не задают default этого регистра. Результат original device остаётся
неподтверждённым; текущий `FOG_SHADER_UNVERIFIED` — явная граница backend.

## Следующие потребители

Exporter сохраняет `SmoExportMesh.LoadedMaterial`, но writer projection пока
берёт базовую texture/color и не представляет весь runtime passes/controllers.
Определять контракт целевого формата; current output не выдавать за lossless
game-material export. Native Importer уже переносит оригинальный graph;
его geometry-only preview не должен снова проходить через flat material reader.

LVL SkyBox edit: `SetModelTransform` помечает world-only change,
`DrawSky` требует свежую local pose. Использовать существующий authoring path
`SmoPlacementTransformWriter`; не вводить ещё один world/local PRS алгоритм.
Сначала установить передачу результата этого path к `SmoSkyBoxPose`, сохранив
guards parent/nonuniform cases. Полный UI refactor не является приоритетом.

## Короткие проверки и ресурсы

Корпус/SQLite не перечитывать целиком. Сначала class index, потом unique
file/object lookup: Sky selector0,030s, Fog0,058s. Сохранять только выбранные
пути; tests одного связанного пакета запускать после нескольких смежных правок.
RAM ориентир около 1GiB, native build два workers. Эмулятор ограничен профилем;
исчерпавший budget/faulted run не возобновлять с потерянным состоянием.

Проверочные builds не являются релизом. Git commits сохранялись локально.
Push в публичный Viewer был отклонён automatic approval review; без нового
подтверждения пользователя повторять эту публикацию нельзя. Не останавливать
из-за этого независимую разработку или локальные commits.
