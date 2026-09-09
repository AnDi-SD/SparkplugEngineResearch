# 9–10 сентября 2026: ядра tools, цикл до 07:00

- 19:35 МСК: запуск по поручению пользователя; root c99abca, Viewer ddd1c52.
- Первое направление — spatial inspectors на actual ResourceGraph; независимый
  аудит Light выполняется параллельно. Правила и редакторские отсрочки сохранены.
- План и последующие результаты: [цикл](../../docs/research/tools-core-cycle-2026-09-10-0700.md).
- 20:03–20:10: spatial ABI5578/managed15076 checks;14 archived original/source
  states повторно совпали,2 pristine уровня покрыли все8 классов. Light6 fresh
  original/ABI +46 initial managed;5consumer builds. Original FF9ED6FF исправил
  ошибку ожидания старого теста (green3F56D6D8), восстановленный код не менялся.
- Явная несовместимость старых PC/PS2 Corpus reruns сообщена; до новых отдельных
  профилей guard предотвращает перезапись historical evidence.
- Параллельный Sphere/Box реверс дал18 successful actual-PC captures; доказан
  Box rounding defect. Реализация следующего блока ещё выполняется.
- 20:13 МСК: первый блок проверен: Light48 managed, общий FormatTests647;
  актуальные5 consumer builds зелёные. Готовится локальный commit; цикл продолжается.
- 20:37–20:44 МСК: второй блок — actual Sphere/Box и общий OBB scalar reader.
  C++ совпал с 18+11 original cases; пять native suites прошли. Удалены три C#
  scalar/size реализации; 35 ABI rows и 166 managed checks, FormatTests647.
  Viewer GUI/FormatTests и Exporter FormatTests собраны без замечаний.
- Vase/Qc whole graphs загружаются. Blooming_flower останавливается раньше Box
  на прежнем source-less TextureData; причина и offsets зафиксированы отдельно,
  не добавлен искусственный source-wrapper. Пользователь уведомлён.
- Следующий блок — Model/Material selection и authoring reference provenance
  Importer/LVLcreator. Аудит обнаружил host loader limit 64 МиБ при сценариях
  80+ МиБ; сообщено, обходной C# parser не вводится, нужны замеры памяти.
