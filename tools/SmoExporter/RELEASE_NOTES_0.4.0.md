# SmoExporter 0.4.0

Версия 0.4.0 заменяет Blender-конвертацию FBX прямой записью через Autodesk FBX
SDK и возвращает явный выбор формата в GUI.

## Главное

- GUI предлагает `GLB`, `FBX`, `OBJ` и экспорт всех форматов.
- FBX создаётся из внутренней сцены напрямую поставляемым `SmoFbxBridge.exe`;
  Blender, Python и временный GLB больше не требуются.
- Нативный writer сохраняет hierarchy, geometry attributes, materials,
  embedded PNG, skin clusters, bind pose и выбранные SAN animation curves.
- Quaternion tracks выпекаются при 30 кадрах/с в непрерывные Euler-кривые FBX.
- Нативный bridge работает в отдельном процессе с timeout и сообщает ошибку до
  публикации неполного файла.
- GLB и OBJ используют прежние проверенные пути и остаются доступными независимо
  от FBX.

## Комплект

Standalone-пакет содержит один общий каталог `native/`:

- `SmoFbxBridge.exe`;
- `libfbxsdk.dll` Autodesk FBX SDK 2020.3.10;
- `FBX_SDK_License.rtf`.

Те же файлы включены один раз в корень SmoViewer suite и совместно используются
Exporter и Importer.

## Проверка

- добавлены textured/skinned smoke tests и GLB ↔ FBX round-trip regressions;
- Release build и упаковщик проверяют наличие native runtime и отсутствие его
  дубликатов в suite;
- OBJ, GLB и FBX не изменяют исходный SMO.

Камеры, lights, morph targets, неподтверждённые SAN events, MASK/alpha cutoff и
additive blending намеренно не синтезируются.
