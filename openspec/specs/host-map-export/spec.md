# host-map-export Specification

## Purpose

Writes a world-space and screen-space reference of whichever farm scene is loaded, so slot positions can be checked from files instead of by eye.

## Requirements

### Requirement: The export is armed from the environment or the command line
The host SHALL write map files only when `HMBTN_EXPORT_MAPS` is `1` or a list of scene ids, or when the process is started with `--export-maps` or `--export-maps` followed by a list of scene ids. When both are present, the command-line value SHALL be the one that applies. When neither is present, the host SHALL write no map file.

#### Scenario: The flag is absent
- **WHEN** `HMBTN_EXPORT_MAPS` is unset and the process has no `--export-maps` argument
- **THEN** no file is written under `debug/maps/`

#### Scenario: The environment arms every scene
- **WHEN** `HMBTN_EXPORT_MAPS` is `1` and a scene is loaded
- **THEN** that scene's map files are written

#### Scenario: The command line names one scene
- **WHEN** the process is started with `--export-maps` and `0x50`, and scene `0x50` is loaded
- **THEN** the files for scene `0x50` are written

#### Scenario: A named list skips other scenes
- **WHEN** `HMBTN_EXPORT_MAPS` is `0x50` and scene `0x60` is loaded
- **THEN** no file is written for scene `0x60`

#### Scenario: The command line wins
- **WHEN** `HMBTN_EXPORT_MAPS` is `0x50` and the process is started with `--export-maps` and `0x60`, and scene `0x60` is loaded
- **THEN** the files for scene `0x60` are written and no file is written for scene `0x50`

### Requirement: Each loaded scene is written once
For each armed scene id that becomes the loaded scene, the host SHALL write `debug/maps/scene_<id>_mesh.json` and `debug/maps/scene_<id>_wireframe.svg`, where `<id>` is that scene id in lowercase hexadecimal with no prefix. The same scene id SHALL NOT be written again in that process. The outdoor farm, the house, the barn, and the coop SHALL use this same rule. A loaded state with no area mesh yet SHALL write nothing.

#### Scenario: The house uses its own scene id
- **WHEN** the export is armed for every scene and the loaded scene id is `0x50`
- **THEN** the host writes `debug/maps/scene_50_mesh.json` and `debug/maps/scene_50_wireframe.svg`

#### Scenario: The barn is not folded into the farm file
- **WHEN** the export is armed for every scene and the loaded scene id is `0x60`
- **THEN** the host writes `debug/maps/scene_60_mesh.json` and does not replace `debug/maps/scene_50_mesh.json`

#### Scenario: A second visit stays quiet
- **WHEN** scene `0x50` is exported and later becomes the loaded scene again
- **THEN** the two files from the first write are left unchanged

#### Scenario: The title writes nothing
- **WHEN** the export is armed and the current mode has no area mesh
- **THEN** no file is written under `debug/maps/`

### Requirement: The JSON names the scene and its three lists
The JSON SHALL name the scene id and the place's map and area. It SHALL contain a polygon list, a zone list, and a slot list. Each list SHALL be present when it is empty.

#### Scenario: An empty house still has the lists
- **WHEN** a scene with no live zone and no present model is exported
- **THEN** the JSON names that scene id and its place, and its zone list and slot list are empty

### Requirement: Polygons, zones, and slots use world coordinates
Each floor polygon of the loaded area mesh SHALL record absolute world X, Y, and Z for every vertex. Each live zone SHALL record its id and its minimum, maximum, and entry point in those units. Each present scene-model slot SHALL record its slot number, TMD name, and world position. An empty zone and an absent model SHALL be omitted.

#### Scenario: A house polygon keeps world coordinates
- **WHEN** scene `0x50` is exported and the area mesh has a triangle
- **THEN** that triangle's JSON entry has three world points, each with X, Y, and Z

#### Scenario: A zone box is included
- **WHEN** the loaded scene has a live zone
- **THEN** the JSON names that zone's id and its minimum, maximum, and entry point

#### Scenario: A slot names its model
- **WHEN** the loaded scene has a present model entry with a slot number and a TMD name
- **THEN** the JSON names that slot number, that TMD name, and the entry's world position

### Requirement: The field grid follows the place
On the outdoor farm the polygon list SHALL include one grid cell per field column and row. In the greenhouse it SHALL include one grid cell per greenhouse column and row. Any other place SHALL include no grid cell.

#### Scenario: The farm grid is present only on the farm
- **WHEN** the loaded place is the outdoor farm
- **THEN** the JSON includes one grid cell for column 0, row 0 and one for the last column and row

#### Scenario: The house has no farm grid
- **WHEN** the loaded place is the house
- **THEN** the JSON grid cells are empty and the area-mesh polygons are still listed

### Requirement: Screen points match the host projection
Each vertex SHALL carry the screen X and Y produced by the host GTE projection of its world point, using the view, pixel offset, and screen distance loaded for that export. A vertex whose transformed depth is not positive SHALL be marked behind and SHALL NOT be given a screen point. With an identity rotation, zero translation, offset `(160, 120)`, and screen distance `1000`, the world point `(0, 0, 1000)` SHALL project to screen `(160, 120)`.

#### Scenario: A point on the axis hits the offset
- **WHEN** the export projects `(0, 0, 1000)` with identity rotation, zero translation, offset `(160, 120)`, and screen distance `1000`
- **THEN** the JSON screen point is `(160, 120)` and the vertex is not marked behind

#### Scenario: A point behind the camera is marked
- **WHEN** the export projects a vertex whose transformed depth is not positive
- **THEN** the JSON marks that vertex behind and stores no screen point for it

### Requirement: The SVG labels each visible polygon
The SVG SHALL draw one polygon for each exported floor polygon and each zone footprint whose every vertex is in front of the camera. The polygon's points SHALL be that polygon's screen points. A text element at the average of those screen points SHALL show the polygon id, or the grid column and row for a grid cell. A polygon with any vertex behind the camera SHALL be omitted from the SVG.

#### Scenario: A grid cell is labeled
- **WHEN** a farm grid cell is in front of the camera
- **THEN** the SVG contains that cell's polygon and a text element with its column and row

#### Scenario: A triangle behind the camera is omitted
- **WHEN** one vertex of an area-mesh triangle is behind the camera
- **THEN** the SVG contains no polygon for that triangle

### Requirement: The write returns and leaves the scene in place
The export SHALL finish in the frame that starts it, and that frame SHALL still be presented. It SHALL NOT wait for a key. After the write, every exported zone point and slot position SHALL equal the value it had before the write.

#### Scenario: The frame is still presented
- **WHEN** an armed scene is exported on a presented frame
- **THEN** that frame is presented and the two map files exist

#### Scenario: Slot positions are unchanged
- **WHEN** a scene with a present model slot is exported
- **THEN** that slot's world position after the write equals its world position before the write

### Requirement: A constructed scene proves the files
With `HMBTN_SELFTEST=1`, the host SHALL export one constructed scene that is not read from the disc: one triangle, one live zone, and one present slot. The check SHALL fail when the JSON world triangle, the zone box, the slot name, or the screen point of `(0, 0, 1000)` disagrees with that scene, or when the SVG lacks the triangle's label. The check SHALL leave the GTE view as it found it.

#### Scenario: The constructed triangle round-trips
- **WHEN** `HMBTN_SELFTEST=1` runs
- **THEN** the constructed scene's JSON contains that triangle's world points, the zone, the slot name, and screen `(160, 120)` for `(0, 0, 1000)`, and the SVG contains the triangle's label
