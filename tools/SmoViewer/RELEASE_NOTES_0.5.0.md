# SmoViewer 0.5.0

SmoViewer 0.5.0 переводит основной viewport на прямой OpenGL 3.3 renderer и
делает большие составные уровни полноценными собранными сценами. Релиз также
добавляет исследовательский режим для 2D/GUI-ресурсов и включает SmoExporter
0.5.0 с экспортом level-инстансов.

## Главное

- Rigid- и skinned-меши, SAN-поза, texture sequences, UV1 и подтверждённые
  base/effect layers рисуются единым GPU path; WPF остаётся для hit testing и
  аварийного fallback при отсутствии OpenGL context.
- Геометрия, UV и vertex diffuse передаются видеокарте напрямую. Shared
  `spModel` placements используют один VBO/EBO и отдельные model matrices.
- Исправлены вертикальное отражение текстур, выбор меша мышью, aspect-aware
  projection, depth прозрачных проходов и ряд подтверждённых level layouts,
  material references, vertex tints и alpha-профилей.
- Для `Alfea03.smo` восстановлены 510 физических мешей и 757 ссылочных
  размещений; повторяемые шкафчики, мебель и другие элементы показываются во
  всех записанных transform.
- Структурно распознаются плоские GUI-сцены и node-only runtime layouts.
  Условная панель **2D / GUI** позволяет выбирать экран, visual state и
  `GUICollision`, не накладывая друг на друга все сериализованные ветви.
- Панель **Слои** прокручивается целиком. Добавлено переключаемое OpenGL MSAA
  `Выкл. / 2× / 4× / 8×`, по умолчанию `4×`; исследовательская красная плашка
  убрана из viewport, но диагностика сохранена в журнале.

## Производительность

На локальном debug-прогоне pristine `Alfea03.smo` подготовка сцены после отказа
от CPU triangle-atlas сократилась примерно с 32,17 до 0,20 секунды, а working
set — примерно с 934 до 281 МиБ. Это контрольный замер одной машины, а не
гарантированный benchmark для любого оборудования.

## Состав Windows x64 suite

- `app/SmoViewer.exe` — SmoViewer 0.5.0;
- `tools/SmoExporter/SmoExporter.Gui.exe` — SmoExporter 0.5.0;
- `tools/SmoImporter/SmoImporter.Gui.exe` — SmoImporter 0.5.0;
- `tools/WinxHairPatcher/WinxHairPatcher.Gui.exe` — Winx Hair Patcher 0.2.0;
- единый `native/` с `SmoFbxBridge.exe`, Autodesk FBX SDK DLL и лицензией.

Корневой `SmoViewer.exe` проверяет Microsoft .NET 8 Desktop Runtime x64 и при
необходимости предлагает установить официальный подписанный runtime Microsoft.
Игровые файлы в комплект не входят.

## Границы

Viewer остаётся исследовательским приближением: точные additive/MASK passes,
runtime-текст GUI, gameplay SPL/SPT и полная collision-семантика ещё не
восстановлены. Структурные тесты и визуальный preview не заменяют проверку
сцены непосредственно в игре.
