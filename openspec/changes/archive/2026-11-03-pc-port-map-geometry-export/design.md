# Design

## Context

See proposal.md for why. The requirements are the ADDED blocks in `specs/host-map-export/spec.md`.

What the code does today:

- The loaded area is `farm_area.mode.script` (`FarmModeScript`): `head.scene`, `head.place`, fifty `FarmZone` boxes, and fifteen `FarmZoneModel` entries. `farm_area.mode.data` points at that script once the area is in. `farm_area.mode.place` is the player's map and area.
- `scene.c` binds `farm_model` from `farm_area.sectionBuffer + SECTION_STRINGS_OFFSET` on every area change, then `ground_enable_objects` sets each object's scale. The comment in `farm_probe.h` says "the farm's TMD", but the bind runs for the area that just loaded. `farm_model` is zero until that bind. `ground_find_object` walks the same TMD and writes the `ground_*` globals the height test uses.
- A present model entry's file name is the C string at `sectionBuffer + SECTION_STRINGS_OFFSET + name`. `scene_populate_objects` copies that entry's position into `scene_entries[]`, which is static inside `scene.c`. Nothing outside that file can see a door after it has swung.
- The outdoor field is 25 by 45 and the greenhouse is 7 by 7. `field_result_coords` writes the cell centre's X and Z. The cell step is 400 and lives only in `field.c`. The tile record has no height. On the farm, a clear occupant settle stores Y as 0.
- `RotTransPers` in `src/pc_port/src/libgte_stub.c` ignores the vertex and returns 0. The screen projection the packets use is `gte_project` in `gte.c`. It reads the same rotation, translation, offset, and screen distance as `gte_h_rtps`. `gte_ReadRotMatrix` copies that rotation and translation back out. `model_load_lighting` rebuilds the view from the current eye and target and loads it into the GTE. It also writes fog, back color, and the color and light matrices.
- `main_pc.c` ignores `argc` and `argv`. `host_present` already runs once per presented frame. `cmake/host.cmake` globs `src/pc_port/src/*.c`.

## Goals / Non-Goals

**Goals:**

- One writer, called with the scene that is resident, for every scene id. The outdoor farm, house `0x50`, barn `0x60`, and coop `0x70` go through that writer.
- World X, Y, Z taken from the loaded TMD, the zone boxes, the model entries, and `field_result_coords`. Screen X, Y taken from `gte_project` after the view is loaded.
- A self-check that builds its own triangle, zone, and slot and does not require the house or the barn.

**Non-Goals:**

- Moving a sprite, a door, a chest, or a piece of furniture.
- Editing `src/main` or `src/overlay`, including adding a reader for `scene_entries`.
- Calling `scene_enter` in a loop to force every area. The writer runs on the scene the game has already loaded.
- Calling `ground_find_object` or `RotTransPers`.
- Changing `gte-screen-transform` or `host-gte-library`.

## Decisions

### 1. The writer takes the loaded scene; the hook only notices a change

`map_export_scene` reads a `FarmModeScript`, a `Model`, and a place. The live hook passes `farm_area.mode.script`, `farm_model`, and `farm_area.mode.place`. The self-check passes a script and a model it built in host memory.

`host_present` calls `map_export_tick` and then continues. The tick returns immediately when the export is disarmed, when `farm_model.tmd` is null, when `farm_area.mode.data` is not `&farm_area.mode.script`, or when that scene id was already written in this process. A scene id is marked written only after both files exist. The tick does not wait, does not read the keyboard, and does not skip the present that called it.

The file name uses the lowercase hex of `head.scene` with no prefix: scene `0x50` is `debug/maps/scene_50_mesh.json` and `debug/maps/scene_50_wireframe.svg`, relative to the working directory the host already uses for `hmbtn-host.log`. The directory is created on the first write.

Alternative considered: a startup loop that calls `scene_enter` for each area. Rejected. That changes the running farm, and the areas are not loaded yet at process start.

### 2. The command line wins, and `1` means every scene

`main_pc.c` passes `argc` and `argv` into the exporter before the game loop. `--export-maps` with no list arms every scene. `--export-maps` followed by one token arms that list. `HMBTN_EXPORT_MAPS` is read only when the argument is absent. The token `1` arms every scene. Any other token is a comma-separated list of scene ids, each decimal or `0x` hex. A scene whose id is not in the list is skipped. An empty list or a token that is not `1` and not an id writes nothing.

Alternative considered: an environment value of `1` meaning "scene 1". Rejected. The spec says `1` arms every loaded scene, and a named id such as `0x50` is the filter.

### 3. The area mesh is read from the TMD, not from the ground test

Vertices are the `SVECTOR` table at `(u8 *)objects + vert_top`. Primitives are the bytes at `(u8 *)objects + primitive_top`. The walk accepts the same triangle and quad headers `ground.c` already groups (`TMD_PRIM_F3` through `TMD_PRIM_GT4`, including the raw and flag-4 forms). A quad stays four vertices. The JSON id is `<object>:<primitive>`. An object whose scale is 0 is skipped, matching the ground test's own skip. The header word is stored on the polygon so a later reader can drop a primitive the ground test would skip. The walk does not call `ground_find_object`.

The script models are the slots. An entry with `SCENE_ENTRY_ABSENT` is omitted. Each remaining entry records its index as the slot number, its flags, the C string at `sectionBuffer + SECTION_STRINGS_OFFSET + name`, `position`, and `angle`. A null name pointer or a name that does not fall inside the section buffer is recorded as an empty string. The zone list keeps a zone whose id is not `ZONE_ID_EMPTY` and whose flags do not include `ZONE_FLAG_DEAD`. Each zone records index, id, flags, min, max, entry, and `modelIndex`.

Alternative considered: export `scene_entries` so a swinging door's current angle is in the file. Rejected. That array is static in `scene.c`, and this change does not add a symbol there. The file records the authored anchor.

### 4. The field grid is a place test around `field_result_coords`

When `place.location` is `PLACE_FARM`, the writer asks `field_result_coords` for `(0, 0)` and `(1, 0)` with `which` 0 and uses the X delta as the cell size. Each cell is a quad on Y = 0, centred on the point `field_result_coords` returns, half a cell toward each corner. The id is `<column>,<row>`. Columns run `0 .. FIELD_COLS-1` and rows `0 .. FIELD_ROWS-1`. `PLACE_GREENHOUSE` does the same with `which` 1 and `GREENHOUSE_SIZE`. Every other place adds no grid cell. The tile id is not required; the column and row are the label.

Alternative considered: copy `FIELD_CELL_SIZE` (400) into the host. Rejected. The step stays the one `field_result_coords` returns.

### 5. Screen points come from `gte_project` on the loaded view

On the live path the tick saves the GTE rotation and translation with `gte_ReadRotMatrix` and saves offset, screen distance, fog, and the color matrices with `gte_debug_settings`. It calls `model_load_lighting`, projects each vertex and each slot position with `gte_project`, then writes the saved matrix and settings back. `gte_project` returns -1 when the depth is not positive; the JSON then stores `{"behind": true}` and no x or y. A polygon or zone footprint with any behind vertex is left out of the SVG. The SVG point list is the screen x, y of the remaining polygons, and the text at their average is the polygon id.

The self-check does not call `model_load_lighting`. It sets an identity rotation, zero translation, offset `(160, 120)`, and screen distance `1000`, which is the case `gte_startup_check` already locks. It projects `(0, 0, 1000)` and expects `(160, 120)`. It restores the matrix and the settings it replaced.

Alternative considered: call `RotTransPers`. Rejected. The stub returns 0 for every vertex, so the SVG would collapse to the origin. `gte_project` is the projection `gte-screen-transform` already requires.

### 6. The self-check does not consume a real scene id

The constructed scene uses id `0x7FE`, one triangle whose first vertex is `(0, 0, 1000)`, one zone, and one slot named `SELFTEST.TMD`. The writer is the same function as the live path. The tick's "already written" set is not updated, so a later real scene is unaffected. The two files stay in `debug/maps/` as `scene_7fe_mesh.json` and `scene_7fe_wireframe.svg`. The check reads them back. `main_pc.c` calls it from the existing `HMBTN_SELFTEST=1` list.

### 7. The JSON shape

Integers are decimal. World units are the game's.

```json
{
  "scene": 80,
  "place": { "map": 0, "area": 2 },
  "polygons": [
    {
      "kind": "mesh",
      "id": "0:1",
      "header": 8192,
      "world": [[0, 0, 1000], [100, 0, 1000], [0, 100, 1000]],
      "screen": [{"x": 160, "y": 120}, {"x": 170, "y": 120}, {"behind": true}]
    },
    {
      "kind": "grid",
      "id": "0,0",
      "column": 0,
      "row": 0,
      "world": [[-4400, 0, 1600], [-4000, 0, 1600], [-4000, 0, 1200], [-4400, 0, 1200]],
      "screen": [{"x": 10, "y": 20}, {"x": 20, "y": 20}, {"x": 20, "y": 30}, {"x": 10, "y": 30}]
    }
  ],
  "zones": [
    {
      "index": 0,
      "id": 34,
      "flags": 0,
      "model_index": 1,
      "min": [0, 0, 0],
      "max": [100, 50, 100],
      "entry": [50, 0, 50]
    }
  ],
  "slots": [
    {
      "index": 0,
      "slot": 0,
      "flags": 0,
      "name": "SELFTEST.TMD",
      "position": [10, 0, 20],
      "angle": [0, 0, 0],
      "screen": {"x": 12, "y": 40}
    }
  ]
}
```

`kind` is `mesh` or `grid`. `header` is only on a mesh polygon. A grid cell has `column` and `row`. A behind vertex is `{"behind": true}`. A slot screen uses the same behind object when that anchor is behind the camera. The numbers above are the shape, not measured farm coordinates.

## Risks / Trade-offs

- [A door has swung, so `scene_entries` no longer matches the script position] → The file keeps the authored anchor. The swung pose is out of scope until a host-visible copy exists.
- [`model_load_lighting` rebuilds `view_matrix` inside `model.c`] → The eye and target are not written. The next view build overwrites that matrix from the same eye. Fog and the color matrices are restored from `gte_debug_settings` before the tick returns.
- [The farm TMD has thousands of polygons, so the SVG is large] → The tick writes the file and returns. It does not draw the SVG in the window.
- [Two places share one `head.scene`] → The first write wins for that process, and the JSON records the place of that write. The house, barn, and coop bases do not share ids.
- [`field_result_coords` leaves Y unset] → The writer sets Y to 0. A sloping field shows its slope on the mesh polygons, not on the grid quads.
- [The section name is not a C string] → The writer copies at most 31 bytes and stops at the first zero. A missing terminator becomes an empty name and the rest of the file is still written.

## Migration Plan

Rebuild the host. The disc image is unchanged. With the flag unset, no map file is written and no position changes. Rollback is leaving the flag unset. The new `.c` file drops out of the glob when it is deleted.

## Open Questions

None. A scene that never becomes resident is not in the files. Loading it is still entering that scene with the flag on.
