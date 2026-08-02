# 2026-08-01 — эквивалентные FFPS-проверки и следы serializer в PC/PS2

## Вопрос

Есть ли в PS2 executable следы исходных имён и независимое подтверждение логики Sparkplug/SMO? Что именно меняют добавленные моды?

## Метод

Read-only проверка заголовков, секций, SHA-256, ASCII-строк и небольших disassembly windows; сравнение текущего PC baseline с debug-menu patch. Baseline не считается pristine-сборкой. Бинарники не изменялись и не добавлялись в Git.

## Результат

- PC и PS2 содержат эквивалентные проверки `FFPS` + `0x26`.
- PS2 ELF очищен от symbol table, но сохранил имена классов, RTTI-like/registration strings, serializer diagnostics и баннер `Sparkplug Engine v1.0`.
- PC сохранил пути к serializer `.cpp` и точный CodeView/PDB identifier.
- В обеих версиях есть сотни имён классов и строковых токенов полей, включая cross-platform и platform-specific mesh/texture ветки.
- Debug Menu является точечным F1-hook через `GetAsyncKeyState`, а не другой сборкой игры.
- Tweak tools — managed .NET; их строки указывают на кандидаты EXE/INI/SPT, а точные операции должна подтвердить декомпиляция.

## Вывод

PS2 ELF — наиболее удобная опора для восстановления сериализатора: нет SecuROM-слоя PC, при этом существенный словарь имён классов и токенов полей сохранён. Адрес `0x00181D80` становится первой именованной точкой входа будущего Ghidra-проекта.

Подробные hashes, адреса и дальнейшие шаги перенесены в [`docs/engine/executable-evidence.md`](../../docs/engine/executable-evidence.md).
