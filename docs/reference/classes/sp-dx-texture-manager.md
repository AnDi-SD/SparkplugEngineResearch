# spDXTextureManager

PC-класс `spDXTextureManager` (`0x3B9304C7`) наследует
[`spTextureManager`](sp-texture-manager.md) (`0x2F434D04`).
[Переносимые исходники](../../../Sparkplug/Code/SparkplugDX/spDXTextureManager.h)
сохраняют native factory, clone, lifetime и две короткие собственные операции.
Original header, translation unit и имена этих операций неизвестны.

## Интерфейсы и графическая граница

Factory выделяет `0x2C` байт, вызывает constructor общего manager и задаёт
primary vtable на `+0x00`, singleton-support vtable на `+0x10`. Сам leaf-класс
полей не добавляет. Primary table имеет девять слотов: семь общих операций
`spBaseObject` и два собственных слота `+0x1C`/`+0x20`.

Оба собственных слота у PC leaf указывают на один буквальный `ret`:
они не читают аргументы, не меняют manager или записи, не вызывают device и
не освобождают графические ресурсы. `NativeHook1CForAnalysis` и
`NativeHook20ForAnalysis` — аналитические названия этих операций. Их no-op
поведение является поведением оригинала; успешный графический backend ему
не приписывается.

`spDXTextureManager` поэтому не даёт доказанного пути создания COM-текстуры,
upload или привязки sampler. Такие операции требуется восстанавливать и
адаптировать на границах texture/renderer-классов. Источник и смысл opaque
records общего manager пока остаются открытыми.

## Clone и Copy

Clone создаёт новый manager через factory, регистрирует source/destination в
clone map, затем вызывает у source inherited root Copy. Root Copy возвращает
true и не изменяет destination. Поэтому clone имеет defaults нового
constructor: flag/count `0`, пустой vector и capacity `0`. Конфигурация и
записи source не переносятся. Новый объект становится singleton; уничтожение
clone безусловно очищает его и не восстанавливает source singleton.

Отдельный Copy в уже настроенный destination сохраняет его собственные
flag/count, vector и capacity. Notification slot также выполняет no-op.

## Освобождение через support view

Deleting adjustor singleton-support interface вычитает `0x10` из `this`,
затем вызывает тот же concrete destructor и, при установленном low flag bit,
освобождает whole object. Обычное удаление и удаление через support view
освобождают vector storage один раз. Переносимый C++ интерфейс нормализует
эти два указателя к одному объекту, сохраняя общий lifetime.

Доказанный объём: все девять primary slots, concrete factory/clone/delete,
support-delete adjustor, настройки/reserve и teardown opaque storage.
Не установлены исходные имена, предметная семантика записей и весь
producer/consumer graph; наличие исходников не означает закрытия этих границ.
