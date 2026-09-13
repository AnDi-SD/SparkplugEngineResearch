# События анимации: завершение,текущее состояние и звук

## Отметки окончания

Для code3 и9 handle берётся из message1C. Вначале сравнивается old,
затем recent. При совпадении очищается только первая совпавшая запись и
счётчик уменьшается на1 modulo32bit. Даже если old==recent==handle,два
декремента не происходят. Совпадение с0 также учитывается;явный counter0
может статьFFFFFFFF. При отсутствии совпадения счётчик сохраняется.

Mark записывается независимо от совпадения с историей. Code3 затем передаёт
исходное сообщение через настоящий spBaseObject forward40F960/100610.
Code9 заканчивается после собственного mark. Для code3 PC использует реальные
4FB5A0+40F960,для9 —4FB5E0;PS2 имеет ту же логику inline.

PC code3 с parent4=0 исполняется полностью. PS2 post-SQ вход2A6DB4 имеет
A0=parent,A1=message,V1=11,как после настоящего prologue,и останавливается
на100610. Code9/ignored доходят до2A6F84 перед LQ. Это не полный PS2 v1.

## Тег сначала поступает состоянию,затем звуку

Затем заново читается character и его поле144/150. Это **wxAudioEmitter**,
class ID CCABEFEA,а не AIAction/AIBehavior. Идентичность найдена через
PS2 initialization2A9798..A8: lookup2933A0(CCABEFEA)→character150.
Это часть реального character v10 2A96B0. На PC protected code13E0F8A
передаёт тот же ID в599120,а return13E0FAA записывает EAX в[this+144].
Исходные getter/factory таблицы независимо связывают vtable701B94/495530
с wxAudioEmitter. Его нужный v14 —586C60/3C7DE0.

## Пересылка наблюдателям

PC container4→head;head0/next0/element8. PS2 end=container4,first вend4,
next4/element8. Лишь используемые поля заданы явно,создание контейнера,
ownership,нормальная допустимость duplicate и mutation/reentry не утверждаются.
На всех случаях целиком сравниваются4000h bytes borrowed storage.

## Предварительный фильтр звукового события

Разрешённая ветвь строит key из tag10 name pointer,неизменённого variant и
константы1000000. PC lookup receiver=audio128,PS2=audio134. Здесь lookup ещё
не исполняется:тип сравнения key,выбор audio entry,загрузка/проигрывание,
random/no-repeat и доставка на устройство остаются открытыми. Отклонённая
PC ветка возвращается полностью;PS2 достигает3C7FEC перед LQ. PS2 start3C7DF8
получает S2=audio,A1=message,A2=variant,как после собственного prologue.

Учёт: wxAnimationController PC35→45,PS2 30→40;wxAudioEmitter PC20→25,
PS2 15→20. Четыре updates,новых полностью закрытых классов нет. Forward и
уже изученный Bird callback служат зависимостями,повторного credit не получают.
