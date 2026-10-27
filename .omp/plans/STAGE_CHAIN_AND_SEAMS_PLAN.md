# Stage chain and seams

## Context

Five decisions are each duplicated or owned by the wrong layer: the page-processing chain is assembled twice (a 43-line ladder in `MainWindow`, a 38-line ladder in `PipelineSession`); every filter builds its `OptionsWidget` inside its constructor, so a stage list cannot exist without a widget tree; the configuration root is obtained by casting `qApp` to `Application`, and the CLI sets a different application name so it reads a different profile directory; deskew's automatic-oblique precedence lives inside `Task::process` while its profile write lives inside a widget; and four `ApplyDialog` classes repeat the same page-scope logic.

The end state gives each of those exactly one owner: `StageChain` builds chains for GUI and CLI alike, options widgets are created on first UI use, `ConfigPaths` owns the config root (CLI runs on `QGuiApplication`), `resolveAutoOblique()` plus a provider setter own the oblique policy, and one `PageApplyDialog` serves all four filters.

Build baseline before starting: `cmake --build build-tests -j8` is green (verified 2026-10-27, warnings only).

## Approach

### Step 1 — Options widgets become optional

For each of the six filters: `deskew`, `fix_orientation`, `output`, `select_content`, `page_split`, `page_layout`.

**1.1 Header.** In `src/core/filters/<filter>/Filter.h` add `#include "PageSelectionAccessor.h"` (the member needs a complete type; that header pulls only `PageId.h`, `PageRange.h`, `PageSelectionProvider.h` and the STL, so it cannot close a cycle with `Filter.h`) and add the member next to the existing `SafeDeletingQObjectPtr<OptionsWidget> m_optionsWidget;`:

```cpp
PageSelectionAccessor m_pageSelectionAccessor;
```

**1.2 Constructor.** Delete the `m_optionsWidget.reset(new OptionsWidget(...));` line and initialize the accessor in the init list instead, keeping the existing `m_settings` initialization. Current shapes, verified in `src/core/filters/*/Filter.cpp` (lines 23–30 of each):

| filter | ctor | deleted line |
|---|---|---|
| deskew | `Filter(const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, pageSelectionAccessor)` |
| fix_orientation | `Filter(const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, pageSelectionAccessor)` |
| output | `Filter(const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, pageSelectionAccessor)` |
| select_content | `Filter(const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, pageSelectionAccessor)` |
| page_split | `Filter(std::shared_ptr<ProjectPages>, const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, m_pages, pageSelectionAccessor)` |
| page_layout | `Filter(std::shared_ptr<ProjectPages>, const PageSelectionAccessor&)` | `new OptionsWidget(m_settings, m_pages, pageSelectionAccessor)` |

**1.3 Creation point.** Add a private `OptionsWidget* ensureOptionsWidget();` to each `Filter.h` and define it in the `.cpp` (`page_split` and `page_layout` pass `m_pages` as second argument):

```cpp
OptionsWidget* Filter::ensureOptionsWidget() {
  if (!m_optionsWidget) {
    m_optionsWidget.reset(new OptionsWidget(m_settings, m_pageSelectionAccessor));
  }
  return m_optionsWidget.get();
}
```

Rewrite each `preUpdateUI` to create-then-update. `fix_orientation`'s `if (m_optionsWidget.get())` guard is deleted — the panel must appear on every `preUpdateUI`:

```cpp
void Filter::preUpdateUI(FilterUiInterface* ui, const PageInfo& pageInfo) {
  OptionsWidget* const widget = ensureOptionsWidget();
  widget->preUpdateUI(/* the arguments that preUpdateUI call passes today */);
  ui->setOptionsWidget(widget, ui->KEEP_OWNERSHIP);
}
```

**1.4 Keep `optionsWidget()` non-creating.** It keeps returning `m_optionsWidget.get()`, which is now nullable. Every dereference in a task must be guarded. Find them with `grep -n 'optionsWidget()' src/core/filters/*/Task.cpp`; the sites verified today are `deskew/Task.cpp:318-320`, `fix_orientation/Task.cpp:116-118`, `output/Task.cpp:499-501`, and the same pattern exists in the `UiUpdater` of the other three filters' `Task.cpp`. Convert each to:

```cpp
if (OptionsWidget* const widget = m_filter->optionsWidget()) {
  widget->postUpdateUI(/* same arguments */);
  ui->setOptionsWidget(widget, ui->KEEP_OWNERSHIP);
}
```

Rationale for the guard: the GUI always creates the widget via `preUpdateUI` (`MainWindow.cpp:1708`) before any task of that filter runs, so the panel keeps updating; a headless run creates nothing. Do not add a "UI enabled" flag or a second getter — the null pointer is the signal.

Check after the step: `cmake --build build-tests -j8 && (cd build-tests && ctest --output-on-failure)` is green. Existing `qt_tests` cover the widgets; nothing may be skipped.

### Step 2 — `StageChain` owns the chain order

**2.1 New module** `src/core/StageChain.h` / `src/core/StageChain.cpp`, added to the `core` sources in `src/core/CMakeLists.txt` (place next to `StageSequence.cpp`). Reference `StageSequence` by `shared_ptr`, never by reference: `MainWindow` replaces `m_stages` on every project load and a stale reference would dangle.

```cpp
#ifndef SCANTAILOR_CORE_STAGECHAIN_H_
#define SCANTAILOR_CORE_STAGECHAIN_H_

#include <memory>

#include "NonCopyable.h"
#include "OutputFileNameGenerator.h"

class BackgroundTask;
class CompositeCacheDrivenTask;
class PageInfo;
class ProjectPages;
class StageSequence;
class ThumbnailPixmapCache;

/** Builds the per-page processing chain: LoadFileTask followed by the enabled stages, in stage order. */
class StageChain {
  DECLARE_NON_COPYABLE(StageChain)

 public:
  /** Everything a chain needs beyond the page itself. */
  struct Deps {
    std::shared_ptr<ThumbnailPixmapCache> thumbnailCache;  // null is fine for cache-driven chains
    std::shared_ptr<ProjectPages> pages;
    OutputFileNameGenerator outFileNameGen;
    bool batch = false;
    bool debug = false;
  };

  StageChain(const std::shared_ptr<StageSequence>& stages, const Deps& deps);

  /** Chain that starts at LoadFileTask and stops after stage \p endStage (clamped to the last stage). */
  std::shared_ptr<BackgroundTask> taskFor(const PageInfo& page, int endStage) const;

  /** Cache-driven counterpart used by thumbnails and page propagators. */
  std::shared_ptr<CompositeCacheDrivenTask> cacheDrivenTaskFor(int endStage) const;

 private:
  std::shared_ptr<StageSequence> m_stages;
  Deps m_deps;
};
#endif
```

**2.2 Body.** Move the two ladders verbatim, in the existing order `output → page_layout → select_content → page_split → deskew → fix_orientation → LoadFileTask` and the cache-driven order `output → page_layout → select_content → page_split → deskew → fix_orientation`, so the produced chain is byte-for-byte what `MainWindow::createCompositeTask` (`MainWindow.cpp:2059-2101`) and `MainWindow::createCompositeCacheDrivenTask` (`:2103-2132`) build today. Concretely:

- Start from the ladder text; replace `m_thumbnailCache`/`m_pages`/`m_outFileNameGen` with `m_deps.thumbnailCache`/`m_deps.pages`/`m_deps.outFileNameGen`.
- Keep the `bool debug` local and the `if (batch) debug = false;` behaviour (`const bool debug = m_deps.debug && !m_deps.batch;` is equivalent, since every stage call receives the same local).
- Keep the per-stage conditions and the argument lists exactly: `outputFilter()->createTask(page.id(), m_deps.thumbnailCache, m_deps.outFileNameGen, batch, debug)`, `pageLayoutFilter()->createTask(page.id(), outputTask, batch, debug)`, `selectContentFilter()->createTask(page.id(), pageLayoutTask, batch, debug)`, `pageSplitFilter()->createTask(page.id(), selectContentTask, batch, debug)`, `deskewFilter()->createTask(page.id(), pageSplitTask, batch, debug)`, `fixOrientationFilter()->createTask(page.id(), deskewTask, batch, debug)`; the cache-driven variants take the same arguments minus `batch`/`debug`, except `outputFilter()->createCacheDrivenTask(m_deps.outFileNameGen)`.
- `taskFor`: `const int last = std::min(endStage, m_stages->count() - 1);` computed once and used for every `last >= ...Idx()` (today the caller computes it; `MainWindow` passes `m_curFilter`, the CLI passes `range.end`), plus `assert(last >= 0)`. Then keep the final `assert(fixOrientationTask);` and `return std::make_shared<LoadFileTask>(m_deps.batch ? BackgroundTask::BATCH : BackgroundTask::INTERACTIVE, page, m_deps.thumbnailCache, m_deps.pages, fixOrientationTask);`.
- `cacheDrivenTaskFor`: return `fixOrientationTask` as `std::shared_ptr<CompositeCacheDrivenTask>` exactly as the ladder does today.

Includes needed in the `.cpp`: `LoadFileTask.h` and the six filter `Task.h` headers already listed by the current ladders.

**2.3 Cut over `MainWindow`.** Delete `createCompositeTask` (`MainWindow.cpp:2059-2101`) and `createCompositeCacheDrivenTask` (`:2103-2132`) and their declarations (`MainWindow.h:290,292`). Add to `MainWindow.h` `#include "StageChain.h"` and:

```cpp
StageChain::Deps chainDeps(bool batch = false, bool debug = false) const;
```

implemented in `MainWindow.cpp` as `return {m_thumbnailCache, m_pages, m_outFileNameGen, batch, debug};`. Replace the five call sites — no others exist (`grep -n 'createCompositeCacheDrivenTask\|createCompositeTask' src/app/MainWindow.cpp`):

| line | new expression |
|---|---|
| 422 | `StageChain(m_stages, chainDeps()).cacheDrivenTaskFor(m_stages->selectContentFilterIdx())` |
| 425 | `StageChain(m_stages, chainDeps()).cacheDrivenTaskFor(m_stages->fixOrientationFilterIdx())` |
| 651 | `const std::shared_ptr<CompositeCacheDrivenTask> task(StageChain(m_stages, chainDeps()).cacheDrivenTaskFor(m_curFilter));` |
| 1235 | `createCompositeTask(page, m_curFilter, /*batch=*/true, m_debug)` → `StageChain(m_stages, chainDeps(true, m_debug)).taskFor(page, m_curFilter)` |
| 1714 | `createCompositeTask(page, m_curFilter, false, m_debug)` → `StageChain(m_stages, chainDeps(false, m_debug)).taskFor(page, m_curFilter)` |

`m_outFileNameGen` is a value member (`MainWindow.cpp:395` assigns a temporary into it) and `OutputFileNameGenerator` documents member-wise copying, so copying it into `Deps` is safe. At lines 422/425 `m_thumbnailCache` is still null (it is created at 430) — `cacheDrivenTaskFor` never reads it.

**2.4 Cut over `PipelineSession`.** Delete `PipelineSession::createCompositeTask` (`PipelineSession.cpp:245-282`) and its declaration (`PipelineSession.h:65`). In the caller (`PipelineSession.cpp:300`) build the chain once and reuse it:

```cpp
const std::shared_ptr<StageSequence> stages = /* the existing member */;
const StageChain chain(stages, StageChain::Deps{/* thumbnail cache member */, m_pages, /* out file name gen member */, true, false});
...
const std::shared_ptr<BackgroundTask> task = chain.taskFor(page, range.end);
```

Take the three `Deps` values from the members the deleted ladder used; keep `batch = true, debug = false` because the CLI is always batch. `range.start` keeps being ignored, as documented at `PipelineSession.cpp:247-249`.

**2.5 Core test** `src/core/tests/TestStageChain.cpp`, added to the `sources` list in `src/core/tests/CMakeLists.txt`:

```cpp
BOOST_AUTO_TEST_CASE(chain_builds_without_widgets) { ... }
```

- `core_tests` has no `QApplication` (`src/core/tests/main.cpp` is `BOOST_AUTO_TEST_MAIN`), so a green run is the proof that the stage graph and its widgets are decoupled.
- Fixture: one `ImageFileInfo(QFileInfo("/nonexistent/page.png"), {})` in a `std::vector<ImageFileInfo>`, a `std::shared_ptr<ProjectPages>` built with `ProjectPages(files, ProjectPages::ONE_PAGE, Qt::LeftToRight)` (`ONE_PAGE` avoids the metadata-driven auto-detection; if that constructor still reads image metadata, fall back to `ProjectPages(Qt::LeftToRight)` plus `setLayoutTypeForAllPages`), a stub `PageSelectionProvider` (copy the three overrides from `src/app_cli/CliPageSelectionProvider.h`, return empty values), a `PageSelectionAccessor` wrapping it, and `std::make_shared<StageSequence>(pages, accessor)`.
- `Deps`: null `thumbnailCache`, the same `pages`, default-constructed `OutputFileNameGenerator`, `batch = false`.
- Assertions: `stages->count() == 6`; `chain.taskFor(page, stages->count() - 1)` is non-null and `dynamic_cast<LoadFileTask*>` on it is non-null; `chain.taskFor(page, 3)` is non-null; `chain.cacheDrivenTaskFor(stages->count() - 1)` is non-null. Nothing deeper is assertable — the rungs are private — and the point of the case is construction without a GUI.

### Step 3 — `ConfigPaths` owns the configuration root

**3.1 New module** `src/core/ConfigPaths.h` / `.cpp` (add to `src/core/CMakeLists.txt`):

```cpp
/** Where this process keeps profiles and settings; set once, before any filter or settings provider is built. */
class ConfigPaths {
 public:
  /** Probes \p applicationDirPath for the portable config directory; returns an empty string when not portable. */
  static QString resolvePortableRoot(const QString& applicationDirPath);
  /** Empty clears the portable root. */
  static void setPortableRoot(const QString& root);
  static bool isPortable();
  /** Portable root when set, else QStandardPaths::writableLocation(QStandardPaths::AppDataLocation). */
  static QString configRoot();
  static QString profilesDir();  // configRoot() + "/profiles"
  /** QSettings::IniFormat for both scopes, and the portable root as the UserScope path when set. */
  static void configureQSettings();
};
```

`resolvePortableRoot` moves the body of `Application::initPortableVersion` (`src/core/Application.cpp:84-95`) verbatim: return empty when `PORTABLE_CONFIG_DIR` is empty; otherwise probe `applicationDirPath + '/' + PORTABLE_CONFIG_DIR` with `QTemporaryDir(...).isValid()`, or `mkpath(".")` when it does not exist yet. `setPortableRoot`/`isPortable`/`configRoot` use a file-static `QString`.

`configureQSettings` is the body currently inlined in `src/app/main.cpp:51-56`: `QSettings::setDefaultFormat(QSettings::IniFormat)` and, when portable, `QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configRoot())`. Delete the unused `QSettings settings;` local at `main.cpp:57`.

**3.2 Point the profile manager at it.** In `src/core/DefaultParamsProfileManager.cpp` replace `static_cast<Application*>(qApp)` usage (line 20) with `ConfigPaths::profilesDir()`, drop the `Application.h` include, and delete the `qApp`/`QApplication` casts left over. `DefaultParamsProfileManager` must not include `Application.h` afterwards: `grep -n 'Application.h\|qApp' src/core/DefaultParamsProfileManager.cpp` returns nothing.

**3.3 `Application` delegates.** `Application::initPortableVersion()` calls `ConfigPaths::setPortableRoot(ConfigPaths::resolvePortableRoot(applicationDirPath()))`; `isPortableVersion()` returns `ConfigPaths::isPortable()`; `getPortableConfigPath()` returns `ConfigPaths::configRoot()` when portable (keep the member in sync or drop it — the only reader is `main.cpp`, which step 3.4 removes).

**3.4 Entry points.** `src/app/main.cpp` lines 51-56 become `ConfigPaths::configureQSettings();` (order stays: identity set at 47-49, then configuration). `src/app_cli/main-cli.cpp`: replace `Application app(argc, argv);` with

```cpp
QGuiApplication app(argc, argv);
QGuiApplication::setApplicationName(APPLICATION_NAME);
QGuiApplication::setOrganizationName(ORGANIZATION_NAME);
ConfigPaths::setPortableRoot(ConfigPaths::resolvePortableRoot(QGuiApplication::applicationDirPath()));
ConfigPaths::configureQSettings();
```

and drop `#include "Application.h"` (add `#include "ConfigPaths.h"` and `"config.h"` for `APPLICATION_NAME`/`ORGANIZATION_NAME`; the `QT_QPA_PLATFORM=offscreen` default at lines 27-30 stays). `QGuiApplication` is enough: after step 1 nothing in the CLI processing path constructs a widget, while thumbnails still need `QPixmap`.

Consequence to accept: the CLI now reads the GUI's profiles and `QSettings` (same application name), so `DefaultParamsProvider` finds the user's active profile instead of falling back to the built-in one. That is the intended fix; do not add a CLI-specific name back.

**3.5 Test** `src/core/tests/TestConfigPaths.cpp` (add to the same `sources` list): `setPortableRoot("/tmp/st-test")` → `isPortable()` true, `configRoot() == "/tmp/st-test"`, `profilesDir() == "/tmp/st-test/profiles"`; `setPortableRoot(QString())` → `isPortable()` false and `profilesDir()` ends with `/profiles`. No QSettings call in the test.

### Step 4 — Deskew's oblique policy gets one home

**4.1** New `src/core/filters/deskew/ObliquePolicy.h` / `.cpp` (add to `src/core/filters/deskew/CMakeLists.txt`):

```cpp
namespace deskew {
/** Auto-oblique for a page without params yet: an explicit request wins, otherwise the profile default. */
bool resolveAutoOblique(std::optional<bool> pageRequest, bool profileDefault);
}
```

Implementation: `return pageRequest.value_or(profileDefault);`.

**4.2** `deskew::Task::process` keeps its prior-params branch and only delegates the other one. Today's logic reads: if the page has params already, `autoOblique` comes from those params; otherwise from `m_settings->takePendingAutoOblique(m_pageId)` falling back to the profile default. Replace that expression with

```cpp
const bool autoObliqueEnabled
    = priorParamsBeforeRecompute
          ? priorParamsBeforeRecompute->autoOblique()
          : resolveAutoOblique(m_settings->takePendingAutoOblique(m_pageId),
                               DefaultParamsProvider::getInstance().getParams().getDeskewParams().isAutoOblique());
```

The `takePendingAutoOblique` call must stay inside the `priorParamsBeforeRecompute == nullptr` branch — it consumes the request, and consuming it on a page that already has params would change behaviour.

**4.3** `DefaultParamsProvider` gains `void setDeskewAutoOblique(bool enabled);` (header + `.cpp`), holding the body currently in the anonymous namespace of `src/core/filters/deskew/OptionsWidget.cpp` that copies `DefaultParams`, sets `DeskewParams::setAutoOblique`, and calls `setParams(params, getProfileName())`. `OptionsWidget` calls the new method and loses that helper.

**4.4** Test `src/core/tests/TestObliquePolicy.cpp` (add to `sources`): request `true` with profile default `false` → `true`; request `false` with profile default `true` → `false`; no request with profile default `true` → `true`; no request with profile default `false` → `false`.

### Step 5 — CLI flag application stops repeating itself

In `src/app_cli/FilterConfigAdapter.cpp`:

- `applyOutput()` (the applier at `:178`) currently runs its page loop even when no output flag is present, round-tripping `getParams`/`setParams` for every page. Insert an early return at the top of the function: if none of the flags that the function's own blocks test is present in `m_request`, `return;`. Compose the condition from exactly the flags used inside the function — no new flag, no new member.
- `applyFixOrientation()` and `applySelectContent()` each build the same `std::set<PageId>` from the request's page selection. Extract that into a private `std::set<PageId> selectedPageIds() const` in `FilterConfigAdapter.h` and call it from both; the bodies must produce the identical set for every request shape (the CLI end-to-end run in Verification is the check).
- Leave the six `if (idx >= start && idx <= end)` guards in `apply()` and the per-flag applier split as they are. Do not introduce a per-flag record or a declarative apply plan: the project's `openspec` design record already rejected a flag god-object, and a record layer would re-open that decision without removing a real duplication.

### Step 6 — One `PageApplyDialog` for the four filters

**6.1 New module** `src/core/PageApplyDialog.h` / `.cpp`, built in code (no `.ui`; the three `.ui` files are per-filter copies of the same 7 radio buttons, 2 hint labels, an option group box and a button box, so one class replaces all of them). Add both files to `src/core/CMakeLists.txt`.

```cpp
/** Asks which pages a set of option changes applies to, and which options to apply. */
class PageApplyDialog : public QDialog {
  Q_OBJECT
 public:
  struct Option {
    QString label;
    bool checked = true;
  };

  PageApplyDialog(QWidget* parent, const PageId& curPage, const PageSelectionAccessor& pageSelectionAccessor,
                  const QString& title, const std::vector<Option>& options, const QString& hint = QString());

  /** Pages the change applies to, resolved from the checked scope. */
  std::set<PageId> pages() const;
  bool isOptionChecked(int index) const;
  /** True when the "All pages" scope was checked. */
  bool allPages() const;

 signals:
  void submitted();
};
```

Layout, matching the labels verified in the four `.ui` files (`QCoreApplication::translate("PageApplyDialog", ...)`): a scope `QGroupBox` with the radios `This page only (already applied)`, `All pages`, `This page and the following ones`, `Selected pages` (+ `Use Ctrl+Click / Shift+Click to select multiple pages.`), `Every other selected page` (+ `The current page will be included.`), the two extra "every other" scopes the four dialogs already carry, an options `QGroupBox` filled with one `QCheckBox` per `Option` in order, an optional hint `QLabel` hidden when `hint` is empty, and a `QDialogButtonBox` (Ok/Cancel). Disable the `Selected pages` pair when the accessor reports at most one selected page, as `page_layout/ApplyDialog.cpp:30-34` does.

`pages()` reproduces `page_layout/ApplyDialog::onSubmit` (`ApplyDialog.cpp:70-100`) exactly: `allPages` → `PageSequence::selectAll()`, `thisPageAndFollowers` → `selectPagePlusFollowers(m_curPage)`, `selectedPages` → the accessor's `selectedPages()`, `everyOther` → `selectEveryOther(m_curPage)`, `thisEveryOther` → `selectThisPageAndFollowingEveryOther(m_curPage)`, `everyOtherSelected` → `PageRange::selectEveryOther(m_curPage)` over the single accessor range (keep the `assert(ranges.size() == 1)`), and the "this page only" scope yields an empty set. The Ok button calls `accept()`; nothing is emitted before that, so a caller reads `pages()` after `exec() == QDialog::Accepted`.

**6.2 Replace the four dialogs.** Delete `ApplyDialog.h`, `ApplyDialog.cpp` and `ApplyDialog.ui` under `src/core/filters/{deskew,fix_orientation,page_layout,select_content}/` and remove their entries from the four filter `CMakeLists.txt` files (`grep -n ApplyDialog src/core/filters/*/CMakeLists.txt`). In each `OptionsWidget`:

| filter | dialog title | options | merge on accept |
|---|---|---|---|
| deskew | `Apply parameters` | `Deskew angle and mode` (on), `Oblique angle and mode` (on) | existing `applySelection` / `applySelectionForAllPages`, chosen by `allPages()` |
| fix_orientation | `Fix Orientation` | none (empty vector) | existing `applyRotation` + `applyTrim` over `pages()` |
| page_layout | `Apply to` (keep the current title) | none | existing `applyAlignment`-style slot over `pages()` |
| select_content | `Select Content` | `Apply page box`, `Apply content box`, hint `If content is cropped incorrectly, check the Split Pages step (Manual + Apply cut often works better with columns).` | existing slot with `isOptionChecked` in place of the two signal booleans |

Keep each filter's existing merge code; only the dialogs' construction and signal plumbing change. Where an old dialog distinguished "all pages" with a separate signal, replace that with `allPages()` — deskew is the only such case.

**6.3 Test** `src/qt_tests/TestPageApplyDialog.cpp` (add to the `sources` list in `src/qt_tests/CMakeLists.txt`), running offscreen: construct the dialog with a two-page `PageSequence` and no option checkboxes; check `All pages` → `pages()` has both ids and `allPages()` is true; check `This page only` → `pages()` is empty and `allPages()` false; construct with two options where the second is `checked = false` → `isOptionChecked(0)`, `!isOptionChecked(1)`. Do not `exec()`; assert on the object state a submit would read.

## Critical files & anchors

- `src/app/MainWindow.cpp:2059-2132` — both ladders being replaced; the five call sites are at 422, 425, 651, 1235, 1714.
- `src/app_cli/PipelineSession.cpp:245-282` — the CLI ladder and the `range.start` comment that explains why only `range.end` bounds the chain.
- `src/core/filters/deskew/Task.cpp` — `process()` holds the oblique precedence and the `takePendingAutoOblique` side effect.
- `src/core/DefaultParamsProfileManager.cpp:16-31` — the `qApp` cast and the built-in-profile fallback.
- `src/core/filters/page_layout/ApplyDialog.cpp:70-100` — the scope→pages mapping `PageApplyDialog::pages()` must reproduce.

## Verification

Run from `/home/rv/dev/repos/scantailor-advanced`. `build-tests/` is Debug with `BUILD_TESTS=ON`; Qt5 comes from `/usr/lib64/cmake`.

1. After each step: `cmake --build build-tests -j8 && (cd build-tests && ctest --output-on-failure)`. Existing suites must stay green with no test disabled or removed.
2. Step 2: `cd build-tests && ./src/core/tests/core_tests --run_test=StageChainTestSuite --log_level=message` passes with no `QApplication` in the process (`core_tests` never creates one — that is the point).
3. Step 3: `grep -n 'Application.h' src/app_cli/main-cli.cpp src/core/DefaultParamsProfileManager.cpp` returns nothing; `QT_QPA_PLATFORM=offscreen ./build-tests/src/app_cli/scantailor-advanced-cli --help` exits 0.
4. Step 3 end-to-end, proving the CLI actually processes without widgets: create an input image with `convert -size 300x400 xc:white /tmp/st-e2e/in.png` (if ImageMagick is missing, use any PNG already in the repository: `find . -name '*.png' -not -path './build*'`); then
   `QT_QPA_PLATFORM=offscreen ./build-tests/src/app_cli/scantailor-advanced-cli --output-dir /tmp/st-e2e/out /tmp/st-e2e/in.png` exits 0 and produces a file under `/tmp/st-e2e/out`. If no PNG can be produced from either source, stop and report the missing fixture instead of declaring the step verified.
5. Step 3 profiles: run the same command with `HOME=/tmp/st-home` and confirm exit 0 and a `/tmp/st-home/...` settings location is created, i.e. the CLI resolves its paths through `ConfigPaths` rather than a hardcoded location.
6. Step 6: `cd build-tests && ./src/qt_tests/qt_tests --run_test=PageApplyDialogTestSuite --log_level=message`, then the manual pass below.
7. Manual GUI pass (`./build-tests/src/app/scantailor-advanced`, a project with at least two images): for each of the six stages open the options panel and confirm it populates; open the Apply dialog in deskew, fix orientation, page layout and select content and confirm the checkboxes, the hint line, the four scope options and that Ok applies to the current page and to all pages. Widget layout is the one thing the automated pass cannot observe, so this run is the acceptance for step 6.
8. Batch mode inside the GUI: run "Apply to all pages" for one stage and confirm thumbnails and the options panel update (this exercises the null-guard of step 1.4 in the place where the widget exists).

## Assumptions & contingencies

- `PageSelectionProvider` subclasses in `core_tests` are needed because the type is abstract. If copying `CliPageSelectionProvider`'s overrides into a test stub turns out to require more than empty returns, build the accessor from `std::make_shared<PageSelectionProvider>`-free path is impossible — in that case move the fixture into `src/qt_tests/` (which already links `app_cli_lib` and has an application object) and keep the assertion set unchanged; the headless-widget claim is then proven by step 4's CLI run instead of by `core_tests`.
- If `PageSelectionAccessor.h` cannot be included from a `Filter.h` without a cycle (unexpected: it only pulls `PageId`, `PageRange`, `PageSelectionProvider`), store the accessor as `std::shared_ptr<PageSelectionAccessor>` built in the constructor and keep `ensureOptionsWidget()` unchanged otherwise.
- If `ProjectPages` with `ImageFileInfo` and `{}` metadata aborts in `ONE_PAGE` mode, use the layout-direction constructor plus `setLayoutTypeForAllPages(ProjectPages::ONE_PAGE_LAYOUT)`.
- Translation contexts change for the four apply dialogs (their strings move to `PageApplyDialog`). Do not chase the `.ts` files for this change; the build does not fail on untranslated strings.
- `openspec/` is tracked even though it is listed in `.gitignore`; no change to it is part of this work.
