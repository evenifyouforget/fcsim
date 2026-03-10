# Attribution and Provenance Investigation

This file documents the forensic investigation into the origin and licensing of
fcsim's third-party components. It is a record of what was tried, what was found,
and what remains uncertain.

---

## atan2 (`src/fpmath/atan2.c`)

### Finding

This file is the IBM Accurate Mathematical Library implementation of atan2,
identical to `sysdeps/ieee754/dbl-64/atnat2.c` in the GNU C Library (glibc).

### Evidence

The following elements are unique to the glibc/IBM libultim implementation and
are all present in the file:

- Section header comments `/* dla.h */`, `/* mydefs.h */`, `/* atnat2.h */`,
  `/* uatan2.tbl */` -- exact filenames from glibc's `sysdeps/ieee754/dbl-64/`
- `cij[241][7]` coefficient table (from `uatan2.tbl`)
- `hij[i][0..15]` high-precision table
- Error-free arithmetic macros `EADD`, `ESUB`, `EMULV` (from `dla.h`)
- `MM = 5`, `pr[] = {6, 8, 10, 20, 32}` multi-precision fallback
- All variable names: `ax`, `ay`, `u`, `du`, `v`, `vv`, `t1`–`t8`, `z`, `zz`,
  `cor`, `s1`, `ss1`, `s2`, `ss2`, `signArctan2`, `normalized`, `atan2Mp`

A direct comparison against `sysdeps/ieee754/dbl-64/e_atan2.c` (2022 glibc) confirmed a match. The fcsim
version stripped glibc's infrastructure includes (`mpa.h`, `SET_RESTORE_ROUND`,
`LIBC_PROBE`, etc.) and inlined the auxiliary headers, but the algorithm and all
constants are identical.

### History

The LGPL copyright header was already absent when the file was first introduced
to the repository (commit `5497b11`, message: "add atan2"). It was restored in a
later commit once this investigation identified the source.

### License

GNU Lesser General Public License v2.1 or later (LGPL-2.1+).
Copyright (C) 2001-2018 Free Software Foundation, Inc.

---

## sin/cos (`src/fpmath/sincos.c`)

### Finding

This file's constants and lookup table match, bit-for-bit, the internal sin/cos
implementation found in Adobe Flash Player 10.0.32.

### Chain of custody

The code was not disassembled by the current maintainers. It arrived as
pre-existing code of unknown provenance -- "a copy of a copy," with the original
disassembly source lost. The fingerprinting below established that it matches
Flash Player, but the intermediate steps (who disassembled what, and when) are
not documented. It is possible the disassembly was performed on a binary
downstream of Flash Player, in which case the license might differ from Adobe's.

### Algorithm fingerprint

The implementation uses:
- A 64-entry static lookup table over a full 2π period (4 doubles per entry,
  entries spaced by π/32)
- Range reduction: multiply by `c8 = 32/π ≈ 10.18592`, add magic constant
  `c9 = 3/2 · 2^52 = 6755399441055744` for fast nearest-integer rounding
- `fp_cos(x)` implemented as `fp_sincos(x, 0x10)` -- quarter-period index shift
- 8-coefficient polynomial correction (interleaved sin/cos)

### Candidates ruled out before Flash was identified

Source files were compared algorithmically (matching against key constants and
table entries). Binary files were searched for the characteristic byte sequences.

| Candidate | Type | Result |
|-----------|------|--------|
| glibc `sysdeps/ieee754/dbl-64/` (LGPL-2.1+) | source | No match -- different algorithm |
| OpenLibm / Sun fdlibm `k_sin.c` (Sun permissive) | source | No match -- polynomial only, no table |
| AMD libm for Windows `sin.asm` (MIT) | source | No match -- 2/π reduction, different structure |
| Intel SVML `svml_z0_sin_d_ha.s` (proprietary) | source | No match -- AVX-512, post-2008 |
| `NPSWF32.dll` stub | binary | SHA256: `d9b35d7d66f3190ea39d334fc3eede7a4df2e7c2f907586d662d10c203d641f7`, 45,640 bytes -- not the real Flash plugin (no code sections) |
| `flash.ocx` ~2003 build | binary | SHA256: `b35b628580870ae8b2a5358b672ebd6dd8085819be23be22907b64a923f1fce5`, 832,872 bytes -- Intel C++ 6.0 build (~2002), no match |
| `msvcrt.dll` 64-bit | binary | SHA256: `00a0f4e76238f507c1093577cc9816356d244f719ace1674ca86c70886c5f61b`, 637,048 bytes -- no algorithm constants found |
| Wine msvcr80/msvcr90 | binary | Not tested -- Wine reimplementations call glibc internally |

### Flash Player fingerprinting

File: `npswf32.dll`
SHA256: `13de33ee8acfd1b2610311b347defe068d5a4a1631513daa9cb67132c89685d2`
Size: 9,608,120 bytes, PE32 x86, Adobe Flash Player 10.0.32.

All 14 polynomial/range-reduction constants (c0–c13) found in exact sequence at
offset 8,653,344. Table entries 0 and 1 found at offsets 8,651,296 and 8,651,328
with all 8 doubles matching bit-for-bit.

```
Offset 8,653,344 (coefficient block):
  c0  = -1.66666666666666657e-01  (0xBFC5555555555555)  ✓
  c1  = -5.00000000000000000e-01  (0xBFE0000000000000)  ✓
  c2  =  8.33333333333333322e-03  (0x3F811111111111B0)  ✓
  c3  =  4.16666666666666644e-02  (0x3FA5555555555555)  ✓
  c4  = -1.98412698412698413e-04  (0xBF2A01A01A01A01A)  ✓
  c5  = -1.38888888888888894e-03  (0xBF56C16C16C16C17)  ✓
  c6  =  2.75573192239858925e-06  (0x3EC71DE3A556C734)  ✓
  c7  =  2.48015873015873016e-05  (0x3EFA01A01A01A01A)  ✓
  c8  =  1.01859163578813019e+01  (0x40245F306DC9C883)  ✓  (= 32/π)
  c9  =  6.75539944105574400e+15  (0x4338000000000000)  ✓  (= 3/2 · 2^52)
  c10 =  3.79818781643997900e-12  (0x3D90B4611A600000)  ✓
  c11 =  3.79818781643997900e-12  (0x3D90B4611A600000)  ✓
  c12 =  9.81747704208828500e-02  (0x3FB921FB54400000)  ✓  (= π/32)
  c13 =  1.26391640549746910e-22  (0x3B63198A2E037073)  ✓

Offset 8,651,296 (table entry 0):
  [0] = 0x0000000000000000  ✓
  [1] = 0x0000000000000000  ✓
  [2] = 0x0000000000000000  ✓
  [3] = 0x3FF0000000000000  ✓  (= 1.0)

Offset 8,651,328 (table entry 1):
  [0] = 0xBF73B92E176D6D31  ✓
  [1] = 0x3FB917A6BC29B42C  ✓
  [2] = 0xBC3E2718E0000000  ✓
  [3] = 0x3FF0000000000000  ✓  (= 1.0)
```

### License

Adobe Flash Player was proprietary software, © Adobe Systems Incorporated.
Flash Player reached end-of-life 2020-12-31.

The inclusion of this code is justified on preservation/interoperability grounds:
replacing it with any other sin/cos implementation would change the numerical
output and break the faithful replication of Fantastic Contraption's behaviour.
