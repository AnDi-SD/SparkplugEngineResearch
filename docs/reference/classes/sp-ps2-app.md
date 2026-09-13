# spPS2App

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2App](../../../Sparkplug/Code/SparkplugPS2/spPS2App.h).

Статус: identity, inheritance, constructor/destructor, обе vtable и базовый
lifecycle-loop восстановлены. Имена lifecycle-методов аналитические. В ELF нет
строки с исходным translation-unit path, поэтому директория
`Sparkplug/Code/SparkplugPS2` восстановлена по имени платформенной подсистемы и
не отмечается как exact source path.

## Identity и layout

Initializer `0x00484BF0` заполняет registration `0x004B0EC0`:

| Поле | Значение |
| --- | ---: |
| class name | `spPS2App` @ `0x0045B2B8` |
| class ID | `0x354B1350` |
| base | `spApp / 0x391B146A` |

Constructor `0x001E7730` вызывает `spApp::spApp` `0x00100230`, затем только
возвращает primary/support vptr к платформенным таблицам. Дополнительных полей
он не инициализирует; `spPS2AppLayout` равен base layout `0x20`. Destructor
`0x001E76C0` аналогично восстанавливает свои vptr и вызывает base destructor.
Clone `0x001E7770` возвращает null; support thunk находится в `0x001E7780`.

## Lifecycle

Полезные callbacks находятся в support table. Адреса table slots считаются
от `0x00491054`; первые два слова MIPS ABI и возможная корректировка `this`
учтены отдельно:

- `0x001E76B0` — немедленно вернуть true (`Initialize`);
- один slot остаётся pure и реализуется concrete наследником (`Update`);
- `0x001E7650` — no-op (`Shutdown`);
- `0x001E7660` — повторять virtual Update до false, отметить termination и
  вернуть ноль (`Run`).

Entry/helper `0x001E75B0` создаёт concrete `wxPS2App` через `0x003E5040`,
выполняет lifecycle через интерфейс и уничтожает объект. Регистрация
`wxPS2App` (`0x004C4DF0`, class ID `0x36973698`, base `0x354B1350`) и factory
`0x003E5C00`, выделяющий `0x48`, независимо подтверждают позицию
`spPS2App` между общим `spApp` и игровым приложением.

## Открыто

1. Original source/header path и имена lifecycle-методов.
2. Точная сигнатура pure Update и место хранения termination state с учётом
   secondary-this adjustment.
3. Роли двух leading ABI words каждой PS2 vtable во всех типах приложения.
4. Аппаратные bootstrap/shutdown вызовы concrete `wxPS2App`.
