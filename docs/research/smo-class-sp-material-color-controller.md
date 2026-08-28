# `spMaterialColorController` (`0x4C633E85`)

Статус: структура и значения полностью декодируются в пределах наблюдаемого
корпуса; только чтение. Безопасное изменение и runtime-эффект отдельных
evaluator-параметров ещё не проверены.

## Распространённость и связи

| Корпус | Объекты | SMO | Формы | Payload |
|---|---:|---:|---:|---:|
| `pc-pristine` | 2 391 | 31 | 1 | 1 уникальный |
| `pc-working` | 2 391 | 31 | 1 | 1 уникальный |
| `ps2-pristine` | 0 | 0 | 0 | 0 |

Каждый экземпляр безымянный, имеет размер 54 байта и является непосредственным
дочерним объектом `spMaterialData`. Других родителей и собственных дочерних
объектов во всём корпусе нет. Самая большая концентрация — 1 644 экземпляра в
`Levels/Swamp/BMS_04B.smo`; следующие группы находятся главным образом в GUI,
HUD и SFX.

`pc-pristine` и `pc-working` побайтно согласны для всех экземпляров класса.
Ни один из 31 PC-пути не имеет прямой same-path PS2-пары после удаления
служебного префикса `data/`. Поэтому отсутствие класса на PS2 нельзя объяснять
простым сопоставлением объекта по ID или пути.

## Внешний layout

| Относительное смещение | Размер | Значение |
|---:|---:|---|
| `0x00` | 4 | class ID `0x4C633E85` |
| `0x04` | 4 | `SBOO` |
| `0x08` | 5 | field 0, raw header `0xE0`, UInt32 size `40` |
| `0x0D` | 40 | пять последовательных evaluator-секций |
| `0x35` | 1 | terminator собственной serializer-секции |

Структурная сигнатура базы: `s0:f0:40|s0:end`. SHA-256 единственного
наблюдаемого 40-байтового payload сохраняется в discriminator варианта базы.

## Вложенные evaluator-секции

Порядок доказан PC serializer-кодом:

1. ambient `ColorFunctionalEvaluator`;
2. diffuse `ColorFunctionalEvaluator`;
3. specular `ColorFunctionalEvaluator`;
4. emissive `ColorFunctionalEvaluator`;
5. alpha `FunctionalEvaluator`.

Первые четыре секции имеют одинаковые шесть байт `63 CD CC CC 3D 00`:
field 3 (`frequency`) равен `0.1`, затем идёт terminator. Alpha-секция:
`61 CD CC CC 3D 62 00 00 00 00 64 00 00 80 3F 00`, то есть
`frequency=0.1`, `amplitude=0`, `yOffset=1`, terminator.

### `ColorFunctionalEvaluator`

| Field | Имя из executable | Тип | Default | Наблюдается |
|---:|---|---|---:|---:|
| 0 | `esfColorFuncEvalColor1` | ARGB UInt32 | `0xFF000000` | нет |
| 1 | `esfColorFuncEvalColor2` | ARGB UInt32 | `0xFF000000` | нет |
| 2 | `esfColorFuncEvalType` | UInt32 | 0 | нет |
| 3 | `esfColorFuncEvalFrequency` | Single | 1 | **0.1** |
| 4 | `esfColorFuncEvalAmplitude` | Single | 1 | нет |
| 5 | `esfColorFuncEvalXOffset` | Single | 0 | нет |
| 6 | `esfColorFuncEvalYOffset` | Single | 0 | нет |
| 7 | `esfColorFuncEvalPitch` | Single | 0 | нет |

### `FunctionalEvaluator` alpha

| Field | Имя из executable | Тип | Default | Наблюдается |
|---:|---|---|---:|---:|
| 0 | `esfFunctionEvalType` | UInt32 | 0 | нет |
| 1 | `esfFunctionEvalFrequency` | Single | 1 | **0.1** |
| 2 | `esfFunctionEvalAmplitude` | Single | 1 | **0** |
| 3 | `esfFunctionEvalXOffset` | Single | 0 | нет |
| 4 | `esfFunctionEvalYOffset` | Single | 0 | **1** |
| 5 | `esfFunctionEvalPitch` | Single | 0 | нет |

Alpha evaluator при `amplitude=0` и `yOffset=1` даёт постоянный коэффициент 1
независимо от frequency. Четыре color evaluator используют одинаковые defaults и
частоту, однако точная формула применения их результата к `spMaterialData` пока
не прослежена. Поэтому весь controller не объявляется «no-op» только по
статической структуре.

## Свидетельства executable

В pristine PC EXE:

- регистрация serializer указывает на load `0x004412E0` и serialize
  `0x00441740`;
- load обращается к evaluator-объектам по смещениям `+0x68`, `+0xB8`, `+0x108`,
  `+0x158`, `+0x1A8` именно в порядке ambient/diffuse/specular/emissive/alpha;
- `CFESerializer` находится около `0x0047E320`/`0x0047E850`,
  `FESerializer` — около `0x0047EE00`/`0x0047F220`;
- сравнения перед записью подтверждают defaults из таблиц выше, включая
  `0xFF000000`, `0`, `1.0`.

PS2 `SLES_532.19` содержит строку регистрации класса по file offset `0x3451B0`
и полный набор диагностик того же serializer по `0x34B8C0..0x34BD10`.
Следовательно, PS2-движок знает этот класс, но 317 уникальных PS2 SMO его не
сериализуют. Это platform/resource-policy difference, а не отсутствие реализации
в executable.

## Реализация и база

`SmoMaterialColorControllerDecoder` строго читает пять завершённых секций,
отвергает неизвестные/повторные поля и применяет только defaults, доказанные
кодом executable. Read-only Viewer field inspector показывает все восстановленные
значения.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spMaterialColorController
```

повторно проверяет весь corpus, оба executable и идемпотентно записывает:

- PC field definition `material_color_controller.evaluators`;
- вариант `shared_evaluator_profile`;
- назначения всех 4 782 объектов двух PC-корпусов;
- четыре evidence-записи: PC code, PS2 strings, полный corpus и отрицательное
  PS2-наблюдение.

Статус редактирования остаётся `read_only_research`: числовые типы и defaults
известны, но диапазоны, runtime-поведение и безопасная мутация ещё не доказаны.
