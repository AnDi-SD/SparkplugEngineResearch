# CP129 — стоимость анимационного кадра Viewer

Рабочая версия Viewer 0.6.1 уменьшает стоимость существующей операции SAN.
Фиксированный измеритель остаётся 16/16: это оптимизация и исправление выбора
кости, а не дополнительная возможность или увеличение class ledger.

## Причина и изменение

Замер настоящего `ApplyAnimationPoseCore` показал лишние создания WPF-геометрии
костей и CPU-обработку normals невидимой копии GPU-мешей. Теперь общие frozen
meshes маркера и сегмента сохраняются между кадрами, меняются только matrices.
При изменении модели/размера кэш перестраивается; UI handlers продолжают
переключать слои и selection. Нулевой сегмент получает конечную вырожденную
матрицу, вместо нормализации нулевого вектора.

CPU-позиции GPU companion по-прежнему обновляются для выбора мышью. Его normals
не пересчитываются: видимое освещение считает OpenGL, а WPF ray test использует
positions/triangle indices. Это проверено настоящим ray test установленного WPF
и согласуется с [исходником MeshGeometry3D](https://raw.githubusercontent.com/dotnet/wpf/main/src/Microsoft.DotNet.Wpf/src/PresentationCore/System/Windows/Media3D/MeshGeometry3D.cs).
Для обычной видимой WPF-геометрии обработка normals сохранена.

Также исправлена коллизия object index при нескольких открытых моделях:
маркер выбранной кости чужого файла использует её собственную bind-позицию,
а не одноимённый индекс активной анимируемой модели.

## Измерение

Один последовательный baseline и один итоговый проход на том же компьютере,
обе сборки Release: 60 прогревочных и 240 измеряемых вызовов на модель/режим.
Окно скрыто, OpenGL host и реальный GUI загружены. Интервал — CPU pose handler;
GPU time, upload, compositing и общий FPS не измерены. Reflection overhead
присутствует с обеих сторон. Background load, GC и JIT влияют на разброс.

| Модель, обычные слои включены | CPU median до → после | Ускорение | Allocations на кадр до → после |
|---|---|---|---|
| Icy / xiwa | 2,5935 → 0,8251 мс | 3,14× | 407 970 → 153 352 байт, −62,41% |
| Knut / Knwa | 2,1526 → 0,6077 мс | 3,54× | 255 776 → 77 272 байт, −69,79% |

P95: Icy 4,388 → 0,9002 мс; Knut 2,9615 → 1,0094 мс.
Peak working set всего benchmark: 196 734 976 → 191 422 464 байт
(около 188 → 183 МиБ). Это наблюдённый peak, не универсальная гарантия.
Промежуточный вариант с одним кэшем дал 1,4671/0,7315 мс; отдельные режимы
без костей и без scene geometry сохранены в исходных reports для анализа.
Итоговое время GUI regression не сравнивается с CP128 как speedup: конфигурации
и число проверок различаются.

## Проверка корректности и границы

Настоящие WPF load/select/slider handlers: 7 клипов, 43 позы, 8 359 checks
за 2,5490236 с; peak 203 517 952 байт. Есть сравнение преобразованной cached
геометрии со старыми helper-функциями (<1e-8), topology/reference reuse,
нулевые сегменты, скрытие/возврат слоёв, движение выбранной кости, шесть ray
checks и отдельная загрузка Icy вместе с Knut для проверки чужого selection.

Независимый NumPy FK/skinning из SHA-проверенных CP128 reference дал 419
mesh/time сравнений, максимальная ошибка 0,00006269157593809065 — прежняя.
Original-PC PRS, SAN decoder, export pipeline и GPU shaders не менялись:
их прежние проверки не запускались повторно. GPU pixels и OS dialogs этим
GUI harness не подтверждаются. Все прежние ограничения
[общего SAN](tool-shared-san-2026-09-08.md) сохраняются.

## Воспроизведение и evidence

[Benchmark](../../research/ViewerAnimationBench/Program.cs) содержит точный
алгоритм измерения, исходно запущенный из игнорируемого временного проекта;
при переносе байты обоих исходных файлов сохранены. Запуск из корня workspace:

```powershell
dotnet run --project research/ViewerAnimationBench -c Release -p:UseSharedCompilation=false -- local-data/results/viewer-animation-bench.json
dotnet run --project tools/SmoViewer/SmoViewer.GuiTests -c Release --no-restore -p:UseSharedCompilation=false -- local-data/results/tool-cycle-20260908-1900/shared-san-export-v4/input.json local-data/results/tool-cycle-20260908-1900/viewer-performance/gui-final
```

Нужны локальные pristine PC Icy/xiwa и Knut/Knwa. Для независимого сравнения
используется [валидатор GUI](../../research/validate_shared_san_gui.py)
в Blender с NumPy; proprietary fixtures и captures остаются в `local-data`.
[Validation manifest](../../research/tool-viewer-performance-validation-2026-09-08.json)
фиксирует SHA исходников, baseline, промежуточного и итогового замера, GUI
capture и независимого результата. [Assessment CP129](../../research/tool-readiness-assessment-2026-09-08-cp129.json)
переоценивает только затронутые свидетельства. Сборка после изменения номера
версии проверяется отдельно; номер версии не изменяет измеренный алгоритм.
