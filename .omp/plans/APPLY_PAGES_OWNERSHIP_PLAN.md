# Apply-to-pages: stage-local ownership without behavior changes

## Context
Deepen the architecture-review candidate “Give apply to pages one owner per stage” across all six stages. The user chose strict preservation of existing behavior, including differences between GUI copy operations and CLI overrides. Put stage-local mutation rules behind each existing Settings module's interface; leave page-selection resolution, dialogs, notifications and cross-stage orchestration in the current adapters. Do not introduce a universal virtual apply interface, a command bus or another project/session module.

## Approach

### 1. Establish behavioral checkpoints before moving rules
From the repository root, use the existing Qt5 `build-tests` configuration. Build the current applications and tests, run the existing suites, and capture the CLI smoke described in Verification before editing production code. Keep the before/after smoke artifacts outside the repository. A red baseline is an existing failure to report and separate from this refactor, not permission to weaken tests.

For each subsequent stage, add behavioral checks through its Settings interface before migrating its caller. Extend the tests as the new command is introduced, then migrate the adapter and verify the same outcomes. Do not create tests that repeat a merge expression in the test instead of executing the production command.

Reuse existing Settings methods and value types. All new enums and structs below are nested public members of the relevant Settings class, declared in its existing `Settings.h`; new command definitions belong in that stage's existing `Settings.cpp`. These files have no equivalent commands except the explicitly reused methods listed below. Keep snapshots free of OptionsWidget/UiData/QWidget dependencies. Factor stage-private helpers in the same translation unit when needed to keep functions short; do not expose helpers solely for tests.

New commands return `void`; use existing getters to observe results. Empty target sets do not create/change records. Keep existing single-page setters for Tasks, project loading and interactive edits: they are not obsolete aliases. Do not change persistence formats, filter ranges, stage order, flag parsing/ranges, profile defaults, or the immediate-edit slots unrelated to bulk application.

Preserve existing locking mechanisms. For new deskew/content merge loops, hold the stage mutex and use its internal map and deviation bookkeeping without calling public lock-taking getters/setters recursively. For split, reuse the existing `getPageRecordLocked`/`updatePageLocked` helpers under the mutex. For layout and output commands, compose the existing lock-taking mutations without an outer lock; no stronger all-pages transaction guarantee is being introduced. Do not change existing setter locking as an unrelated refactor.

### 2. Reuse the Orientation owner and deepen Deskew's selective merge

**Orientation:** `fix_orientation::Settings::applyRotation(const std::set<PageId>&, OrthogonalRotation)` and `applyTrim(const std::set<PageId>&, const ImageTrim&)` already own bulk writes (`Settings.cpp:51-70`). Keep the two GUI apply slots calling both, and keep the CLI calling rotation only. No façade or replacement interface is necessary. Preserve ImageId-keyed behavior: selecting either split sibling affects that image's rotation/trim, not just one PageId. Applying disabled trim erases the trim record; rotation-only CLI overrides must retain trim.

**Deskew:** add these public declarations to `deskew::Settings`:

```cpp
enum class AngleSelection { Neither, Deskew, Oblique, Both };
void applyAngles(const std::set<PageId>& pages, const Params& source,
                 AngleSelection selection);
```

Move the behavior of the anonymous-namespace `mergeParamsForApply` in `deskew/OptionsWidget.cpp:18-31` into this command:
- `Neither` and an empty page set are no-ops, including no record creation or dependency changes.
- Copy angle plus mode together for each selected axis.
- Preserve both angle and mode for an unchecked axis on an existing target.
- On a missing target, an unchecked axis is angle zero with MODE_AUTO.
- Whenever at least one axis is selected, replace dependencies with the source dependencies, even if only one axis is selected. Do not clear the separate pending-auto-oblique map.
- Refresh deviation bookkeeping exactly as `setPageParams` currently does.

In both `OptionsWidget::appliedTo` and `appliedToAllPages`, retain their existing guards and notification sections. Construct one `deskew::Params` snapshot from `m_uiData`'s effective angles, dependencies and two modes, translate the two dialog booleans into `AngleSelection`, and call `applyAngles`. Delete `mergeParamsForApply` and the duplicated per-target mutation loops. The two confirmed production callers of the deleted helper are `OptionsWidget.cpp:79,99`; source/graph verification is authoritative because clangd returned an incomplete references list during planning.

In CLI `applyDeskew`, retain the current `setAlgoContentBased(mode == DeskewMode::AUTO)` mapping, including MANUAL/OFF behavior. Replace only the manual-angle per-page loop with existing `Settings::setDegrees(pages, Params(angle, Dependencies(), MODE_MANUAL))`. This deliberately replaces complete records: oblique angle becomes zero and oblique mode MODE_AUTO, unlike GUI selective copy. Do not “fix” that difference in this refactor.

### 3. Put Split's geometry adaptation behind its Settings interface
Add to `page_split::Settings`:

```cpp
enum class CutTransfer { KeepTarget, CopySource };
void applyLayout(const std::set<PageId>& pages, LayoutType layoutType,
                 const Params& source, CutTransfer transfer);
```

Move the mutation portion of `OptionsWidget::layoutTypeSet` (`OptionsWidget.cpp:207-237`) into the command. Preserve set iteration order and ImageId-keyed behavior, including processing both PageIds when split siblings appear in the set. Do not deduplicate targets by ImageId: the original loop may inspect a record updated by an earlier sibling.

For each target, build a `Settings::UpdateAction` and set its layout type. Only for non-AUTO, non-SINGLE_PAGE_UNCUT layout with CopySource, copy source Params. If target Params exist, adapt source PageLayout through existing `PageLayoutAdapter::adaptPageLayout(source.pageLayout(), targetOutline.boundingRect())`, use the adapted layout's resulting LayoutType, and preserve the target dependencies except for that layout type. If the target has no Params, retain source Params unchanged. Apply through `updatePageLocked` so existing layout/Params conflict removal remains intact. AUTO and KeepTarget paths must retain the original conflict behavior rather than copying Params.

Keep each returned Record in a named local for the lifetime of any `params()` pointer/reference; do not reproduce the current pointer-to-temporary pattern. The new command accepts a valid Params reference, so it needs no nullable source argument or fallback geometry. In the GUI slot, retain a named source Record and copy its Params before calling the command; if the source record no longer has Params when the non-modal dialog submits, return without mutation or notifications, matching the dialog's existing missing-source refusal in `showChangeDialog`. Keep scope-label updates, reload conditions and thumbnail signals in the slot exactly as today.

Keep CLI `applyPageSplit` using existing `setLayoutTypeForAllPages`. It updates the default and removes conflicting per-image records; it is not a GUI copy operation. Do not substitute `setLayoutTypeFor`: its current implementation constructs an empty UpdateAction and is not equivalent to the GUI loop.

### 4. Put Content box-copy rules behind Settings without repairing CLI units or modes
Add `<set>` to the existing header and add to `select_content::Settings`:

```cpp
enum class BoxSelection { Neither, Content, Page, Both };
void copyBoxes(const std::set<PageId>& pages, const PageId& sourcePage,
               const Params& source, BoxSelection selection);
void setManualBox(const std::set<PageId>& pages, const QRectF& rect);
void useUnboundedPageDetection();
```

`copyBoxes` owns the mutation behavior currently at `OptionsWidget.cpp:211-255`:
- Skip `sourcePage` exactly as the GUI currently does.
- Start from the source snapshot but reset its dependencies to `Dependencies()` before each target merge. This is the current invalidation behavior; do not replace it with the actual source outline or otherwise enable currently unreachable geometry correction.
- Reproduce the existing manual-page/manual-content conditional preservation and translation rules verbatim. For example, unchecked manual content preserves the target content rectangle; checked non-empty manual content translates by the resulting page rectangle's origin minus the source page rectangle's origin. Preserve contentSizeMM, modes and fine-tuning values exactly as the original construction does, even when a rectangle is subsequently retained/translated.
- For selected content with source MODE_AUTO, store the current forced-recompute dependencies built from an empty polygon, MODE_DISABLED for content, source page-detection mode and source fine-tuning state.
- Missing target Params begin from the source snapshot/default dependencies. `Neither` is NOT a no-op: the original handler may still copy modes/fine-tuning and other values. Preserve this distinction from Deskew.
- Refresh deviations through the existing map/bookkeeping behavior.

In GUI `applySelection`, keep the empty-set guard and all notifications/reload calls, build a Params snapshot from `m_uiData` using its real dependencies, translate the two booleans into BoxSelection, and call `copyBoxes`. The command, not the adapter, performs the dependency reset. Delete the old mutation/merge loop; no Params/UiData callback is allowed inside Settings.

`setManualBox` owns the CLI's current fresh-Params construction: contentRect=pageRect=rect, contentSizeMM=(rect.width(), rect.height()), default dependencies, both modes MODE_MANUAL, fine-tuning false, applied to every target. It intentionally keeps the current numeric interpretation; do not add a mm-to-pixel conversion. `useUnboundedPageDetection` sets the existing global detection box to QSizeF(100000,100000) and tolerance to zero, without altering any per-page detection mode. Name the detection-box dimension as a translation-unit constant rather than scattering the literal.

CLI `applySelectContent` retains the OFF-only global action and the independent optional content-box action, in that order. Convert the CLI rectangle to QRectF as today, then call the two commands. AUTO/MANUAL flags without a box must retain their current effects/no-effects; no new per-page mode semantics are authorized.

### 5. Own Margins copying and Alignment application in page_layout::Settings
Add `<optional>`, `<set>` and complete types needed by these declarations to the existing Settings header:

```cpp
struct MarginValues {
  std::optional<double> left;
  std::optional<double> top;
  std::optional<double> right;
  std::optional<double> bottom;
};
void copyMargins(const std::set<PageId>& pages, const PageId& sourcePage,
                 const MarginValues& selectedValues);
void overrideMargins(const std::set<PageId>& pages, const Margins& marginsMm);
void copyAlignment(const std::set<PageId>& pages, const PageId& sourcePage,
                   const Alignment& alignment);
void applyAlignment(const std::set<PageId>& pages, const Alignment& alignment);
```

`copyMargins` snapshots `isPageAutoMarginsEnabled(sourcePage)` once before any target writes. For every target, copy that flag first. If true, invalidate content size without modifying hard margins. If false, read target hard margins and replace only engaged MarginValues, then write them using existing `setHardMarginsMM`. Even four disengaged values must copy the flag, create/update the same records as today and perform the same automatic-content invalidation. SourcePage is included if present in the set. Reuse existing pimpl-backed setters so aggregate/deviation structures remain coherent.

`overrideMargins` sets all four margins using the existing setter and therefore preserves each target's current auto-margins flag and other fields. `copyAlignment` skips sourcePage; `applyAlignment` does not. Both retain existing `setPageAlignment` behavior and do not invent aggregate notifications/return values.

In GUI `applyMargins`, retain the existing empty-set guard and the two final signals. Translate each checked side into the corresponding optional value from `m_marginsMM`, then call copyMargins; remove the old merge loop. In GUI `applyAlignment`, retain its guard and invalidate-all signal and delegate the mutation to copyAlignment using `m_alignment`.

In CLI `applyPageLayout`, retain the current MmRect-to-Margins mapping and the three-way CLI-alignment-to-stage-Alignment translation. Call overrideMargins/applyAlignment instead of writing per-page values. Do not retain a target's alignment-null bit: the CLI currently replaces the entire Alignment object.

Keep `ApplyMarginsDialog` and `PageApplyDialog` unchanged. Their every-other-selected scope semantics differ: the old margins dialog may include preceding selected pages, while PageApplyDialog is forward-only. Dialog replacement would violate the approved behavior-preserving scope.

### 6. Own Output's bulk edits and CLI partial override in output::Settings
Add `<set>` and `<optional>` to the existing Settings header as needed. Add these declarations using existing output value types:

```cpp
void applyDpi(const std::set<PageId>& pages, const Dpi& dpi);
void applyColors(const std::set<PageId>& pages, const ColorParams& colors,
                 const PictureShapeOptions& pictureShape);
void applySplitting(const std::set<PageId>& pages, const SplittingOptions& options);
void applyDespeckleLevel(const std::set<PageId>& pages, double level);
void applyDewarping(const std::set<PageId>& pages, const DewarpingOptions& options);
void applyDepthPerception(const std::set<PageId>& pages, const DepthPerception& depth);
void copyProcessing(const std::set<PageId>& pages, const PageId& sourcePage);

enum class IlluminationUpdate { Preserve, Normalize };
struct Overrides {
  std::optional<Dpi> dpi;
  std::optional<ColorMode> colorMode;
  std::optional<int> thresholdAdjustment;
  std::optional<DespeckleLevel> despeckleLevel;
  std::optional<DewarpingMode> dewarpingMode;
  std::optional<double> depthPerception;
  IlluminationUpdate illumination = IlluminationUpdate::Preserve;
  bool empty() const;
};
void applyOverrides(const std::set<PageId>& pages, const Overrides& overrides);
```

The six value-application commands own the mutation loops now in `OptionsWidget::dpiChanged`, `applyColorsConfirmed`, `applySplittingOptionsConfirmed`, `applyDespeckleConfirmed`, `dewarpingChanged`, and `applyDepthPerceptionConfirmed`:
- applyDpi changes only AUTO dewarping to MANUAL before writing the requested DPI. Do this even if the new DPI equals the existing one, because the current slot does. Leave MARGINAL/OFF/MANUAL and distortion models unchanged.
- applyColors copies both ColorParams and PictureShapeOptions, preserving every unrelated output field.
- The splitting/despeckle/dewarping/depth commands change only their respective fields, retaining existing setter behavior on missing targets.
- copyProcessing snapshots the source's black-on-white value, applies it to each target, and sets blackOnWhiteSetManually=true in each target's own OutputProcessingParams without overwriting its other processing fields. An unknown source uses the existing getParams default; an empty page set reads/mutates nothing.

Replace only those seven mutation loops in OptionsWidget with Settings calls. Keep every original signal, local widget-state assignment, current-page membership check, delayed reload behavior, dewarping-tab condition and display update in the GUI slot. Do not refactor thumbnail-notification boilerplate or immediate-edit color/slider logic in this work.

Move the entire Params patching rule in CLI `applyOutput` (`FilterConfigAdapter.cpp:181-247`) into `applyOverrides`, expressed in stage-owned types rather than CliRequest. `Overrides::empty` checks optional engagement and illumination==Preserve; an empty override must not create output Params records. For each target, start with existing/default Params, apply only engaged fields in the original order, then setParams once:
1. DPI replacement, with NO automatic-dewarp freezing.
2. Color mode, threshold and illumination on a single ColorParams copy. Normalize sets both BW and color-common illumination flags true; absent illumination does not reset either one.
3. Despeckle enum converted to double exactly as today.
4. Dewarping mode changed on existing DewarpingOptions, retaining its other values.
5. Depth value converted to existing DepthPerception.

Do not copy GUI color-mode couplings, mode validation or manual-model generation into this command. A CLI DPI-only override must retain AUTO; explicit DPI+dewarping flags must retain the CLI-requested mode. Missing targets begin from Params(), while omitted fields on existing targets retain their exact values. Split private color/other-field patch helpers if needed rather than creating a large public method.

The CLI adapter constructs `output::Settings::Overrides` directly from request values, including Dpi(n,n). Return immediately when `overrides.empty()` before evaluating selectedPageIds(); otherwise delegate. Leave its range gate untouched; eliminate its old field-patching implementation and duplicated no-output-options predicate in favor of the single definition in Overrides::empty. The Settings command also checks empty() so direct callers cannot accidentally create records.

### 7. Integrate target-set reuse and protect the actual adapters
Migrate every bulk caller identified above in the same step as its Settings command; no old merge implementation or compatibility alias remains. Orientation and CLI global split application stay deliberately unchanged because they already have owners with the correct semantics.

The CLI now needs target PageIds for more stages. Avoid constructing a separate std::set for every apply method or building one when only global options/no options are used. In FilterConfigAdapter add `std::optional<std::set<PageId>> m_selectedPageIds` and change the private helper to `const std::set<PageId>& selectedPageIds()`. Lazily emplace/fill it once on first use; reset it at the beginning of `apply(range)` so a reused adapter never retains a stale page-set snapshot. `applyFixOrientation` and `applySelectContent` are the existing source-confirmed callers; migrate those plus the new Deskew/Layout/Output callsites to the borrowed reference. Include PageId.h in the adapter header because the cached container needs a complete key type. Keep FilterRange behavior and apply-stage ordering exactly unchanged.

Register five new core test translation units in `src/core/tests/CMakeLists.txt`: `TestOrientationApply.cpp`, `TestDeskewApply.cpp`, `TestPageSplitApply.cpp`, `TestPageLayoutApply.cpp`, `TestOutputApply.cpp`. Extend existing `TestSelectContentApply.cpp` for the content command. Use fully qualified stage types and only Settings/Params/domain headers, not concrete OptionsWidget headers or generated ui_OptionsWidget headers.

Remove only the two `apply_merge_*` cases in existing `TestDeskewParams.cpp` that manually construct a merged Params. Replace their coverage with calls to applyAngles in TestDeskewApply. In TestSelectContentApply, replace the hand-constructed forced-recompute case with one that calls copyBoxes then compares the stored dependencies against current geometry through compatibleWith; keep the existing independent compatibility tests.

Extend `TestCliQt` in `src/qt_tests/tst_cli_qt.h/.cpp` with behavioral integration cases named `appliesStageOverrides`, `preservesUnspecifiedStageSettings`, `respectsApplicationRange`, `guiDeskewCopyPreservesOblique`, and `guiDpiCopyFreezesAutoDewarp`. Reuse the real ProjectPages/StageSequence construction pattern from TestStageChain: real metadata entries, at least two pages, PageSelectionAccessor backed by a small fixed selection provider. Feed parseCommandLine output through the real FilterConfigAdapter and inspect production Settings, not a mock invocation log.

For the two GUI integration cases, obtain actual options widgets by seeding filter Settings and calling the public Filter::preUpdateUI with a test FilterUiInterface that captures a `FilterOptionsWidget*`. Implement its other UI methods with ordinary test-owned widget handling; they are not the behavior assertions. Do not include concrete filter OptionsWidget headers: their generated `ui_OptionsWidget.h` include paths are private and ambiguous outside each filter target. Use Qt::DirectConnection `QMetaObject::invokeMethod` on the captured QObject to exercise the existing registered slots:
- Deskew: set source angles via `manualDeskewAngleSetExternally(double)` and `manualObliqueAngleSetExternally(double)`, then invoke `appliedTo(std::set<PageId>,bool,bool)` with deskew-only selected; assert the target's stored deskew/mode changed and its oblique/mode did not.
- Output: seed a target with AUTO dewarping, invoke `dpiChanged(std::set<PageId>,Dpi)`, and assert stored DPI plus MANUAL mode. The widgets are real, not mocked; source state for deskew comes from its actual slots rather than test reimplementation of the merge.

The existing qt_tests executable runs multiple QTest classes sequentially; run the whole binary, not one class's method as a command-line filter. Keep application/library target structure unchanged.

## Critical files & anchors
- `src/core/filters/deskew/Params.cpp:28-36`: the existing three-argument constructor intentionally resets oblique to zero/AUTO, unlike the GUI merge.
- `src/core/filters/select_content/OptionsWidget.cpp:211-255`: default dependencies, source exclusion and geometry/mode behavior must survive relocation literally.
- `src/core/filters/page_split/Settings.cpp:59-66,74-121`: existing locked record/conflict helpers are reusable; the bulk setLayoutTypeFor helper is not an equivalent replacement.
- `src/core/filters/page_layout/Settings.cpp:517-529`: setHardMarginsMM preserves target autoMargins; do not silently disable automatic margins for CLI overrides.
- `src/core/filters/output/OptionsWidget.cpp:445-484`: dewarping UI/tab/reload conditions remain outside the new Settings command.

## Verification

### Commands and environment
Working directory: repository root. Existing `build-tests` is Debug, BUILD_TESTS=ON, Qt5; `CMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE` is already recorded in CMakeCache. Exact runnable binaries are at the build root, including `build-tests/core_tests`, `qt_tests`, `scantailor-advanced-cli`, and `scantailor-advanced`.

After approval, before baseline capture:

```sh
cmake -S . -B build-tests -DBUILD_TESTS=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build-tests -j8
QT_QPA_PLATFORM=offscreen ctest --test-dir build-tests --output-on-failure
```

After implementation build all targets, not core_tests alone, then run the same CTest command. For a focused domain check use these exact suite names for the new files: OrientationApplyTestSuite, DeskewApplyTestSuite, PageSplitApplyTestSuite, PageLayoutApplyTestSuite, OutputApplyTestSuite; the content suite remains SelectContentApplyTestSuite.

```sh
build-tests/core_tests --run_test=OrientationApplyTestSuite,DeskewApplyTestSuite,PageSplitApplyTestSuite,SelectContentApplyTestSuite,PageLayoutApplyTestSuite,OutputApplyTestSuite --log_level=message
QT_QPA_PLATFORM=offscreen build-tests/qt_tests
```

Run Qt checks with XDG_CONFIG_HOME and XDG_DATA_HOME pointing to temporary directories so first-time widget/profile initialization never reads or writes user settings. Keep those directories alive for the whole test process. All these commands are execution steps, not commands already run in planning.

### Required domain outcomes
1. Orientation: LEFT_PAGE and RIGHT_PAGE for the same ImageId share rotation/trim; unrelated images retain state. Rotation-only application retains trim. Disabled trim erases the shared trim.
2. Deskew: source=(deskew 2/MANUAL, oblique 4/AUTO), target=(1/AUTO,3/MANUAL). Deskew-only yields (2/MANUAL,3/MANUAL); oblique-only yields (1/AUTO,4/AUTO); Both copies both axes; Neither leaves values/dependencies unchanged. Exercise missing targets and source dependencies. CLI manual-angle replacement still yields zero/AUTO oblique, proving GUI and CLI operations are intentionally distinct.
3. Split: use source PageLayout(QRectF(0,0,100,200), QLineF(50,0,50,200)) and a target TWO_PAGES layout with outline QRectF(0,0,200,400) and a different cutter. Give target Dependencies a 90-degree orientation. CopySource must store target outline (0,0,200,400), cutterLine(0)=QLineF(50,0,50,400), TWO_PAGES type and the target's 90-degree orientation dependency. These expected coordinates come from border intersection, not proportional scaling; do not invoke adaptPageLayout to construct the expectation. Also assert KeepTarget/AUTO conflict handling, missing target Params and shared-image siblings. Use the existing Params(PageLayout,Dependencies,MODE_MANUAL) and Dependencies(QSize,OrthogonalRotation,LayoutType) constructors.
4. Content: use source pageRect=(10,20,100,200), contentRect=(20,30,40,50), both modes MANUAL, and target pageRect=(100,200,100,200), contentRect=(110,210,20,30). Content-only copy must retain target pageRect and store contentRect=(110,210,40,50); Page-only copy must store the source pageRect and retain the target contentRect, without adding a new clipping step. Assert source exclusion, automatic-content forced recomputation and missing targets. Include Neither: it still copies original modes/fine-tuning/contentSizeMM behavior while preserving the conditional manual rectangles. Assert actual stored values and dependency compatibility. The CLI manual-box command keeps current numeric values and the global detection command changes box/tolerance without changing per-page modes.
5. Layout: source manual margins (10,20,30,40), target (1,2,3,4), only left/right selected gives (10,2,30,4). Automatic source copies the auto flag and invalidates content size but retains hard margins; four unchecked sides still copy the flag. CLI override replaces four values and retains target autoMargins=true. GUI alignment skips source; CLI alignment applies to all targets, including its existing null-bit replacement semantics.
6. Output: each copy command preserves unrelated Params; colors includes picture-shape options; processing copy preserves other per-target processing flags. GUI DPI application turns AUTO into MANUAL even for equal DPI, but retains MARGINAL/MANUAL/OFF and the existing distortion model. CLI DPI-only override retains AUTO; combined overrides preserve unmentioned nested color/dewarping fields; zero threshold and zero/Off despeckle must be recognized by optional engagement. Empty override creates no records.
7. GUI integration assertions execute the actual widget slots listed in Step 7. CLI integration tests cover all six stage intentions, no-flags preservation on pre-seeded pages, and range gating (e.g. start=3/end=4 must not apply orientation/deskew/output flags even if supplied). Do not assert incidental implementation order or source text.

### Real executable smoke and before/after comparison
Use a throwaway directory from `${TMPDIR:-/tmp}` and a small Python standard-library driver, created only during execution outside the checkout. The driver uses tempfile, struct, zlib, subprocess, json and xml.etree.ElementTree; no new package dependency is needed.

Generate two deterministic 512x768 grayscale PNG inputs with a white background, black horizontal text-like strokes in a central rectangle, and a PNG pHYs chunk of 11811 pixels/metre on both axes (300 DPI). Use distinct basenames. Keep input files unchanged between the two runs so source file metadata is stable.

Copy the current executable to `<smoke>/bin/scantailor-advanced-cli` and execute that copy for both runs, replacing it with the rebuilt executable for the after run. This matters because the configured PORTABLE_CONFIG_DIR is `config`: both runs must resolve to the same isolated `<smoke>/bin/config`, not the user's or build directory's portable settings. Set QT_QPA_PLATFORM=offscreen and isolated XDG_CONFIG_HOME/XDG_DATA_HOME for both runs.

Exact arguments, substituting the smoke directory for each placeholder:

```text
<smoke>/bin/scantailor-advanced-cli
--layout=1 --orientation=90 --deskew=manual --deskew-angle=1.5
--content-detection=off --margins=1,2,3,4 --alignment=center
--output-dpi=300 --color-mode=mixed --threshold=10 --despeckle=normal
--dewarping=off --depth-perception=2.0 --equalize-illumination
--output=json --output-project=<smoke>/result.ScanTailor
<smoke>/page-a.png <smoke>/page-b.png <smoke>/out
```

Capture return code/stdout/stderr. Require code 0, JSON status="ok", pages_processed=2, pages_failed=0, empty errors, and two distinct output_file paths corresponding to the two input_file paths. Require both files to be actual TIFFs (valid II/MM TIFF header), not just placeholder paths.

Before replacing the executable, preserve the JSON and saved project. Preserve a canonical representation of the `<filters>` subtree: element tag, sorted attributes, text and child sequence. Exclude only descendant `<output-params>` cache records, whose file timestamps change on regeneration; retain actual `<params>`, `<processing-params>`, zones and all stage-level attributes. Before the after run, remove only the generated `<smoke>/out` directory to force real rendering again, leaving inputs and portable configuration unchanged. Run the rebuilt copy with identical arguments/paths and require equal JSON and equal canonical filter state. This exercises real parsing, stage configuration, processing and serialization across all six stages; tests alone are not the smoke proof.

If the baseline full-output smoke already fails, preserve its exact evidence and use the same driver with `--end-filter=4` as the predetermined reachable baseline/after comparison; expect output_file=null instead of TIFF paths. Complete the domain/adapter checks and report the existing Output-stage blocker rather than altering CLI semantics to make the smoke pass. Do not call that fallback a successful full-output smoke.

## Assumptions & contingencies
- User-confirmed choices: all six stages, strict behavior preservation. Existing suspicious CLI behavior (deskew OFF mapping, box units/detection, alignment replacement, DPI/dewarping differences) is deliberately preserved, not fixed under this refactor.
- No glossary term is being newly coined: use existing stage/page/Params/Settings names. No glossary or ADR write is needed to implement these ownership moves.
- Keep dialogs and page-scope semantics untouched. Keep allPages-specific notification differences, current-page exclusions, guards and signal order even where they appear redundant. Do not move cross-stage propagators into Settings or add new CLI propagation.
- Source records passed by reference to copy commands are valid snapshots, not nullable handles. Missing target records follow each stage's existing defaults/merge rules; they are not errors. New commands add no clamping, validation, errors or fallback values beyond the behavior specified above.
- A baseline failure must be classified and reported. If a new test exposes one of the deliberately preserved CLI divergences, assert the observed current behavior; if a test fails because this refactor changed a defined behavior, fix the refactor rather than redefining the expectation.
- No commit, tag, push, deployment or version bump is authorized by this plan. The pre-existing untracked `.omp/config.yml` and `.omp/plans/STAGE_CHAIN_AND_SEAMS_PLAN.md` are user files and stay untouched.
