## Context

ScanTailor Advanced (SCA) is a Qt6-based C++ application for post-processing scanned pages. The existing codebase has a single GUI binary at `src/app/` that uses `QApplication`. The core processing pipeline lives in `src/core/` and is shared with the GUI. A reference CLI implementation exists in the `trufanov-nok/scantailor-universal` fork, from which we port and adapt code.

**Constraints**:
- C++17, Qt6, CMake build system
- The core uses `std::shared_ptr<T>` (unlike Universal's `IntrusivePtr<T>`)
- The 6 filter classes do not currently expose `getSettings()` publicly
- Must work headless: `QCoreApplication`, no `libQt6Gui`, no X11/Wayland at runtime

## Goals / Non-Goals

**Goals:**
- A single `scantailor-advanced-cli` binary that processes images or project files headlessly
- Clean separation: CLI adapter code in `src/app_cli/`, no changes to core filter logic
- `--help` with examples, `--version`, distinct exit codes, structured logging
- JSON output mode for scripting
- CLI flags override project file settings when both are present

**Non-Goals:**
- Interactive mode, TUI, or any display-dependent features
- Project creation wizard
- Plugin system or config file persistence for the CLI itself
- Docker image or CI workflow (these are downstream deliverables, not part of the code change)

## Decisions

### Decision 1: Hybrid Request Model (Approach B)

`CliRequest` is a value-type DTO containing only:
- Primitive types (`QString`, `int`, `double`, `bool`)
- Core enum types (`page_split::LayoutType`, `output::ColorMode`, `output::DewarpingMode`, `output::DespeckleLevel`)

**Rejected**: Pure DTO (Approach A) — would require mapping enums through string intermediaries, adding ~150 LOC of boilerplate with no real decoupling benefit since the enums ARE the stable contract of the core.

**Rejected**: Full God Object (Approach C / Universal-style) — `CommandLine` knowing 50+ internal types makes it a recompilation magnet and violates SRP.

`FilterConfigAdapter` converts `CliRequest` primitive fields (e.g., `double margins[4]`) into domain types (`Margins`) and calls filter settings methods. This is the ONLY place where CLI-origin data touches filter-internal types.

### Decision 2: getSettings() accessor on Filter classes

Each of the 6 filter classes gains a single-line public method:
```cpp
std::shared_ptr<Settings> getSettings() const { return m_settings; }
```

**Rationale**: The Settings classes are already public, well-documented, and used by the GUI's `OptionsWidget` classes. Adding a getter is the minimal surgical change. Alternatives considered:
- **Friend class**: Couples filter headers to `ConsoleBatch`, breaks when adding new CLI features.
- **Pass settings through constructor**: Requires changing filter construction everywhere — too invasive.

### Decision 3: ConsoleBatch as Use Case Orchestrator

`ConsoleBatch` takes a `CliRequest` and a `StageSequence` (already constructed by `main`). It:
1. Reads `startFilter/endFilter` to determine the filter range
2. Uses `FilterConfigAdapter` to apply CLI overrides to each filter in range
3. Builds the task chain bottom-up via `createCompositeTask()`
4. Executes tasks sequentially

**Rationale**: `ConsoleBatch` does NOT parse flags — that's `CliRequest`'s job. It does NOT know how to map strings to filter types — that's `FilterConfigAdapter`'s job. It only orchestrates: configure → chain → execute.

### Decision 4: Exit codes as enum class

```cpp
enum class ExitCode {
    SUCCESS = 0,
    INVALID_INPUT = 2,
    FILE_NOT_FOUND = 3,
    FILTER_ERROR = 4,
    OUTPUT_ERROR = 5,
    INTERNAL_ERROR = 6
};
```

Code 1 is reserved for "general error" from unhandled exceptions. Codes 2-6 are documented in `--help`.

### Decision 5: Flag naming (kebab-case, one grammar)

All flags use `--kebab-case`. The CLI is a flat command (no subcommands — there's only one operation: `process`). Grammar: `scantailor-advanced-cli [flags] <inputs...> <output_dir>` or `scantailor-advanced-cli [flags] <project.ScanTailor>`.

### Decision 6: CMake target

The CLI binary `scantailor-advanced-cli` links against the same static libraries as the GUI (`fix_orientation`, `page_split`, `deskew`, `select_content`, `page_layout`, `output`, `core`, `dewarping`, `foundation`, `imageproc`, `math`). It does NOT link `Qt::Gui` or `Qt::Widgets` directly — only `Qt::Core` and `Qt::Xml`. The transitive dependency on Qt::Gui through the filter libraries is link-time only; at runtime, since `QCoreApplication` is used, no display connection is attempted.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| `getSettings()` exposes mutable shared state. A buggy CLI could corrupt settings used by concurrent operations. | The CLI is single-threaded per invocation. Settings are per-filter, per-invocation. No shared process-level state. |
| `FilterConfigAdapter` is tightly coupled to all 6 filter types — changes to any filter Settings API break it. | This is by design: the adapter IS the coupling point. When filter APIs change, the adapter is the single place to update. Tests on the adapter catch regressions. |
| The CLI links Qt::Gui transitively. On minimal Docker images, `libQt6Gui.so` must be present even though unused. | This is a packaging concern, not a code concern. The binary itself never calls `QGuiApplication` or any GUI function. A future change could refactor the core to not link Gui, but that's out of scope. |
| `ConsoleBatch` does not implement Chain of Responsibility as a middleware pipeline. | The filter pipeline IS already a chain (each task delegates to the next). Adding a second middleware layer on top would be redundant. Each filter's `createTask()` is the handler; `ConsoleBatch` wires them. |

## Open Questions

- **`--tiff-compression` flag**: Universal has `GlobalStaticSettings`. SCA may not have an equivalent. Defer to post-v1 or drop.
- **`--skew-deviation` / `--content-deviation`**: Universal's filters had `setMaxDeviation()`. SCA uses `DeviationProvider<T>` with run-time computation. These flags can be added later if needed.
- **Thread pool configuration (`--jobs=N`)**: The CLI inherits the global thread pool from the core. Exposing a flag requires adding an API to the core. Deferred.
