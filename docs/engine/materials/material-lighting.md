# PC material lighting and color sources

## Exact consumer rules

The raw mode at complete renderer+C888 is written first. Every lighting
call next dispatches device state29: true only for modes3..5 and
`float(E4E4)>0`. Zero, negative and quiet NaN produce false; positive infinity
produces true. Values above7 then return success without changing the other
colors/sources/lighting. Device HRESULT never controls success.

The17-word material block is diffuseE4A4, ambientE4B4, specularE4C4,
emissiveE4D4, powerE4E4. The following are raw native mode values, not
recovered original enum names:

| Mode | Color mutation | Device lighting137 | Diffuse/ambient source |
| ---: | --- | ---: | --- |
| 0 | Emissive white | 1 | 10/10 |
| 1 | Emissive receives old diffuse; other colors black; preserve old diffuse alpha | 1 | 10/10 |
| 2 | Unchanged | 0 | 11/10 |
| 3 | Unchanged | 1 | 10/10 |
| 4 | Unchanged | 1 | 10/11 |
| 5 | Unchanged | 1 | 11/10 |
| 6 | Emissive receives ARGB C194; other colors black, alpha1 | 1 | 10/10 |
| 7 | Unchanged | 1 | 10/10 |

Black is (0,0,0,1), white (1,1,1,1). Source setters cache raw values at
E4E8/E4EC before dispatch, suppress identical raw values, map10/11/12 to
0/1/2 for device states145/148. Other values still update raw cache but do
not submit. The lower `4B0A90` cache can independently suppress a command.

ARGB conversion `4A9200` (and wrapper424700) uses the actual float32
coefficient at6DCA9C, `0.003921568859368563`, in R/G/B/A order. It is shared
with the already restored material-color controller through
`Analysis/PC/spColorMath.h`; no double-precision replacement coefficient.

## Reconstruction and bounded proof

`spDXRenderer::LightingStateForAnalysis` is explicitly supplied consumer
state, not recovered constructor defaults. State8 without that input is a
host rejection, not a fake successful native path. Source remains in the
same renderer and calls the already checked common device-cache helper.
