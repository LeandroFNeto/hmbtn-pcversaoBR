# Proposal

## Why

Shipping bins, chests, doors, tools, and furniture are placed by reading world slots, and checking those slots by eye in the running game is slow. The host needs a reference of the loaded scene's floor polygons, collision boxes, and anchor slots, in world units and in the screen projection the GTE already uses, for whichever scene is resident.

## What Changes

- A host exporter writes `debug/maps/scene_<id>_mesh.json` and `debug/maps/scene_<id>_wireframe.svg` for the scene in `farm_area.mode`. The scene id comes from `farm_area.mode.script.head.scene`. The farm is one scene the exporter can see, not a special case.
- The JSON lists floor polygons with absolute world X, Y, Z, the screen point of each vertex, the zone boxes, and each scene-model slot with its TMD name and position.
- The SVG draws each exported polygon and prints that polygon's id, or its grid column and row, at the centre.
- `HMBTN_EXPORT_MAPS=1`, or `--export-maps` on the host command line, arms the export. A value or argument that names scene ids exports only those ids. Each distinct loaded scene is written once. The write returns without waiting on the frame loop.
- `HMBTN_SELFTEST=1` checks a constructed scene: one triangle, one zone, one slot, and a known screen point. It does not need the house or the barn to be loaded.
- No sprite, door, or piece of furniture is moved. `src/main` and `src/overlay` are not edited.

## Capabilities

### New Capabilities

- `host-map-export`: For any loaded farm scene, the host writes one JSON mesh and one SVG wireframe of that scene's floor polygons, zone boxes, and model slots, using the screen projection the host GTE already applies.

### Modified Capabilities

## Impact

- New host files `src/pc_port/src/map_exporter.c` and `src/pc_port/include/map_exporter.h`. `cmake/host.cmake` already globs `src/pc_port/src/*.c`.
- The arming flag is read from the environment and from `argv` in `src/pc_port`. The one-shot write is called from the existing present path in `src/pc_port/src/host.c`. The self-check joins the list in `src/pc_port/src/main_pc.c`.
- The exporter reads `farm_area`, `farm_model`, and the public field helpers. It projects with `gte_project` after the view is loaded. It does not call `RotTransPers`.
- The README diagnostic paragraph names `HMBTN_EXPORT_MAPS` and the two output files.
- `gte-screen-transform` and `host-gte-library` stay as they are. This change only consumes the projection they already define.
