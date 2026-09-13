# spFogSerializer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spFogSerializer](../../../Sparkplug/Code/Sparkplug/spFogSerializer.h).

## Идентичность и наследование

PS2 factory выделяет ровно `0x14` байт. Класс устанавливает только две vptr
базового serializer-контракта и не добавляет состояние. PC factory защищён, но
destructor/vtables и совпадающая структура дают наблюдаемый extent `0x14`; это
зафиксировано как observed, а не как прямое PC allocation proof.

## Формат поля

Собственная секция содержит ровно один известный field ID:

| ID | Имя диагностики | Payload |
| ---: | --- | --- |
| 0 | `esfFog` | `UInt32 type`, `UInt32 ARGB`, `Single start`, `Single end`, `Single density` |

Writer обеих платформ всегда открывает field 0 на пять значений и пишет их из
смещений target `+0x14..+0x24` строго в указанном порядке. Reader требует
ненулевой target, принимает field 0, последовательно читает те же пять 32-битных
значений и пропускает неизвестные field ID общим datablock-механизмом.

Portable `FogPayload` сохраняет именно wire-порядок и raw `type`. Runtime renderer
независимо подтверждает поведение `0=disabled`, `1=exp`, `2=exp2`, `3=linear`;
это аналитические имена D3D mapping, а не заявление об original C++ enum spelling.
`BuildWritePlanForAnalysis()` всегда возвращает
единственный `Field::Fog`; `IsKnownReadFieldForAnalysis()` принимает только 0.

## Что остаётся неизвестным

Полный runtime layout, defaults, blank clone и renderer slot `26` описаны в
[`native-class-sp-fog.md`](sp-fog.md).

Следующий сериализатор в текущем порядке регистрации —
`spMatColorControllerSerializer`.
