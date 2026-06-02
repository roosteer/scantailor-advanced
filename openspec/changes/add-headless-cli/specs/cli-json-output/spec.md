## ADDED Requirements

### Requirement: JSON output mode
The CLI SHALL support `--output json` to produce a structured JSON report on stdout after processing completes.

#### Scenario: Successful processing with JSON output
- **WHEN** user runs with `--output json`
- **THEN** stdout contains a JSON object with keys `status`, `pages_processed`, `pages_failed`, `output_files`, and `errors`

#### Scenario: JSON schema for successful run
- **WHEN** processing completes successfully with `--output json`
- **THEN** stdout contains valid JSON with:
  - `status`: `"ok"`
  - `pages_processed`: integer count
  - `pages_failed`: integer count (0 on full success)
  - `output_files`: array of objects, each with `page_id`, `input_file`, `output_file` (relative paths)
  - `errors`: empty array

#### Scenario: JSON schema for partial failure
- **WHEN** some pages fail processing with `--output json`
- **THEN** stdout contains valid JSON with:
  - `status`: `"partial"`
  - `pages_failed`: integer > 0
  - `errors`: array of objects, each with `page_id`, `filter` (which filter failed), `message`

#### Scenario: JSON output and verbose logs
- **WHEN** user runs with both `--output json` and `--verbose`
- **THEN** valid JSON appears on stdout and verbose timestamped logs appear on stderr with no mixing

### Requirement: Default output mode
When `--output` is not specified, the CLI SHALL produce no data on stdout and emit progress information to stderr.

#### Scenario: Default output (no --output flag)
- **WHEN** user runs without `--output` flag
- **THEN** stdout is empty and progress/diagnostic information goes to stderr
