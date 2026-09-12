# Native main camera → Remix API в игре

13 сентября 2026. Реализован и проверен следующий блок
[прямой сцены](winx-remix-direct-scene-plan-2026-09-12.md): **основная камера
поступает из native `spCamera` в `SetupCamera` через согласованную x86/x64
пару bridge.** Stock x64 renderer не изменён. D3D setters/Present сохранены
для запуска, интерфейса и сравнения; дополнительного Present нет.

## Реализация

Собственный `winx_native_camera_source.h` перехватывает успешный возврат AL
из PC427D40, после штатного refresh dirty matrices. Копируется общий
`spCameraObservedLayout`; portable класс не накладывается на память игры.
Источник — только engine main camera текущей сцены, с подтверждённым vtable,
`projectionBranch==0`, `twoDimensional==0` и конечными float32.

Packet содержит исходные 128 bytes view/projection, scene/camera identity,
frame, apply sequence и device epoch. Native calculation не заменяется,
транспонирование/FOV reconstruction отсутствуют. Успешный apply с dirty=0
тоже даёт свежий packet. Foreign/UI apply не заменяет основную камеру;
неудачный либо неподходящий main apply инвалидирует pending snapshot.

Перед первым потенциальным мировым draw, до lights и API geometry,
проверяются актуальные scene/main/frame/epoch, primary target и точное
равенство actual D3D view/projection. D3D здесь является проверкой прохода,
передаются байты native snapshot. За кадр возможна одна попытка SetupCamera.
Если ранний perspective draw не квалифицирован, поздняя отправка запрещена:
Remix использует первое обновление Main за frame. Нулевой draw не закрывает
окно; foreign/offscreen perspective draw закрывает его консервативно.

Отдельный selected packet выявляет отличающийся повторный main apply даже
в observation mode. Reset/CreateDevice/failed Present инвалидируют snapshot
и меняют epoch. Camera/scene switch внутри уже занятого frame не посылает
вторую камеру. Это проверка адаптера; полный device recovery/temporal history
самого renderer этим не доказан.

Запуск: `Run-WinxRemix.ps1 -NativeCameraSource` — наблюдение,
`-NativeCameraSubmit` — direct с начала работы; оба требуют verified debug
EXE, submit также RTX. Live `winx.nativeCameraSubmit = True/False` действует
только при заранее установленном source hook. Launch записывает hashes всех
трёх runtime components. Режим opt-in; обычные launcher profiles не менялись.

## Проверки до игрового включения

- `qualify_native_camera.py`: **3/3** regions pristine/debug равны;
  hook переносит целые `push esi; mov esi,ecx; test [esi+224],1`, 10 bytes,
  с сохранением flags для исходного JZ. Original behavior — прежние CP6/47,
  новых native эмуляций нет.
- `Test-NativeCamera.ps1`, x86 actual system D3D9 + synthetic ABI + recording
  API: **165 checks**, 12 SetupCamera, 2.117 s, exit0. Точность всех bytes,
  один вызов, stale/foreign/orthographic/Is2D, cached dirty0, late apply,
  missed first draw, scene/epoch, actual offscreen target, null API и errors.
  Это тест обвязки, не renderer acceptance.
- x86/x64 adapter compile прошли. x64 native hooks отключены; warnings C4505
  относятся к удалённым неиспользуемым helpers.
- [Общий camera bridge](../../research/rtx-remix/direct-camera-bridge/README.md):
  866 serializer checks и отдельные 20 Camera→Draw→Present кадров.
  Потеря обязательного UID-response завершает host с `0xE052CA01`;
  обычная ошибка API возвращается. Это явная transport fail-stop policy.

## Оригинальная игра

### Observation, Домино

`play-rtx-20260913-004549-854`, adapter v1 `B9F584FC…`, stock bridge.
**1440 подходящих кадров**, 0 mismatch/missed/failure; все шесть sampled
packets после apply at draw0 и перед draw1. Player x изменился −1693.677→
−1441.805, движение видно. Geometry/layout: 1436 кадров по446 instances,
224 сравнения поколений, 0 upload/layout mismatch.

Ограничение v1 observer: lateUpdates тогда считал только после API submit,
поэтому его ноль не доказывает отсутствие более позднего отличающегося apply.
v2/v3 уже сравнивают отдельный selected packet в обоих режимах.

### Direct API, Домино → Гардиния1 → Алфея

`play-rtx-20260913-005541-034`, PID12328 штатно закрыт. Adapter v3:
`A36537CD08EF6DE8C06A5DFCB083146F663ABEDF23FAB2532028FBE0B4BF1D09`.

| Компонент | SHA256 |
|---|---|
| installed x86 client `d3d9.remix-original.dll` | `79E88A694D233112605E7AB0F4F258F1FF536AD8471F67623C412DDAD6564F11` |
| installed x64 server | `C9D3E807CA36BFF6D3437D4DA3B8BB68DEED0EBEDDECC2E32B6E9D5547FD1927` |
| неизменённый stock renderer | `F7C310821AA98BCDFDEC120330B0A89457B7C5EBA58D21464AF32639611C809F` |

Исходные client/server сохранены в
`local-data/rtx-remix/native-camera-tests/bridge-install-v1/` с installation.json.
Имя `d3d9.remix-original.dll` теперь обозначает загружаемый backend, а не
побайтово исходный client; для rollback брать именно сохранённые оба файла.

Записаны **9103 успешных direct camera frames**, ни одного повторного submit,
API failure, state mismatch, late update или missed first window. Все43
sampled camera packets идут до draw1 после apply at draw0. Log2.06MB не
достиг cap16MiB. SUCCESS — реальный renderer API return через bridge;
внутренние GPU camera values отдельно не инструментированы.

Домино A/B: view/projection совпали точно, x/z игрока те же, y отличается
на 0.0000076 при idle. При native движении x −1693.677→−1442.778;
view меняется, projection сохраняется. Алфея A/B: обе matrices и позиция
совпали точно; после возвращения native player переместился
[-1024.890,25.877,807.769]→[-949.443,25.873,728.517]. Окружение и HUD видны.
F1 UI, загрузки и смены сцен прошли с direct mode; Гардиния2 не запускалась.

Есть честная граница автоматизации: длинный Enter в LOAD LEVEL выбрал
default Гардинию1; после первого перехода27 короткий F1 не закрыл меню,
и последующие Enter снова загружали default1. Финальный переход27 подтверждён
индексом26, готовностью27 и `debugOpen=false` перед движением. Поэтому
`camera-alfea-native.*` содержит промежуточную Гардинию1/loading; финальные
Алфея artifacts — `alfea-ready-menu-closed.*` и `alfea-camera-*`.

## Остатки и следующий блок

На переходе API mesh cache512 временно сократил native submission в Алфее
до394 instances; после TTL вернулись871. D3D fallback сохранял draw, но это
не окончательная direct ownership/lifetime. Следующий блок — освобождение
неиспользуемых ресурсов при давлении на прежние лимиты и native материалы.

Bridge32/64 logs не содержат err/Fatal. Renderer при закрытии сообщил
`41 common device objects were not disposed of`; clean renderer shutdown
не заявляется. Аналогичная запись была и в прежнем stock log, но причина
этим не установлена. Configs/saves совпали с резервными копиями; bridge
config восстановлен штатным watcher. Игра и bridge завершены.

Camera cuts/history, все offscreen/sky/viewmodel роли и device recovery
остаются отдельной проверкой: stock `processExternalCamera` не выполняет
весь bookkeeping ordinary `processCamera`. Проценты функциональной готовности
изображения остаются около45%; подтверждён именно новый источник/транспорт.

Машинный отчёт: [winx-remix-native-camera-submit-2026-09-13.json](../../research/winx-remix-native-camera-submit-2026-09-13.json).
