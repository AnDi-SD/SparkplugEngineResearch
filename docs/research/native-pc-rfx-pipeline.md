# PC RFX: связанный путь от файла к compiled shader

CP61, 7 сентября 2026. Оригинал `local-data/pc-pristine/WinxClub.exe`,
SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

На одном retained object graph исполнена последовательность исходных API:
`4D56D0` file/regex metadata -> `4D0650` template Initialize -> `4CFFE0`
compiler-result consumer. Объекты между фазами не заменяются подготовленными
копиями: возвращённый template содержит настоящий file document; его же
исходный XML parser `6B52D0` и callbacks `4D4750/4D6050` создают pass;
тот же vertex shader record затем попадает в compiler function.

Три минимальных валидных XML документа дают assembly, HLSL и assembly с
`MatDiffuse REGISTER="4"`. Metadata ID1/nameX/kind2/ready0 становится ready1
после XML. Assembly descriptor из файла и HLSL reflection descriptor из
внешнего SDK оба разрешаются оригинальным `4AF940` в type8/register4/count1;
исходный текст shader равен `x`. Пять переданных SDK bytes1020304050
копируются и принадлежат engine shader до его destruction.

Это последовательность трёх полностью законченных native API calls,
не один вызов полного manager startup. Shared regex library прогрета
законченным оригинальным сценарием CP60; холодный startup остаётся открытым.
SDK implementation и GPU не запускаются; текст `x` и bytes намеренно
синтетические, их shader validity не утверждается.

В source `LoadFileForAnalysis` передаёт тот же template в существующие
`InitializeFromEventsForAnalysis` и `CompileShaderForAnalysis`. XML decoder
в source — явная внешняя граница: comparator получает события ElementTree
из того же документа. Native исполняет также оригинальную XML библиотеку.
Сравнение включает полный document, все shader strings/flags/parameters,
SDK request, resolved descriptors, row sum, owned bytecode и active flag.
Оригинальные file/regex/XML/template/shader allocations освобождаются;
также проверены COM releases и уничтожение shared critical sections.

```text
python research/native_workbench.py run pc-rfx-pipeline --deadline-utc 2026-09-07T16:00:00Z
```

3/3 exact captures,27 native assertions и18 source-harness assertions.
Phase instruction counts:

| Input | File/regex | XML initialize | Compiler consumer | Arena bytes |
|---|---:|---:|---:|---:|
| assembly |36476|59294|4432|42992|
| HLSL/reflection |36476|65994|10832|43200|
| assembly constant |36476|72013|6571|43376|

Отдельная shared-library initialization максимум81341. Лимиты100k/2s,
30s child,64KiB arena и32KiB allocation не увеличены. Real large RFX corpus,
manager startup/device/scene integration, cold regex и error/reload cases
не закрываются этими тремя документами.
