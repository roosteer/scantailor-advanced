## ADDED Requirements

### Requirement: Process from project file
The CLI SHALL accept a `.ScanTailor` project file path and execute the configured pipeline on all pages defined in the project.

#### Scenario: Process a valid project file
- **WHEN** user runs `scantailor-advanced-cli project.ScanTailor`
- **THEN** all pages in the project are processed using the filter settings stored in the project file, and output is written to the project's output directory

#### Scenario: Project file not found
- **WHEN** user specifies a `.ScanTailor` file that does not exist
- **THEN** the CLI SHALL exit with code 3 (FILE_NOT_FOUND) and print an error message to stderr

#### Scenario: Corrupt project file
- **WHEN** user specifies a `.ScanTailor` file that is not valid XML or has missing required elements
- **THEN** the CLI SHALL exit with code 4 (FILTER_ERROR) and print a descriptive error to stderr

#### Scenario: Project references missing images
- **WHEN** a project file references image paths that no longer exist
- **THEN** the CLI SHALL exit with code 3 (FILE_NOT_FOUND) for each missing image and report the specific paths

### Requirement: CLI flags override project settings
When processing from a project file, CLI flags SHALL override the corresponding settings stored in the project.

#### Scenario: Override DPI from project
- **WHEN** user runs `scantailor-advanced-cli --output-dpi=600 project.ScanTailor`
- **THEN** all pages are output at 600 DPI regardless of the DPI stored in the project file

#### Scenario: Override color mode from project
- **WHEN** user runs `scantailor-advanced-cli --color-mode=color project.ScanTailor`
- **THEN** all pages are output in color regardless of the color mode stored in the project file
