# PC Skin: palette -> mesh -> shader constants -> draw (CP64)

2026-09-07, pristine WinxClub.exe SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Лимиты неизменны: 100000 инструкций / 2 секунды / 64 KiB arena, 30 секунд
на отдельный child. Никакого живого устройства или процесса игры.

## Полный исходный вызов

Весь `46A240` выполняется с настоящими Skin/Node/DXMesh/материалом/pass/layer/
buffers/declaration/shader-manager factories. Поля mesh и world cache Node
подготовлены явно как вход потребителя; загрузка их SMO в CP64 не исполняется.
Renderer — объявленное backing storage `F2F8` с исходными vtables, а не
заявление о полной фабрике или инициализации устройства.

Пройденная цепочка:

```text
46A240 Skin render
  423FD0 pre (реальный direct callback, fallback material, NULL fog)
  461D70 cached Node PRS -> affine matrix
  426B00 inverseBind * boneWorld -> renderer C9B8 palette
  462680 global identity3 -> identity4
  4BBB60 world input -> SetTransform(256)
  renderer C9BC = boneCount
  4BC670 DXMesh -> 4BC4A0 buffers/material/pass
  4BC290 -> 4C8980 existing weighted shader -> 4AE930 constants -> 4BE210
  device vertex shader/constants/indexed draw
  4240D0 post; C9BC = 0 только на полном успехе
```

BlendMatrices/MatDiffuse descriptors создаются оригинальным `4AF940`, shader
сохраняется настоящим `4C87A0` под ключом `{20011,0}`. Shader/GPU handles
нулевые, явные внешние device contracts принимают запросы; валидность
байткода или реальный GPU результат не утверждается. Compiler miss не
подменяется и не вызывается в этом срезе.

Вход Node world position `{1,2,3}`, scale `{2,3,4}`, orientation identity;
inverse bind translation `{5,6,7}`. Полученная палитра имеет diagonal
`2,3,4,1` и translation `{11,20,31}`. Три строки BlendMatrices содержат
транспонированную affine часть, следующая строка — MatDiffuse. Сверяется
вся палитра и каждый вызов устройства, включая состояние cache/count в
момент вызова, а не только итоговые координаты.

## Порядок и ошибки

- Исходный pre callback видит прежний C9BC=9. Его false даёт **false Skin
  render**, сохраняет 9, палитру и world matrix. Это отличается от Model
  caller, где пропуск через false pre возвращает успешный результат.
- SetTransform(identity) тоже видит прежнее 9: новое число костей публикуется
  только после него. Все geometry/device calls и post видят 1.
- Post false оставляет C9BC=1 и уже произошедшие draw/state изменения.
- Отрицательные HRESULT всех device calls не меняют успешный результат;
  число костей сбрасывается в 0 на исходном успешном пути.
- Skin weights `+60=4` или `0` не участвуют в данном render caller. Тип
  shader key определяется флагами vertex components mesh; Skin считает
  слоты палитры по `+64`.
- Конструктор Model/Skin задаёт слово `+28=FF000000`, и pre переносит его
  в renderer C194. Исправлен ранее нулевой source default.

## Перенос и границы

`spSkin::RenderUnlitForAnalysis` использует существующие Node affine,
palette multiply, callback dispatch, renderer matrix setter и
`SubmitUnlitGeometryForAnalysis`; внутренние методы не заменены callbacks
с выдуманным success. Аналитический `SkinRenderContextForAnalysis` явно
передаёт состояния renderer/материала/cache и внешние device функции.

Текущий source entry принимает fallback unlit material и NULL fog. Custom
material/alpha queue, non-NULL fog, отсутствующие источники и нехватка
palette storage отклоняются как ещё не перенесённые ветви. Shared DXMesh
получает declaration как явно разрешённый внешний input, поскольку его
существующая portable InitializeShared не занимается этим resolver.
Source буферы содержат CPU storage; нативный consumer использует factory
буферы с нулевыми COM handles. Это граница уже выбранных device ресурсов.

`probe_pc_skin_render.py` / `compare_pc_skin_render.py`: **5 exact captures**,
41 native assertions; `SparkplugSkinRenderTests` —70 assertions. Максимум
15288 инструкций рабочего вызова, 64576 bytes arena с полной очисткой
поздно созданного ResourceManager. В native teardown Skin освобождает
mesh/его ресурсы; отдельно освобождаются оставшиеся prepared renderer cache
inputs. Полный destructor самого renderer не заявляется.

```powershell
python research/native_workbench.py run pc-skin-render --deadline-utc 2026-09-07T16:00:00Z
```

Read->render на одном сохранённом графе проверен в [CP65](native-pc-skin-loaded-render.md).
Открыты SAN actor->bone->palette, full frame,
lit/custom material paths и вся арифметика с исключительными float inputs.
Точная побитовая проверка здесь относится к выбранному целочисленному
translation/scale примеру; portable float helper не эмулирует все x87 rounding.
