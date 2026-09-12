# Общий рендерный проход геометрии для RTX

Блок цикла до 18:00 МСК, 12 сентября. Собственная обвязка оригинального
renderer, не восстановленный алгоритм видимости. Нет правил по уровню,
имени объекта, мешу, текстуре или её хешу.

## Поведение

`-SceneGeometry` расширяет основной perspective-проход исходного SceneRender.
После настоящего выбора видимости оригинальная выборка сохраняется в исходном
порядке, включая специальные узлы и null slots. Из текущего графа владения
добавляются отсутствующие `spStaticRenderObject`, `spPCPartitionRenderable`
и узлы с обычным support `spRenderNode`. Граф читается заново каждый проход:
это не cache старых draws и не предположение, что исчезновение draw удалило объект.

Оригинальная игра сама рисует эти supports, назначает matrices/materials,
обновляет palette и выполняет shader/pass submission. Partition SceneRender
передаёт `forceVisible=1`; исходный `424B60` при этом пропускает повторный
frustum gate, но **сохраняет проверку Enabled**. Ни камера, ни transform, ни
материал, ни Enabled в обвязке не переписываются.

Реестр обходит подтверждённые prefix layouts base/Octree/BSP PartitionNode,
их owned children, static refs, dynamic refs, payload и связанные Zone roots.
Исходное владение и layouts: [CP12](native-pc-partition-runtime.md),
[CP13](native-pc-visibility-runtime.md), [CP14](native-pc-scene-render-runtime.md),
[RenderNode](native-pc-render-node-runtime.md). Это перечисление существующих
native объектов для платформенного renderer, а не вторая реализация culler.

## Владение и ограничения

При запуске hash-verified debug EXE добавлены два runtime detours: прежний
Select `46D270` и SceneRender `45EC70`. Для SceneRender проверены пять bytes
`83 EC 20 53 55`: три целые инструкции без relative addressing. Трамплин
сохраняет ABI и весь возвращаемый EAX. Файлы EXE и NVIDIA DLL не меняются.

На время вызова manager vector заимствует стабильный массив scope; allocator
word не меняется. Новые supports получают текущий visibility stamp. Исходные
begin/end/capacity и прежние stamps возвращаются до следующего native Select,
вложенного SceneRender и при выходе, включая SEH `finally`. Наш массив не
попадает в native free/reallocate. При конфликте владения расширение отключается;
исключения самой игры не превращаются в успешный результат.

Ограничения: 4096 partition nodes и итоговых supports, до восьми children,
проверка границ vectors, self/scene links и известных таблиц support. Неполный
или неверный реестр оставляет исходную выборку. Специальные dynamic supports
с другими методами остаются на обычном пути и считаются отдельно. Сцены без
partition, alternate/дополнительные камеры и override camera не расширяются.
Полная регрессия portal/occluder-сцен, эффектов и всех игровых cameras открыта.

Чтение собственного процесса на render thread оптимизировано: вместо тысяч
ReadProcessMemory используются ограниченный memcpy и SEH rejection недоступной
страницы. В первом варианте обход Гардинии 1 занимал 32–47 мс; после изменения
он оказался ниже шага GetTickCount64 в наблюдавшихся выборках. Это не точный
sub-millisecond benchmark и не обещание такого ускорения всех кадров.
Полный native draw-проход дороже обычной выборки; оптимизация submissions впереди.

## Проверки

`Test-SceneGeometry.ps1`: **29 проверок** literal ABI fixtures — static/dynamic
ownership, duplicates, foreign links, unknown support, caps, inaccessible page,
заимствование/возврат vectors и stamps, nested scope, normal return и настоящая
RaiseException с внешним обработчиком. Это проверка обвязки, не эмуляция GPU.
Регрессия света **109 проверок**, 21 create/21 destroy, no handles.
x86 `/W4` собран без warnings; вспомогательный x64 build также проходит,
с прежним warning C4505 неиспользуемого NVIDIA light helper. Native hooks x86-only.

Основные изолированные запуски:

| Запуск | Вариант | Проверка |
| --- | --- | --- |
| `2026-09-12-scene-geometry-gardenia01` | v2, `5D8FCC16…` | Статические объекты, исходное RPM-чтение. 5 кадров одинаковой камеры. |
| `2026-09-12-scene-geometry-fast` | v3, `072DE9D9…` | Быстрый static-проход, Гардиния 1 → Алфея, движение. Ещё 5 совпавших кадров исходной камеры. |
| `2026-09-12-scene-geometry-final` | v4, `B828C475…` | Статика + RenderNode; Домино → Гардиния 1 → Алфея, движение Домино. 5 совпавших кадров Домино. |

В финальном запуске:

| Сцена | Поддержанный реестр | Обычные RenderNode | Пример original + added |
| --- | ---: | ---: | ---: |
| Домино, 4 | 251 | 251 | 173 + 79 (один специальный support остаётся исходным) |
| Гардиния 1, 1 | 1311 | 247 | 644 + 668 (один специальный support остаётся исходным) |
| Алфея, 27 | 1039 | 215 | 112 + 927 |

В этом переходе адрес Scene переиспользовался между уровнями, но реестр и
выборка менялись по текущему владению: 251 → 1311 → 1039. Старый список
не удерживается. Sampled expanded/restored records проверяются попарно.
Гардиния 2 не запускалась. Capture USD не вызывался.

На первом ракурсе Гардинии 1 синий контур дорожки проявился при возврате
обычной выборки и отсутствовал при расширенной. Это ограниченная визуальная
проверка, а не закрытие всех исчезновений геометрии. При движении иногда
меняется native view даже после отпускания клавиш: такие сравнения останавливаются,
их файлы сохранены как rejected. В Алфее совпали первые два кадра одной серии,
следующий был отклонён; **темнота не исправлена**.

Финальный material audit: 44359 draws / 58 sampled frames, 67 входов, пять VS,
0 failures/rejected. Передача и компиляция observed shaders проверены отдельным
shader-coverage.json; это не доказательство полной правильности skinning/normal.

## Использование и готовность

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -SceneLights -SceneGeometry -StartLevel 1 -Windowed
```

Дополнительный `-LiveConfig` у Start-Probe допускает
`winx.keepSceneGeometry = True` для исходной выборки и False для расширения.
По умолчанию экспериментальный SceneGeometry включается явно.
SceneAudit без SceneGeometry остаётся наблюдателем исходной выборки.

Видимость/время жизни: **15% → 40%**. Другие категории без изменения.
Общий расчёт **39,5% → 43,25%**, округлённо **≈40% → ≈45%**.
Остаются специальные supports, дополнительные камеры, регрессия движения,
стоимость полного прохода, поведение внутри Remix и открытый USD capture crash.
