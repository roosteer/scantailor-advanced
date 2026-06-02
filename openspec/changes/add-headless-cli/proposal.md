## Why

ScanTailor Advanced (SCA) provides only a GUI binary (`scantailor-advanced`). For batch processing and Docker/CI environments, users must resort to `xvfb-run` (a virtual X11 framebuffer), a fragile workaround with significant overhead. Adding a headless CLI binary eliminates this dependency, enables automated scan processing pipelines, and makes SCA usable in containers without any X11 libraries at runtime.

## What Changes

- **New binary**: `scantailor-advanced-cli` — a headless CLI using `QCoreApplication` (no X11/Wayland dependency)
- **New class `CliRequest`**: a value-type DTO representing parsed CLI arguments. Depends only on core enum types (`LayoutType`, `ColorMode`, `DewarpingMode`, `DespeckleLevel`), not on filter implementation classes.
- **New class `FilterConfigAdapter`**: translates `CliRequest` into filter settings calls. Isolated adapter that knows filter types but is separate from both parsing and orchestration.
- **New class `ConsoleBatch`**: use case orchestrator that configures the filter pipeline from a `CliRequest` and executes batch processing via `StageSequence`.
- **New module `src/app_cli/`**: all CLI-specific code, cleanly separated from the GUI app and the core.
- **Minor modifications to 6 filter headers**: add `getSettings()` accessor to `fix_orientation::Filter`, `page_split::Filter`, `deskew::Filter`, `select_content::Filter`, `page_layout::Filter`, and `output::Filter`. One line per header. No behavioral change.
- **Build system**: new `CMakeLists.txt` in `src/app_cli/`, one-line addition to `src/CMakeLists.txt`.

## Capabilities

### New Capabilities

- `cli-batch-processing`: Process scanned images through the full SCA pipeline (fix orientation → split → deskew → select content → page layout → output) from the command line, without a display server.
- `cli-project-processing`: Load an existing `.ScanTailor` project file and execute its configured pipeline headlessly.
- `cli-json-output`: Produce a structured JSON report on stdout (`--output json`) with per-page status, output file paths, and error details for scripting and CI integration.

### Modified Capabilities

None. The existing GUI binary and core filter pipeline are unchanged. The `getSettings()` additions are purely additive — they expose existing private state through a const-qualified accessor pattern already used elsewhere in the codebase.

## Impact

- **New files**: `src/app_cli/main-cli.cpp`, `src/app_cli/CliRequest.h/.cpp`, `src/app_cli/FilterConfigAdapter.h/.cpp`, `src/app_cli/ConsoleBatch.h/.cpp`, `src/app_cli/ExitCode.h`, `src/app_cli/CMakeLists.txt`
- **Modified files**: `src/core/filters/{fix_orientation,page_split,deskew,select_content,page_layout,output}/Filter.h` (add `getSettings()`), `src/CMakeLists.txt` (add `add_subdirectory(app_cli)`)
- **Dependencies**: The CLI links against the same core libraries as the GUI. At runtime, it uses `QCoreApplication` only — no `libQt6Gui`, no X11/Wayland.
- **Build**: The CLI binary is built alongside the GUI binary. A single CMake flag could optionally disable it, but by default both are built.
