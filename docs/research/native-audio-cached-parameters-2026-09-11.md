# Audio: cached параметры менеджера и состояние голоса

37 успешных original calls из38 attempts:PC18/PS219. Из них31 whole return
и6 явных device boundaries. Исследуются конкретные `spDXAudioManager`,
`spPS2AudioManager`, `spPS2AudioVoice`;PC `spDXAudioVoice` проверен только
на отсутствии device pointer и нового балла не получает. Проигрывание
звука,hardware startup и выполнение COM/SDK запросов здесь не заявлены.

## Параметры менеджера

PC4C4160 записывает два float arguments вD0/CC,ограничивает каждый [0,1]
и устанавливает byte154=1. PS2 counterpart1EC630 тоже ограничивает оба
аргумента,но первое значение дополнительно умножает на float3D4CCCCD
(приблизительно0.05) и пишет вC0;второе пишет вBC;byte130=1.
PS2 ABI использует F12/F13. Семь пар покрывают отрицательные значения,
0/1,выход выше1 и дроби. Это параметры без приписанного неподтверждённого
акустического назначения;NaN/denormal/general hardware rounding не проверены.

Отдельный PC4C4140 сохраняет только low byte аргумента вC8 и ставит154=1.
Проверены0,1,FF,100,12345678;значение100 даёт stored0. PS2 смежный1EC6C0
содержит SQ в conditional delay slot;его whole behavior в этом пакете не
квалифицирован,CPU allowlist ради него не расширяется.

PC4C3E50 возвращает32,PS21ED820 —48. Это исходные значения конкретных
virtual queries,а не измеренная пропускная способность физических устройств.

## Запрос громкости и граница платформы

PC4C3DF0 иPS21EDA40 при нулевом byte14 ничего не записывают и возвращаются.
При active оба сохраняют исходный float в18. PC для положительного значения
рассчитывает2000*log10(value) и вызывает original conversion helper60DB90;
для неположительного передаёт−10000. Проверены0,0.5,1:−10000,−602,0.
Пределы произвольных float и отсутствие зависимости от режима FPU не заявлены.

PC остановлен **до** исходного COM call4C3E21/4C3E47. На стеке стоят
`[deviceThis,level]`;receiver взят изmanagerF0. Fixture содержит только
память receiver/table,возврат COM не подставляется и целевой метод не исполняется.
Первое ожидание стенда ошибочно переставило эти два аргумента;исправлен
только oracle,failed probe/result сохранены,два успешных parameter calls
не повторялись. Original method и восстановленный C++ не менялись.

PS2 считает4096*value,конвертирует в integer и достигает настоящего1DF400
с `[A0,A1,A2]=[0,level,level]`:0/2048/4096 на выбранных inputs. Остановка
до входа SDK;последующие original calls сA0=1/2/3 видны статически,но здесь
не исполнены. **Открытая граница:**SDK1DF400 проверяет A0 на0 и уводит этот
первый вызов вветвь1DF5EC. Продолжение и результат применения громкости не
подтверждены. Рекомендуется сначала разобрать эту ветвь и вызывающий контекст;
нельзя выдавать сохранение параметра за успешную настройку звука.

## PS2 AudioVoice

Original1EE360 читает voice index изobj10,вызывает настоящий1DE410 и
проверяет его return на0. Helper проверяет unsigned index<48 и читает
ровно соответствующий byte из4B0670. Для допустимого index конечный query
возвращаетtrue при ненулевом byte;для48/FFFFFFFF —false. Проверены indices
0/16/17/47,values0/1/2/FF,соседние bytes имеют противоположное значение.
Ни звуковой статус,ни return helper не подменяются. Источник заполнения
этого48-byte SDK cache пока не исполнен.

PC4CCC70 при нулевом obj28 возвращаетAL0. Ненулевой device pointer ведёт
кCOM status query с последующим bit0 mask,но этот путь пока только виден
статически. Полный PC/PS2 audio hardware parity из этого не следует.

## Identity, проверка и остаток

Использованы уже доказанные concrete tables6F26D8/4911B0 и6F321C/491230.
PS2 manager factory1EE2A0 вызывает123AC0,затем пишет primary4911B0 и
secondary4911D4 вobj10. Base ctor123AC0 отдельно пишет48D040/48D064.
PS2 таблица содержит secondary header/destructor между общими и audio
методами;числовые slot indices нельзя переносить с PC буквально.
Первые exploratory table lists останавливались на0;продолжение снято отдельно.

Pilot1:3 attempts /0,305911s,два successes иCOM argument-order oracle error.
Pilot2:3 /0,211998s;batch32 /2,214146s. Guard4KiB,неизменные PS2 code bytes,
SDK cache,полный frame у whole returns;5 stack-spill operations.
Повторной общей CPU регрессии нет:wrapper не менялся.

3 assessments +5:PC Manager20→25,PS2 Manager15→20 иVoice15→20.
Остальные scouted audio/bank/base классы не получают балла,новых C++ и
полностью закрытых классов нет. Названия ранних scouts `manager-fade`
и `base-lookup` не доказывают семантику:первый содержит volume setter,
второй — bank removal/recreate path,дальнейшее отдельное исследование.

[Контракт и версии проб](../../research/audio-cached-parameters-contracts-2026-09-11.json),
[platform manifest](../../research/native-platform-audio-cached-parameters-2026-09-11.json).
Локальный evidence: `local-data/results/native-cycle-20260911-0730/audio-runtime/`.
