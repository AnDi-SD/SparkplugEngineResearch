# PC: целый SMO с деревом, моделью, материалом, туманом и мешем

CP103, 7 сентября 2026. Неизменённый `Menus/logo_screen.smo` полностью прочитан
оригинальным `00422B50` и реконструкцией C++; выбранные состояния и связи совпали
точно. Вход 703 байта, SHA-256
`DBD6A1F261008BBF1C2971030517B7C9D60A5E27F58A4A69F7C14EAF10E2E3C7`.

Дополнение CP105: [целый `bloom_projectile.smo`](native-pc-whole-light-scene.md)
также совпал с оригиналом, включая общий mesh path и LightData→DXLight.
Ниже сохранены исходные измерения CP103; capture теперь поддерживает оба случая.

CP106 добавил [идентификацию по FAT и целый `g_crystal.smo`](native-pc-whole-scene-identities.md):
26 объектов, включая повторяющиеся Node/DXLight. Старый capture по классам
сохранён для воспроизводимости прежних малых fixtures.

Это первое в текущем цикле сравнение реального целого SMO с непустым DX batch,
материалом и графом. Здесь одна оригинальная цепочка выполняет header, оба FAT
index, hook, factories, readers, references и финальный FAT clear.

## Сравнение

Whole load: **189 653 инструкции, 1,413 секунды**. Профиль `file`: 1 млн
инструкций / 8 секунд на вызов, child 30 секунд, arena 128 КиБ. Максимум bump
arena — **71 536 байт**, предел одного allocation 32 КиБ. Startup allocation
profile CP102 здесь не используется.

| Объект | Проверено |
| --- | --- |
| Node и RenderNode | Имена, flags, точные local/world float bits, child и renderable edges |
| Model | Alpha flag, priority, projection group; material, Fog и mesh edges |
| DXMaterial | 11 render states, vertex alpha, четыре цвета, явно инициализированная power, pass count |
| Material pass / StdLayer | Blend operation, layer count, class, 9 PC texture states, UV matrix, static flag |
| Fog | Type, ARGB, start/end/density, точные биты |
| DXMesh | Flags/counts/sizes/FVF/stride/ranges, 144 vertex bytes, 8 index bytes, 40 declaration bytes |

Итого **6 объектов, 5 основных связей, 430 state bytes, 85 layer bytes,
192 buffer/declaration bytes и 50 явных native assertions**. Имена также
сравниваются побайтно. Pointer values нормализованы к runtime Class ID;
выбран файл без повторных runtime классов, для которого это однозначно.
Повторяющиеся классы требуют расширения capture, а не пропуска объектов.

Native root teardown удалил граф; отдельно выполнено документированное
освобождение оставленного оригинальным hook combiner и declaration registry.
**Все 81 tracked native allocation освобождены**, оба COM buffers имеют ноль
references и locks. Cleanup combiner — действие fixture, не приписанный native
hook автоматический teardown или rollback.

## Метод и воспроизводимость

[Native probe](../../research/probe_pc_scene_file_profile.py) задаёт consumer-derived
manager/FAT containers, корректное дерево из семи RTTI записей, byte-file,
allocator/CRT/name seams и COM storage. Фабрики Node, RenderNode, Model, material,
Fog, mesh, pass, StdLayer, texture payload и readers исполняются из pristine PC
image. MeshData factory slot не используется: data serializer создаёт DXMesh
через original header hook. Для renderer заданы минимальные поля и declaration
map; настоящего устройства D3D нет.

[C++ capture](../../Sparkplug/Tests/spSceneFileCapture.h) вызывает общий
`LoadResourcesForAnalysis` и существующие шесть serializer bindings. Отдельной
реализации парсера в capture нет:

```text
SparkplugSceneSerializationTests.exe --asset-file PATH_TO_LOGO_SCREEN_SMO
python research/probe_pc_scene_file_profile.py
```

Первое сравнение отличалось схемой capture: host `MaterialTexture` хранит
12 texture states для PS2, PC использует первые 9. Capture исправлен на явный
`PCTextureStateCount`; алгоритмы engine не менялись. Затем свежий original/source
run прошёл сравнение и teardown. Сборка, **61/61 CTest и 88/88 Python tests** прошли.
Сводка с fingerprints — [CP103](../../research/native-cycle-checkpoint-2026-09-08-cp103.json).
Полные captures находятся локально в `local-data/results/cycle-20260908-0700/`.

Не закрыты: полный startup, textured/skinned/level SMO, внешние файлы, общая
lossless запись неизвестных полей, все отказы и реальный renderer. Этот успех
не заменяет исследование runtime DXMaterial/DXMesh save graph. Class scores и
семь readiness gates за композицию не повышены автоматически.
