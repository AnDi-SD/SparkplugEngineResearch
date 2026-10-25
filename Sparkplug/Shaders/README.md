# Восстановленные шейдеры Sparkplug

Здесь находится читаемое HLSL-представление известных PC-программ: [каталог
стадий](../../docs/reference/shaders.md), [машиночитаемый реестр](../../docs/reference/shaders.json)
и [карточки](../../docs/reference/shaders/README.md) описывают их назначение,
входы, выходы и границы восстановления. Файлы в `PC/` объединены по эффекту,
поэтому один исходник может содержать несколько shader entry points.

В том же справочнике отдельно учтены [встроенные PS2 upload-образы](../../docs/reference/shaders/ps2-vu.md).
Их диапазоны и связи с pipeline установлены, но читаемая семантическая
реконструкция VU-инструкций ещё не завершена. Сырые payload и полные listings
остаются локальными данными исследования; они не подменяются выдуманным HLSL.

## Состав и происхождение

Исследованная внешняя PC-папка содержит семь standalone assembly-программ и
два RFX с vertex/pixel-парами: всего 11 стадий. Для assembly восстановлены
именованные вычисления; для текстовых RFX восстановлено семантическое
представление с явными параметрами специализации. Это не побайтовые копии
игровых ресурсов. Оригинальные файлы, bytecode и данные проверок сюда не входят.

Исходные профили — `vs_1_1`/`ps_1_1`. Имена функций, типов и compile-time
переключателей в этой папке аналитические. Читаемая реконструкция проверяется
профилями `vs_3_0`/`ps_3_0`; это не доказательство идентичного bytecode,
регистрового ABI, поведения на всех GPU или готовности современного backend.

| Исходник | Entry points | Назначение |
| --- | --- | --- |
| [Basic.hlsl](PC/Basic.hlsl) | `BasicVS` | Проекция позиции |
| [Fixed.hlsl](PC/Fixed.hlsl) | `FixedVS`, `FixedPS` | Skinning, свет, ColorMode, UV и передача цвета |
| [Bumpmap.hlsl](PC/Bumpmap.hlsl) | `BumpmapVS`, `BumpmapPS` | Tangent-space освещение по normal map |
| [BallisticPFX.hlsl](PC/BallisticPFX.hlsl) | `BallisticVS` | Вычисляемые шейдером частицы |
| [Blur.hlsl](PC/Blur.hlsl) | `BlurVS`, `BlurPS`, `BlurLuminancePS` | Экранное размытие и яркость |
| [Luminance.hlsl](PC/Luminance.hlsl) | `LuminancePS` | Фильтр яркости |
| [ShadowVolumePoint.hlsl](PC/ShadowVolumePoint.hlsl) | `ShadowVolumePointVS` | Геометрия теневого объёма |

## Специализация

Для `BlurVS` нужен `SPARKPLUG_BLUR_VERTEX=1`; без него доступны pixel entries.
Для `BumpmapVS` нужен `SPARKPLUG_BUMPMAP_VERTEX=1`, а число весов задаёт
`SPARKPLUG_BUMPMAP_WEIGHTS` (0–4, по умолчанию 0). Эта возможность шаблона
не доказывает автоматический выбор взвешенного Bumpmap в игре.

`FixedVS` использует `SPARKPLUG_FIXED_WEIGHTS` (0–4), `COLOR_MODE`,
`LIGHT_COUNT` (0–8), `SPECULAR`, `TEXCOORDS` (0–8), `UV_MASK` и `LIGHT0`…`LIGHT7`
с тем же префиксом. Типы света: directional 0, point 1, spot 2. По умолчанию
выбраны четыре веса, ColorMode4, один directional, один UV без преобразования.
Остальные комбинации задаются явно. Штатный manager обходит создание VS при
нулевом числе весов; доступная ветвь шаблона и наблюдавшийся draw различаются.

Регистры standalone-программ обозначены явно. Для RFX расположение констант
зависит от специализации и reflection: нельзя переносить номера регистров
одного варианта на другой. Смысл загруженных матриц и данных CPU описан в
[контракте shader-параметров](../../docs/engine/materials/shader-constants.md).

## Использование и проверка

Это общий эталон восстановления GPU-вычислений. Файлы пока не подключены
к игровому runtime или backend приложения. Существующие C++-модели численных
выражений остаются в `Analysis/PC`; новая папка не заменяет их и не меняет
работающее поведение потребителей.

Регистрация всех стадий и публичные ссылки проверяются без игры и SDK:

```powershell
python -X utf8 -B tools/RepositoryMaintenance/catalog.py --check
python -X utf8 -B tools/RepositoryMaintenance/check_docs.py
```

Полнота списка PC-стадий относится к исследованной внешней папке. В PS2 ELF
выделены четыре ограниченных образа с producer/dispatch-связями. Runtime-only
программы, все игровые версии, пакеты и дополнительные пути PC/PS2 не объявлены
исчерпывающе перечисленными. [Условия проверки полноты](../../docs/engine/materials/shader-discovery.md)
разделяют извлечение, связывание и восстановление поведения.
Статус исходника не подменяет численное сравнение, проверку draw в игре или
приёмку адаптера Remix.
