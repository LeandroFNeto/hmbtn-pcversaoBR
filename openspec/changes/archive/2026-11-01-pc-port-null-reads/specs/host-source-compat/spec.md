# Spec Delta

## ADDED Requirements

### Requirement: Null reads the PS1 tolerates read what the PS1 reads
Where the game reads through a pointer that is null on the PS1 too, and the PS1 carries on because address 0 is RAM, the host SHALL read the bytes at that offset from the PS1's address 0 instead of faulting. Each covered read SHALL be listed with why the pointer is null on the PS1. The game sources SHALL stay unedited.

#### Scenario: Farm entry on a new game
- **WHEN** a new game enters the farm and the place of the current area is read before the area's data pointer is set
- **THEN** the host reads place 3 and area 0, as the PS1 does from its garbage area, takes neither mine branch, and the farm keeps loading

#### Scenario: Pointer already set
- **WHEN** a covered read happens with a non-null pointer
- **THEN** it reads through that pointer exactly as the original code does

### Requirement: Null writes the PS1 tolerates land in low PS1 RAM
Where the game writes through a pointer that is null on the PS1 too, and the written bytes fall in the PS1 kernel's unused low RAM, the host SHALL write them at that offset of its low PS1 RAM instead of faulting. Each covered write SHALL be listed with its offset and why the pointer is null on the PS1. A covered write SHALL NOT reach the kernel words the host seeds.

#### Scenario: Area without a marked model
- **WHEN** the farm populates an area whose models carry no marked flag, and the game updates the hidden flag of both marked entries
- **THEN** the host changes the halfword at 0x80000028, as the PS1 changes the one at 0x28, and the farm keeps loading

#### Scenario: Marked model present
- **WHEN** the area has a marked model
- **THEN** the write goes to that model's entry exactly as in the original code

### Requirement: Low PS1 RAM holds the kernel's values
A covered null read or write SHALL reach the host's PS1 RAM from physical address 0, the memory the PS1 reaches through address 0 and through 0x80000000. Once mapped, that RAM SHALL hold the values psx-spx documents for the PS1 kernel after boot (the garbage area at 0x0 to 0xF, and the words at 0x60, 0x64, and 0x68), and zero elsewhere in the kernel area.

#### Scenario: Self-check
- **WHEN** the host starts with `HMBTN_SELFTEST=1`
- **THEN** the log shows the low RAM check passing: the words at 0x80000000, 0x80000004, 0x80000008, and 0x8000000C are 0x00000003, 0x275A0C80, 0x03400008, and 0, the word at 0x80000060 is 2, and a covered access through a null pointer goes to 0x80000000

### Requirement: A host patch can make several replacements
A host patch of one source file SHALL apply every replacement it lists to the same copy, and configuring SHALL fail, naming the file and the text, when any one of them is not found.

#### Scenario: Two replacements in one file
- **WHEN** a patch lists an include to add and an expression to wrap in the same file
- **THEN** the compiled copy has both changes and the original file is unchanged

#### Scenario: One replacement missing
- **WHEN** the second replacement's text is not in the file
- **THEN** configuring fails and names that file and that text

## MODIFIED Requirements

### Requirement: A new game reaches the farm
With the game running from the extracted disc, confirming the introduction's summary SHALL load the farm and keep it running without a fault, and without the host stalling in the game's debug trap or anywhere else.

#### Scenario: First morning
- **WHEN** the player confirms "This is OK" after the name, birthday, farm, and dog questions
- **THEN** the farm scene draws and the frame counter keeps advancing for at least 30 seconds without a fault or a watchdog stall report
