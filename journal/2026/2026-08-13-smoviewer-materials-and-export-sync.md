# 2026-08-13 — материалы персонажей и синхронизация Exporter

## Вопрос

После исправлений отображения Spirit, Knut, BloomX и knutBoss требовалось
проверить, что общие выводы формата используются не только Viewer, но и
экспортом GLB/FBX/OBJ.

## Корпус

- `Media/Characters/Spirit/Spirit.smo`;
- `Media/Characters/Knut/knut.smo` и `knutBoss.smo`;
- `Media/Characters/BloomX/bloomx.smo`;
- `Media/Characters/Knut/Knid.san` для rigid-анимации очков.

Игровые файлы находятся только в игнорируемом `local-data` и в репозиторий не
добавляются.

## Метод

Сопоставлены object graph, vertex layouts, texture bindings, поля
`spMaterialData` и результат построения экспортной сцены. Для каждого из четырёх
SMO выполнен полный GLB и OBJ export с разбором JSON GLB; FBX использует этот же
GLB как вход конвертера Blender.

## Наблюдение

- `0x093E` имеет serialized stride 44 при runtime stride 56; blend weights лежат
  по `+12`, четыре локальных palette index — по `+28`;
- rigid mesh `[6]` `knut.smo` принадлежит анимируемому render node
  `[2] Knut_TEMP_glasses` и должен сохранять локальный bind transform к нему;
- однотонный чёрный diffuse rigid-меша без texture — настоящий материал, а не
  основание для наследования character atlas;
- двухслойные meshes `[103]` и `[105]` BloomX используют base texture по UV0 и
  анимированную effect-map по UV1;
- field type `3`, payload `UInt32` в `spMaterialData` хранит render flags:
  `0x2` — непрозрачный материал, бит `0x4` (`0x6`) — alpha blending;
- у shield mesh `[13]` `knutBoss.smo` материал имеет `0x6`, а `gr_01` содержит
  211 прозрачных, 746 полупрозрачных и 67 непрозрачных пикселей. Материал тела
  имеет `0x2`, несмотря на неоднородный служебный alpha его atlas.

## Вывод

Общее ядро теперь передаёт material blend state вместе с texture binding.
Viewer рисует alpha-blend geometry после opaque. Exporter сохраняет rigid node
hierarchy и SAN tracks, `alphaMode=BLEND` в GLB, `map_d` в OBJ/MTL, а также base
UV0 и первый effect frame UV1 двухслойного материала. Полная анимация texture
sequence в стандартном glTF пока не представлена и явно отмечается warning.

Регрессии Exporter прошли: Knut — 20, knutBoss — 19, Spirit — 16, BloomX — 20
assertions.

## Что не подтвердилось

Определение прозрачности только по alpha texture неверно: обычные atlas могут
содержать служебный alpha. Источником blend mode должен быть material render flag.

## Следующий эксперимент

Проверить способ переноса всей animated texture sequence в расширение glTF либо
отдельный Blender material/driver без потери совместимости базового GLB.

Следующий согласованный цикл версий, ещё без релизных пакетов: Viewer `0.3.1`,
Exporter `0.2.1`, Importer `0.2.0`, WinxHairPatcher `0.1.2`, SMOTextureTool
`2.0.1`.
