# CP118: внешний источник TextureData

8 сентября 2026. Общий обработчик source field4 восстановлен и используется
обоими TextureData readers.20 original/source сравнений совпали по1816 mip
bytes, восьми полям texture state и собранному пути. Пакет4 workers занял8,63 с;
максимум459427 инструкций,90512 arena bytes. Один отдельный original случай
подтвердил отказ открытия и cleanup до создания texture pixels.

## Original42EA50

Field4 читает через `416DC0` u16 byte count и C-string с завершающим NUL.
Из parent stream word18 берётся имя. Внешний MSVCR71 `_splitpath` по IAT
`6D9358` выделяет drive/directory, original последовательно дописывает directory
к drive и reference к этому префиксу. Затем `6BD580` создаёт PC file stream,
`6BE7C0/6BD710` открывает его mode1. Пустой actual package manager `45C4E0`
исполняет lookup `45B8A0`, после чего используется обычный file backend.

Тот же serializer secondary read вызывается в `42EBF4` с новым stream и
прежним texture object. Это payload-файл, без нового FFPS или SBOO header.
При успехе `6BD980` закрывает stream, `6BE2A0` уничтожает его; только после
этого source handled становится1. Outer stream продолжает свой wrapper.

| Parent name | Reference | Original result path |
|---|---|---|
| `C:\Media\SFX\scene.smo` | `image.tex` | `C:\Media\SFX\image.tex` |
| `Media/SFX/scene.smo` | `sub/image.tex` | `Media/SFX/sub/image.tex` |
| `scene.smo` | `image.tex` | `image.tex` |
| `C:\Media\scene.smo` | `D:\image.tex` | `C:\Media\D:\image.tex` |

Последняя строка подтверждает именно literal concatenation: абсолютный
reference не заменяет parent prefix. Fixture явно принимает получившееся имя
и выдаёт supplied bytes. Это не утверждение, что такой путь можно открыть
обычным Windows filesystem. Пути не нормализуются и не превращаются в URI.

## Отказ и утечка оригинала

При объявленном CreateFileA failure `FFFFFFFF` и GetLastError2 original
сохраняет false, пропускает recursive read и уничтожает PC stream. Destructor
пытается CloseHandle для невалидного ненулевого handle; его failure игнорируется.
Случай занял27920 инструкций, cursor17 из18 outer bytes: terminator после
провалившегося field не потреблён. Texture остаётся неинициализированной.

Успех и отказ оставляют выделенную `416DC0` строку reference неосвобождённой.
Она зафиксирована после уничтожения texture, serializers, managers и stream;
потом освобождена отдельным явно указанным fixture cleanup. В20 положительных
случаях это235 bytes, по одной строке на случай; всего1080 allocations,
из которых20 освобождены именно fixture cleanup. На ошибке осталось ещё10 bytes.
Source хранит строку в `std::string` и не воспроизводит утечку.

## Перенос и границы

`spSerializerReadContextForAnalysis` содержит явную factory owned `spStream`.
Без неё field4 отклоняется. Фабрика выбирает backend, общий handler сам
собирает имя, вызывает Open, тот же payload reader, Close и destruction.
Это host injection point для нативного PC file factory, не новый engine resolver
и не замена функции загрузки текстуры. Общие RGBA/P8 conversion и DXT1
missing mips проходят восстановленные [CP115–116](native-pc-texture-cross-upload.md)
и [CP113](native-pc-texture-compressed-mips.md).

Host guards ограничивают пути259 байтами, проверяют точный u16/NUL extent,
parent stream name, размер source≤16 МиБ и recursion depth64. Ошибки Open,
GetSize и recursive read освобождают owned stream и восстанавливают depth.
Семь C++ failure cases проверяют эти границы; Texture suite1007 assertions.
Свойства malformed inputs, всех Win32 failures, package-backed sources и
произвольных рекурсивных цепочек не считаются закрытыми.

Guest использует прежний [read-only Win32 byte fixture](native-pc-parser-file-text.md),
identified `_splitpath`/char_traits/diagnostic CRT contracts и declared COM
storage. Все engine factories, string concatenation, PC stream, package lookup,
serializer recursion, conversion и cleanup выполняются original instructions.
Caps не менялись: file1M/8 с, child30 с, arena128 КиБ, allocation32 КиБ,
пять mip levels и≤2048 bytes/surface. Новых whole-SMO или PS2 доказательств нет.

```powershell
python research/native_workbench.py run pc-texture-external-source --workers 4
python research/probe_pc_texture_external_source.py rgba drive dx failure-open open
```

[Манифест CP118](../../research/native-cycle-checkpoint-2026-09-08-cp118.json)
сохраняет fingerprints,20 сравнений и отдельный failure. Fixtures и captures
остаются локальными. Class scores не повышены.
