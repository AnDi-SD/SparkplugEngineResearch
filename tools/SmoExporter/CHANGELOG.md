# Changelog

## 0.2.1 — в разработке

- автопоиск Blender расширен до `BLENDER_PATH`, Windows App Paths/Uninstall, `Program Files (x86)` и пользовательских установок;
- в GUI добавлены ручной ввод папки или `blender.exe`, выбор файла и сохранение проверенного пути между запусками.
- экспорт использует compact skinning layout `0x093E`, восстановленные texture/material bindings и однотонный vertex material из общего ядра;
- rigid-меши сохраняют локальную привязку к анимируемому `spRenderNode`, а сам render node и его SAN-треки включаются в GLB/FBX;
- alpha-blend render flag материала преобразуется в GLB `alphaMode=BLEND` и OBJ `map_d`; неоднородный alpha обычного атласа сам по себе больше не считается признаком прозрачного материала;
- двухслойные материалы сохраняют базовую текстуру UV0 и первый кадр effect-map по UV1; об усечении анимированной texture sequence выдаётся явное предупреждение;
- добавлены регрессии экспорта `Spirit.smo`, `knut.smo`, `knutBoss.smo` и `bloomx.smo`.

## 0.2.0 — 2026-08-12

- GUI явно проверяет зависимость от Blender, блокирует недоступные FBX-режимы и показывает подробный журнал этапов экспорта;
- SmoViewer передаёт экспортеру все подключённые SAN через временный manifest; собственный автопоиск экспортера в этом режиме отключён;
- общее представление сцены дополнено иерархией узлов, bind pose, palettes, skin weights и SAN-анимациями;
- GLB 2.0 теперь содержит skins, joints, inverse bind matrices и TRS animation channels;
- добавлен бинарный FBX через фоновый Blender с armature, материалами, встроенными текстурами и выбранными actions;
- GUI автоматически находит соседние SAN и позволяет добавлять отдельные файлы или папку и выбирать клипы галочками;
- CLI получил `--fbx` и повторяемый параметр `--animation`;
- SmoViewer запускает отдельный SmoExporter для открытой модели;
- Dragon/dnaf проверены обратным импортом FBX в Blender: один armature, 45 костей и анимационные кривые.

## 0.1.0 — 2026-08-10

Первый исследовательский релиз SmoExporter.

- самодостаточный GLB 2.0 для Blender с meshes, materials и PNG-текстурами;
- compatibility export OBJ/MTL/PNG;
- normals, UV0/UV1 и vertex colors для подтверждённых layouts;
- преобразование triangle strips в triangle lists;
- importer metadata и SHA-256 исходного SMO в GLB extras;
- CLI и минимальный WPF GUI с выбором модели, каталога и формата;
- исходный SMO открывается только для чтения и не изменяется.
