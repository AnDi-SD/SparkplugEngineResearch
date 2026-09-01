# Native Gate 3/5: rigid geometry, materials и textures

Дата: 29 августа 2026 года. Статус: пройден.

## Что подтверждено

- один production-путь импортирует rigid OBJ, GLB и FBX в проект, переживает
  archive/reopen и собирает байт-идентичный immediate-writer результат;
- оси, scale `100`, отражение Z и winding `0,1,2 -> 0,2,1` согласованы;
- отсутствующие OBJ normals восстанавливаются как конечные единичные векторы;
- GLB `TEXCOORD_1` сохраняется отдельно от UV0, writer проверен на реальном
  двухканальном E1 layout `0x1940`;
- меш больше 65 535 вершин детерминированно делится без потери material index,
  normals, UV0/UV1, vertex colors и порядка треугольников;
- opaque и подтверждённый rigid texture-alpha engine contract сохраняются;
  каждая встроенная текстура после записи точно совпадает с исходным RGBA как
  BGRA, включая alpha;
- общая текстура нескольких частей встраивается один раз; duplicate placement
  не копирует mesh или texture resource;
- add, move, duplicate, redirect, delete, owner promotion и Undo/Redo проходят
  через `.smolvlproj` без изменения исходного `data.bin`.

## Реальные модели

| Формат | Fixture | Части | Проверки | Результат |
|---|---|---:|---:|---|
| GLB | Shrek | 2 | 31 | passed |
| OBJ | Layla Enchantix | 7 | 46 | passed |
| FBX | Flora Enchantix | 30 | 57 | passed |

Финальные SMO:

- GLB SHA-256: `D0731A4A0B2576EB628050640A128520C09D88273399E2335FF5365151F88063`;
- OBJ SHA-256: `769B0E15841FD79ECC82D4BBC1D4C4E2A40557EFC62FFD5468160BC0EE48D359`;
- FBX SHA-256: `21310ECD03120E876BDA8F2977B2635847780E9AE4A8F6195B87B269FC59A68D`.

Inspector подтвердил `SignatureMismatchCount=0` и полное декодирование мешей:
704/704, 709/709 и 732/732 соответственно.

## Автоматические регрессии

- `SmoLVLcreator.CoreTests`: 1 856 assertions на `Alfea02_old.smo`;
- rigid preparation: 65 538 вершин, 21 846 треугольников, два безопасных чанка;
- alpha component, GLB normal repair/resource safety, texture catalog и rigid
  texture resize regressions пройдены;
- direct writer и batch worker используют один идемпотентный preparation-путь.

## Native scene-ready

Manifest:
[`mvp-gate35-rigid-import.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate35-rigid-import.json).

Run:
`local-data/validation-results/mvp-gate35-rigid-native-final-20260829/run-20260829-154952-935`.

| Кейс | Время | Результат |
|---|---:|---|
| Alfea02 pristine before | 34.923 s | scene-ready |
| GLB Shrek | 22.208 s | scene-ready |
| OBJ Layla | 19.366 s | scene-ready |
| FBX Flora | 19.611 s | scene-ready |
| Alfea02 pristine after | 19.238 s | scene-ready |

Итого: 5/5, `crash=none/none`.

## Граница результата

- OBJ и текущий native FBX bridge предоставляют один UV-канал; distinct UV1
  импортируется из GLB и сохраняется, когда target layout его поддерживает;
- MASK записывается через подтверждённый alpha-blend путь: отдельный native
  cutoff пока не подтверждён;
- additive preset importer не создаёт и поэтому он не входит в этот gate;
- Viewer консервативно включает transparent ordering у FinalBlend 2 только при
  UV-покрытии partial-alpha texels. Engine alpha tuple и точный texture payload
  проверяются отдельно, поэтому отсутствие Viewer-сортировки у полностью
  opaque/binary-covered части не считается потерей материала.
