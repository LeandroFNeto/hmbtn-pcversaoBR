# Tasks

## 1. The folder and the match

- [x] 1.1 Load `pageX_pageY_u0_v0_u1_v1.bmp` from `mods/textures` beside the executable, or from `HMBTN_MODS`, per design.md decisions 1 and 2. `texture_resolve` returns that image's OpenGL name when it is the only match, and `0` when there is no match, two files share the key, or the file is not a readable BMP. Verify `HMBTN_SELFTEST=1` covers those four results. Document the folder and `HMBTN_MODS` in the README diagnostic paragraph, and verify the paragraph names both.

```
mods check ok: one bmp returns its name
mods check name 0x2
mods check ok: another uv stays on vram
mods check ok: two files stay on vram
mods check ok: a broken bmp stays on vram
```

## 2. The draw

- [x] 2.1 Use the return that `libgpu.c` currently ignores. A non-zero name flushes the batch, binds that texture, and the shader samples the file across the primitive's UV bounds, per design.md decision 4. Verify `HMBTN_SELFTEST=1` draws the generated red BMP on its key and reads red, draws another UV and reads that primitive's VRAM color, and the red primitive's VRAM word is unchanged. Verify `git diff -- src/main src/overlay src/include` is empty.

```
mods check ok: the bmp primitive is red
mods check bmp pixel 255,0,1
mods check ok: the bmp primitive leaves vram unchanged
mods check ok: another uv samples vram
mods check vram pixel 0,0,255
```

`git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-texture-mods` after both tasks are verified.
- A portrait file is the same match once its page and UV are known. This change does not search for them.
