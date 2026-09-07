# PC file -> owned text: общий helper RFX

CP59, 7 сентября 2026. Оригинал `local-data/pc-pristine/WinxClub.exe`,
SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Семантика свободного helper `4D0A30` восстановлена; принадлежность API
`spParser::ReadFileTextForAnalysis` — выбор реконструкции, не symbol claim.

## Полная последовательность

`4D0A30` создаёт PC file stream через `6BD580`, открывает mode1; внутри
`6BD710` действительно создаётся пустой `spPCPKManager` (`45C4E0`) и
исполняется его lookup `45B8A0`. Пустой manager переходит к обычному файлу.
`CreateFileA` получает GENERIC_READ, share3, OPEN_EXISTING3, flags1.

Затем создаётся memory stream `465560`, открывается с пустым именем
(`6DB70C`). `416D70` запрашивает физический размер и вызывает настоящий
memory stream-to-stream writer; последний читает PC stream `6BDB60` через
ReadFile. PC stream уничтожается до дописывания `uint32(0)` helper `416F30`.
`465540` снимает ownership/resize flags, возвращает buffer; memory stream
уничтожается, а буфер переходит вызывающей стороне.

Входные байты сохраняются целиком, включая встроенный NUL и `FF`. Всегда
добавляются четыре нулевых байта, а не один. Capacity растёт по 5000:
4996-byte input ->5000, 5000/5001 ->10000. Это capacity allocation,
не длина текста; последующий RFX wrapper использует strlen и поэтому
считает только prefix до первого NUL.

Пустой файл: Win32 ReadFile возвращает success и actual0; PC stream
формирует EOF diagnostic и возвращает false. Memory stream не двигает
position, `416D70` игнорирует неуспех destination writer, `4D0A30` продолжает
и возвращает четыре нуля. Для этой пробы явный optional debug-output flag
`73FF60=0` выбирает реальную отключённую ветвь `47D8F0`; создание,
форматирование и dispatch diagnostic не заменены успехом.

## Границы и исходный код

`pc_win32_file_fixtures.py` предоставляет только пять идентифицированных
KERNEL32 imports: CreateFileA/ReadFile/GetFileSize/SetFilePointer/CloseHandle.
Никаких host handles, API forwarding или записи файлов. CRT strncpy/strncat
и sprintf `%i` — отдельные идентифицированные внешние контракты диагностики.
Все оригинальные engine factories, copy/growth/detach/destructors исполнены.
Пустое состояние package manager не доказывает чтение архивного пакета.

C++ helper принимает владение `spStream`, выполняет Open/CopyTo/destruction,
дописывает uint32 zero и забирает buffer существующего `spMemoryStream`.
Поставляемый stream — явный выбор фабрики; тестовый stream воспроизводит
только уже проверенный файловый I/O contract. Original Open-failure early
return оставляет созданный stream без destruction; host RAII устраняет эту
статически видимую утечку. Host allocation/pointer guards документированы.
Ошибка открытия, short positive reads, size/read failure и package lookup
пока не проверены полными дифференциальными случаями.

```text
python research/native_workbench.py run pc-parser-file-text --deadline-utc 2026-09-07T16:00:00Z
```

Профиль содержит tiny, embedded-NUL, 4996/5000/5001 bytes и empty. Сравнение
включает весь возвращённый буфер, capacity и последовательность file calls.
Все6/6 exact captures прошли:36 native assertions. Максимум27120 инструкций
на вызов,21168 bytes arena. Native case проверяет освобождение всех отслеженных
file/stream/manager/result allocations. C++ Parser suite:51 assertions,
из них18 относятся к новому helper. Лимиты исследования не увеличены.
