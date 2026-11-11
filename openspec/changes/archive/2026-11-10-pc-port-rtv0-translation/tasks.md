# Tasks

## 1. Translation in RTV0

- [x] 1.1 `gte_h_rtv0` in `src/pc_port/src/gte.c` adds `TR * 4096` before the shift. `gte_h_rtir` is unchanged.
- [x] 1.2 Build the host and run `HMBTN_SELFTEST=1`: no check fails.

## 2. Remove the workarounds

- [x] 2.1 Delete `force_bed.c` and `force_bed.h`, the hooks in `gte.c`, `libgte_math.c`, and `model_tex.c`, the `model.c` patch in `cmake/host.cmake`, and the bed checks in `force_props_check.c`. The `nclip` check stays.

## 3. In the game

- [x] 3.1 Run the game: the door, the chest and its lid, the bed, and the shipping bin appear in place and animate. Verified 2026-11-10.
