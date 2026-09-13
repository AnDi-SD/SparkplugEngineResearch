# spMaterialPassLayer / spMaterialTextureLayer / spStdLayer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMaterialPassLayer](../../../Sparkplug/Code/Sparkplug/spMaterialPassLayer.h), [spMaterialTextureLayer](../../../Sparkplug/Code/Sparkplug/spMaterialTextureLayer.h), [spStdLayer](../../../Sparkplug/Code/Sparkplug/spStdLayer.h).

## Иерархия

```text
spBaseObject
├─ spMaterialPassLayer (0x3A8905A5)
└─ spMaterialTextureLayer (0x7F577C6D)
   └─ spStdLayer (0x234C576B)
```

Первые два класса регистрируются напрямую от `spBaseObject` и имеют factory. `spStdLayer` — concrete derived leaf.

## `spMaterialPassLayer`

Объект одинаков на двух платформах:

| Offset | Поле |
| ---: | --- |
| `0x00..0x0F` | `spBaseObject` |
| `0x10` | raw `FinalBlendOperation` |
| `0x14` | layer count |
| `0x18..0x34` | восемь owned layer pointers; на PC direct-delete, не intrusive |
| extent | `0x38` |

Constructor обнуляет все десять слов после base. Destructor освобождает все
восемь slots независимо от текущего count. Copy сначала очищает destination,
копирует blend/count и разрешает каждый layer через clone manager. Значит это
не serializer-only record, а владелец фиксированного runtime pass-графа.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| размер | observed `0x38` | exact `0x38` |

## Texture-layer wrapper

`spMaterialTextureLayer` добавляет к base лишь один owned relationship по
`+0x10`; размер равен `0x14`. Default base layer оставляет его null. Copy
освобождает прежнее значение и клонирует исходный `spMaterialTexture` через
общий clone manager.

| Факт | `spMaterialTextureLayer` PC / PS2 | `spStdLayer` PC / PS2 |
| --- | --- | --- |
| initializer | `0x006D2900 / 0x00482B90` | `0x006D3850 / 0x00482BD0` |
| outer extent | observed `0x14` / exact `0x14` | observed `0x14` / exact `0x14` |

Оба texture-layer vtable содержат дополнительную операцию: PC
`0x00423590` остаётся защищённым thunk, PS2 `0x00170760` выгружает первые
девять texture-state words nested объекта. Исходное имя и полная сигнатура
этой операции пока не назначаются.

Открыты original paths/API spelling, остальные PC allocator/lifetime variants,
`FinalBlendOperation` enum, имя дополнительной texture-state операции и
state-application/render dispatch, который потребляет pass непосредственно
перед mesh submission.

## PS2 update и texture-stage dispatch

Consumer pass-графа частично закрыт. `spPS2Material::Update` (`0x001F2360`)
вызывает `0x001700B0(pass, passIndex)` для каждого pass. Эта функция читает
`layerCount +0x14`, проходит relationships с `+0x18` и выбирает texture stage:
переданный индекс либо собственный индекс слоя, если аргумент равен `-1`.
