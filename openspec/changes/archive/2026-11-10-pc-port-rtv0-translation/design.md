# Design

`model_compose_gte_matrix` multiplies the object matrix onto the camera matrix column by column with `gte_rtir` (no translation), then transforms the object position with `gte_rtv0` and stores IR1-3 as the new translation. The game redefines `gte_rtv0` as `0x4A480012`: sf 1, mx rotation, v V0, cv TR. The host header replaces `src/include/gte.h` and cannot emit that opcode, so `gte_h_rtv0` must do the same arithmetic in C: `(TR * 4096 + R * V0) >> 12`, MAC1-3 kept, IR1-3 saturated to 16 bits. It is the only caller of `gte_rtv0` in the game.
