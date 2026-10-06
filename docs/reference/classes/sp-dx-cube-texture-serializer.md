# spDXCubeTextureSerializer

`spDXCubeTextureSerializer` (`0x269C2481`) имеет физическую и регистрационную базу `spDXTextureSerializer` (`0x196D44FE`). [Исходники](../../../Sparkplug/Code/SparkplugDX/spDXCubeTextureSerializer.h) восстанавливают самостоятельные cube read/write overrides. Target ID getter `0x004B8550` возвращает `spDXCubeTexture` (`0x5C542AD9`).

PC payload writer `0x004B88D0` и reader `0x004B8A80` используют flat little-endian последовательность:

| Порядок | Содержимое |
| --- | --- |
| 1 | width, height, raw D3DFORMAT: три `u32` |
| 2 | palette present: один byte |
| 3 | при наличии palette: `1024` bytes entries |
| 4 | mip count: `u32` |
| 5 | шесть faces по порядку, внутри каждой все mips от старшего к младшему |

Format word содержит raw `D3DFORMAT`: `0x31545844` (DXT1), `0x33545844` (DXT3), `0x35545844` (DXT5), `0x15`, `0x16`, `0x29`, `0x17` или `0x1A`. Helper `0x004B86C0` передаёт этот word непосредственно в `CreateCubeTexture`; attachment сохраняет его в `+0x44`, writer пишет его обратно. Ordinary DX texture payload использует другую enum mapping, которую cube codec не вызывает.

Reader принимает **любое ненулевое** значение palette flag; writer пишет `0` или `1`. Это отличается от ordinary DX texture reader, который проверяет точное равенство `1`.

Row bytes вычисляются по surface descriptor: DXT1 — `max(1,width >> 2) * 8`, DXT3/DXT5 — `max(1,width >> 2) * 16`; незжатые форматы используют один, два или четыре bytes на pixel. Однако row-count branch в `0x004B873C` и `0x004B882A` перечисляет DXT1, DXT3 и **DXT4**, пропуская DXT5. Для runtime DXT5 сохраняется `height` rows, даже когда обычное описание DXT5 дало бы block rows. Восстановленная реализация сохраняет эту особенность игры.

Physical pitch определяет шаг по surface storage, но padding не входит в записываемые bytes. Byte count использует packed rows для DXT1/DXT3 и physical pitch для остальных поддержанных formats, включая указанную DXT5 ветвь.

CPU reader ограничивает payload и storage, проверяет square power-of-two dimensions, полную цепочку и точный extent. Эти проверки — политика нашего host. После staging всех шести faces он вызывает runtime attachment; для нескольких mips требуется backend regeneration provider, описанный в [карточке текстуры](sp-dx-cube-texture.md). Writer пишет текущие surface bytes после такой regeneration. Прочитанный multi-mip payload сам по себе не доказывает окончательные surface bytes: оригинал затем запускает D3DX filtering.

Успешная проверка полного native codec относится к однородному уровню без D3DX calls; multi-mip rows изучены независимо от unresolved filtering. Live GPU/device output и PS2 codec не заявляются.
