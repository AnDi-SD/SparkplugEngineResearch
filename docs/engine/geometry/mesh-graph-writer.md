# PC MeshData: indexing и generic reference writer

Whole `4672C0` индексирует mesh и исполняет native `5A7DB0`: true без
relationships. FAT получает один ID1; CPU index/vertex buffers остаются
локальным owning payload, не отдельными графовыми ресурсами.
Whole `467350` выполняет найденный writer, записывает header CPU MeshData
`33C34CF0/SBOO`, исправляет размер и actual FAT metadata
`[id,class,offset,size,written]`. Следующая ссылка — ID1,size0; null — u32zero.
Все bytes и metadata совпадают с compiled source generic protocol.

В source оба MeshData serializer подключены к существующим IndexReference/IndexResource/WriteReference через подтверждённый пустой IndexRelationships. Host type guards сохраняют отказ неподтверждённым derived serializers. `SparkplugMeshReaderTests`: **346/346**.
