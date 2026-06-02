## ADDED Requirements

### Requirement: Process images directly from command line
The CLI SHALL accept one or more image file paths and an output directory as positional arguments, and process them through the full ScanTailor Advanced filter pipeline without requiring a display server.

#### Scenario: Process single image with defaults
- **WHEN** user runs `scantailor-advanced-cli scan.png /tmp/out`
- **THEN** the CLI processes `scan.png` through all 6 filters with default settings and writes output to `/tmp/out/`

#### Scenario: Process multiple images
- **WHEN** user runs `scantailor-advanced-cli scan1.png scan2.png /tmp/out`
- **THEN** both images are processed sequentially and output files are written to `/tmp/out/`

#### Scenario: Output directory does not exist
- **WHEN** user specifies a non-existent output directory
- **THEN** the CLI SHALL create it before processing

#### Scenario: No display server available
- **WHEN** the CLI runs in an environment without X11 or Wayland (e.g., Docker container, SSH session without forwarding)
- **THEN** the CLI SHALL start and process images successfully without errors related to missing display

### Requirement: Configure processing via command-line flags
The CLI SHALL support flags to override default filter settings for all pages being processed.

#### Scenario: Set output DPI
- **WHEN** user runs with `--output-dpi=300`
- **THEN** all output images are produced at 300 DPI

#### Scenario: Set color mode
- **WHEN** user runs with `--color-mode=bw`
- **THEN** all output images are produced in black and white

#### Scenario: Configure deskew
- **WHEN** user runs with `--deskew=auto`
- **THEN** automatic deskew is applied to all pages

#### Scenario: Configure content detection
- **WHEN** user runs with `--content-detection=auto`
- **THEN** automatic content detection is enabled for all pages

#### Scenario: Set page layout type
- **WHEN** user runs with `--layout=1`
- **THEN** all pages are treated as single-page layouts

#### Scenario: Unknown flag
- **WHEN** user passes an unrecognized flag `--foo=bar`
- **THEN** the CLI SHALL exit with code 2 (INVALID_INPUT) and print an error message to stderr listing the unknown flag

#### Scenario: Invalid flag value
- **WHEN** user passes `--color-mode=invalid`
- **THEN** the CLI SHALL exit with code 2 (INVALID_INPUT) and print an error message listing valid values

### Requirement: Filter selection
The CLI SHALL support `--start-filter` and `--end-filter` flags to restrict which filters are executed.

#### Scenario: Start from a specific filter
- **WHEN** user runs with `--start-filter=3`
- **THEN** only filters from index 3 onward are executed (0-based: 0=fix_orientation, 1=page_split, 2=deskew, 3=select_content, 4=page_layout, 5=output)

#### Scenario: End at a specific filter
- **WHEN** user runs with `--end-filter=4`
- **THEN** processing stops after filter index 4 (page_layout), and the output filter is skipped

#### Scenario: Invalid filter index
- **WHEN** user runs with `--start-filter=99`
- **THEN** the CLI SHALL exit with code 2 (INVALID_INPUT) and print an error message

### Requirement: Save project after processing
The CLI SHALL support `--output-project=<file>` to save the configured pipeline as a `.ScanTailor` project file after processing completes.

#### Scenario: Save project after image processing
- **WHEN** user runs with `--output-project=result.ScanTailor`
- **THEN** after processing completes, a valid `.ScanTailor` project file is written containing all pages and their computed settings

### Requirement: Verbosity control
The CLI SHALL support `--verbose` for detailed progress output and `--quiet` to suppress all non-error output.

#### Scenario: Verbose mode
- **WHEN** user runs with `--verbose`
- **THEN** each filter execution step is logged to stderr with timestamp and filter name

#### Scenario: Quiet mode
- **WHEN** user runs with `--quiet`
- **THEN** only fatal errors are printed to stderr; no progress output is emitted

### Requirement: Exit codes
The CLI SHALL use distinct exit codes for different error classes.

#### Scenario: Success
- **WHEN** processing completes without errors
- **THEN** the CLI exits with code 0

#### Scenario: Invalid input
- **WHEN** input validation fails (unknown flag, invalid value, missing required argument)
- **THEN** the CLI exits with code 2

#### Scenario: File not found
- **WHEN** a specified image or project file does not exist
- **THEN** the CLI exits with code 3

#### Scenario: Filter processing error
- **WHEN** a filter fails during processing (e.g., corrupt image, unsupported format)
- **THEN** the CLI exits with code 4

#### Scenario: Output write error
- **WHEN** the output directory is not writable or disk is full
- **THEN** the CLI exits with code 5

### Requirement: Structured logging
All diagnostic output SHALL follow the format `<ISO8601-timestamp>[<operation>]: <message>` and be written to stderr.

#### Scenario: Progress log format
- **WHEN** processing a page through the deskew filter
- **THEN** stderr contains a line matching `\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[deskew]: processing page \d+`

#### Scenario: Data on stdout, diagnostics on stderr
- **WHEN** `--output json` is used
- **THEN** valid JSON appears on stdout and all log/diagnostic lines appear on stderr
