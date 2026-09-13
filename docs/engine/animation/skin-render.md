# PC Skin: palette -> mesh -> shader constants -> draw

## Полный исходный вызов

Пройденная цепочка:

```text
46A240 Skin render
  423FD0 pre (реальный direct callback, fallback material, NULL fog)
  461D70 cached Node PRS -> affine matrix
  426B00 inverseBind * boneWorld -> renderer C9B8 palette
  462680 global identity3 -> identity4
  4BBB60 world input -> SetTransform(256)
  renderer C9BC = boneCount
  4BC670 DXMesh -> 4BC4A0 buffers/material/pass
  4BC290 -> 4C8980 existing weighted shader -> 4AE930 constants -> 4BE210
  device vertex shader/constants/indexed draw
  4240D0 post; C9BC = 0 только на полном успехе
```

BlendMatrices/MatDiffuse descriptors создаются оригинальным `4AF940`, shader
сохраняется настоящим `4C87A0` под ключом `{20011,0}`. Shader/GPU handles
нулевые, явные внешние device contracts принимают запросы; валидность
байткода или реальный GPU результат не утверждается. Compiler miss не
подменяется и не вызывается в этом срезе.

Вход Node world position `{1,2,3}`, scale `{2,3,4}`, orientation identity;
inverse bind translation `{5,6,7}`. Полученная палитра имеет diagonal
`2,3,4,1` и translation `{11,20,31}`. Три строки BlendMatrices содержат
транспонированную affine часть, следующая строка — MatDiffuse. Сверяется
вся палитра и каждый вызов устройства, включая состояние cache/count в
момент вызова, а не только итоговые координаты.

## Границы описания

`spSkin::RenderUnlitForAnalysis` использует существующие Node affine,
palette multiply, callback dispatch, renderer matrix setter и
`SubmitUnlitGeometryForAnalysis`; внутренние методы не заменены callbacks
с выдуманным success. Аналитический `SkinRenderContextForAnalysis` явно
передаёт состояния renderer/материала/cache и внешние device функции.
