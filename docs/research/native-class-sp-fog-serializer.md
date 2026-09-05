# `spFogSerializer`

Статус: восстановлена подтверждённая граница класса, RTTI/lifetime, раздельный
PC/PS2 ABI и полная известная wire-грамматика. Реальный stream codec пока не
перенесён: portable-класс предоставляет безопасное представление payload и плана
записи, не объявляя недоказанные имена внутренних интерфейсов.

## Идентичность и наследование

Обе сборки регистрируют `spFogSerializer` с Class ID `0x576A70CA`, прямым base
`spSerializer` (`0x42429877`) и factory. Target hook независимо возвращает
`spFog` (`0x7AC95AEC`). Точное имя исходного `.cpp` в PC executable не найдено;
поэтому файл размещён в доказанном общем модуле `Code/Sparkplug`, но его полный
оригинальный путь не заявляется.

PS2 factory выделяет ровно `0x14` байт. Класс устанавливает только две vptr
базового serializer-контракта и не добавляет состояние. PC factory защищён, но
destructor/vtables и совпадающая структура дают наблюдаемый extent `0x14`; это
зафиксировано как observed, а не как прямое PC allocation proof.

## Формат поля

Собственная секция содержит ровно один известный field ID:

| ID | Имя диагностики | Payload |
|---:|---|---|
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

Подробная статистика значений по корпусу и побайтовое сравнение PC/PS2 находятся
в [`smo-class-sp-fog.md`](smo-class-sp-fog.md).

## PC evidence

Контрольный файл: pristine `WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D2EE0..0x006D2F05`, registration record
  `0x0075EAC8`;
- getter `0x0043B7F0`, target hook `0x0043B820`;
- protected factory entry `0x0043B830`, destructor `0x0043B800`, clone
  `0x0043B8A0`, deleting destructor `0x0043B8F0`;
- reader `0x0043B910..0x0043BCE5`, writer
  `0x0043BCF0..0x0043C074`;
- primary/interface vtables `0x006DFB5C` / `0x006DFB50`;
- strings `spFogSerializer` and `esfFog` diagnostics at
  `0x006DFE04` and `0x006DFC5C..0x006DFDA8`.

## PS2 evidence

Контрольный файл: `SLES_532.19`, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483450`, registration record `0x004AA3F0`;
- registration getter `0x0018ADD0`, target hook `0x0018B270`;
- reader `0x0018ADE0..0x0018B004`, writer
  `0x0018B010..0x0018B250`;
- deleting destructor `0x0018B280`, clone `0x0018B2F0`, factory
  `0x0018B3D0`, exact allocation `0x14`;
- read/finalize/write thunks `0x0018B440`, `0x0018B450`, `0x0018B460`;
- primary/interface vtable headers `0x0048F540` / `0x0048F564`.

## Что остаётся неизвестным

- точные исходные header/source paths и оригинальные имена методов;
- семантика общего serializer slot, который возвращает success без собственной
  fog-логики;
- rollback при частично прочитанном field;
- runtime-проверка изменённых параметров на копии игрового ресурса.

Полный runtime layout, defaults, blank clone и renderer slot `26` описаны в
[`native-class-sp-fog.md`](native-class-sp-fog.md).

Следующий сериализатор в текущем порядке регистрации —
`spMatColorControllerSerializer`.
