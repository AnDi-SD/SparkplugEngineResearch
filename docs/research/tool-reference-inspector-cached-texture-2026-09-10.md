# Общий reference reader в инспекторе и уточнение cached TextureData

`SmoSerializedFieldInspector.TryFormatRelationships` больше не читает ID/size
своим parser. Общий `spSerializer::ReadReferencePrefixForAnalysis`, через
existing `spv_reference_prefix` и `SmoNodeDecoder`, определяет точный extent:
NULL занимает4 bytes, non-NULL —8 плюс inline body. HOST projection сохраняет
формат отображения, уникальность resource ID и проверки inline class/size.
NULL не разрешается как ресурс с ID0. Это metadata, без создания inline target.

Старый inspector требовал8 bytes на любую ссылку и отвергал допустимый NULL Font.
Один новый test DLL с прежним Core воспроизвёл `IsDecoded=false`; с новым Core
прошло70 checks вместо59: public NULL Font и десять коротких sequence cases,
плюс прежние Text/TextNode проверки на `Media/Menus/menu.smo`. Две NULL,
NULL+sized и sized+NULL не теряют последний четырёхбайтовый ID; malformed
extent и trailing bytes отклоняются. Сборка FormatTests чистая; UI и native
игровые алгоритмы не менялись.

## Alfea01: отдельная граница authoring

Прежний общий статус слишком близко связывал пустой source-less TextureData
и cached `1848 marble`. Это разные случаи. Фактические объекты695 и1848
в Alfea01 имеют одинаковые16448 bytes и SHA256
`fb02110009530ec21d387f236ba43484a0d460f75c001aa7a582f07bd092f0a8`.
Оба содержат embedded → SourceNone → platform6 → native64×64 BGRA,
один stored mip16384 bytes. Независимый общий PC source inspector прочитал
оба payload полностью (cursor16440) и создал по7 runtime mip levels.

Сохранённый actual file-reader trace раньше прочитал695, а1848 разрешил из
cache: `spResourceManager::FindForAnalysis`/`spSerializer::ReadReferenceForAnalysis`
уже реализуют этот путь. External source resolver здесь не требуется.
Повторного original/file-loader запуска не было; использован прежний trace.
Независимое чтение копии не объявляется чтением skipped payload в исходном
file-reader trace. Authoring range `[2844011,2860459)` по-прежнему требует
отдельного решения coverage; этот guard не снят. Поддержка действительно
пустого source-less ресурса также не заявляется.

Артефакты текущего цикла: `reference-inspector/` и `cached-texture/`;
[предыдущая граница trace](tool-authoring-reference-trace-2026-09-10.md).
