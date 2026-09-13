# Entity: копирование изменённых полей 24 подклассов

## классы

| Класс | Переносов | Класс | Переносов |
| --- | ---: | --- | ---: |
| wxArrowTrap | 3 | wxCabinetPuzzle | 4 и 4 обратных |
| wxCharacter | 11 | wxColourfulFlower | 3 |
| wxDamageProxy | 3 | wxDoorPuzzle | 15 |
| wxDragonAIBehavior | 2 | wxFallingRock | 25 |
| wxGlyph | 6 | wxIntelliCam | 42 |
| wxKikoHole | 6 | wxLabelDisplayText | 7 |
| wxOneLinerMovingCharacter | 1 | wxProjectileManager | 14 |
| wxQuietusCarnivorous | 13 | wxRacingCheckpoint | 6 |
| wxSideQuestGivenObject | 4 | wxSparxBehavior | 1 |
| wxSpiderUpsieDaisyAIBehavior | 3 | wxSpinningStatuesPuzzle | 12 |
| wxSwampGazBubble | 11 | wxSwampPlatformRing | 3 |
| wxThunderMaker | 2 | wxUpsieDaisyGuardAIBehavior | 7 |

### Character, камера и ArrowTrap

У IntelliCam большинство пар отличаются на12 байт, но последние PC3E0/3E4
соответствуют PS23E8/3EC, то есть+8. Причина раскладки здесь не устанавливалась.
У ArrowTrap PC128/12C соответствуют PS2134/138, а word84 копируется по
одинаковому смещению на обеих платформах. Универсальное правило «всегда+12»
для Entity не вводится.

После изменения pointer-like payload teardown не исполняется. Normal gameplay,
владение ссылками, self-copy, общие непустые имена, failure родителя на PC и
полная вложенная PS2 транзакция остаются открытыми. 40 исключённых кандидатов
не получают результатов этого пакета. Новая граница не требует замены игровых
алгоритмов или исключения из правил разработки.
