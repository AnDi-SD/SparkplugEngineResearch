# внешний источник TextureData

## Original42EA50

Field4 читает через `416DC0` u16 byte count и C-string с завершающим NUL.
Из parent stream word18 берётся имя. Внешний MSVCR71 `_splitpath` по IAT
`6D9358` выделяет drive/directory, original последовательно дописывает directory
к drive и reference к этому префиксу. Затем `6BD580` создаёт PC file stream,
`6BE7C0/6BD710` открывает его mode1. Пустой actual package manager `45C4E0`
исполняет lookup `45B8A0`, после чего используется обычный file backend.

Тот же serializer secondary read вызывается в `42EBF4` с новым stream и
прежним texture object. Это payload-файл, без нового FFPS или SBOO header.
При успехе `6BD980` закрывает stream, `6BE2A0` уничтожает его; только после
этого source handled становится1. Outer stream продолжает свой wrapper.

| Parent name | Reference | Original result path |
| --- | --- | --- |
| `C:\Media\SFX\scene.smo` | `image.tex` | `C:\Media\SFX\image.tex` |
| `Media/SFX/scene.smo` | `sub/image.tex` | `Media/SFX/sub/image.tex` |
| `scene.smo` | `image.tex` | `image.tex` |
| `C:\Media\scene.smo` | `D:\image.tex` | `C:\Media\D:\image.tex` |

## Отказ и утечка оригинала

При объявленном CreateFileA failure `FFFFFFFF` и GetLastError2 original
сохраняет false, пропускает recursive read и уничтожает PC stream. Destructor
пытается CloseHandle для невалидного ненулевого handle; его failure игнорируется.
Случай занял27920 инструкций, cursor17 из18 outer bytes: terminator после
провалившегося field не потреблён. Texture остаётся неинициализированной.
