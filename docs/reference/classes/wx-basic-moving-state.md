# wxBasicMovingState

`wxBasicMovingState` имеет Class ID `1D533B89` и физически наследует
`wxCharacterState` на PC и PS2. Его собственный конструктор устанавливает
selector `+10` в ноль. Размер объекта на обеих платформах — `3C` байт.
Переносимый класс находится в [wxBasicMovingState.h](../../../Winx/Code/wxBasicMovingState.h)
и [wxBasicMovingState.cpp](../../../Winx/Code/wxBasicMovingState.cpp).

| Контракт | PC | PS2 |
| --- | ---: | ---: |
| Factory | `402B40` | `3F3EA0` |
| Constructor | — | `2C7C00` |
| Vtable | `6F8810` | `49A3F0` |
| Permission, slot 13 | `517790` | `2C7B40` |
| Movement request, slot 12 | `5177E0` | `2C77E0` |

При проверке разрешения перехода target `10` или `11`, текущий selector,
отличный от `9`, и нулевой pending handle немедленно дают true. В остальных
случаях вызывается общий потребитель `4FB330`/`2A6C30` с receiver из поля
`+18`, handle из `+24` и `consume=1`; возвращается его результат. Потребитель
может обнулить совпавшие pending записи receiver. Переносимый код вызывает
существующий [wxCharacterStateHost](../../../Winx/Analysis/Host/wxCharacterStateHost.h),
который предоставляет эту операцию; фактическая привязка receiver к игре ещё
не восстановлена.

PC factory, пустой clone и удаление проверены исполнением оригинала. PS2
constructor, vtable и slot 13 проверены отдельно. Реализация slot 12 требует
дополнительного исследования и явно сигнализирует о невосстановленном пути;
она не подменяется пустым базовым hook. Тест `WinxBasicMovingStateTests`
проверяет обе короткие ветви, вызов потребителя и сохранение границы slot 12.
