# Живой material runtime через общие классы Sparkplug

Блок 14 цикла 9 сентября до 19:00. SceneRuntime сохраняет GraphHandle рядом с
SceneHandle: оба владеют одним ResourceGraph, повторной загрузки материалов нет.
Новый Materials API обращается к загруженным объектам этого графа. Общая
проекция texture/pass/material вынесена из SmoLoadedResources и используется
также живым runtime. Отдельного C# reader/sampler или UV evaluator не добавлено.

## Вызовы и границы

- ApplyControllers вызывает actual spRenderController::ApplyForAnalysis.
  Список controller IDs и delta являются явными входами инструмента. Это не
  эмуляция AnimationManager gate, регистрационного порядка или полного кадра.
- UpdatePass вызывает actual spMaterialPassLayer → spMaterialTextureLayer →
  spMaterialTexture. UV обновляется условно, его matrix передаётся host callback,
  затем выполняется AnimTex update. Сохраняется нативный порядок и backlink.
- ReadClock отдаёт accumulated/applied и AnimTex playback, ReadMaterial — все
  текущие pass/layer ссылки и состояния. Pixel uploads кешируются отдельно от
  изменяемого снимка material. Источник — выбранная loader CPU texture.
- UpdateColor вызывает actual spDXMaterial frame-cache method. Полный loader
  MaterialColorController остаётся ограничен известной неподтверждённой factory;
  реальный файл с таким контроллером этим блоком не объявляется рабочим.

Native API проверяет границы массивов и clocks до batch apply. Failed pass
может сохранить изменения предыдущих слоёв, как оригинал: rollback или
подставной результат не вводится. Наружный GraphHandle освобождается вместе с
runtime; вызов через оставшуюся managed ссылку после Dispose явно отклоняется.

Удалены два старых uniform texture clocks из OpenGL/WPF обвязки и их неактивная
регистрация кадров. После блока 9 общий texture resolver уже не поставлял
FrameDuration, поэтому эта очистка не отключает новую рабочую анимацию. Layout
UI не менялся. Подключение actual pass outputs к автоматическому renderer frame
ещё предстоит; оно не подменено очередным предполагаемым shader/scheduler.

## Проверки

| Файл | Материалов | Контроллеров | UV submissions | Checks |
|---|---:|---:|---:|---:|
| BloomX | 7 | 2 | 0 | 89 |
| Alfea02 | 986 | 16 | 16 | 6042 |
| Alfea01 | 1087 | 70 | 70 | 6980 |
| Icy | 3 | 0 | 0 | 23 |
| PC menu | 108 | 0 | 0 | 653 |

Всего 2191 material, 88 controllers, 86 изменившихся UV layers, 13787 checks.
У этих пяти файлов не оказалось shared-controller aliases; они не выдаются за
новое corpus-покрытие alias случая. Его original/source proof переиспользован.
Проверены отдельное накопление/consumption, idle UV, actual selected textures,
strict `> duration`, отрицательный шаг и float boundaries. BloomX: 14 событий.

Python независимо повторил эти 14 входов через C ABI и сравнил clocks/texture
IDs с managed output. Также проверена actual UV callback matrix и 37 ABI guards
на BloomX/Alfea02; всего 73 дополнительных assertions. Повторная static snapshot
проверка пяти файлов после выделения общего projector дала 30938 assertions.
Native MaterialController/UVFunction/FullLoader suites passed; пять consumer
projects собраны с нулём warnings/errors. После удаления мёртвых UI clocks WPF
собран повторно. Восстановленные алгоритмы не менялись, original EXE повторно
не исполнялся: доказательства CP20, UV renderer и pass-binding переиспользованы.

Native DLL SHA256:
`E3074E3EDEACD875B3A2738C802F7F826CAFB0604E0D795CC6D0D24EEEA4D050`.
Релиз не упакован, визуальная проверка frontend не заявляется.

Evidence: research/tools-core-material-runtime-block-2026-09-09.json.
