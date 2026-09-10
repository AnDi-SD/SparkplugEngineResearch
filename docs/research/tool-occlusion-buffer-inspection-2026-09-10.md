# Общие readers буферов в OcclusionVolume metadata — 10 сентября 2026

Две ручные C# leaf-функции в `SmoOcclusionVolumeDecoder` заменены вызовами
общих `spIndexBuffer::ReadForAnalysis` и `spVertexBuffer::ReadForAnalysis`
через owning C ABI. Это перенос чтения metadata, **не полный serializer,
Init или регистрация OcclusionVolume в ResourceGraph**.

## Подтверждённый контракт

- IndexBuffer: PC `45FB80`, PS2 `159330`; общий reader читает type/count/flags,
  разворачивает primitive count и выбирает UInt16/UInt32 по bit0 flags.
- VertexBuffer: PC `460300`, PS2 `15C610`; общий reader читает component mask,
  vertex count и flags, затем использует собственный общий layout.
- Оба существующих reader проверяют expanded count относительно переданного
  byte budget до выделения памяти. Восстановленные исходники не изменены.

Основания: [IndexBuffer dossier](native-class-sp-index-buffer.md),
[VertexBuffer dossier](native-class-sp-vertex-buffer.md),
[OcclusionVolume layout/corpus](smo-class-sp-occlusion-volume.md).

## Граница обвязки

`tools/SparkplugViewer.Native/BufferInspection.{h,cpp}` владеет настоящими
CPU buffers. `BorrowedInput` удерживает исходные bytes только во время
синхронного чтения; handle не хранит указатель на managed input. `SafeHandle`
освобождает native owner. На выход копируются значения общих getters;
XYZ извлекаются из owned vertex data по stride/offset общего layout.
Нет повторного чтения wire header, вычисления topology или component layout.

Host guards: payload от 12 bytes до 16 MiB, exact конечный cursor, точные длины
выходных массивов, null-pointer checks. Generic inspection сохраняет raw flags,
UInt32 indices и нулевые counts; она не навязывает геометрию occluder.

Managed decoder сохраняет прежний узкий inspection profile: две строго
упорядоченные секции, type2/nonempty/format0, declaration0/count≥3/flags0,
finite positions, индексы в диапазоне, все вершины использованы, треугольники
не вырождены. Это **host policy**, а не доказательство успешного game Init.
Обработка неизвестных полей и полный runtime остаются за пределами переноса.

## Выбранный оригинальный пример

`local-data/pc-pristine/Media/Levels/Challenges/race_02.smo`, SHA-256
`AD39C13B896718755CC98445D943FA191640636F940930086CB802629E268436`:
physical index 7 / resource ID 8, `oclusion_wall02`, object 138 bytes.

| Буфер | Payload offset | Размер | Header | Значения |
|---|---:|---:|---|---|
| IB |190026|24|`[2,2,0]`|`[2,1,0,1,2,3]`|
| VB |190055|60|`[0,4,0]`|4 position-only вершины|

IB SHA-256: `7C6497660ACCB32E25668A6915529A86B7285E53DC4A6C977F1B3502BD3BEDF3`.
VB SHA-256: `D594F9C576A576B18E62C6834E8676C750E502B51D09DA56D248FECFDCF726F9`.
Сохранённые `race02-first-object.json` и `race02-shape-run1.json` находятся в
`local-data/results/tools-core-cycle-20260910-0730/occlusion-init-next/`.
Второй файл подтверждает return 1 / cursor 24 для original IB reader и
return 1 / cursor 60 для original VB reader. Последующий shape call остановлен
лимитом; этот результат не подтверждает полный Init.

## Проверки

После source freeze root последовательно выполнил проверки:

- `ViewerBufferInspectionChecks`: owned lifetime, input immutability,
  UInt16/UInt32 и raw flags, native stride, zero counts, exact extent,
  oversized counts и input cap до чтения, ошибочные output buffers.
  **26 checks PASS**, CTest 1/1 за 0,64 с (общий CTest 1,91 с).
- `SmoViewer.Sparkplug.Tests --buffer-inspection`: ABI sizes, P/Invoke,
  SafeHandle lifetime, независимость от input pin, output/input guards.
  **16 checks PASS**. Release build 10,44 с, warnings 0 / errors 0.
- `SmoViewer.FormatTests --occlusion-buffer-inspection INPUT REPORT`:
  прежний synthetic positive и strict-profile negatives, затем один
  фиксированный `race_02.smo` объект и existing serialized field inspector.
  **16 strict-profile assertions +10 real-fixture checks PASS**.
  Release build 13,18 с, warnings 0 / errors 0.

Проверенная native DLL:
`F422A92E84008E837D402B6EB6B621B743D4E2F49BD0CC7E543B0EDAE2F95028`.
Тот же hash подтверждён в native build directory и обоих managed test outputs.
Логи и `metadata.json` сохранены в
`local-data/results/tools-core-cycle-20260910-0730/occlusion-buffer-inspection/`.
Native assertion count прочитан из `artifacts/native/viewer/Release/Testing/Temporary/LastTest.log`
без повторного запуска.

Первый прямой запуск `& Build-Native.ps1` не исполнил сборку из-за PowerShell
execution policy. Его shell exit 0 не учитывается как успех. Обычный запуск
`powershell -NoProfile -ExecutionPolicy Bypass -File` выполнил сборку и
CTest; именно его вывод находится в `native-run2.log`. Подтверждённый вызов:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/SparkplugViewer.Native/Build-Native.ps1 -Configuration Release -RunChecks -CheckSuites BufferInspection -BuildWorkers 1
```

Для исторических managed-сборок подтверждены Release и
`SkipSparkplugNativeBuild=true`; остальные флаги не записаны. Отдельные команды
воспроизведения в manifest не выдаются за полные исторические вызовы.
Массовые suites не повторялись: общий игровой код не изменён. `git diff --check`
для root и Viewer прошёл. Local SMO, original EXE и captures не входят в Git.
Команды, результаты и ограниченный набор fingerprints закреплены в
[evidence manifest](../../research/tools-core-occlusion-buffer-inspection-2026-09-10.json).
