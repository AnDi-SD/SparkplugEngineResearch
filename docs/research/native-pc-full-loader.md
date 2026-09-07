# PC: внешний materializer и переносимый full-file loader

6 сентября 2026, checkpoint5 незавершённой цели PC SMO/SAN.
Основание: оригинальный PC executable SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [loader](native-pc-smo-san-loader.md) и
[read-reference](native-pc-read-reference.md), не новая параллельная система импорта.

## Original outer materialization

`422940` обходит insertion-order FAT. Entry с уже заполненным `object+20`
пропускается целиком, **включая выбор возвращаемого root**. Для inline entry
с fileID0 cache lookup предшествует seek и выбору serializer. При cache miss
Seek(start, offset) проверяется; header/factory публикует object20 до payload.
FAT size не ограничивает reader: направленный size1 читает фактические55 bytes.
Ошибка payload возвращает null, но опубликованный частичный объект остаётся.

Важные отличия от inline reference resolver467670:

- Outer сначала Register cache, затем SetName; resolver делает SetName первым.
- Outer fileID!=0 вызывает466490, но **не сохраняет результат** в object20.
- Отдельный флаг первого обработанного entry сбрасывается даже при null object.
  Поэтому первый unresolved external entry и второй успешный Animation дают
  null root, хотя второй объект реально создан. Это не «взять первый ненулевой».
- Outer helper сам не очищает FAT. Это обязанность whole caller422B50.

`probe_pc_materialization.py`:42 checks/8 режимов. Empty/one/two/prebound/
external-root/bad-offset/short-size/failed-payload. Все исполняются в прежнем
100k/2sec guest cap, native game/Windows/GPU не запускаются. Partial objects
и FAT entries уничтожаются отдельно; это fixture cleanup, не native rollback.
Missing serializer branch с внешним форматированием пока не исполнена.

## DX hook: platform gate и точный serialized ID

`4AAB80` допускает `4AA870` только если platformMask содержит **bit2**.
Bit4 без bit2 не включает hook. В `4AA870` prefill кэша касается лишь
unmaterialized entries с **точным** serialized MeshData ID33C34CF0.
Базовый Mesh ID3F077B6C тоже может найти MeshData через category cache, но hook
его не обрабатывает. Нельзя заранее резолвить все FAT классы: это меняет root.

`probe_pc_dx_hook_cache.py`:48 checks/6 режимов. Actual MeshData factory,
resource registration/find/remove и hook; cache-hit не читает metadata,
не создаёт combiner и не входит в GPU. RTTI/name startup и zero owner pointers
50/54 заданы явной empty-resource fixture, не выдаются за native Initialize.

Уточнение предыдущего checkpoint1: whole bbush header содержит platform1.
Hook slot действительно вызывался, но **4AA870 body пропускался по gate**.
Исторический manifest неизменяем; это уточнение не добавляет ему mesh evidence.

## Перенос существующей цепочки

В `spSerializerManager` добавлены явные analytical entry points
`LoadResourcesForAnalysis` и `MaterializeResourcesForAnalysis`: прежние
header/FAT/RTTI/serializer/stream и SAN field core собраны в полный путь.
Source DX hook теперь соблюдает native bit2/exact-ID gate.

Whole loader проверяет header, читает FAT и compatibility file index, проверяет
dataOffset, меняет logical origin потока, готовит hook и материализует entries.
Generic payload adapter пока реализован для Animation. Непустой DX batch plan
возвращает **явный unsupported**, а не частично успешную модель.

Host safeguards отмечены отдельно от native: ограниченный count/depth/object
count, строгие extents и read/seek checks; external fileID пока явный unsupported.
FAT очищается RAII даже при ошибке, новые объекты принадлежат read context,
cache pointers заимствуются. Ошибка poisons context; retry не разрешён.
Context/borrowed managers обязаны иметь согласованный lifetime; общие SMO cycles
этим не доказаны. Logical origin сохраняется после возврата, как у original.
`ReadString` теперь различает wire NULL и allocated empty name для FAT lookup.
Host string representation всё ещё не обещает lossless malformed terminators.

`spFullLoaderTests`160 checks, `spReadReferenceTests`266, CTest26/26.
Неизменённый **полный bbush.san** через original и portable loader совпал по227
captured values/owned names/PRS. Два одновременно живых загруженных SAN и
удаление первого не разрушают bindings второго. Portable full-file path на
bbush/bflower/barrel/bw совпал с прежним owned field core:4 файла/62 tracks.
Последнее — composed portable regression, **не whole original** на трёх ранее
capped файлах. Эти native calls не возобновлялись, лимиты не повышались.

## Оставшееся

Непустая DX mesh batch/materializer, другие concrete SMO payload adapters,
whole native save orchestration и unknown-field preservation, startup/FAT
constructor, все error/cache/cyclic lifetime ветви, real resource→PC backend.
MemoryStream::Open сохраняет native reuse hazard; test helper теперь Close-ит
старый buffer и сбрасывает origin. Это не глобальное исправление original Open.
100% не объявляется: перечисленное остаётся в контракте завершения.
