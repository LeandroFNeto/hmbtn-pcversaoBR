# Proposal

## Why

The host owns a window loop that the game never enters. The game has its own loop in `ps1_main`, which paces itself with `VSync`, draws through a `VSyncCallback`, and spins on `DrawSync` and CD status. Running `ps1_main` on the main thread means the host has to live inside those calls instead of beside them.

## What Changes

- `main_pc.c` calls `ps1_main()` once, on the main thread, after SDL, OpenGL, and the PS1 address space are ready. Its own `while (running)` loop goes away.
- Host `VSync` becomes the frame boundary. A blocking call (`VSync(0)`, `VSync(n)` with `n >= 2`) waits that many 59.94 Hz vblanks. For each one it runs the registered `VSyncCallback`, polls SDL events, presents OpenGL if anything was drawn, and paces to the vblank deadline. `VSync(1)` and negative modes return a counter without waiting.
- Closing the window during that wait leaves `ps1_main` with `longjmp` back to `main_pc.c`, which tears down and returns 0.
- `DrawSync` reports an idle GPU. The CD calls the game waits on (`CdSearchFile`, `CdControl`, `CdControlB`, `CdRead`, `CdReadSync`, `CdSync`) report success at once and do not write the destination, so the boot path reaches `VSync` instead of spinning.
- Until the game draws its first ordering table, each vblank presents the host green quad, so the window proves the loop is alive. After that, a vblank in which the game drew nothing presents nothing and the previous frame stays, because the game draws on only one of the two vblanks of each `VSync(2)`.
- `ResetCallback` initializes the callbacks only on its first call, as in PSY-Q. `afile.c` calls it before every read, after the VSync callback is installed.

Dependencies: `pc-port-psyq-stubs` (game sources on the link) and `pc-port-ps1-memory` (PS1 addresses mapped). The VSync and CD behavior in groups 1 and 2 of tasks.md can land before them and is checked with host code.

## Capabilities

### New Capabilities

- `host-frame-loop`: The host runs `ps1_main` on the main thread and turns its `VSync`, `VSyncCallback`, `DrawSync`, and CD waits into presented, paced frames that close cleanly.

### Modified Capabilities

- `host-gl-present`: Presentation moves from each host loop iteration to each vblank inside `VSync` that drew something, and closing the window works while the game waits there.

## Impact

- `src/pc_port/src/main_pc.c`, host `libetc` (`VSync`, `VSyncCallback`, `ResetCallback`), host `libgpu` (`DrawSync`, draw tracking), and host `libcd` stubs from `pc-port-psyq-stubs`.
- No threads and no new package.
- Out of scope: pad input, sound, real CD or `/mods` data, and any edit under `src/`. With no disc data, scenes after boot may stop on empty buffers; this change owes the boot VSyncs and a clean close, not a rendered title.
