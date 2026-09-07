# PC RFX: файл, regex, ID, имя и создание template

CP60, 7 сентября 2026. Оригинал `local-data/pc-pristine/WinxClub.exe`,
SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Установленный engine caller

Полный `4D56D0` теперь исполняется: `4D0A30` читает файл; owned bytes
копируются через strlen в MSVC string; два настоящих `4D5560 ->476550`
выполняют regex search, original match-results accessor `4D27B0` отдаёт
capture1, `sscanf("%x")` создаёт identity, `4D0960` строит68-byte template,
`417090` копирует имя. Loader `540` содержит borrowed pointer на возвращённый
template. Template kind2, ready0; XML ещё не разобран.

Точные встроенные выражения:

```text
6F3E64 <RmStringVariable NAME="ID"(?:.*?)VALUE="(.*?)"/>
6F3E98 <RmDirectXEffect NAME="(.*?)" TYPE(?:.*?)>
```

Поиск чувствителен к регистру, принимает перевод строки внутри `.*?`,
выбирает первое совпадение каждого выражения независимо. Поэтому duplicate
documents дают первые ID/имя, raw XML entity в имени не декодируется,
пустое имя разрешено. Буквальные пробелы/порядок атрибутов имеют значение:
переставленный NAME или пробел перед `/>` не равнозначны исходной записи.
Файл не обязан быть валидным XML для этого metadata stage. Встроенный NUL
обрезает document, хотя нижний file helper сохранил все bytes.

Hexadecimal scan принимает знак/0x/пробельный prefix и останавливается на
первом недопустимом символе. Это явный контракт импортированного CRT, не
повторная реализация scanf движком. Engine отвергает только return0;
EOF(-1) может передать неинициализированный stack ID дальше. Последнее пока
установлено статически; host guard отвергает unwritten identity и не
объявляется точным native error behavior. Integer overflow вне fixture.

## Ошибки и владение

Проверены точные сообщения `Effect file contains no class ID`,
`Effect file contains invalid class ID`, `Effect file does not contain a
valid effect name`. Эти завершённые пути очищают global active byte73FE6B.
Native no-ID освобождает file buffer; invalid-ID и missing-name оставляют
ровно одну allocation5000. Error objects, regex objects, locks, strings,
template/loader/file manager освобождаются. C++ RAII устраняет обе утечки;
содержимое результата/diagnostic/scan calls сверяется отдельно от этой
намеренной разницы владения. Open-failure active flag остаётся1 статически;
это не засчитано как полный проверенный файл с ошибкой открытия.

## Инициализация библиотеки и предел доказательства

Cold construction исходного ID regex достиг instruction/time cap100000/2s.
Этот путь не продолжался из остановленной памяти, пределы не увеличены.
Независимый короткий regex `A(.*?)B` полностью создаётся за81341 инструкцию,
включая общие library tables. В свежей пробе после его успешного завершения
оба исходных regex создаются отдельными обычными вызовами:56533 и56000
инструкций. Таким образом доказан caller при уже инициализированной общей
библиотеке; весь cold startup остаётся открытым. Это новый явно заданный
сценарий состояния, не успешное завершение прежнего capped call.

Regex compile/search bodies оригинальные, без regex success seam. В binary
есть RTTI `.?AVbad_expression@boost@@`; точная версия Boost не установлена.
После удаления всех трёх regex оригинал освобождает и shared locale state,
и обе critical sections; fixture проверяет balanced enter/leave/delete.

Внешние Win32/CRT/MSVCP71 контракты выделены в отдельные reusable fixtures.
Locale response задаёт ASCII C1 bits, ASCII lowercase/conversion и NT5.1
OSVERSIONINFOA; high-byte classification — явно непретендующий на реальную
codepage zero input. Все specimens ASCII. Структура C1/сигнатуры проверены
по [GetStringTypeA](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getstringtypea),
[C1 bits](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-getstringtypew)
и [LCMapStringA](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-lcmapstringa).
Fixtures не вызывают host APIs. Конкретная Windows locale в них не эмулируется.

## Source и проверка

`spPCRFXFileLoader::LoadFileForAnalysis` использует восстановленный file-text
helper и общий `LoadDocumentForAnalysis`; передаёт владение готовым metadata
template вызывающему коду. Два фиксированных regex воспроизведены через
стандартную библиотеку C++, `.*?` заменён на byte-spanning class с тем же
наблюдавшимся поведением. External scanf callback сохраняет видимый контракт.
API/member/header names аналитические.

```text
python research/native_workbench.py run pc-rfx-file --deadline-utc 2026-09-07T16:00:00Z
```

Профиль14 случаев: normal/signed/hex prefix, lowercase, newlines, raw entity,
empty name, duplicates, NUL suffix, missing ID/name, invalid ID, spaced close,
reordered attributes. Все14/14 exact captures прошли:78 native assertions,
максимум45075 инструкций на file call (81341 на отдельную library init),
35104 bytes arena. Source suite50 assertions. Full large corpus file
wrapper, cold startup, archive files, locale variants, EOF ID и соединение
с XML initialization/renderer startup ещё не объявлены закрытыми.
