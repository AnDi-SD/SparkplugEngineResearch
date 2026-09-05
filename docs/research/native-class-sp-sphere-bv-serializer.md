# `spSphereBVSerializer`

Статус: подтверждены identity, RTTI/lifetime, раздельный PC/PS2 ABI, два поля,
native epsilon подавления позиции и зеркальные target offsets reader-а.

Обе сборки регистрируют `spSphereBVSerializer` с Class ID `0x7294634F` и
прямым base `spSerializer` (`0x42429877`). Target `spSphereBV` имеет corpus Class
ID `0x390946D2`; отдельного target-ID virtual override в native vtable этого
serializer-а не найдено, поэтому эта связь помечена как аналитическая, а не как
адрес несуществующего hook-а. Exact PS2 и observed PC размер serializer-а —
`0x14`.

## Поля

| ID | Имя | Запись | Source offset | Mirror после чтения |
|---:|---|---|---:|---:|
| 0 | `esfSphereBVPosition` | если любой компонент дальше `0.001` от нуля | `+0x28` | `+0x18` |
| 1 | `esfSphereBVRadius` | всегда | `+0x34` | `+0x24` |

Reader копирует прочитанную позицию и радиус одновременно в source и mirror
offsets. Writer сравнивает модуль каждого компонента позиции с float-константой
`0.001` (`0x3A83126F`); NaN вследствие ordered comparison не подавляется.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer/call `0x006D2E20/0x006D2E40`, record `0x0075E948`;
- getter `0x0043A160`, protected factory `0x0043A190`, destructor
  `0x0043A170`, clone `0x0043A200`, deleting destructor `0x0043A250`;
- reader `0x0043A2E0`, writer `0x0043A4D0`, shared finalize `0x005A7DB0`;
- primary/interface vtables `0x006DF4F0/0x006DF4E4`;
- class string `0x006DF620`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x004833D0`, record `0x004AA330`;
- getter `0x0018A2C0`, reader `0x0018A2D0`, finalize `0x0018A460`, writer
  `0x0018A470`;
- deleting destructor `0x0018A670`, clone `0x0018A6E0`, factory
  `0x0018A7C0`, exact allocation `0x14`;
- thunks `0x0018A830/0x0018A840/0x0018A850`;
- primary/interface vtable headers `0x0048F490/0x0048F4B4`;
- class string `0x0044CD10`.

## Неизвестное

- оригинальные header/source paths и точные method names;
- полный `spSphereBV` inheritance/layout и смысл двух копий параметров;
- когда и кем синхронизируются source/mirror offsets после runtime mutation;
- допустимость отрицательного, NaN и бесконечного radius;
- контролируемый in-game mutation test.

Следующий естественный сосед — `spBoxBVSerializer`, но он переносится только
после такой же проверки source/mirror offsets и правила default suppression.
