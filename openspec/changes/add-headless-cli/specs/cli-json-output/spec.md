## ADDED Requirements

### Requirement: JSON output mode
The CLI SHALL support `--output=json` to produce a structured JSON report on stdout after processing completes.

#### Scenario: Successful processing with JSON output
- **WHEN** user runs with `--output=json`
- **THEN** stdout contains a JSON object with keys `status`, `pages_processed`, `pages_failed`, `output_files`, and `errors`

#### Scenario: JSON schema for successful run
- **WHEN** processing completes successfully with `--output=json`
- **THEN** stdout contains valid JSON with:
  - `status`: `"ok"`
  - `pages_processed`: integer count
  - `pages_failed`: integer count (0 on full success)
  - `output_files`: array of objects, each with `page_id` (0-based integer index in the processed sequence), `input_file` (absolute path), `output_file` (absolute path, or `null` when the output stage did not run)
  - `errors`: empty array

#### Scenario: JSON schema when the output stage is skipped
- **WHEN** user runs with `--end-filter=4 --output=json`, so the output filter never runs
- **THEN** each `output_files` entry has `output_file: null`, and no output image is written

#### Scenario: JSON schema for partial failure
- **WHEN** some pages fail processing with `--output=json`
- **THEN** stdout contains valid JSON with:
  - `status`: `"partial"`
  - `pages_failed`: integer > 0
  - `errors`: array of objects, each with `page_id` (0-based integer index), `input_file` (absolute path), `filter` (`null`, because the pipeline does not attribute a failure to a stage), and `message`

#### Scenario: JSON output and verbose logs
- **WHEN** user runs with both `--output=json` and `--verbose`
- **THEN** valid JSON appears on stdout and verbose timestamped logs appear on stderr with no mixing

### Requirement: Default output mode
When `--output` is not specified, the CLI SHALL produce no data on stdout and emit progress information to stderr.

#### Scenario: Default output (no --output flag)
- **WHEN** user runs without `--output` flag
- **THEN** stdout is empty and progress/diagnostic information goes to stderr

#### Scenario: Unsupported format
- **WHEN** user runs with `--output=yaml`
- **THEN** the CLI SHALL exit with code 2 (INVALID_INPUT) and report that only `text` and `json` are accepted
