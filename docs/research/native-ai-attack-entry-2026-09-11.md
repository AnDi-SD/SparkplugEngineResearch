# AIAction: начальные поля девяти атак

82 успешных исполнения PC/PS2 проверяют virtual v10 девяти классов.
По25 полных возвратов и16 границ настоящего owner selector на платформу.
Начальные поля, оригинальный поиск цели и выбранная ветвь подтверждены;
выполнение всех последующих атак и штатный startup не заявляются.

## Источники цели

Ниже смещения и адреса шестнадцатеричные. Указатель owner: action20/24;
character: owner144/154; command: character130/13C; perception:
character154/160. Входной граф заимствован явно и целиком покрыт24KiB guard.

- BacoAttack берёт цель из настоящего registry lookup `4E21D0/369B60`.
  Проверены существующий cached target и пустой registry count0.
  Глобальный registry задан заранее по765AD8/49FD88; startup и поиск
  непустого некэшированного registry остаются открытыми.
- DroidAttack, FrogAttack, IceWormAttack, ShadowBeastAttack и SpiderAttack
  вызывают настоящий [поиск ближайшей цели](native-ai-perception-control-2026-09-11.md)
  `4F1910/36EBC0`. Здесь проверены пустой список и одна цель.
- IceGargoyleAttack, IceGargoyleClaw и IceGargoyleWithdrawl берут Node
  через существующий camera manager765AF0/49FC78: manager28→view18.
  Ненулевой Node, null Node и null view проверены отдельно.

При отсутствии цели первые шесть всё равно сначала записывают начальные
поля, затем запрашивают owner action key1, кроме IceWorm, который запрашивает0.
Камерные три класса также запрашивают0. Parameter всегда0 на этих путях.
Выполнение останавливается на настоящем `591C80/225DF0`, выбранном
через original base AIBehavior vtable. Возврат selector не подменяется.

## Начальные поля

В таблице указаны PC offsets; для перечисленных собственных полей action
PS2 offset больше на4. Command offsets одинаковы.

| Класс | Эффекты входа |
|---|---|
| BacoAttack | command1D=1;target3A8;state3AC=4;word3B4=47AFC800(90000f);word3B0=0;byte3BC=0;word354=0 |
| DroidAttack | command1D=1,1C/20/21=0;target3A8;state3AC=0;word3B4=47742400(62500f);word3B0=0;byte3BC=0 |
| FrogAttack | command1D=1,command pointer3E8;target3A8;state3AC=0;word3B0=0;byte3BC=0;float3B4=160000×параметр character config |
| IceWormAttack | command1D=1;target3A8;state3AC=0;word3B0=0;условное копирование трёх координат в3B8/3BC/3C0 |
| ShadowBeastAttack, SpiderAttack | command1D=1;target3A8;state3AC=0;word3B0=0;byte3BC=0;float3B4 сначала160000,при найденной цели уточняется расстоянием |
| IceGargoyleAttack | Node3D8;state3DC=2;word3E0=0;byte3B4=0 |
| IceGargoyleClaw, IceGargoyleWithdrawl | Node3AC;state3B0=1;word3B4=0 |

Droid/Shadow/Spider при найденной цели дополнительно записывают perception:
byte70=0,word74=7F7FFFFF,byte78=1. При отсутствующей цели эти поля сохраняются.
PC оба раза получает существующий perception через оригинальный515350,
PS2 читает поле character непосредственно.

Frog читает config pointer из character138/144, множитель из config138/144.
Значения0,0.5,1,2,−1 проверены, произведения совпали на обеих платформах.
Отрицательное значение описывает машинное поведение заданного входа,
а не утверждение, что такой config встречается в обычной игре.

IceWorm проверяет **первую** сохранённую координату3B8/3BC: только при x<1
копирует тройку из config. Config pointer character138/144; индекс
config188/194; начало массива config13C/148, шагC. Проверены x−1,0,0.5,1,2,
индексы0/2 и target/null target. При x>=1 все три прежние координаты
сохраняются. Нельзя заменить эту проверку тестом всей тройки или её нулевости.

Shadow/Spider вызывают полный original distance helper `597660/293210`
для собственных/целевых Node,force0,tolerance0. При square distance<24000
float3B4=0, иначе160000. Статически порог подтверждён exact word46BB8000
на обеих платформах. Проверены позиции(3,4,0),(0,0,0),(154,0,0),
(155,0,0),(0,160,0). Значение точно на пороге этим integer-square набором
динамически не проверено. Включение Y подтверждается вертикальным случаем.
Shadow дополнительно сохраняет move из character12C/138 в action3DC/3E0
и записывает target Node в move1E0/1EC. Это реальная ссылка на Node,
не указатель perception или action.

## Проверка и исправления стенда

Пилот1 сделал16 попыток за1,190s:15 прошли, PS2 Shadow остановился на
unmapped fetch при переходе из distance helper в отсутствующую зависимость.
В новый probe не были подключены ранее квалифицированные abs423B58 и
vector-store109AF0; также был короче хвост293210. Подключён прежний
полный контекст из `probe_ps2_integer_square_integration.py`, без изменения
игровых инструкций. Сохранены первоначальные probe/selection/failed result.

При отборе только оставшихся случаев обнаружена ошибка runner: пустой
список платформ у уже успешного случая приводил к обращению к несуществующей
переменной row. Этот запуск не исполнил ни одной guest instruction;
ошибка и source сохранены отдельно. После фильтра пустых групп новый пилот
проверил3 оставшихся исполнения за0,180s, общий пакет64 за4,891s.
Итого83 guest attempts,82 успешных; успешные случаи не повторялись.

В успешных результатах60 original perception calls,4 registry calls,
20 distance calls; каждый отдельно наблюдался. PS2 интерпретировал370
SQ/LQ и93 ACC операции в ранее квалифицированных узких профилях;
все адреса/значения сохранены, исходные code bytes неизменны.
Численные входы конечны и ограничены; general EE FPU, нечисловые значения,
произвольные масштабы и нормальная подготовка всех записей не заявляются.

18 platform updates по5 за девять новых v10 контрактов. Старый поиск цели,
distance helper и v12 повторно не засчитываются. Нового C++, полностью
закрытых классов или изменения игровых алгоритмов нет.

Контракт: [ai-attack-entry-contracts](../../research/ai-attack-entry-contracts-2026-09-11.json).
Probe: [probe_ai_attack_entry.py](../../research/probe_ai_attack_entry.py).
Локальные данные: `local-data/results/native-cycle-20260911-0730/ai-attack-entry/`.
Основные methods captures использованы из предыдущего пакета
`local-data/results/native-cycle-20260911-0730/ai-entry-reset/methods/`;
дополнительные windows и точные зависимости закреплены в новом контракте.
