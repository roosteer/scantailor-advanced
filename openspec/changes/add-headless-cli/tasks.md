## 1. Filter Settings Accessors

- [x] 1.1 Add `getSettings()` to `src/core/filters/fix_orientation/Filter.h` — one-line accessor returning `std::shared_ptr<Settings>`
- [x] 1.2 Add `getSettings()` to `src/core/filters/page_split/Filter.h`
- [x] 1.3 Add `getSettings()` to `src/core/filters/deskew/Filter.h`
- [x] 1.4 Add `getSettings()` to `src/core/filters/select_content/Filter.h`
- [x] 1.5 Add `getSettings()` to `src/core/filters/page_layout/Filter.h`
- [x] 1.6 Add `getSettings()` to `src/core/filters/output/Filter.h`

## 2. CLI Module Scaffold

- [x] 2.1 Create `src/app_cli/` directory
- [x] 2.2 Create `src/app_cli/CMakeLists.txt` — add executable target `scantailor-advanced-cli`, link core libraries and `Qt::Core Qt::Xml`
- [x] 2.3 Modify `src/CMakeLists.txt` — add `add_subdirectory(app_cli)` after the `app` subdirectory

## 3. ExitCode and CliRequest

- [x] 3.1 Create `src/app_cli/ExitCode.h` — enum class with documented codes (SUCCESS=0, INVALID_INPUT=2, FILE_NOT_FOUND=3, FILTER_ERROR=4, OUTPUT_ERROR=5, INTERNAL_ERROR=6)
- [x] 3.2 Create `src/app_cli/CliRequest.h` — value-type DTO with fields for all CLI flags (images, projectFile, outputDir, flags map, parsed enum values)
- [x] 3.3 Create `src/app_cli/CliRequest.cpp` — `parse(QStringList)` static method that tokenizes argv, validates flag names/values, populates the DTO
- [x] 3.4 Implement `printHelp()` — formatted help with flag descriptions, valid values, examples
- [x] 3.5 Implement `--version` — print version from `version.h.in` with commit info

## 4. FilterConfigAdapter

- [x] 4.1 Create `src/app_cli/FilterConfigAdapter.h` — class that takes a `StageSequence` and applies `CliRequest` settings to all filters in range
- [x] 4.2 Create `src/app_cli/FilterConfigAdapter.cpp` — implement per-filter configuration methods:
  - `applyFixOrientation()` — map `--orientation` flag to `OrthogonalRotation`
  - `applyPageSplit()` — map `--layout` to `LayoutType`
  - `applyDeskew()` — map `--deskew`, `--rotate` to deskew params
  - `applySelectContent()` — map `--content-detection`, `--content-box` to select_content params
  - `applyPageLayout()` — map `--margins-*`, `--alignment-*` to page_layout settings
  - `applyOutput()` — map `--output-dpi`, `--color-mode`, `--threshold`, `--despeckle`, `--dewarping`, `--depth-perception` to output params

## 5. ConsoleBatch

- [x] 5.1 Create `src/app_cli/ConsoleBatch.h` — use case orchestrator class with two constructors: `ConsoleBatch(CliRequest, StageSequence)` and `ConsoleBatch(CliRequest, QString projectFile)`
- [x] 5.2 Create `src/app_cli/ConsoleBatch.cpp` — implement `process()`:
  - Determine filter range from `startFilter`/`endFilter`
  - Call `FilterConfigAdapter::apply()` to configure filters
  - Build task chain: `createCompositeTask()` (bottom-up, output → fix_orientation, wrapped in `LoadFileTask`)
  - Execute tasks using `BackgroundTask::BATCH` mode
  - Collect results for JSON output

## 6. Main Entry Point

- [x] 6.1 Create `src/app_cli/main-cli.cpp` — Humble Object:
  - `QCoreApplication app(argc, argv)`
  - `CliRequest::parse(app.arguments())`
  - Handle `--help`, `--version` early exits
  - Construct `ProjectPages` (from images or project file)
  - Construct `StageSequence`
  - Wire `ConsoleBatch`, execute, return exit code

## 7. JSON Output

- [x] 7.1 Implement `--output json` in `ConsoleBatch::process()` — collect per-page results into a JSON structure
- [x] 7.2 Emit JSON to stdout when `--output json` is active
- [x] 7.3 Ensure all diagnostic output (verbose, errors) goes to stderr, never stdout

## 8. Build Verification

- [x] 8.1 Build: `cmake .. -DCMAKE_BUILD_TYPE=Release && make scantailor-advanced-cli -j$(nproc)`
- [x] 8.2 Smoke test: `./scantailor-advanced-cli --help` — prints help, exits 0
- [x] 8.3 Smoke test: `./scantailor-advanced-cli --version` — prints version, exits 0
- [x] 8.4 Smoke test: `./scantailor-advanced-cli` (no args) — prints help, exits 0
- [x] 8.5 Headless test: run with `QT_QPA_PLATFORM=offscreen` or in env without `DISPLAY` — confirms no X11 dependency
- [x] 8.6 Process a single image through the full pipeline: `./scantailor-advanced-cli test.png /tmp/out`
- [x] 8.7 Process a `.ScanTailor` project file headlessly
- [x] 8.8 Verify exit codes: invalid flag → 2, missing file → 3
- [x] 8.9 Verify `--output json` produces valid JSON on stdout with all diagnostics on stderr
