# PC SAN: полный source-файл принимается оригинальным reader

CP99, цикл до 07:00 МСК 8 сентября 2026. PC executable SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

`spSerializerManager::BuildResourceFileForAnalysis` собирает целый FFPS из
восстановленного объекта: индексирует граф, пишет SBOO и payload корня,
использует общий reference writer для зависимостей, формирует FAT и семь слов
заголовка. Root расположен в начале data section; exportTag передаётся явно,
его семантика не выдумывается. Данные записываются в ограниченные staging
streams, результат заменяется только при успехе, временный FAT очищается.
Занятый FAT отклоняется без уничтожения контекста вызывающего кода.

Это **host orchestration** вокруг подтверждённых PC index/reference/payload
контрактов и grammar оригинального reader. Оригинальная внешняя функция
полного сохранения пока не найдена. Просмотр manager/FAT cluster, xrefs
сохранённых diagnostics и FFPS signature её не локализовал; это не доказательство
отсутствия во всём EXE. Новое API не получает выдуманного native адреса.

## Проверенный результат

- C++ whole load → whole save для `bbush.san` воспроизвёл исходный файл
  **побайтно**, 2554 байта, SHA-256
  `706BD0E5C70111BBD7C9524B3A37B1B2D867EC86FDC5A7A7908FAC8BBFCA428E`.
- Этот результат принял полный оригинальный `422B50`: 79 064 инструкции,
  27 явных native assertions и 227 совпадающих значений/PRS. Проверены реальные
  header/FAT/RTTI factory/reader/name-registry/cleanup стадии; освобождены все
  39 native allocations. Guest heap 64 КиБ, использовано 5936 байт.
- Отдельный portable whole load/save/reload на `bbush`, `bflower`, `barrel`,
  `bw`: 4/4 файла, 2800 совпадающих значений после отдельной проверки capacity.
  Native writer всегда пишет field64 = число tracks; reader резервирует N+1.
  Поэтому у `barrel` capacity меняется 20 → 21. Это известная canonicalization,
  не потеря tracks. Остальные три capacity не изменились.
- Полная сборка и CTest: 61/61. Проверены точный byte limit, переполнение,
  ошибки payload, неподдержанный root, occupied FAT и повторная новая операция
  после неудачи. Ранее capped whole-load трёх других SAN не повторялся.

```powershell
python research/compare_pc_san_file_roundtrip.py
python research/compare_pc_san_file_roundtrip.py --portable-corpus
```

Команды требуют собственных pristine файлов и собранного
`SparkplugSanReaderTests.exe`. Созданные SAN и подробный capture остаются в
`local-data/results/cycle-20260908-0700/`; в Git входят исходники и
[сводка CP99](../../research/native-cycle-checkpoint-2026-09-08-cp99.json).

Проверка касается SAN subset. Полные SMO graphs, external file IDs,
lossless unknown fields, все варианты ошибок и native whole-save orchestration
остаются открытыми. Новых class scores за сборку уже подтверждённых частей
не начислено; семь completion gates остаются 0 passed, 6 partial, 1 open.
