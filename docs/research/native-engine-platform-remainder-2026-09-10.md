# Платформенные классы движка: PC и PS2, 10 сентября 2026

Проверены 51 отдельное имя: 27 PC и 24 PS2. Имена `spDX*` и `spPS2*`
не объединяются в одну запись по сходству назначения. Для PC выполнены
17 полных холодных lifetimes (factory, RTTI, Clone, delete clone/original),
всего 91 вернувшаяся операция класса; семь попыток имеют сохранённые границы.
На PS2 независимо исполнены 24 getters, найдены 21 обычный constructor/table
match и отдельно подтверждены два вторичных интерфейса. Это частичное
исследование классов, не проверка графических, аудио и видео устройств.

## Объекты и платформенные различия

Полные PC lifetimes: AsmShaderParser16, AudioBank40, AudioBankEntry260,
AudioManager344, AudioVoice56, BallisticPFX80, ShaderEffect36,
ShadowVolume244, ShadowVolumeMesh68, TextureManager44, PCBloomFX140,
PCFXFileLoader1344, PCPixelShader84, PCThread36, PCVideoStream36,
WindowsError40, WindowsFont4524 (число — байты оригинальной аллокации).
Имена без PC-префикса в этой строке относятся к `spDX*`.

Примеры независимо установленных размеров PS2: AudioBank44,
AudioBankEntry148, AudioManager308, AudioVoice60, BallisticPFX36,
CubeTexture56, DynamicMeshData256, GamePad204, Keyboard80, Mouse224,
Light256, Texture320, TextureManager40, VRAMCacheManager212,
VideoStream36. Все записи и остальные размеры — в контракте.
Нулевые registered factories: PC DXBloomFX/DXInputDevice/DXPixelShader,
PS2 InputDevice. Нулевой factory сам по себе не доказывает абстрактность.

У PC BallisticPFX, PCThread и PCVideoStream интерфейс BaseObject расположен
на +4. Первый прогон ошибочно использовал primary table: Thread/Video
вернули посторонние значения, Ballistic вышел за таблицу. Исходники игры
не менялись: исправлен адрес интерфейса в исследовательском runner.

| PC класс | Primary table | BaseObject +4 | Clone | deleting adjustor |
|---|---:|---:|---:|---:|
| spDXBallisticPFX | 006EF8E8 | 006EF8CC | 004AC860 | 004AC6A0 |
| spPCThread | 007291FC | 007291E0 | 006BE630 | 006BE5B0 |
| spPCVideoStream | 006F2AC4 | 006F2AA8 | 004C73B0 | 004C7330 |

Каждый Clone возвращает новый whole-object+4 и вызывает Copy по secondary
interface; deleting adjustor вычитает четыре перед деструктором/освобождением.
Все три повторных полных lifetimes прошли. У PS2 BallisticPFX и VideoStream
это независимо подтверждено оригинальными post-base constructor prefixes
и getter adjustors (4 исполнения за 0,129 с), с проверкой всех 36 байт записи:
secondary tables 00491D30/00491E30, getter adjustors 0020A150/0020D940.
Автоматические candidates direct-getter-reference minus24 здесь неверны:
прямой getter дополнительно находится за семью BaseObject slots. Они сохранены
как НЕ подтверждённые candidates, верные интерфейсы записаны отдельно.
Полные PS2 конструкторы, Clone и деструкторы этого эксперимента не исполнялись.

## spPCThread: решения по ответам ОС

23 случая за 5,772 с: actual factory 006BE5C0, первичные методы
006BE490/4C0/4F0/530/550/570 и original cold delete через +4.
Импорты сверены по pristine PE. Стенд подаёт явные ответы Kernel32;
потоки ОС и пользовательские callbacks не запускаются.

- CreateThread: security/stack=0, flags=4, callback из первого аргумента,
  parameter=this; второй аргумент метода сохраняется в +18. Handle записывается
  в +20, результат метода — handle!=0. Thread ID идёт в локальную стековую запись.
- WaitForSingleObject: при нулевом handle сразу true. При ненулевом false
  только для ответа 0x102; ответы 0,0x80,FFFFFFFF дают true.
- GetExitCodeThread: статус 0x103 сохраняет handle и даёт true. Статусы 0 и
  FFFFFFFF очищают +20 и дают false. BOOL API не проверяется оригиналом;
  в этих тестах API возвращал 1 и записывал явный код. Вариант провала API
  без записи выходного кода остаётся за пределами подтверждённого результата.
- ResumeThread/SuspendThread: при нулевом handle false. При ненулевом всегда
  true, включая ответы API 0 и FFFFFFFF. Resume дополнительно обнуляет byte1C.
- TerminateThread: false при нулевом handle; при ненулевом true только если
  результат API равен 1. Handle этот метод не обнуляет.

Во всех случаях проверен весь объект36; заёмный тестовый handle обнулён перед
холодным original teardown. Эти решения не доказывают корректность политики
управления ресурсами, синхронизации, завершения callback или нового backend.
Отдельного зарегистрированного spPCThread в PS2 каталоге нет; PC-выводы ему
не приписываются.

## Сохранённые границы

DXCubeEnvMapLayer20, DXEnvironmentMapLayer24 и DXMirrorLayer20 созданы,
но Clone выходит на 00412C01 с отсутствующей исходной ссылкой. Mirror factory
сначала упёрся в micro cap; свежий protected-block прогон прошёл factory,
затем выявил CRT memmove IAT006D9300. Повтор с существующей bounded-memory
моделью дошёл до той же игровой границы 00412C01. Это не доказательство null Clone.
DXCubeTexture и DXGamepad/Keyboard/Mouse требуют неинициализированного в
холодном стенде контекста устройств; fake renderer/input не добавлялся.
Адреса, memory errors, состояния и все неудачные прогоны сохранены.

Первый пакет24 попыток/3 процесса занял 10,561 с; четыре адресных повторения
(три интерфейса и Mirror) — 2,969 с. Большой asset corpus не сканировался.
Shared collector дополнен безопасным чтением отсутствующего pending после
post-return assertion; это исправление отчётности, не игровой логики.

## Учёт и источники

Новые оценки: PC полный cold lifetime20, частичный15, getter/зависимость10;
PCThread45 за проверенные решения методов. PS2 constructor/getter15,
InputDevice getter10; два вторичных интерфейса20. Полностью закрытых классов
не добавлено. Оценки не являются долей инструкций или готовностью backend.

Контракт: `research/engine-platform-remainder-contracts-2026-09-10.json`.
Дополнительные исполнения: `secondary-interfaces-run1.json`,
`thread-decisions-run1.json` в локальной папке этого блока.
Manifest сохраняет SHA контракта, документа, raw captures, всех попыток,
точных версий runner и используемых вспомогательных источников.
