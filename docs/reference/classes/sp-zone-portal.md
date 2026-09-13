# spZonePortal

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spZone](../../../Sparkplug/Code/Sparkplug/spZone.h), [spZonePortal](../../../Sparkplug/Code/Sparkplug/spZonePortal.h).

Статус: **полный структурный/read-only decode PC и PS2**. Запись и мутация
поля пока намеренно не разрешены.

## Итог

`spZonePortal` — направленное ребро между двумя `spZone`. Источник не хранится
в payload самого портала: это field 3 `Zone` физического родителя
`spPartitionNode`. Назначение задаёт field 0 `DestinationZone`.

У класса одна serializer-секция:

```text
spZonePortal section
field 0: DestinationZone  # одно ненулевое relationship -> spZone
field 1: Polygon          # UInt32 count + Vector3[count]
field 2: Open             # Boolean
field 0, empty: terminator
```

## Сопоставление платформ

Разница фактических байтов inline relationship обусловлена вложенным содержимым
zone и platform-specific object IDs, а не layout `spZonePortal`.

Разбор продолжен в
[`spZonePortalNode`](sp-zone-portal-node.md): подтверждены две секции,
унаследованный node-transform и фиксированный порядок двух portal-ссылок.
