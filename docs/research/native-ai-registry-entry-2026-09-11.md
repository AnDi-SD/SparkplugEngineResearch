# AIAction: входы через реестр и сообщения владельцу

52 исполнения прошли с первого раза:16 PC и36 PS2. Golem,Knut,Yeti
получили новые v10 контракты;Darcy,Icy,Stormy,WinxFly — полную PS2 проверку
ранее раздельных registry/caller компонентов. Во всех случаях вызван
исходный поиск цели4E21D0/369B60 с существующим реестром:cached target
либо пустой список. Инициализация singleton и поиск в непустом списке
здесь не исследуются повторно.

## Новые поля и порядок вызовов

Смещения ниже PC,шестнадцатеричные;соответствующие PS2 own поля больше на4.
Входы — две заливки5A/A5,аргументы0/FFFFFFFF,target/null target.

| Класс | Поведение до указанной границы |
|---|---|
| GolemAttack,5B7B90/24DCB0 | target3A8;state3AC=0,word3B0=0,float3B4=160000,byte3BC=0;сообщение271F владельцу с bool low byte1;затем при отсутствии target selector key0,parameter0 |
| KnutAttack,5AC9D0/25D5C0 | target3A8,state3B0=5;нулевые words3BC,3B4,3C0,3C4,3C8,3CC,3FC,400 и bytes3B8,3AC,3E8,404;два сообщения2725:target_face/true,затем target_back/false;byte3DC=1;собственная Node local position20..28→own3D0..3D8;без target selector key1,parameter0 |
| YetiAttack,5AFEA0/270CE0 | command1D=1,target3A8;нулевые words3AC,3BC,3B0,3B4,3C4,408,42C,47C и bytes3C0,3C8,424,425,3DC,3DD;без target selector key0,parameter0 |

Golem с target полностью возвращается;проверен также отсутствующий
owner entity,при котором исходная ветвь пропускает отправку сообщения.
Knut с target остановлен до5ACB26/25D72C — обращения к cached Node field.
Yeti с target остановлен до5AFF46/270D5C — загрузки объекта из3E0/3E4.
Тип/подготовка последнего объекта ещё не установлены;его virtual+34/+3C
не заменялся произвольным callback. Дальнейшие поля и сообщения Yeti,
видимые в линейном окне,не считаются исполненными.

## Реальные сообщения и неинициализированные байты

Использован явно заданный экземпляр базового wxEntity с оригинальной
таблицей6F4EB8/49B8D0. Notify5B7A00/100810 действительно пустой в этом
классе;исполнялся исходный ret/jr. Это проверка вызовов с базовым получателем,
не доказательство реакции произвольного производного игрового владельца.
Оригинальные таблицы,PC прежняя identity и PS2 factory3F8570→ctor286CA0
подтверждают выбор. Раннее окно registration t2=2866C0 ошибочно подписано
constructor в exploratory capture;оно не используется для этой identity.

Envelopes имеют8 words:code,0,0,0,sender-action,0,payloadA,payloadB.
Golem payloadA=0. У Knut payloadA указывает на строки target_face
(PC708F48/PS245E8E8) и target_back (PC6FF4F8/PS245E8D8).
Проверено24 actual Notify calls:8 Golem и16 Knut.

Исходный код записывает **только младший байт** временного bool,после чего
копирует полный word. Стек непосредственно до входа явно заполнен5A/A5;
payloadB оказывается5A5A5A01/A5A5A501 для true и5A5A5A00/A5A5A500
для false. Старшие байты не нормализуются в0:это незаданные исходным кодом
данные,а не новый 32-bit bool контракт. Пробные заливки стека заданы
до первой игровой инструкции;возвраты callbacks не подставлялись.

## PS2: связанные вызовы вместо отдельных компонентов

Darcy248790,Icy2591D0,Stormy266580 иWinxFly26D070 теперь проходят реальные
prologues,registry call,собственные записи и epilogues.20 whole returns.
Общие поля сохранены из
[ранее проверенного контракта](native-ai-action-entry-fields-2026-09-11.md);
его field oracle импортирован,не переписан. WinxFly при существующем
command дополнительно обнуляетcommand4 **до поиска цели**;null command
пропускает только эту запись. Проверены обе ветви и target/null target.
Прежние PC исполнения не повторялись и не получают нового балла.

## Проверка и ограничения

Пилот10 /0,561724s,пакет42 /2,275002s. Всего28 полных возвратов,
16 selector boundaries,4 resource-field и4 cached-node-field boundaries;
52 registry calls,24 Notify calls,136 PS2 SQ/LQ. Guard16KiB проверяет
весь граф,original PS2 code bytes сохранены. На полных PS2 возвратах
восстановлены SP,RA,S0..S4 и все upper64 GPR.

Нормальный lifecycle,заполнение реестра,поведение derived Notify,
возвраты selector и указанные продолжения Knut/Yeti остаются открыты.
Ошибок основных исполнений не было. Общий CPU wrapper не менялся,
повторная CPU регрессия после пакета unsigned division не требовалась.
10 platform updates +5:три класса на обеих платформах ичетыре PS2.
Новых C++ реализаций и полностью закрытых классов нет.

Контракт: [ai-registry-entry-contracts](../../research/ai-registry-entry-contracts-2026-09-11.json).
Probe: [probe_ai_registry_entry.py](../../research/probe_ai_registry_entry.py).
Данные: `local-data/results/native-cycle-20260911-0730/ai-registry-entry/`.
