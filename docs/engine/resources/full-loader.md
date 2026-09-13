# PC: внешний materializer и переносимый full-file loader

## Original outer materialization

Важные отличия от inline reference resolver467670:

- Outer сначала Register cache, затем SetName; resolver делает SetName первым.
- Outer fileID!=0 вызывает466490, но **не сохраняет результат** в object20.
- Отдельный флаг первого обработанного entry сбрасывается даже при null object.
  Поэтому первый unresolved external entry и второй успешный Animation дают
  null root, хотя второй объект реально создан. Это не «взять первый ненулевой».
- Outer helper сам не очищает FAT. Это обязанность whole caller422B50.

## DX hook: platform gate и точный serialized ID

`4AAB80` допускает `4AA870` только если platformMask содержит **bit2**.
Bit4 без bit2 не включает hook. В `4AA870` prefill кэша касается лишь
unmaterialized entries с **точным** serialized MeshData ID33C34CF0.
Базовый Mesh ID3F077B6C тоже может найти MeshData через category cache, но hook
его не обрабатывает. Нельзя заранее резолвить все FAT классы: это меняет root.

Source DX hook теперь соблюдает native bit2/exact-ID gate.

FAT очищается RAII даже при ошибке, новые объекты принадлежат read context, cache pointers заимствуются. Ошибка poisons context; retry не разрешён. Context/borrowed managers обязаны иметь согласованный lifetime; общие SMO cycles этим не доказаны. Logical origin сохраняется после возврата, как у original. `ReadString` теперь различает wire NULL и allocated empty name для FAT lookup. Host string representation всё ещё не обещает lossless malformed terminators.

## Оставшееся

Непустая DX mesh batch/materializer, другие concrete SMO payload adapters,
whole native save orchestration и unknown-field preservation, startup/FAT
constructor, все error/cache/cyclic lifetime ветви, real resource→PC backend.
MemoryStream::Open сохраняет native reuse hazard; test helper теперь Close-ит
старый buffer и сбрасывает origin. Это не глобальное исправление original Open.
100% не объявляется: перечисленное остаётся в контракте завершения.
