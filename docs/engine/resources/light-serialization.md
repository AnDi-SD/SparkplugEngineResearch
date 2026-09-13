# PC Light and LightData complete scalar codecs

Actual protected factories43FFD0/471590 allocate14 bytes. Data primary
vtable6E1974/secondary6E1968 and base6E8EDC/6E8ED0 are observed. Actual
4400B0 ignores both8-byte header words and creates DXLight4AC000/158 bytes,
without reading serializer platform. Empty external read returnsNULL and
one diagnostic. A nonzero short PC read can still succeed: the source
memory-stream boundary is stricter; this is not a native header validator.

Whole readers440640/471670 first execute Node463A70, then a separate light
section. Fields0..8 are type u32,shadow byte,ARGB u32,attenuation byte,
intensity/range/hotspot/falloff raw u32 IEEE words,enabled byte. Every known
field directly stores and ORs NodeB0 bit8, even equal values. Unknown fields
skip; later repetitions win. OpaqueDC remains unchanged. A missing color
word returnsfalse after preserving an earlier type mutation. Source uses
the common bounded section cursor and explicitly rejects missing terminator
or malformed field extent; original returns success on missing terminator.
Noncanonical native boolean bytes are canonicalized by host bool storage.

Whole writers440110/471AF0 execute Node463F10 then fields in ascending order.
Type0/shadowfalse/attenuationfalse/Enabledfalse are omitted. Intensity and
range compare raw words3F800000/43480000; angles compare numerically to0,
so negativezero is omitted and NaN retained. Color comparison43FF20 uses
the mutable global73FE9C converted through424700 and tolerance3A83126F.
Color pack41D150 multiplies each channel by255, truncates through60DB90,
and keeps the low byte. Source bounds exceptional integer conversion;
all serialized ARGB channels are exact. Source write-plan helpers share
this same codec rather than a separate duplicate field predicate.
