# spPCThread

PC реализация `spThread`: class ID `0x438758EA`, регистрационная база
`spThread` (`0x3DFE3B16`). Код —
[spPCThread](../../../Sparkplug/Code/SparkplugPC/spPCThread.h), обязательные
платформенные сервисы —
[spPCThreadHost](../../../Sparkplug/Analysis/Host/spPCThreadHost.h).

Оригинальный объект занимает `0x24` байт. Он сохраняет составной префикс
[spThread](sp-thread.md) и добавляет uint32 handle по `+0x20`, исходно `0`.
Переносимый handle имеет uintptr_t, чтобы host мог хранить handle своей платформы;
это не изменение исторической 32-битной раскладки.

## Собственная логика

| Операция | Поведение оригинала |
| --- | --- |
| Create | Записывает параметр тела, вызывает CreateThread с null security attributes, stack size `0`, caller entry и аргументом `this`, flags `4` (CREATE_SUSPENDED); сохраняет возвращённый handle и проверяет его ненулевое значение |
| Wait | При null handle возвращает true; иначе вызывает WaitForSingleObject и возвращает false только для `0x102` (WAIT_TIMEOUT), включая true для WAIT_FAILED |
| IsRunning | При null handle возвращает false; иначе вызывает GetExitCodeThread, игнорирует его bool result и проверяет output code `259`; иной code очищает handle и возвращает false |
| Resume | При null handle false; иначе очищает байт `+0x1C`, вызывает ResumeThread и возвращает true независимо от его результата |
| Suspend | При null handle false; иначе вызывает SuspendThread и возвращает true независимо от результата |
| Sleep | Передаёт caller duration в Sleep, не обращаясь к handle |
| Terminate | При null handle false; иначе передаёт handle и caller exit code в TerminateThread, возвращает true только при результате ровно `1` |

Create не проверяет прежний handle перед перезаписью, сохраняет параметр тела
и при неудаче OS. Wait, Resume, Suspend и Terminate не очищают handle. Ни проверка
завершения, ни деструктор не вызывают CloseHandle; деструктор также не ждёт
завершения и не прекращает поток.

Clone создаёт новый объект и вызывает NamedCopy через вторичный subobject.
Копируется имя; параметр тела, байт и handle сохраняют фабричные нули.

## Host и границы

Все OS операции требуют явно предоставленного сервиса; успешных callback
по умолчанию нет. Host отвечает за реальное исполнение start routine, ожидание,
планирование и освобождение handle. В переносимом Create адрес entry заменён
явным uint32 token, аргументом остаётся объект spPCThread. Token отображается
на callable только платформенным провайдером.

При GetExitCodeThread, не записавшем output, оригинал сравнивает оставшееся
содержимое stack cell. Переносимый слой требует наблюдаемое значение этой cell;
если оно неизвестно, операция явно отклоняется. Успех OS отдельно сохраняется
в описании наблюдения и не влияет на восстановленную проверку code.

Квалификация собственных методов использует точные аргументы OS вызовов и
явные выходы внешних сервисов. Реальное многопоточное исполнение entry, races,
shutdown caller и полная интеграция игры этим компонентом не замыкаются.
PS2 реализация не утверждается.
