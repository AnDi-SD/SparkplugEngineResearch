# `spMaterialPassLayer`, `spMaterialTextureLayer` и `spStdLayer`

Дата проверки: 5 сентября 2026 года. Статус: RTTI-граф, полный внешний layout,
factory/lifetime, deep-clone relationships и связь стандартного слоя с
`spMaterialTexture` подтверждены на PC и PS2.

## Иерархия

```text
spBaseObject
├─ spMaterialPassLayer (0x3A8905A5)
└─ spMaterialTextureLayer (0x7F577C6D)
   └─ spStdLayer (0x234C576B)
```

Первые два класса регистрируются напрямую от `spBaseObject` и имеют factory.
`spStdLayer` — concrete derived leaf. Ни один original header/source path в
исполняемых файлах не сохранился; reconstructed placement в `Code/Sparkplug`
поэтому помечено inferred.

## `spMaterialPassLayer`

Объект одинаков на двух платформах:

| Offset | Поле |
|---:|---|
| `0x00..0x0F` | `spBaseObject` |
| `0x10` | raw `FinalBlendOperation` |
| `0x14` | layer count |
| `0x18..0x34` | восемь intrusive layer pointers |
| extent | `0x38` |

Constructor обнуляет все десять слов после base. Destructor освобождает все
восемь slots независимо от текущего count. Copy сначала очищает destination,
копирует blend/count и разрешает каждый layer через clone manager. Значит это
не serializer-only record, а владелец фиксированного runtime pass-графа.

| Факт | PC | PS2 |
|---|---:|---:|
| registration / initializer | `0x0075FE80 / 0x006D37C0` | `0x004A9670 / 0x00482B50` |
| factory | protected `0x0045F610` | `0x00170320` |
| vtable | `0x006E7388` | header `0x0048E910` |
| copy | `0x0045F740` | `0x0016FF80` |
| размер | observed `0x38` | exact `0x38` |

## Texture-layer wrapper

`spMaterialTextureLayer` добавляет к base лишь один owned relationship по
`+0x10`; размер равен `0x14`. Default base layer оставляет его null. Copy
освобождает прежнее значение и клонирует исходный `spMaterialTexture` через
общий clone manager.

`spStdLayer` не добавляет полей. Его factory создаёт внешний объект `0x14`, а
затем обычный [`spMaterialTexture`](native-class-sp-material-render-target-texture.md)
в relationship `+0x10`: nested object имеет observed `0x6C` на PC и exact
aligned `0x80` на PS2. Это отделяет сериализуемый «тип слоя» от фактического
набора texture states, texture/controller relationships и UV matrix.

| Факт | `spMaterialTextureLayer` PC / PS2 | `spStdLayer` PC / PS2 |
|---|---|---|
| registration | `0x0075DF70 / 0x004A96D0` | `0x0075FFA8 / 0x004A9730` |
| initializer | `0x006D2900 / 0x00482B90` | `0x006D3850 / 0x00482BD0` |
| factory | protected `0x00423470` / `0x00170940` | protected `0x00460E50` / `0x00170D30` |
| vtable | `0x006DC91C / 0x0048E940` | `0x006E7700 / 0x0048E970` |
| copy | `0x004235E0 / 0x001706D0` | `0x00460F20 / 0x00170B20` |
| outer extent | observed `0x14` / exact `0x14` | observed `0x14` / exact `0x14` |

Оба texture-layer vtable содержат дополнительную операцию: PC
`0x00423590` остаётся защищённым thunk, PS2 `0x00170760` выгружает первые
девять texture-state words nested объекта. Исходное имя и полная сигнатура
этой операции пока не назначаются.

## Переносимый срез и проверка

Восстановлены три native RTTI-типа, bounded pass API, standard-layer factory и
deep clone внешнего pass/layer/nested-texture графа. Portable ownership хранится
в `shared_ptr`; это безопасная модель наблюдаемого intrusive lifetime, а не
заявление о byte-compatible host layout.

`python research/inspect_material_layers.py` выполняет 45 read-only проверок:
образы и class strings, все шесть RTTI initializer-ов, vtables, body hashes,
protected PC thunks, exact PS2 allocations, fixed bound `8` и делегирование
copy. Полная последовательная сборка и оба CTest-набора проходят.

Открыты original paths/API spelling, exact PC allocator sizes за SecuROM,
`FinalBlendOperation` enum, имя дополнительной texture-state операции и
state-application/render dispatch, который потребляет pass непосредственно
перед mesh submission.

## PS2 update и texture-stage dispatch

Consumer pass-графа частично закрыт. `spPS2Material::Update` (`0x001F2360`)
вызывает `0x001700B0(pass, passIndex)` для каждого pass. Эта функция читает
`layerCount +0x14`, проходит relationships с `+0x18` и выбирает texture stage:
переданный индекс либо собственный индекс слоя, если аргумент равен `-1`.

Thunk `0x00170790` раскрывает `spMaterialTextureLayer +0x10`, после чего
`0x001732E0` обновляет animation/static UV state и передаёт 3×3 матрицу с
`spMaterialTexture +0x48` в renderer slot `23`. Это точный
pass → layer → material texture → texture-transform edge; он не означает, что
вся таблица texture states уже применена. Evidence включён в scanner
`inspect_ps2_material.py` (31/31). PC slot `23` независимо подтверждает ту же
роль через `D3DTS_TEXTURE0 + stage` и `IDirect3DDevice9::SetTransform`.
