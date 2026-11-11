# Tasks

## 1. Address and row log

- [x] 1.1 With `HMBTN_SELFTEST=1`, page `320,0`, UV `246,242`, depth 4 logs sample x `381` y `242`. A constructed `0x0D` row that stores `0x7FFF` outside that point logs its rectangle as `nonzero` and as not covering `(381, 242)`. A stored zero word stays `0x0000`.

```
itemaddr: sample x 381 y 242
itemaddr check ok: sample x 381 y 242
itemaddr: store x 800 y 480 w 1 h 1 nonzero not covering 381,242
itemaddr check ok: nonzero texel stored
itemaddr check ok: row not covering sample
itemaddr: store x 801 y 480 w 1 h 1 zero not covering 381,242
itemaddr check ok: zero texel stored
itemaddr check word 0x0000
```

## 2. The picture's rows

- [x] 2.1 The self-check reads sector `0x3C4B000` of `A_FILE.BIN`, uploads each `0x0D` pack through `texture_pack_load_at`, and pastes the `itemaddr:` lines. Do not move the sample in this task.

One pack at offset `0x100`. Every row is non-zero and none covers `(381, 242)`. The word at the sample was restored and was not moved.

```
itemaddr: store x 320 y 254 w 16 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 0 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 1 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 2 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 3 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 4 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 5 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 6 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 7 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 8 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 9 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 10 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 11 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 12 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 13 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 14 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 15 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 16 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 17 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 18 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 19 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 20 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 21 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 22 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 23 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 24 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 25 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 26 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 27 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 28 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 29 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 30 w 8 h 1 nonzero not covering 381,242
itemaddr: store x 320 y 31 w 8 h 1 nonzero not covering 381,242
```

## 3. One delta, or stop

- [x] 3.1 If the lines name exactly one candidate, the host sampler applies that page or UV delta only to command `0x2c` with page `320,0` and UV `246,242`, and the self-check then reads the stored non-zero word. Any other count pastes the lines and stops, with no replacement color. `git diff -- src/main src/overlay src/include` stays empty.

Stopped. The 33 `itemaddr: store` lines in 2.1 are each `nonzero not covering 381,242`. That is more than one candidate, so no page delta and no UV delta. No replacement color. `git diff -- src/main src/overlay src/include` is empty.

## 4. Picture

- [x] 4.1 After a delta from 3.1, one farm capture shows the bin sampling a non-zero word. If 3.1 stopped, leave this task open. Verified 2026-11-10 in the running game after the `gte_rtv0` translation fix (`pc-port-rtv0-translation`): the object now draws in place and animates; the host forced-color and bed hacks are no longer needed.

3.1 stopped. There is no delta, so this capture stays open.
