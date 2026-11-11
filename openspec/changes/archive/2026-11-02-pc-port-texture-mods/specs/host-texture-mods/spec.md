# Spec Delta

## Purpose

Lets one textured primitive sample a BMP from the host mods folder when that file is the only match for the primitive's page and UV bounds.

## ADDED Requirements

### Requirement: One file replaces the sample
When exactly one loaded BMP matches a textured primitive's page and UV bounds, the primitive SHALL sample that image. The words already stored in VRAM SHALL stay unchanged.

#### Scenario: A colored square covers the primitive
- **WHEN** one BMP matches the primitive's page and UV bounds and the image is a solid color
- **THEN** a pixel of that primitive is that color and the VRAM word at the packet's address is unchanged

### Requirement: No match samples VRAM
When no loaded BMP matches a textured primitive's page and UV bounds, the primitive SHALL sample VRAM as it does with an empty mods folder.

#### Scenario: A different UV stays on VRAM
- **WHEN** the mods folder has a BMP for one page and UV bounds, and a primitive uses another UV
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
