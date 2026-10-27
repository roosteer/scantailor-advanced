# Restructure `src/app_cli/` around one session module, one flag table, and a report value

## Context

`scantailor-advanced-cli` is built from 9 files in `src/app_cli/` (1,339 lines) and is the only
headless entry point of ScanTailor Advanced. The ask: analyse that structure and improve its
architecture. End state: the same command-line surface, restructuring the module behind it so that
the CLI's behaviour lives behind a small interface that a test can drive without a display —
`parseCommandLine()` for the grammar, `PipelineSession` for the pipeline run, `renderJsonReport()`
for the output. Behaviour fixes that the restructure makes unavoidable (see *Defects*) are included,
because each one is currently a divergence between the advertised interface and the implementation.

Scope is `src/app_cli/` plus the two build files and the test sources. **No core file is touched**;
the GUI binary keeps using the untouched filter and project classes. The uncommitted edit already
present in `main-cli.cpp` (construct `core::Application` instead of `QCoreApplication`, because
`DefaultParamsProfileManager` casts `qApp` to `Application`) is the intended baseline and is kept.

## Defects found (each maps to a step)

1. **Not headless.** `main` constructs `QApplication` (through `core::Application`) and nothing sets a
   platform plugin. Reproduced: `env -u DISPLAY -u WAYLAND_DISPLAY -u QT_QPA_PLATFORM
   ./build/scantailor-advanced-cli --version` → exit 134 (SIGABRT); the same command with
   `QT_QPA_PLATFORM=offscreen` → exit 0. The Dockerfile already compensates with
   `ENV QT_QPA_PLATFORM=offscreen`, i.e. a required-configuration fact lives in the deployment
   instead of the binary.
2. **Pipeline wiring duplicated and divergent.** `main-cli.cpp` and `ConsoleBatch` both build page
   state, stages and an `OutputFileNameGenerator`; `main` reads the project file *twice*
   (`loadProject` and then again just to get `outputDirectory()`); `ConsoleBatch`'s
   "fall back to the project file's directory" branch is dead because `main` always fills
   `outputDir` first.
3. **Output naming is not registered.** `MainWindow::switchToNewProject` (the canonical wiring)
   reuses `ProjectReader::namingDisambiguator()` and registers every page path into it before
   naming. The CLI creates a fresh `FileNameDisambiguator` and never registers. Two inputs with the
   same basename therefore produce the same output name (`fileNameFor` appends `(%1)` only for a
   label ≠ 0) and the second overwrites the first; project mode also ignores the labels stored in the
   project.
4. **`--start-filter` does not mean what the help says.** `createCompositeTask` honours only
   `endFilter`; `startFilter` merely limits which filters receive overrides, so pages still run
   filters 0..N-1. The chain cannot start mid-pipeline (no cached inputs to feed it).
5. **JSON report diverges from its spec** (`openspec/changes/add-headless-cli/specs/cli-json-output`):
   error objects lack `filter`, `page_id` is a stringified loop index, `output_file` is reported even
   when `--end-filter` skipped the output stage (nothing was written), paths are absolute where the
   spec says relative.
6. **`--output` accepts any value.** Only `"json"` is honoured; `--output=yaml` silently degrades to
   text mode instead of exiting 2 as the batch spec requires.
7. **Misleading flags.** `--white-margins` sets `BlackWhiteOptions::setNormalizeIllumination`, which
   is *equalize illumination* in the GUI; `--equalize-illumination` is parsed into a field that
   `FilterConfigAdapter` never reads. `savitzkyGolaySmoothing` and `morphologicalSmoothing` are
   declared in `CliRequest` with `*Set` flags, parsed by nothing, applied by nothing. Help says
   `--threshold` default 43 while the real default is 0 (it is `thresholdAdjustment`, a delta).
8. **Silently ignored flag combinations**: `--deskew-angle` without `--deskew=manual`,
   `--content-detection=manual` without `--content-box`, `--quiet` together with `--verbose`.
9. **Shallow interface on the request.** 24 fields of which 7 carry a paired `*Set` boolean and 6 use
   `-1` as "unset"; every consumer re-derives presence. Two dead error channels
   (`FilterConfigAdapter::apply(..., QString& errorMsg)` — each `applyX` ignores it;
   `PageResult::errorFilter` — never assigned), and a dead exit-code branch in `process()`
   (`if (failCount > 0 && successCount == 0) return FILTER_ERROR;` followed by
   `if (failCount > 0) return FILTER_ERROR;`).
10. **Nothing is testable.** `app_cli` is an executable, so no test links it; `src/qt_tests/` exists
    and is wired by `BUILD_TESTS`, but the CLI has zero coverage despite a pure parser and a pure
    report renderer.

## Target module map

```
src/app_cli/
  ExitCode.h              ExitCode + CliError { code, message, ok() }
  CliRequest.h            value DTO only — std::optional per option, no parser, no printing
  CliUsage.h/.cpp         the flag table; printHelp() is GENERATED from it; printVersion()
  CliParser.h/.cpp        parseCommandLine(args) -> ParseResult { CliRequest, CliError }
  FilterRange.h/.cpp      kFilterCount, FilterRange {start,end}, resolveFilterRange()
  FilterConfigAdapter.h/.cpp   request -> filter Settings calls (kept; apply(FilterRange) -> void)
  PipelineSession.h/.cpp  loads images or a project, owns naming/stages/cache, runs the batch
  BatchReport.h/.cpp      PageOutcome, BatchReport, renderJsonReport(), exitCodeFor()
  ConsoleLog.h/.cpp       timestamped stderr output with a quiet/verbose policy
  main-cli.cpp            ~60 lines: platform plugin, app, parse, dispatch, render, exit code
  CMakeLists.txt          static app_cli_lib + executable
```

Deleted: `CliRequest.cpp` (its three jobs move to `CliUsage`/`CliParser`), `ConsoleBatch.h/.cpp`
(absorbed by `PipelineSession` + `BatchReport`). `ExitCode.h` keeps its enum and gains `CliError`.

Module dependencies stay one-way: `main` → `PipelineSession`/`CliParser`/`BatchReport`/`ConsoleLog`;
`PipelineSession` → `FilterConfigAdapter`/`BatchReport`/`ConsoleLog`; `CliParser` → `CliUsage`/
`CliRequest`/`FilterRange`; nothing in the CLI is included by core.

## Steps

Each step leaves the tree compiling; run `cmake --build build -j"$(nproc)" --target
scantailor-advanced-cli` after it.

### 1. Split the target so it can be tested

`src/app_cli/CMakeLists.txt`: replace the single `add_executable` with a static library holding
every source except `main-cli.cpp`, and an executable that only links it. Drop
`set(CMAKE_AUTOMOC ON)` — no class in this directory is a `QObject`. Include directories become
`PUBLIC` on the library so `src/qt_tests` can include the CLI headers.

```cmake
add_library(app_cli_lib STATIC <all sources except main-cli.cpp>)
target_link_libraries(app_cli_lib PUBLIC core dewarping ${EXTRA_LIBS})
target_include_directories(app_cli_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
    "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")

add_executable(scantailor-advanced-cli main-cli.cpp)
target_link_libraries(scantailor-advanced-cli PRIVATE app_cli_lib)
install(TARGETS scantailor-advanced-cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
```

`src/qt_tests/CMakeLists.txt`: add `app_cli_lib` to the `qt_tests` link libraries and
`tst_cli_qt.cpp` to `sources`.

### 2. `ExitCode.h`: add the error value

```cpp
#include <QString>

namespace cli {

enum class ExitCode { SUCCESS = 0, INVALID_INPUT = 2, FILE_NOT_FOUND = 3,
                      FILTER_ERROR = 4, OUTPUT_ERROR = 5, INTERNAL_ERROR = 6 };

/** Error channel for CLI operations: SUCCESS/empty means "no error". */
struct CliError {
  ExitCode code = ExitCode::SUCCESS;
  QString message;
  bool ok() const { return code == ExitCode::SUCCESS; }
};

}
```

### 3. `ConsoleLog`: one place that writes to stderr

Interface (`ConsoleLog.h`):

```cpp
class ConsoleLog {
 public:
  explicit ConsoleLog(const CliRequest& request);          // captures quiet/verbose
  void progress(const QString& operation, const QString& message) const;  // hidden by --quiet
  void detail(const QString& operation, const QString& message) const;    // only with --verbose
  void error(const QString& operation, const QString& message) const;     // always
  static void writeError(const QString& message);          // pre-parse: "Error: <message>"
 private:
  bool m_quiet;
  bool m_verbose;
};
```

`progress`/`detail`/`error` emit
`QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << " [" << operation << "]: " << message`
on `stderr`. `writeError` emits `Error: <message>` (no timestamp: verbosity is unknown before
parsing). Replace in `main-cli.cpp` every inline `QTextStream err(stderr)` block and in
`ConsoleBatch.cpp` the `logStderr` free function; operation names are `"process"` for page progress
and failures, `"output"` for project-save failures.

### 4. `BatchReport`: the run becomes a value

```cpp
struct PageOutcome {
  int index = 0;            // 0-based position in the processed sequence
  QString inputFile;
  QString outputFile;       // empty when the output stage did not run
  bool ok = false;
  QString errorMessage;
};

struct BatchReport {
  std::vector<PageOutcome> pages;
  int processed() const;    // count of ok pages
  int failed() const;       // count of failed pages
};

QString renderJsonReport(const BatchReport& report);
ExitCode exitCodeFor(const BatchReport& report);   // FILTER_ERROR if any page failed, else SUCCESS
```

`renderJsonReport` returns JSON with exactly these keys and this order:
`status` (`"ok"` when `failed()==0`, `"partial"` when `processed()>0`, else `"error"`),
`pages_processed` (int), `pages_failed` (int),
`output_files` (array, ok pages only: `page_id` int, `input_file` string, `output_file`
string-or-`null`), `errors` (array: `page_id` int, `input_file` string, `filter` always `null`,
`message` string). Indented, plus a trailing `"\n"`. `filter` is `null` because the composite task
chain throws without stage attribution; `--start-filter`/`--end-filter` are the only stage-level
facts the CLI can state.

Remove from `ConsoleBatch::process()` the JSON assembly and the exit-code tail (including the dead
duplicated `FILTER_ERROR` branch); `process()` fills a `BatchReport` and `main` renders it.
In the same edit, update `openspec/changes/add-headless-cli/specs/cli-json-output/spec.md`: error
object `filter` is `null`, `page_id` is the 0-based index, `output_file` is absolute and `null` when
the output stage was skipped.

### 5. `FilterRange`: one validated range for the whole run

```cpp
namespace cli {
constexpr int kFilterCount = 6;   // fix_orientation, page_split, deskew, select_content, page_layout, output
struct FilterRange { int start = 0; int end = kFilterCount - 1; };  // inclusive, 0-based

/** Validates the request's filter indices against the stages actually available.
    On failure sets error (INVALID_INPUT) and returns std::nullopt. */
std::optional<FilterRange> resolveFilterRange(const CliRequest& request, int availableFilters,
                                              CliError& error);
}
```

Rules: `start = request.startFilter.value_or(0)`, `end = request.endFilter.value_or(availableFilters
- 1)`; `start > end` → `"--start-filter (%1) must not be greater than --end-filter (%2)"`;
`end >= availableFilters` → `"--end-filter (%1) out of range (0..%2)"`. This is the only filter-index
validation left; `FilterConfigAdapter::apply` becomes `void apply(const FilterRange& range)` and its
per-filter `QString& errorMsg` parameters are deleted, as is `ConsoleBatch`'s duplicated range check.

`createCompositeTask` keeps honouring `range.end` only; add a comment stating that `range.start`
never affects the chain because the CLI has no cached inputs to start from, and that `range.start`
limits where `FilterConfigAdapter` applies overrides.

### 6. `PipelineSession`: one owner for the headless run

```cpp
class PipelineSession {
  DECLARE_NON_COPYABLE(PipelineSession)
 public:
  /** Loads images or the project file, prepares pages, stages, naming and the thumbnail cache.
      Returns nullptr and fills error on failure. */
  static std::unique_ptr<PipelineSession> open(const CliRequest& request, CliError& error);

  const PageSequence& pages() const;                        // PAGE_VIEW
  const std::shared_ptr<StageSequence>& stages() const;

  void configure(const FilterRange& range);                 // FilterConfigAdapter over m_request
  BatchReport process(const FilterRange& range, const ConsoleLog& log);
  bool saveProject(const QString& path, CliError& error);   // ProjectWriter

 private:
  explicit PipelineSession(const CliRequest& request);
  ~PipelineSession();                                       // resets cache, removes its dir
  bool loadImages(CliError& error);
  bool loadProject(CliError& error);
  std::shared_ptr<BackgroundTask> createCompositeTask(const PageInfo& page, const FilterRange& range);

  CliRequest m_request;                       // by value; no reference to main's locals
  std::shared_ptr<ProjectPages> m_pages;
  std::shared_ptr<StageSequence> m_stages;
  OutputFileNameGenerator m_outFileNameGen;
  std::shared_ptr<ThumbnailPixmapCache> m_thumbnailCache;
  QString m_outputDir;
  QString m_thumbnailDir;
};
```

`open()` steps, in this order (mirrors `MainWindow::switchToNewProject`; the order is load-bearing):

1. Project mode (`!request.projectFile.isEmpty()`):
   - `QFile` existence/unreadable → `FILE_NOT_FOUND`, messages
     `"Project file not found: <path>"` / `"Cannot open project file: <path>"`;
     `QDomDocument::setContent` failure or `!reader.success()` → `FILTER_ERROR`
     (`"Invalid or corrupt project file: <path>"` / `"Failed to read project file: <path>"`).
   - Build the session with `reader.pages()`, **`reader.namingDisambiguator()`**, and
     `outputDir = request.outputDir`, else `reader.outputDirectory()`, else
     `QFileInfo(projectFile).absolutePath()`.
   - Missing referenced images: collect `page.id().imageId().filePath()` over
     `pages->toPageSequence(IMAGE_VIEW)` into a `std::set<QString>`, keep those failing
     `QFileInfo::exists`, and if the set is non-empty return `FILE_NOT_FOUND` with
     `"Project references missing image(s): a; b; c"`.
   - `m_stages = std::make_shared<StageSequence>(m_pages, PageSelectionAccessor(provider))`, then
     `reader.readFilterSettings(m_stages->filters())`.
2. Image mode: for each `request.images` path — `QFileInfo::exists` → `FILE_NOT_FOUND
   "File not found: <path>"`; `ImageMetadataLoader::load` != `LOADED` → `FILE_NOT_FOUND
   "Cannot load image: <path>"`; collect `std::vector<ImageFileInfo>`, build
   `ProjectPages(files, ProjectPages::AUTO_PAGES, Qt::LeftToRight)`, new
   `FileNameDisambiguator`, and call `filter->loadDefaultSettings(page)` for every page in
   `PAGE_VIEW` order for every `stages->filters()` entry.
3. `pages()->toPageSequence(PAGE_VIEW).numPages() == 0` → `INVALID_INPUT "No pages to process."`
4. `m_outFileNameGen = OutputFileNameGenerator(disambiguator, m_outputDir, m_pages->layoutDirection())`
   and **then** register, in IMAGE_VIEW order:
   `for (const PageInfo& page : m_pages->toPageSequence(IMAGE_VIEW))
   m_outFileNameGen.disambiguator()->registerFile(page.imageId().filePath());`
   (the same order `MainWindow` uses; this is the fix for defect 3).
5. `m_thumbnailDir = QDir::tempPath() + "/scantailor-cli-thumbs-" +
   QString::number(QCoreApplication::applicationPid())`, then
   `m_thumbnailCache = std::make_shared<ThumbnailPixmapCache>(m_thumbnailDir, QSize(100, 100), 100, 50)`.
   The destructor runs `m_thumbnailCache.reset(); QDir(m_thumbnailDir).removeRecursively();`
   (cache released first, because the cache still owns open files).

`process(range, log)`:

- `const bool producesOutput = (range.end >= m_stages->outputFilterIdx());`
- For each page of `m_pages->toPageSequence(PAGE_VIEW)`: `log.progress("process", "Page %1/%2: %3")`
  (`i+1`, `numPages`, `page.id().imageId().filePath())`; build `createCompositeTask(page, range)`;
  `try { (*task)(); outcome.ok = true; if (producesOutput) outcome.outputFile = m_outFileNameGen.filePathFor(page.id()); }
  catch (const std::exception& e) { outcome.errorMessage = QString::fromUtf8(e.what()); }
  catch (...) { outcome.errorMessage = QStringLiteral("unknown error"); }`;
  on success `log.detail("process", "%1 -> %2")`, on failure
  `log.error("process", "Page %1 failed: %2")`; append to the report.
- `createCompositeTask` is the current body of `ConsoleBatch::createCompositeTask` verbatim
  (bottom-up `createTask` chain gated by `range.end`, `const bool batch = true; const bool debug = false;`,
  `LoadFileTask` on top) with the `assert(fixOrientationTask)` kept.
- The old `if (!m_request.quiet)` / `[process]: Complete. N succeeded, M failed.` summary moves to
  `main` (`log.progress`) so the report stays a value.

`saveProject(path, error)`: `ProjectWriter writer(m_pages, SelectedPage(pages().pageAt(0).id(),
PAGE_VIEW), m_outFileNameGen); return writer.write(path, m_stages->filters());` — on `false` set
`error = { OUTPUT_ERROR, "Failed to save project file: <path>" }`.

Delete `ConsoleBatch.h/.cpp` and move `CliPageSelectionProvider` into an anonymous namespace in
`PipelineSession.cpp` unchanged.

### 7. Headless by default

At the top of `main()`, before any application object exists:

```cpp
if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
}
Application app(argc, argv);
```

A CLI never creates a window, so an explicit `QT_QPA_PLATFORM` from the environment still wins and
the choice is otherwise deterministic. Keep the existing comment explaining why
`core::Application` (not `QCoreApplication`) is required.

### 8. `CliRequest` with `std::optional`, a flag table, and the parser

`CliRequest.h` becomes header-only and contains, besides the positional/reporting fields:

```cpp
enum class OutputFormat { TEXT, JSON };
enum class DeskewMode { AUTO, MANUAL, OFF };
enum class ContentDetection { AUTO, MANUAL, OFF };
enum class Alignment { CENTER, ORIGINAL, AUTO };
struct MmRect { double left = 0, top = 0, right = 0, bottom = 0; };

struct CliRequest {
  QStringList images;  QString projectFile;  QString outputDir;  QString outputProjectFile;
  OutputFormat outputFormat = OutputFormat::TEXT;
  bool verbose = false;  bool quiet = false;  bool help = false;  bool version = false;
  std::optional<int> startFilter;  std::optional<int> endFilter;
  std::optional<int> orientation;
  std::optional<page_split::LayoutType> layoutType;
  std::optional<DeskewMode> deskewMode;  std::optional<double> deskewAngle;
  std::optional<ContentDetection> contentDetection;  std::optional<MmRect> contentBox;
  std::optional<MmRect> margins;  std::optional<Alignment> alignment;
  std::optional<int> outputDpi;  std::optional<output::ColorMode> colorMode;
  std::optional<int> thresholdAdjustment;
  std::optional<output::DespeckleLevel> despeckleLevel;
  std::optional<output::DewarpingMode> dewarpingMode;  std::optional<double> depthPerception;
  bool equalizeIllumination = false;
};
```

Delete every `*Set` boolean, every `-1` sentinel, `outputMode`, `whiteMargins`,
`savitzkyGolaySmoothing`, `morphologicalSmoothing`, and `static CliRequest::parse`.

`CliUsage.h/.cpp` holds the flag table — one source of truth for parsing *and* help:

```cpp
using FlagHandler = ExitCode (*)(const QString& value, CliRequest& out, QString& errorMessage);
struct FlagSpec {
  const char* name;       // "--output-dpi"
  const char* valueHint;  // "N"; nullptr => boolean flag, must not carry '=value'
  const char* group;      // "General", "Filter selection", "fix_orientation", ...
  const char* description;
  FlagHandler handler;
};
const std::vector<FlagSpec>& flagSpecs();
const FlagSpec* findFlagSpec(const QString& name);
void printHelp();     // stdout: usage/indices/exit codes/examples preamble + table-driven flag list
void printVersion();  // stdout: VERSION and PROJECT_VERSION
```

Table rows (name, hint, group, description):

| name | hint | group | description |
|---|---|---|---|
| `--help` | – | General | Show this help and exit |
| `--version` | – | General | Show version and exit |
| `--verbose` | – | General | Detailed progress to stderr |
| `--quiet` | – | General | Suppress progress output; errors are still printed |
| `--output` | `text\|json` | General | Report format on stdout (default: text) |
| `--output-project` | `file` | General | Save a .ScanTailor project after processing |
| `--start-filter` | `N` | Filter selection | Apply flag overrides from filter N onward (does not skip earlier stages) |
| `--end-filter` | `N` | Filter selection | Stop the pipeline after filter N; later stages are skipped |
| `--orientation` | `deg` | fix_orientation | Rotate: 0, 90, 180, 270 |
| `--layout` | `n` | page_split | Page layout: 0=auto, 1=single, 2=1+offcut, 3=two |
| `--deskew` | `mode` | deskew | Deskew: auto, manual, off |
| `--deskew-angle` | `deg` | deskew | Manual deskew angle (requires --deskew=manual) |
| `--content-detection` | `mode` | select_content | Content detection: auto, manual, off |
| `--content-box` | `l,t,r,b` | select_content | Manual content box in mm (requires --content-detection=manual) |
| `--margins` | `l,t,r,b` | page_layout | Hard margins in mm |
| `--alignment` | `mode` | page_layout | Alignment: center, original, auto |
| `--output-dpi` | `N` | output | Output DPI (>= 72) |
| `--color-mode` | `mode` | output | Color mode: bw, color, mixed |
| `--threshold` | `N` | output | Binarization threshold adjustment (0-255, default 0) |
| `--despeckle` | `level` | output | Despeckle: off, cautious, normal, aggressive |
| `--dewarping` | `mode` | output | Dewarping: off, auto, manual, marginal |
| `--depth-perception` | `val` | output | Dewarping depth perception (1.0-3.0) |
| `--equalize-illumination` | – | output | Normalize illumination before binarization |

Handlers are `static` free functions in `CliUsage.cpp`, one per row, each validating its own value
and setting the corresponding `std::optional` (or bool). `--output` accepts `text` and `json` only,
error message `"--output requires 'text' or 'json'"` (defect 6). `--start-filter`/`--end-filter`
validate `0 <= N < kFilterCount`. `--white-margins` has no row (defect 7).

`CliParser.h/.cpp`:

```cpp
struct ParseResult { CliRequest request; CliError error; };
ParseResult parseCommandLine(const QStringList& args);
```

Grammar: any argument starting with `-` (except a bare `-`) is a flag, split at the first `=`.
Errors, all `INVALID_INPUT`: unknown name → `"Unknown flag: <name>"`; missing value →
`"--<name> requires a value (e.g. --<name>=<hint>)"`; value on a boolean flag →
`"--<name> takes no value"`; then the handler's own error. Positional resolution is unchanged
(1 positional ending in `.ScanTailor` case-insensitively → project; ≥2 with a project first →
project + output dir, >2 → `"Unexpected arguments after output directory in project mode: <arg>"`;
≥2 otherwise → last is the output dir; otherwise the existing "No input files" / "Expected images
and output directory" messages). `--help`/`--version` short-circuit the positional requirement.
After positional resolution reject these combinations (defect 8):
`quiet && verbose` → `"--quiet and --verbose are mutually exclusive"`;
`contentDetection == MANUAL && !contentBox` → `"--content-detection=manual requires --content-box=l,t,r,b"`;
`deskewAngle && deskewMode != MANUAL` → `"--deskew-angle requires --deskew=manual"`.

`FilterConfigAdapter` field accesses become optional-driven (`if (m_request.outputDpi)`,
`m_request.despeckleLevel.value_or(...)` never needed), and gain the illumination fix: when
`equalizeIllumination` is set, take `ColorParams`, set `setNormalizeIllumination(true)` on both
`blackWhiteOptions()` and `colorCommonOptions()`, write them back, and `params.setColorParams(...)`.

`main-cli.cpp` becomes:

```cpp
qputenv guard; Application app(argc, argv); QApplication::setApplicationName("scantailor-advanced-cli");
const cli::ParseResult parsed = cli::parseCommandLine(QApplication::arguments().mid(1));
if (!parsed.error.ok()) { cli::ConsoleLog::writeError(parsed.error.message);
  QTextStream(stderr) << "Use --help for usage information.\n"; return (int)parsed.error.code; }
const cli::CliRequest& request = parsed.request;
if (request.help) { cli::printHelp(); return 0; }
if (request.version) { cli::printVersion(); return 0; }
const cli::ConsoleLog log(request);
cli::CliError error;
const auto session = cli::PipelineSession::open(request, error);
if (!session) { cli::ConsoleLog::writeError(error.message); return (int)error.code; }
const auto range = cli::resolveFilterRange(request, session->stages()->count(), error);
if (!range) { cli::ConsoleLog::writeError(error.message); return (int)error.code; }
session->configure(*range);
const cli::BatchReport report = session->process(*range, log);
if (!request.outputProjectFile.isEmpty() && report.failed() == 0) {
  if (!session->saveProject(request.outputProjectFile, error)) {
    log.error("output", error.message); return (int)error.code; } }
if (request.outputFormat == cli::OutputFormat::JSON) {
  QTextStream(stdout) << cli::renderJsonReport(report); }
log.progress("process", QString("Complete. %1 succeeded, %2 failed.").arg(report.processed()).arg(report.failed()));
return (int)cli::exitCodeFor(report);
```

Wrap the body after application construction in `try/catch (const std::exception&)` →
`log.writeError` + `INTERNAL_ERROR`, and `catch (...)` → `INTERNAL_ERROR`, so exception 1 stays
reserved as `ExitCode.h` documents.

Update `openspec/changes/add-headless-cli/specs/cli-batch-processing/spec.md` in the same step:
grammar is `--flag=value` only (fix the `--output json` example), `--start-filter` applies overrides
and does not skip stages, and the help text of `--threshold` describes an adjustment.

### 9. Tests

New `src/qt_tests/tst_cli_qt.h/.cpp` (`class TestCliQt : public QObject`), registered in
`src/qt_tests/main.cpp` next to the existing two suites, with these slots and assertions:

- `parsesDefaults` — `parseCommandLine({"--output-dpi=300", "a.png", "/tmp/out"})`: `ok`,
  `outputDpi == 300`, `images == {"a.png"}`, `outputDir == "/tmp/out"`, every other option
  disengaged, `outputFormat == TEXT`.
- `parsesValueFlags` — `--margins=1,2,3,4` → `margins->right == 3`; `--color-mode=mixed` →
  `colorMode == output::MIXED`; `--deskew=manual --deskew-angle=1.5` accepted.
- `rejectsUnknownFlag` — `{--foo=bar}`: `!ok`, `code == INVALID_INPUT`, message contains `--foo`.
- `rejectsMalformedValues` — `--output-dpi` (no value), `--verbose=1`, `--layout=7`,
  `--margins=1,2,3`, `--output=yaml`, `--start-filter=6`: each `INVALID_INPUT`.
- `rejectsConflictingOptions` — quiet+verbose, manual content detection without a box,
  `--deskew-angle` without `--deskew=manual`.
- `resolvesPositionals` — `{"p.ScanTailor"}` → `projectFile == "p.ScanTailor"`, `outputDir` empty;
  `{"p.ScanTailor", "/o"}` → both set; `{"p.ScanTailor", "/o", "x"}` → `INVALID_INPUT`;
  `{"a.png", "b.png", "/o"}` → two images; `{"a.png"}` → `INVALID_INPUT`.
- `flagTableIsWellFormed` — for every entry of `flagSpecs()`: `name` starts with `"--"`,
  `description` and `group` are non-empty, `description` contains no newline, `handler != nullptr`,
  `findFlagSpec(name) == &spec`; descriptions are unique across the table;
  `findFlagSpec("--white-margins") == nullptr`.
- `resolvesFilterRange` — `--end-filter=4` → `{0,4}`; `--start-filter=2` → `{2,5}`;
  `--start-filter=4 --end-filter=3` → `nullopt` with `INVALID_INPUT`; `--end-filter=9` → `nullopt`.
- `rendersReport` — report with one ok page (`outputFile == "/o/a.tif"`) and one failed page
  (`"boom"`): parse the output with `QJsonDocument::fromJson`; assert `status == "partial"`,
  `pages_processed == 1`, `pages_failed == 1`, `output_files[0].page_id` is a number equal to 0,
  `output_files[0].output_file == "/o/a.tif"`, `errors[0].filter.isNull()`,
  `errors[0].message == "boom"`. Second case: ok page with empty `outputFile` → `output_file` is
  `null`.
- `mapsExitCode` — zero failures → `SUCCESS`; one failure → `FILTER_ERROR`.

## Critical files & anchors

- `src/app_cli/main-cli.cpp:154-232` — current `main`, including the uncommitted `Application`
  switch; the whole file is rewritten in step 8.
- `src/app_cli/ConsoleBatch.cpp:49-67` — `createCompositeTask`, the exact chain-building code to
  preserve verbatim inside `PipelineSession`.
- `src/app_cli/ConsoleBatch.cpp:80-228` — `process()`, source of the JSON block, the dead exit-code
  branch and the log calls.
- `src/app/MainWindow.cpp:374-404` — `switchToNewProject`, the canonical wiring order (disambiguator
  choice → `OutputFileNameGenerator` → register pages → stages → read settings) that
  `PipelineSession::open` must reproduce.
- `src/app_cli/FilterConfigAdapter.cpp:105-160` — `applyFixOrientation`/`applyDeskew`/
  `applySelectContent`; the `settings` accessors and `Params` constructors used here are the API the
  adapter must keep calling unchanged.

## Verification

Prerequisites: repo root, Qt5 present (`build/CMakeCache.txt` shows `Qt5`, `Qt6_DIR-NOTFOUND`), and a
test-enabled build directory. `build/` is configured with `BUILD_TESTS=OFF`, so create a second one:

```bash
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build-tests -j"$(nproc)" --target qt_tests scantailor-advanced-cli
./build-tests/qt_tests TestCliQt
```

Expected: every `TestCliQt` slot passes, exit 0.

1. **Headless proof (the fix of defect 1).** Baseline already observed: exit 134.
   ```bash
   mkdir -p /tmp/st-out
   env -u DISPLAY -u WAYLAND_DISPLAY -u QT_QPA_PLATFORM \
     ./build-tests/scantailor-advanced-cli --output=json \
     src/resources/scantailor-advanced.png /tmp/st-out; echo "exit=$?"
   ```
   Expected: `exit=0`; stdout is one JSON object with `"status": "ok"` and `"pages_processed": 1`;
   `ls /tmp/st-out` shows the produced `.tif`; no `xcb`/`wayland` error on stderr.
2. **Flag surface.**
   ```bash
   ./build-tests/scantailor-advanced-cli --output=yaml src/resources/scantailor-advanced.png /tmp/st-out   # exit 2, "text" or "json"
   ./build-tests/scantailor-advanced-cli --white-margins src/resources/scantailor-advanced.png /tmp/st-out # exit 2, Unknown flag
   ./build-tests/scantailor-advanced-cli --quiet --verbose src/resources/scantailor-advanced.png /tmp/st-out # exit 2
   ./build-tests/scantailor-advanced-cli --help | grep -c '\-\-white-margins'   # 0
   ./build-tests/scantailor-advanced-cli --help | grep -F -- '--threshold'      # shows "adjustment (0-255, default 0)"
   ```
3. **Truncated pipeline reports no output file.**
   ```bash
   ./build-tests/scantailor-advanced-cli --end-filter=4 --output=json src/resources/scantailor-advanced.png /tmp/st-trunc
   ```
   Expected: exit 0, JSON `output_files[0].output_file` is `null`, and `/tmp/st-trunc` contains no
   `.tif`.
4. **Same-basename outputs no longer collide (defect 3).**
   ```bash
   mkdir -p /tmp/st-dup/a /tmp/st-dup/b
   cp src/resources/scantailor-advanced.png /tmp/st-dup/a/page.png
   cp src/resources/scantailor-advanced.png /tmp/st-dup/b/page.png
   ./build-tests/scantailor-advanced-cli /tmp/st-dup/a/page.png /tmp/st-dup/b/page.png /tmp/st-dup/out
   ls /tmp/st-dup/out
   ```
   Expected: exactly two `.tif` files, named `page.tif` and `page(1).tif` (label 0 then 1, per
   `FileNameDisambiguator::registerFile` + `OutputFileNameGenerator::fileNameFor`). Before the change
   only `page.tif` exists.
5. **Illumination flag now reaches the filter.**
   ```bash
   ./build-tests/scantailor-advanced-cli --equalize-illumination --output-project=/tmp/ill.ScanTailor \
     src/resources/scantailor-advanced.png /tmp/st-ill
   grep -c 'normalizeIlluminationBW="1"' /tmp/ill.ScanTailor
   ```
   Expected: ≥ 1. (Run the same command once without the flag on a fresh project: the attribute is
   `"0"`, so the check is not vacuous.)
6. **Project round-trip and project-mode naming.**
   ```bash
   ./build-tests/scantailor-advanced-cli --output-dpi=300 --output-project=/tmp/rt.ScanTailor \
     src/resources/scantailor-advanced.png /tmp/st-rt
   grep -o 'dpi="[0-9]*"' /tmp/rt.ScanTailor | head -1        # 300
   ./build-tests/scantailor-advanced-cli --output=json /tmp/rt.ScanTailor /tmp/st-rt2; echo "exit=$?"
   ./build-tests/scantailor-advanced-cli --output=json /tmp/does-not-exist.ScanTailor; echo "exit=$?"  # 3
   ```
   Expected: 300; second run exit 0 with the page reported; missing project → exit 3.
7. **GUI regression** (the GUI shares `core` but no CLI file): `cmake --build build -j"$(nproc)"`,
   launch `./build/scantailor-advanced`, open `src/resources/scantailor-advanced.png` as a page in a
   new project, confirm the filter list and the Output options widget still render.

## Assumptions & contingencies

- **`--start-filter` semantics are documented, not implemented as a pipeline start.** Honouring it
  would need cached stage inputs, which the CLI does not have (it always starts from
  `LoadFileTask`). If a true mid-pipeline start is required later, it is a separate change built on
  cache-driven tasks, not on this restructure.
- **`--white-margins` is removed, not renamed.** Its only effect today is
  `setNormalizeIllumination`, i.e. exactly `--equalize-illumination`; keeping both names would keep
  two flags for one setting. If a user script depends on the old spelling, the fallback is to keep a
  deprecated alias row in `flagSpecs()` that maps to the same handler and prints a stderr warning —
  do that only if such a caller is found (grep the repo and Docker docs first; none exists today).
- **Exit codes stay as they are**: any page failure yields `FILTER_ERROR` (4) even when some pages
  succeeded. If the run should distinguish partial success, that needs a new code in `ExitCode` and
  the spec; not part of this change.
- **Project mode with an empty `outputDirectory` attribute** falls back to the project file's
  directory. If evaluation shows real projects always carry the attribute, the fallback is harmless
  dead code; do not remove it, `ProjectReader` can legitimately leave it empty for a project saved
  without an output directory.
- **`core::Application` stays** because `DefaultParamsProfileManager` casts `qApp` to `Application`
  and 6 filter constructors build `OptionsWidget`s eagerly. Removing that coupling means editing
  core (lazy widget creation + a portable-path accessor) and switching the CLI to
  `QGuiApplication`; explicitly out of scope here.
