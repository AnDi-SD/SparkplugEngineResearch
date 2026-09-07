# PC Skin: фабрика и полный поток сериализатора (CP62)

2026-09-07, цикл до 19:00 МСК. Pristine `WinxClub.exe`, SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Все вызовы ограничены 100000 инструкций, 2 секундами, arena 64 KiB;
каждый сценарий начинается в новом процессе. Исходный EXE не меняется.

## Что исполнялось

Фабрика Skin `46A120` полностью проходит защищённый код и выделяет **0x70**
байт; `+60=4`, `+64/+68/+6C=0`, vtable `6E8C5C`. Это исправляет прежнее
реконструированное значение весов 0. Фабрика SkinSerializer `490C50`
выделяет **0x14** байт, vtable `6EC7CC`, secondary `+10=6EC7C0`.
Старые формулировки «только observed extent» больше не актуальны.

Исходный `491170` вызывает Model `4938F0`, затем читает отдельную секцию
Skin. Field 0: UInt32 weights, UInt32 boneCount, затем для каждого слота
ссылка `4678B0` и ровно 64 байта матрицы. Разрешение inline-ссылки,
Node factory `421E20`, Node reader `463A70`, обновление world cache,
FAT lookup и повторное использование указателя выполняются оригиналом.
Manager/FAT/RTTI переданы как явно подготовленное корректное стартовое
состояние; это не доказательство полного startup или whole-file SMO loader.

Индексирование `4672C0 -> 490D30 -> 4935A0` регистрирует Skin до обхода
костей. Две ссылки на один узел получают один ID. Полный writer `490DA0`
выполняет Model writer `4935F0`, всегда открывает field 0 через
`472D30(...,0,7)` с UInt32 длиной, пишет веса/число костей и последовательность
`467350(reference) + WriteData(matrix,64)`, затем `472E20` и terminator.
Первое использование узла содержит inline payload, второе — ID и нулевой размер.

## Наблюдаемые особенности

- При отсутствии Skin field остаются фабричные веса 4 и пустая палитра.
- Field с нулевым boneCount всё равно выполняет два запроса allocation(0).
  Весовое слово переносится без ограничения, включая 0 и `FFFFFFFF`.
- Неизвестные поля пропускаются; последнее полное field 0 заменяет палитру.
- Матрицы копируются побитово: отрицательный ноль, NaN payload и бесконечности
  не нормализуются. Здесь не исполняется арифметика матриц.
- Ссылки на кости заимствованные: intrusive refcount Node остаётся 0;
  Skin deleting destructor `46A7A0` освобождает массивы, но не Node.
- Повторное field 0 и очистка ранее непустой палитры теряют прежние массивы:
  в проверенных случаях ровно 4+64 байта. Это исходная утечка, не поведение
  portable vector. Тест отдельно учитывает её и освобождает только после
  завершённого нативного teardown как действие стенда.
- NULL bone: reader сначала потребляет матрицу, затем устанавливает ошибку
  `75AB9C=10010003`, возвращает false, оставляет старую палитру/веса и теряет
  новые массивы 4+64. Реальный error-object/format/dispatch исполняется;
  MSVCR71 strncpy/strncat/sprintf — ограниченные внешние контракты,
  optional debug output flag `73FF60=0` является явным входом.
- По статическому коду matrix ReadData result не проверяется. Примеры
  недочитанной матрицы не выдаются за exact normal-case соответствие.

## Portable реализация и проверка

`spModelSerializer` предоставляет protected helpers для своих секций;
публичные Model методы сохраняют проверку конкретного класса.
`spSkinSerializer` добавляет реальные read/index/write adapters поверх этих
же helpers и общего resolver. Это устраняет прежний stub только с write plan.
Источник требует корректный field extent, наличие явного владельца Node,
не допускает переполнения count и проверяет полное чтение матриц. Эти guards
строже оригинала. После [CP107](native-pc-whole-skin-scene.md) загруженные
bindings заимствуют кости через weak_ptr к владельцам context/graph; это
устраняет цикл с ancestor Node. Ручные palettes и clone могут владеть костями
явно. Vector освобождает заменённые массивы; побайтовый heap ABI и исходные
утечки не имитируются.

`research/probe_pc_skin_serializer.py` и `compare_pc_skin_serializer.py`
сверяют **10** сценариев: empty, zero, one, repeat-bone, repeat-field, clear,
unknown, prebound, raw-bits, null. Сверяются весь вход, результат и cursor,
weights/count, все биты матриц, canonical bone/world position и **весь**
выходной wire payload. C++ `SparkplugSkinSerializationTests` получает те же
байты на stdin, использует обычные registry/FAT/context APIs. Отдельные
20 проверок C++ охватывают guards и исходную пустую запись.

```powershell
python research/native_workbench.py run pc-skin-serialization --deadline-utc 2026-09-07T16:00:00Z
```

Открыты полный SMO с Skin/mesh/material, renderer consumer в том же графе,
копирование с полной clone transaction, все ошибки I/O и malformed native
поведение. Эти 10 маленьких сценариев не означают готовность exporter/runtime.
