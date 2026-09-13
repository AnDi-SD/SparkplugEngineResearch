# CharacterState: управляющие записи и события анимации

## Управляющие записи v12

Offsets hex. На обеих платформах state14 указывает на character; от него
move находится PC12C/PS2138. Имена полей аналитические, не original symbols.

| Классы | PC | Поведение |
| --- | --- | --- |
| FrogJumping | 520E50 | Обнуляет move word4 только при state byte3C=0; значения1/255 сохраняют move |
| MinotaurStunned | 521B60 | Обнуляет move byte60 иword4 в этом порядке |

## Имена событий v15

Входная цепочка одинакова: argument1C→event10→указатель на строку.

| Класс | PC / PS2 | Точное событие и запись |
| --- | --- | --- |
| BirdFlyingState | 517C40 / 2E5480 | `event_takeoff`: character byte PC26C/PS2278=0 |
| GhoulJumpingState | 51AD60 / 2E5010 | `air`: в move три word PC1C8/1CC/1D0, PS21D4/1D8/1DC получают0/43BB8000/0; следующий byte PC1D4/PS21E0=1 |
| TrollMovingState | 51A680 / 2F8D40 | `event_air_begin` →state byte3D=1; `event_air_end` →0 |
| YetiMovingState | 51A0C0 / 2F00D0 | Те же два события меняют state byte3C |

## Граница PS2 strcmp

Обработчики исполнены после SQ-пролога до LQ-эпилога с заданными borrowed
входами. Настоящий strcmp вызывается с entry40A8A8,без замены результата.
Для string address сalignment1 он сам выбирает байтовую ветвь40A9D8;
сalignment8 — 64-битную ветвь40A96C. Обе ветви исполнены и записаны в trace.
Исходные литералы загружены по своим реальным адресам и сверены с PC.
