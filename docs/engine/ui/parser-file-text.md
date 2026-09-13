# PC file -> owned text: общий helper RFX

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

## Границы и исходный код

`pc_win32_file_fixtures.py` предоставляет только пять идентифицированных
KERNEL32 imports: CreateFileA/ReadFile/GetFileSize/SetFilePointer/CloseHandle.
Никаких host handles, API forwarding или записи файлов. CRT strncpy/strncat
и sprintf `%i` — отдельные идентифицированные внешние контракты диагностики.
Все оригинальные engine factories, copy/growth/detach/destructors исполнены.
Пустое состояние package manager не доказывает чтение архивного пакета.
