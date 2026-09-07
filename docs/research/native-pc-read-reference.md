# PC: чтение ссылок, inline materialization и кэш

6 сентября2026. Только PC executable SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
115 направленных native checks,4 fingerprints,167 portable checks.
Это продолжение [save-reference](native-pc-save-reference.md), не объявление
полной поддержки SMO или завершения цели100%.

## Адреса и точный порядок

| PC VA | Наблюдаемый контракт |
|---|---|
| `4678B0` | ReadReference(expectedClass, ID stream, payload stream) |
| `467670` | resolve(ID, payload stream), чтение inline-size |
| `4664C0` | FAT map20: resource ID -> entry |
| `458CB0` | register named resource в PC cache |
| `4586B0` | cache lookup по category/name |

Имена API здесь аналитические; exact TU — `spSerializer.cpp`. Вторичный
interface пока без исходного имени. Обёртка4678B0 сначала вызывает actual
ErrorManager gate, читает u32 ID из **второго** аргумента; первый аргумент,
expected class ID, вообще не используется. Третий аргумент передаётся в
resolver и обслуживает size/header/payload. Обычно callers передают один
stream дважды, но тест с разными cursor-ами подтвердил разделение.

Нулевой ID возвращает null, не читая size. Ненулевой ID:

1. Читает u32 inline-size; **результат этого Read не проверяется**.
2. Находит entry по ID. Missing ID ведёт в ErrorManager diagnostic.
3. Если entry.object20 уже заполнен, возвращает этот pointer; ненулевой
   inline-size пропускается Seek(current,size), чей результат игнорируется.
4. Иначе выбирает serializer по entry.classID, затем ищет ресурс в кэше.
   Cache hit сохраняется в entry.object20 и пропускает inline-size. Даже
   отсутствие serializer не мешает cache-hit: pointer ещё не разыменован.
5. При cache miss читает object header через primary slot1C, создаёт объект,
   **сохраняет pointer в entry20 до secondary reader slot8** и читает fields.
   Это делает адрес доступным вложенным ссылкам; полное cyclic ownership
   отдельно ещё не подтверждено.
6. После успешного payload применяет entry.name при engine IsKindOf NamedObject,
   затем пытается зарегистрировать NamedObject в resource cache. Только
   Mesh/Texture categories поддержаны самим cache manager.

Resolver НЕ использует FAT offset/size/fileID для fresh inline object.
Направленный fileID42 case читает inline, не вызывает file lookup466490.
Это не отменяет отдельную fileID-ветвь внешнего manager materialization422940.
Inline-size1 и999 при фактическом объекте55 bytes оба принимаются: новый
объект читается до завершения его serializer-а, размер не ограничивает read.
ExpectedClassDEADBEEF тоже не мешает получить реальный Animation.

Возвращаемые pointers **не retain-ятся**. Две FAT entries могут ссылаться
на один живой объект. Entry clear удаляет записи/имена, не сами объекты.
Заимствование кэша и владение newly created объектами — разные обязанности
вызывающего кода, их нельзя свести к «каждый pointer удалить один раз».

## Направленные отказы и границы

- Failed ID read: null, diagnostic, resolver не вызывается.
- Failed size read: stop467698 после AL0 доказывает продолжение без проверки;
  дальнейшее использование неопределённого size не исполнялось.
- Missing ID: stop4676B9 перед sprintf/внешним форматированием, без подставленного
  успеха и без пересылки IAT в Windows.
- Truncated duration payload: actual Animation reader вернул false, object20
  уже заполнен. Stop4677DA перед diagnostic formatting; partial object
  уничтожен отдельно в fixture teardown. Полная error UI-ветвь не исполнена.
- Existing-object skip1000 за пределами bytes: Seek false, resolver всё равно
  вернул object и не сообщил ошибку. Position осталась после ID/size.

Проба cache-hit использует реальную фабрику MeshData41A270 и реальную
регистрацию458CB0, lookup4586B0 и teardown. Startup RTTI tree и name-storage
остаются явно заданными fixtures. MeshData factory оставляет owner pointers
50/54 неопределёнными: первая попытка cache-only teardown дошла до CCCCCC.
Это известное constructor-свойство, не дефект cache. Fixture исправлен на
явный empty-resource input, **не** на выдуманный native Initialize(null,null).
После этого все отслеживаемые allocations освобождены. Register возврат
EAX0 не выдаётся за bool false: success установлен по реальной вставке8 bytes.

## Реальный SAN и переносимый исходник

`bbush.san` (hash в [loader карточке](native-pc-smo-san-loader.md)) обёрнут
ID/size вокруг неизменённых SBOO+fields. Original ReadReference выполнил
58848 инструкций с прежним100k/2sec cap. Actual Animation factory/reader,
actual NameManager и PRS; все allocations освобождены. Это directed reference
envelope, **не** ещё один full FFPS load. Cold capped whole loaders других
SAN не возобновлялись.

`spSerializer.*` теперь содержит generic read protocol и explicit host context.
Animation reader использует прежний field core, но наполняет уже созданный
объект, не заменяя опубликованный адрес. Optional name manager даёт owned
leases. `--inspect-reference` в portable tests идёт через FAT/dispatch/factory,
а не напрямую в field reader. С original совпали227 captured runtime values
(tracks, slots, capacity, tags, PRS); parser-derived pool metadata исключена.

Host guards намеренно строже: проверяются size/read/seek/extents, missing
serializer/factory и depth64/object4096. Error poisons context; retry больше
не потребляет bytes. Частично созданные объекты живут до context teardown,
который обнуляет owned FAT aliases и освобождает их; cache pointers остаются
borrowed. Менеджеры и borrowed ресурсы должны переживать context. Это host
lifetime policy, не доказательство общей native rollback/cyclic-graph схемы.

CTest25/25; `SparkplugReadReferenceTests`167 checks; native profile15/15
(14 guest modes + fingerprints), differential profile1/1.

Открыто: полные malformed/cache-error/tell/allocation/header failure ветви,
native error gate states, source-level interface/signatures, произвольное
cyclic ownership SMO, whole file save, все
concrete payload adapters/unknown preservation и scene-to-PC backend.
Raw direct-call search не нашёл отдельного FFPS save caller для467260;
это **не доказательство отсутствия** whole save в protected/indirect коде.

Checkpoint5: [full portable FFPS loader для SAN](native-pc-full-loader.md)
добавлен поверх этого core; reference tests теперь266, full-loader160,
CTest26/26. Непустая DX materialization и остальные concrete payload adapters
остаются открытыми; ранние counts выше — исторический checkpoint4.
