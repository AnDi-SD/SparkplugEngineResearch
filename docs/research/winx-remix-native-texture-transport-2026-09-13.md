# Winx / Remix: native texture transport, 13 сентября 2026

Завершена проверка собственной платформенной границы для чтения текущей texture
по указателю из native материала, независимо от `GetTexture(0)`. Результат этого
checkpoint — **CPU fixture 157 PASS и system D3D9 fixture 697 PASS**. Игра и
Remix bridge в этих проверках не запускались; игровое покрытие и прямая подача
сцены этим отчётом не объявляются готовыми.

Раньше одного native COM pointer было недостаточно: он не доказывал, что texture
ещё существует, относится к нужному device и сейчас не изменяется через mip
surface или GDI. Новый реестр связывает успешный `CreateTexture` с наблюдаемыми
событиями записи, `Release` и `Reset`. Код находится в
[winx_native_transport_source.h](../../research/rtx-remix/winx_native_transport_source.h)
и texture hooks
[winx_d3d9_probe.cpp](../../research/rtx-remix/winx_d3d9_probe.cpp). Это наш backend;
восстановленные классы и оригинальная игра не изменялись.

Поддерживается только обычная 2D texture: `D3DPOOL_MANAGED`, `Usage == 0`,
`A8R8G8B8` или `X8R8G8B8`. Device берётся из успешного Create, mip layout — из
`GetLevelCount/GetLevelDesc` его ещё принадлежащего вызывающему результата.
Проверяются формат, pool, usage, размеры, последовательность mip размеров и
отсутствие multisampling. Реестр ограничен 4096 textures, 8192 surface relations
и 32 mip descriptors; существующий DDS/hash backend дополнительно ограничивает
своё чтение 2048×2048 и 13 уровнями. RT/default/dynamic/autogen/paletted и другие
форматы остаются вне этого пути.

`BorrowTexture(lock, device, texture, out)` использует pointer только как ключ.
Он требует принадлежащий вызывающему `unique_lock` именно общего `guard` и
возвращает device, texture, размеры/формат, **creation generation** и отдельную
**content generation**. Реестр не удерживает COM references. Временный canonical
`IUnknown` запрашивается только у owned API result или выполняющегося receiver
и сразу освобождается. Creation serial означает новое наблюдение Create,
а не доказательство нового физического COM object.

Borrow нужно употребить до освобождения lock или собственной reentrant mutation.
После чтения требуются `CurrentTexture` и отдельные scene/object/phase fences
вызывающего кода; texture registry не заменяет lifetime native материала/сцены.
Writable Lock, включая `NO_DIRTY_UPDATE`, меняет content serial, инвалидирует
texture hash и исключает borrow до успешного Unlock. Readonly сохраняет content
serial. Открытый DC также исключает borrow; failed Unlock/ReleaseDC не снимает
блокировку. Доступ учитывается по mip storage, поэтому texture/surface aliases
не становятся независимыми версиями одной памяти.

| Interface | Перехваченные slots и назначение |
|---|---|
| Device | 0 QueryInterface, 2 Release, 13 CreateAdditionalSwapChain, 14 GetSwapChain, 16 Reset, 23 CreateTexture, 33 GetFrontBufferData, 64 GetTexture |
| Texture | 0 QueryInterface, 2 Release, 16 GenerateMipSubLevels, 18 GetSurfaceLevel, 19 LockRect, 20 UnlockRect |
| Level surface | 0 QueryInterface, 2 Release, 11 GetContainer, 13 LockRect, 14 UnlockRect, 15 GetDC, 16 ReleaseDC |
| Swapchain | 0 QueryInterface, 4 GetFrontBufferData |

Actual vtable slots проверяются до квалификации; surface hooks устанавливаются
до передачи успешного `GetSurfaceLevel` результата игре. Оба FrontBuffer пути
консервативно удаляют destination texture из реестра **до** оригинального вызова;
`GenerateMipSubLevels` также прекращает её квалификацию. Остальные рассмотренные
copy/render методы требуют destination DEFAULT или SYSTEMMEM и не добавляют
поддержку managed writer: [UpdateTexture](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-updatetexture),
[UpdateSurface](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-updatesurface),
[StretchRect](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-stretchrect),
[ColorFill](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-colorfill),
[GetRenderTargetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getrendertargetdata).
Граница Lock/DC/FrontBuffer опирается на Microsoft
[LockRect](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dtexture9-lockrect),
[GetDC](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dsurface9-getdc)
и [FrontBuffer contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb324479(v=vs.85)).

COM не гарантирует одинаковый адрес для повторного запроса не-IUnknown interface.
Поэтому неизвестный успешный IID, отличающийся alias, отказ canonical identity
проверки или неполное hook coverage блокируют texture qualification данного
device. **Reset и повторное использование device address этот блок не снимают**:
escaped interface может ещё существовать. Список ограничен 64 devices; отказ его
учёта выключает всю texture qualification. Это консервативный fallback до конца
процесса, не поддержка неизвестного интерфейса. Основание:
[QueryInterface contract](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface(refiid_void)).

| Проверка | Контекст и результат |
|---|---|
| CPU `texture-cpu-v2` | **157 PASS = прежние 67 + 90**. Собственные literal identities; без COM/GPU/native code. Descriptors, generations, readonly/write/mip/surface/DC, retirement/reset, bounds, allocation failure и постоянный alias block. |
| D3D `native-texture-v1` | **697 PASS = прежние 614 + 83**. Реальный system D3D9, собственное скрытое окно, recording Remix API, watchdog 30 s; первая сборка и запуск успешны. |
| Сохранённые группы | Native source **171**, native material **137**, resource transport **53**; прежние alpha и остальные integration проверки сохранены. |

В D3D fixture texture A читалась через `BorrowTexture → ChannelTextureHash(A)`
и `WriteTextureSource(A, ...)`, когда stage 0 содержал B либо null. Hash и DDS
полностью совпали с обычным bound-A результатом; binding не изменился,
`CurrentTexture` после readonly helper calls остался true. Проверены реальные
mip/surface записи, X8R8G8B8 `GetDC → SetPixelV → ReleaseDC` с проверкой пикселя,
Reset с живыми caller-owned managed references и последующий final Release.
FrontBuffer fixture использовала маленькие 4×4 destinations: доказана dispatch
и консервативная retirement, **не успешный захват desktop**.

Закреплённый [локальный evidence](../../local-data/rtx-remix/native-transport-tests/texture-evidence-v1/evidence.json)
содержит **179 ранее проверенных SHA-256** исходников, wrappers, результатов,
EXE и DDS. Его hash:
`D3F44C7BF73747C583221EFE1A1359C71504385EDC1F8E6857937A4D34F50044`.
[Машиночитаемый checkpoint](../../research/winx-remix-native-texture-transport-2026-09-13.json)
связывает эти evidence с точными x86 source snapshots и двумя итоговыми EXE.
Начальный CPU151 сохранён отдельно; итоговый CPU157 добавил проверки постоянного
alias block. Во время review до GPU исправлены пропуск canonical-QI failure,
device-QI writer escape и чтение GetDC output при failed HRESULT.

Открыты проверка этого transport пути с Remix bridge и в игре, его фактическое
покрытие ресурсов и подключение к independent scene submit. Этот checkpoint не
содержит нового live результата или итоговой x64 binary qualification.
