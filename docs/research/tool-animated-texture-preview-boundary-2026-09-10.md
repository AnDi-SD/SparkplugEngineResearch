# Animated texture preview: граница ordinary renderer

Узкая попытка подключить actual material runtime к существующему одному
texture slot OpenGL не закончена и убрана из рабочего кода. Её patch и тесты
сохранены локально как **paused-unverified**, без заявления о готовой анимации.
Viewer после удаления среза пересобран. Общий material runtime остаётся тем,
который был проверен ранее; игровой sampler не переписывался.

Исходный положительный пример выбрали неверно: наличие двух AnimTex в BloomX
не доказывает существование подходящего однопроходного draw item. Короткая
проверка actual shared ABI пяти файлов показала:

| Файл, material index | Непустые passes: blend | Потребитель |
| --- | --- | --- |
| BloomX, 89 | 0, 6, 6 | Model/Skin |
| bloom_crystal, 81 | 0, 6 | Model/Skin |
| FloraX, 87/112/125 | 0, 6 | Model/Skin |
| TecnaX, 75 | 0, 6 | Model/Skin |
| Darcy_shadow_bats, 10 | 2 | ParticleSystem, не ordinary Model |

Каждый перечисленный pass действительно содержит один layer: это не пустые
слоты, которые можно без доказательства отбросить. Остальные шесть материалов
BloomX статические. Отдельный raw material положительный результат для Darcy
не считается проверкой ordinary renderer: в файле нет Model/Skin/MeshData.
Поиск ограничен этими пятью входами; отсутствие других подходящих файлов во
всём корпусе не заявляется.

Следующий полезный шаг — backend для реальных base/overlay passes с
подтверждёнными blend/depth/texture правилами. До него текущая диагностика
`MATERIAL_FRONTEND_SHAPE` сохраняется. Отрисовка pixels и новое GPU acceptance
не выполнены. Первый неудачный тест не ослаблен до успеха пустого адаптера.

Экономия дальнейшей работы: в манифест добавлена проверка реального потребителя
и структуры исходного примера **до** реализации узкого интеграционного среза.
Выборка по индексу типа БД заняла 0,315с; первоначальный query остановлен
существующим cap и не выдаётся за успешный. Оригинальный EXE не исполнялся.

Локальные доказательства: [полные shapes и hashes](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/shape-audit.json),
[воспроизводимая проба](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/inspect_material_shapes.py),
[сохранённый patch manifest](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/pending-slice/manifest.json).
