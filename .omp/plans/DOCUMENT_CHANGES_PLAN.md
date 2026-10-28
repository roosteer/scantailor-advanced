# The document says when it changed

## Context
ProjectDocument owns persisted-state equality and change notification. User decisions: restoring saved values restores cleanliness; autosave runs 60 seconds after the first persisted change without postponement; selected page and computed results count, presentation-only state does not. Preserve XML format and existing save feedback.

## Approach
### 1. Instrument existing mutation owners
Use the exact inventory and primitive below. Append an optional shared notifier constructor argument to StageSequence, six Filters, six Settings and both ImageSettings; factories pass their document notifier. Existing standalone construction can omit it. Notifications mean possible change, not definite dirtiness; avoid adding Params equality operators. Keep callbacks non-reentrant and restricted to enqueueing. Do not move all ProjectPages consumers or change thumbnail refresh behavior in this task.

## 1. Notifier API and exact semantics

New `src/core/DocumentChangeNotifier.h`:

```cpp
class DocumentChangeNotifier {
 public:
  using Handler = std::function<void()>;
  void setHandler(Handler handler);   // GUI thread
  void clearHandler();                // GUI thread; waits for in-flight handler
  void notify();                      // any thread; never blocks on settings
 private:
  QMutex m_mutex;
  Handler m_handler;
  std::atomic<bool> m_hasHandler{false};
};

class DocumentChangeGuard {
 public:
  explicit DocumentChangeGuard(DocumentChangeNotifier* notifier) : m_notifier(notifier) {}
  ~DocumentChangeGuard() { if (m_notifier) m_notifier->notify(); }
  DocumentChangeGuard(const DocumentChangeGuard&) = delete;
  DocumentChangeGuard& operator=(const DocumentChangeGuard&) = delete;
 private:
  DocumentChangeNotifier* m_notifier;  // Settings owns the shared_ptr; no refcount churn
};
```

Implementation:

```cpp
void DocumentChangeNotifier::setHandler(Handler handler) {
  QMutexLocker lock(&m_mutex);
  m_handler = std::move(handler);
  m_hasHandler.store(m_handler != nullptr, std::memory_order_release);
}
void DocumentChangeNotifier::clearHandler() {
  QMutexLocker lock(&m_mutex);
  m_handler = nullptr;
  m_hasHandler.store(false, std::memory_order_release);
}
void DocumentChangeNotifier::notify() {
  if (!m_hasHandler.load(std::memory_order_acquire)) return;
  QMutexLocker lock(&m_mutex);
  if (m_handler) m_handler();
}
```

The guard stores a raw pointer, not a `shared_ptr`: the Settings method itself owns a `shared_ptr` member, so the notifier cannot be destroyed during the guard's lifetime. `notify()`'s atomic fast path removes mutex traffic in CLI/tests where no handler is installed.

Add `DocumentChangeNotifier.cpp/.h` to `src/core/CMakeLists.txt` `common_sources` (near `ProjectDocument.cpp`). No Q_OBJECT/moc is involved.

## 2. Persisted-state coverage inventory (exact)

All maps are private; the only persisted writers are these methods. Add `DocumentChangeGuard guard(m_changeNotifier.get());` as the first statement of each leaf mutator, before the existing `QMutexLocker`/locked scope. Guards listed once per public method; private `...Locked` helpers must never notify.

### 2.1 fix_orientation::Settings (`src/core/filters/fix_orientation/Settings.cpp`)
- `clear` (17), `performRelinking` (23)
- `applyRotation(const ImageId&, OrthogonalRotation)` (46)
- `applyRotation(const std::set<PageId>&, OrthogonalRotation)` (51)
- `applyTrim(const std::set<PageId>&, const ImageTrim&)` (59)
- `setTrim(const ImageId&, const ImageTrim&)` (92)
- `clearTrim(const ImageId&)` (110)
- Do not instrument `setImageRotationLocked` (83).

### 2.2 deskew::Settings (`deskew/Settings.cpp`)
- `clear` (26), `performRelinking` (33)
- `setPageParams` (61), `clearPageParams` (67), `setDegrees` (84)
- `applyAngles` (92): put the guard **after** the `selection == Neither || pages.empty()` early return, immediately before the locker, so a no-op call does not notify.
- `setAlgoContentBased` (125)
- Do NOT notify from `setPendingAutoOblique` (135) or `takePendingAutoOblique` (140): transient, not serialized.

### 2.3 select_content::Settings (`select_content/Settings.cpp`)
- `clear` (34), `performRelinking` (40)
- `setPageParams` (59), `clearPageParams` (65)
- `copyBoxes` (87), `setManualBox` (134)
- `setPageDetectionBox` (154), `setPageDetectionTolerance` (162): add `QMutexLocker` here and to the matching getters (150, 158). They are currently unsynchronized and read by `select_content::Task` (`Task.cpp:100-101`) from a worker while the GUI/save may write/read them. This is required now that serialization can run while workers run.
- `useUnboundedPageDetection` (145) needs no guard: it calls the two setter leaves.

### 2.4 page_split::Settings (`page_split/Settings.cpp`)
- `clear` (start of file), `performRelinking`
- `setLayoutTypeForAllPages` (43)
- `setLayoutTypeFor` (60): currently a no-op (it builds a default `UpdateAction`, all DONT_TOUCH). Keep the guard for future correctness; no callers today.
- `applyLayout` (70), `updatePage` (118)
- `conditionalUpdate` (159): worker path (`page_split/Task.cpp:160`). One guard in the public method; `updatePageLocked` must not notify.

### 2.5 page_layout::Settings (`page_layout/Settings.cpp`)
- `clear` (220), `performRelinking` (224), `removePagesMissingFrom` (228)
- `setPageParams` (240), `updateContentSizeAndGetParams` (244)  [worker: `Task.cpp:82`, `CacheDrivenTask.cpp:41`]
- `setHardMarginsMM` (258)  [worker: `Task.cpp:77`]
- `setPageAlignment` (266), `setContentSizeMM` (270), `invalidateContentSize` (274)
- `setPageAutoMarginsEnabled` (300)
- `enableShowingMiddleRect` (320): add a `QMutexLocker` in `Impl` for the bool.
- Replace `std::vector<Guide>& guides()` (312) with `std::vector<Guide> guides() const` (locked copy) and `void setGuides(std::vector<Guide>)` (locked swap + guard). Update `page_layout/Filter.cpp` (copy once in saveSettings; build a local vector then setGuides in loadSettings) and `page_layout/ImageView.cpp:785-790` (build local vector, call setGuides once).
- Do NOT notify from `setAggregateHardSizeFrozen` (288) or `isAggregateHardSizeFrozen`: transient.
- `copyMargins` (324), `overrideMargins` (352), `copyAlignment` (358), `applyAlignment` (369) need no guard; they only call notifying leaves.

### 2.6 output::Settings (`output/Settings.cpp`)
Guarded leaves: `clear` (20), `performRelinking` (32), `setParams` (94), `setColorParams` (99), `setPictureShapeOptions` (112), `setDpi` (125), `setDewarpingOptions` (138), `setSplittingOptions` (151), `setDistortionModel` (164), `setDepthPerception` (177), `setDespeckleLevel` (190), `removeOutputParams` (214), `setOutputParams` (219), `setPictureZones` (246), `setFillZones` (251), `setOutputProcessingParams` (299), `setBlackOnWhite` (309).
Worker writers covered: `output/Task.cpp:154` (setOutputProcessingParams), `:334` (setParams), `:408` (removeOutputParams), `:422` (setOutputParams).
Do NOT notify from `setDefaultPictureZoneProperties`/`setDefaultFillZoneProperties` (266/271): header says they are not persistent.
`applyDpi`/`applyColors`/`applySplitting`/`applyDespeckleLevel`/`applyDewarping`/`applyDepthPerception`/`copyProcessing`/`applyOverrides` need no guard; they call notifying leaves and correctly notify zero times for an empty target set.

### 2.7 ImageSettings (`src/core/ImageSettings.cpp`)
- `clear` (13), `performRelinking` (18), `setPageParams` (31).
- Worker writers: `fix_orientation/Task.cpp:89`, `deskew/Task.cpp:287`.

### 2.8 ProjectPages (`src/core/ProjectPages.cpp`)
Fix these signal defects (they are the worker-write bridge):
- `insertImage` (221-235): `return insertImageImpl(...)` inside the locked scope makes the trailing `emit modified()` unreachable. Store the result, close the scope, emit if `wasModified`, return the result. `ProjectDocument::insertImage` then registers disambiguator labels and explicitly notifies at the end.
- `performRelinking` (143-150): locker spans the function and there is no emit. Scope the locker, then `emit modified()`.
- `updateMetadataFrom` (304-325): writes metadata with no emit. Compare `image.metadata != it->second` (operator!= exists), set a `wasModified`, close the lock scope, emit after it.
- Existing emits at 80, 162, 175, 188, 201, 246, 261 are already after lock release. Keep them.

### 2.9 ProjectDocument (`src/core/ProjectDocument.cpp`)
Notify explicitly after complete composite operations:
- `selectPage` (currently inline in the header, line 64): move to .cpp, call `m_selectedPage.set`, then `m_changeNotifier->notify()`. Selection is persisted (`ProjectWriter::processPages`).
- `insertImage` (97-108): after all inserted pages are registered, notify.
- `setLayoutDirection` (109-112): after `m_outputFileNameGenerator.setLayoutDirection`, notify.
- `performRelinking` (114-118): after pages/stages/generator, notify.
Connect pages in the constructor: `QObject::connect(m_pages.get(), &ProjectPages::modified, m_pages.get(), [notifier = m_changeNotifier] { notifier->notify(); }, Qt::DirectConnection);` (context object = pages, so auto-disconnect; lambda keeps the notifier alive).



### 2. Share serialization between equality and saving
Replace ProjectWriter::write (ProjectWriter.cpp:80-112; its caller is ProjectDocument::save) with `QByteArray serialize(const QString& filePath, const std::vector<FilterPtr>& filters) const` and `static bool writeFile(const QString& filePath, const QByteArray& bytes)`. Preserve DOM construction, indentation 2, QTextStream encoding, and destination-relative paths. Serialize through QTextStream into QByteArray; flush and destroy the stream before returning bytes. writeFile uses the existing QFile path and checks open, complete byte count, flush and close error; false never advances a checkpoint. Do not change the XML format or add a new persistence mechanism.

ProjectDocument retains `bool save(const QString& filePath) const` so existing save helpers and OutOfMemoryDialog remain callers. Add `bool hasUnsavedChanges(const QString& filePath) const`, `void markSavedState(const QString& filePath)`, and `const std::shared_ptr<DocumentChangeNotifier>& changeNotifier() const`; private `QByteArray serializedState(const QString& filePath) const`, mutable QByteArray saved bytes, QString saved path and bool checkpoint-valid initially false. Normalize nonempty paths using QFileInfo(path).absoluteFilePath(). Different destination or absent checkpoint is unsaved; otherwise serialize at the checkpoint destination and compare bytes. This is model-normalized equality, not original-file whitespace equality. save serializes once, writes exactly those bytes, and only on success checkpoints those bytes and destination. A worker mutation during writing must remain different from the bytes actually written. markSavedState serializes the loaded model for initialization only. No writeSnapshot helper: step 4 deletes backup writing entirely.

Add a regression proving serialization determinism across repeated calls, including non-ASCII paths and every stage. Use the existing DOM serialization order; do not introduce a second handwritten field list. Existing lossy XML omissions stay unchanged: omitted values are not persisted state.

### 3. Queue change delivery and drive autosave
MainWindow adds `std::atomic<bool> m_documentChangePending{false}` and private Qt slot `void projectDocumentChanged()`. Install one notifier handler capturing this: exchange pending true; only on the false-to-true transition post `QMetaObject::invokeMethod(this, "projectDocumentChanged", Qt::QueuedConnection)`; reset pending if posting fails. Use the named-slot overload compatible with existing Qt requirements. Handler must never serialize or take settings locks. The slot resets pending, returns if no saved project path, no loaded document, or autosave disabled, and starts the existing single-shot timer at 60000 ms only when inactive. Notifications may be no-op writes; do not serialize per notification. autoSaveProject repeats existing enabled/path guards and skips when hasUnsavedChanges(path) is false; otherwise call existing project_save::saveWithFeedback. Failed saves retain dirty state, show existing feedback, and do not create a retry loop. The next notification can schedule another interval.

In switchToProject: synchronously clear the old handler, stop timer/reset pending, perform existing cancellation, then assign new document/path. For nonempty loaded paths markSavedState BEFORE UI/default initialization, initial selection changes, updateMainArea or processing. Install the handler immediately afterward, before any such changes. New unnamed documents have no checkpoint and do not arm unattended Save As. XML loading already finishes before switchToProject. Do not checkpoint after UI initialization: persisted initialization/computed changes count under the user decision. Directly connect ProjectPages::modified to the notifier with pages as context and Qt::DirectConnection, since the callback only touches the thread-safe notifier. This covers worker emissions without waiting on pages' affinity thread.

At MainWindow destruction clear the handler and stop timer before worker shutdown. clearHandler synchronizes with in-flight posts; QObject destruction removes queued receiver events. Settings retain shared notifier ownership, permitting worker-side destruction without QObject affinity. Stale queued slots from a prior document can at most arm an equality-gated timer, never write a clean replacement document. Do not stop an active timer on manual save: a worker may have changed state while it was active; its eventual equality gate skips clean state.

Remove updateAutoSaveTimer declaration/definition and its currentPageChanged call. Selection now notifies through ProjectDocument::selectPage. Preserve current application autosave enabled setting; this task does not alter preferences UI.

### 4. Replace backup comparisons with document decisions
Rewrite closeProjectInteractive to retain the early unloaded return and use hasUnsavedChanges for known saved paths. Clean: close directly. Dirty: prompt; CANCEL returns false; DONT_SAVE closes; SAVE invokes project_save::saveWithFeedback and closes only on success. No Backup file, compareFiles or rename is involved. For unnamed documents retain the existing prompt, but use project_save::saveAs return value so cancelled Save As or failed writing keeps the document open instead of discarding it. Recent-project and current path updates remain exclusively in existing successful save helpers.

Use LSP references before deleting compareFiles declaration/definition and ProjectWriter::write, or changing page_layout::Settings::guides. Migrate Filter save/load and ImageView guide writes to the copied getter/setter; remove the mutable accessor without an alias. Remove only includes made obsolete by these changes. The out-of-memory dialog and CLI still save through ProjectDocument::save and therefore checkpoint successful writes without separate logic.

## Critical files & anchors
- src/core/ProjectWriter.cpp:80-112: equality and writes must share exact serialization and destination path rules.
- src/core/ProjectDocument.cpp:56-68,133-136: loaded settings precede checkpoints; all writers converge here.
- src/core/filters/page_layout/Settings.cpp:312 and ImageView.cpp:785-790: guides currently escape as a mutable reference.
- src/app/MainWindow.cpp:371-425,958-994,1684-1746,2088-2092: project switch lifetime, autosave and close must change together.
- src/qt_tests/CMakeLists.txt:20-31: app_tests links app_lib and real application resources and runs offscreen.

## Verification
Repository-root prerequisites: existing configured build-tests, Qt/Boost dependencies, isolated temporary settings and files. Executables observed: build-tests/core_tests, app_tests, scantailor-advanced and scantailor-advanced-cli. Extend existing TestProjectDocument.cpp and tst_project_document_ui.cpp using their real temporary document/image factories, not copied widgets. Establish failing behavior regressions before implementation; avoid source-text and callback-forwarding assertions.

1. Checkpoint loaded state; repeated serialize/compare is clean. Change and restore selection, deskew params, guides, metadata, output params and each other stage's serialized settings: dirty then clean. Include both ImageSettings and one excluded transient preference. Check non-ASCII paths and repeated serialization determinism.
2. Save, change, fail writing to a missing-parent destination: return false and retain dirty checkpoint. Successful Save As writes the expected relative paths and becomes clean at the destination. Reopen file and assert actual values. Failed/cancelled save must never checkpoint.
3. Run actual worker-style Settings mutations and ProjectPages metadata/insert changes on a thread. Drive the GUI event loop and prove saved results reflect them. Clear handler while a worker is active, join it, and verify no delivery reaches the former observer. Assert state/lifetime behavior rather than wiring counts.
4. Real MainWindow with autosave enabled: change a persisted setting without navigation; timer becomes active. Later edits do not increase remaining time. Restore baseline then fire timeout: bytes and modification time unchanged. Dirty timeout persists the setting and yields clean state. Exercise at least one REAL 60-second timer expiry with event-loop wait exceeding 60 seconds, not only direct timeout invocation.
5. Clean close has no prompt or Backup artifact. Dirty Cancel retains document; failed Save retains it; successful Save reopens with correct state. Unnamed Save As cancellation retains it. Switch documents with a pending timer and verify the new clean file is not rewritten.

After integration run `cmake --build build-tests --parallel 8` then `QT_QPA_PLATFORM=offscreen ctest --test-dir build-tests --output-on-failure` once. Include the new behavior tests in existing targets.

Smoke beyond tests: launch build-tests/scantailor-advanced with a temporary saved project, enable autosave in isolated QSettings, edit without navigation, wait 60 seconds and reopen the file to observe the setting. Restore an edit and close without prompt/Backup file. If native desktop access is unavailable, use a throwaway offscreen Qt driver linked to app_lib plus application resources, with real MainWindow and actual timer expiry; use existing core-test image fixture creation, QTemporaryDir and temporary XDG_CONFIG_HOME. Report whether native or offscreen proof ran; do not claim a screenshot from headless assertions.

## Assumptions & contingencies
The user chose exact saved-state equality, non-postponing autosave and all persisted changes. No-path projects continue prompting only on explicit save/close. A notification is an invalidation: a no-op setter may arm the timer, but clean state must never be written by autosave. If tests reveal nondeterministic QDom attribute enumeration, replace the equality operand with a canonical DOM representation (sort attributes, preserve child order) derived from the SAME serialized bytes on each side; retain original bytes for writing. Do not silently switch to monotonic dirty flags. This task does not repair unrelated serialization fidelity gaps or add retries.
