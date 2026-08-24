# SmoViewer: прямой GPU-render уровней

Дата: 2026-08-24.

## Основание

Исследованный fixed-function material path игры преобразует
`ColorOperation = 3` в `D3DTOP_MODULATE`. Для level vertex layout `0x940`
подтверждён `D3DFVF_DIFFUSE`: ARGB хранится по `+24`, UV0 — по `+28` при stride
36. Следовательно, texture и vertex diffuse не требуется объединять на CPU:
их интерполяцию и умножение выполняет GPU во время rasterization.

## Реализация

В WPF-интерфейс встроен OpenGL 3.3 viewport. Статические rigid mesh передают в
него исходные positions, indices, UV0 и diffuse ARGB. Texture загружается один
раз, sampling использует repeat и mipmapped linear filtering. Каждый физический
`spMeshData` имеет один VAO/VBO/EBO; shared level placements используют отдельные
model matrices без копии геометрии.

Рендер разделён на opaque и sorted transparent passes с общим depth buffer.
Сетка пола перенесена в тот же проход, иначе прозрачный WPF viewport рисовал её
поверх стен независимо от depth сцены. Выбор элемента дерева меняет GPU tint и
visibility/opacity, не пересоздавая buffers.

После первого визуального прогона удалён лишний shader-переворот `1-V`:
`TexImage2D` уже связывает первую переданную строку decoded SMO texture с
координатой `V=0`, поэтому native UV0 должен передаваться без изменения. Для
mouse picking невидимая WPF-копия mesh сохраняет полностью прозрачный material;
без material WPF исключал `GeometryModel3D` из ray hit testing. Контрольный клик
по `Alfea03.smo` выбрал mesh `[2811]` и раскрыл соответствующую ветвь дерева.

Skinned geometry, texture animation и подтверждённые static two-layer materials
пока используют существующий WPF fallback. Его private triangle-atlas остаётся
техническим обходом ограничений WPF и не считается поведением игры.

## Контрольный замер

Pristine `Media/Levels/Alfea/Alfea03.smo`, локальная debug-сборка:

| Этап | CPU-atlas path | Direct GPU path |
|---|---:|---:|
| Decode | 1,49 с | 1,56 с |
| Подготовка сцены | 32,17 с | 0,20 с |
| GPU upload | 0,107 с (частичный path) | 0,085 с |
| Working set | ~934 МиБ | ~281 МиБ |

Direct path создал 509 unique mesh buffers и 63 unique textures для 1 263
placements. Время автоматизации окна не включено в сравнение: скрипт намеренно
ждёт перед screenshot. Замер не является гарантией для другого GPU/CPU.

## Граница достоверности

Прямая передача geometry/UV/diffuse и `MODULATE` соответствуют восстановленному
пути. Точные blend equations всех `FinalBlendOp`, динамические two-layer passes,
SAN-деформация rigid render nodes и GPU skinning ещё требуют отдельного
восстановления; Viewer не должен называть их побайтно идентичными native render.
