# host-texture-mods Specification

## Purpose

Lets one textured primitive sample a BMP from the host mods folder when that file is the only match for the primitive's page and UV bounds.

## Requirements

### Requirement: One file replaces the sample
When exactly one loaded BMP matches a textured primitive's page and UV bounds, and no model file matches the TMD that built that primitive, the primitive SHALL sample that image. The words already stored in VRAM SHALL stay unchanged.

#### Scenario: A colored square covers the primitive
- **WHEN** one BMP matches the primitive's page and UV bounds, no model file matches the TMD that built it, and the image is a solid color
- **THEN** a pixel of that primitive is that color and the VRAM word at the packet's address is unchanged

### Requirement: No match samples VRAM
When no loaded BMP matches a textured primitive's page and UV bounds, and no model file matches the TMD that built that primitive, the primitive SHALL sample VRAM as it does with an empty mods folder.

#### Scenario: A different UV stays on VRAM
- **WHEN** the mods folder has a BMP for one page and UV bounds, a primitive uses another UV, and no model file matches the TMD that built it
- **THEN** that primitive samples the VRAM word for its own address

### Requirement: Two files do not replace
When two loaded BMPs match the same page and UV bounds, the primitive SHALL sample VRAM.

#### Scenario: A duplicate key
- **WHEN** two BMPs match one primitive's page and UV bounds
- **THEN** that primitive samples VRAM

### Requirement: An unreadable file does not replace
When the only file for a page and UV bounds cannot be read as a BMP, the primitive SHALL sample VRAM.

#### Scenario: A broken file
- **WHEN** the only file for that page and UV bounds is not a readable BMP
- **THEN** the primitive samples VRAM

### Requirement: A model file replaces that model
When exactly one loaded BMP is named for the TMD that built a textured primitive, the primitive SHALL sample that image and SHALL NOT sample a page-and-UV file. The words already stored in VRAM SHALL stay unchanged. The name SHALL be `tmd_` plus eight hexadecimal digits of the TMD header address.

#### Scenario: One model takes its own file
- **WHEN** the TMD that built a primitive has exactly one readable BMP under that name and the image is a solid color
- **THEN** a pixel of that primitive is that color and the VRAM word at the packet's address is unchanged

#### Scenario: A page file does not win
- **WHEN** the same primitive also matches one page-and-UV BMP of a different color
- **THEN** the pixel is the model file's color

### Requirement: Two models do not share one file
When two TMDs build textured primitives that use the same page and UV, each primitive SHALL sample only the BMP named for its own TMD.

#### Scenario: The same UV, two files
- **WHEN** two TMDs emit the same page and UV and each has its own solid-color BMP
- **THEN** each primitive shows its own color

#### Scenario: The second model has no file
- **WHEN** the first TMD has a model BMP and the second TMD has none, and both emit the same page and UV
- **THEN** the second primitive samples VRAM

### Requirement: A missing or duplicate model file does not replace
When a TMD has no readable model BMP, or more than one BMP for the same address, its primitives SHALL follow the page-and-UV rules.

#### Scenario: No model file
- **WHEN** the TMD that built a primitive has no model BMP and one page-and-UV BMP matches
- **THEN** the primitive samples that page-and-UV image

#### Scenario: Two files for one address
- **WHEN** two readable BMPs are named for the same TMD address
- **THEN** that TMD's primitive does not sample a model file

#### Scenario: An unreadable model file
- **WHEN** the only file named for that TMD address is not a readable BMP
- **THEN** that TMD's primitive does not sample a model file

### Requirement: Each TMD address is logged once
The host SHALL log the TMD header address the first time a textured primitive is built for that address in the session, including when no model file exists.

#### Scenario: The first primitive of a TMD
- **WHEN** a textured primitive is built for a TMD address that has not been logged in this session
- **THEN** the log contains that address once

#### Scenario: The same TMD again
- **WHEN** another textured primitive is built for an address already logged in this session
- **THEN** the log does not gain a second line for that address
