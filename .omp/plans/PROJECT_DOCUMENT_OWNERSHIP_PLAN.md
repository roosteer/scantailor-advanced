## Context

La richiesta riguarda il rilievo «Nothing owns the project document» del rapporto `/tmp/architecture-review-20261027-230714.html`. Nel documento il titolo appartiene a `#c3`; `#c2` riguarda invece «apply to pages». L'obiettivo è un unico `ProjectDocument` condiviso da GUI, CLI e salvataggio d'emergenza, con invarianti centralizzati e uno strato app collegabile ai test.

## Findings verified so far

- Il rapporto propone di riunire pagine, disambiguatore, generazione nomi, selezione, stadi e directory di output; richiede eliminazione del salvataggio duplicato e verifica dello strato app.
- Le evidenze derivano dal codice letto, con fallback sorgente per i riferimenti che indice e clangd non ricostruiscono completamente; non basare la migrazione sui soli archi del grafo.
- Stato iniziale: file non tracciati `.omp/config.yml`, `.omp/plans/APPLY_PAGES_OWNERSHIP_PLAN.md`, `.omp/plans/STAGE_CHAIN_AND_SEAMS_PLAN.md`. Sono preesistenti e non fanno parte della modifica.

- `ProjectReader` e `ProjectWriter` esistono già in `core`: risolvono percorsi relativi rispetto al file di progetto, conservano il mapping di disambiguazione e serializzano le impostazioni degli stadi. Riutilizzarli: nessun nuovo codec o repository astratto.
- La scrittura attuale usa `QFile` direttamente (`ProjectWriter.cpp:80-111`), non una sostituzione atomica; il refactor non deve introdurre incidentalmente una nuova politica I/O.
- Le quattro costruzioni del writer sono `MainWindow::closeProjectInteractive`, `MainWindow::saveProjectWithFeedback`, `OutOfMemoryDialog::saveProjectWithFeedback`, `PipelineSession::saveProject`.
- `SelectedPage` conserva la sotto-pagina quando si passa a `IMAGE_VIEW` sulla stessa immagine. La GUI ripristina esatta → sinistra → destra → singola → prima miniatura visualizzata. Quest'ultima può essere ordinata diversamente dall'ordine naturale del documento.
- La CLI usa una fotografia `PAGE_VIEW` per la corsa, applica i default solo alle immagini nuove e salva oggi la prima pagina della fotografia come selezionata. Conservare questa politica esplicitamente nell'adapter CLI, senza ricostruire un documento al salvataggio.
- `OutputFileNameGenerator` possiede già disambiguatore e directory; non aggiungere copie autonome di questi due dati al nuovo proprietario.

## Approach

Eseguire i passaggi nell'ordine seguente. Il passaggio 1 è indipendente dagli adapter; i passaggi GUI e CLI possono essere implementati separatamente dopo il suo completamento. Nel passaggio 2 mantenere temporaneamente i metodi di feedback esistenti facendoli delegare a `document->save`; il passaggio 3 li elimina quando introduce l'interazione comune. Registrare inizialmente la nuova sorgente app nell'eseguibile attuale; il passaggio 5 sposta l'intero insieme nella libreria, senza lasciare un albero non compilabile fra i passaggi.


### 1. Unico proprietario in core

Creare `src/core/ProjectDocument.{h,cpp}`, registrati nel target `core`. Non esiste un proprietario equivalente nei simboli e consumatori verificati; `ProjectReader` e `ProjectWriter` restano i codec interni riutilizzati, non sono sostituiti.

Interfaccia da fissare nell'implementazione:

```cpp
static std::shared_ptr<ProjectDocument> createEmpty(const PageSelectionAccessor& accessor);
static std::shared_ptr<ProjectDocument> create(
    const std::shared_ptr<ProjectPages>& pages, const QString& outputDir,
    const PageSelectionAccessor& accessor);
static std::shared_ptr<ProjectDocument> fromProject(
    const ProjectReader& reader, const QString& outputDir,
    const PageSelectionAccessor& accessor);
const std::shared_ptr<ProjectPages>& pages() const;
const std::shared_ptr<StageSequence>& stages() const;
const OutputFileNameGenerator& outputFileNameGenerator() const;
const SelectedPage& selectedPage() const;
bool isLoaded() const;
void selectPage(const PageId& page, PageView view);
PageId resolveSelection(const PageSequence& availablePages, PageView view,
                        const PageId& fallback) const;
std::vector<PageInfo> insertImage(const ImageInfo& image, BeforeOrAfter position,
                                  const ImageId& existing, PageView view);
void setLayoutDirection(Qt::LayoutDirection direction);
void performRelinking(const AbstractRelinker& relinker);
bool save(const QString& filePath) const;
```

Il documento è non copiabile. Possiede solo `shared_ptr<ProjectPages>`, `shared_ptr<StageSequence>`, `OutputFileNameGenerator` e `SelectedPage`; il generatore possiede già disambiguatore e directory. I riferimenti condivisi ai modelli servono ai task esistenti, ma gli adapter non devono conservarne copie come stato autonomo.

Le factory creano generatore, registrano le immagini in ordine `IMAGE_VIEW`, creano gli stadi, poi caricano le impostazioni se provengono da `ProjectReader`. `createEmpty` produce pagine/stadi nuovi e selezione nulla; `fromProject` richiede un reader già valido e prende la sua selezione e il suo disambiguatore. Nessun accesso a widget, filesystem per creare directory, QSettings o default CLI nelle factory.

Le factory ricevono dipendenze già valide: `create` richiede pagine non nulle, `fromProject` richiede `reader.success()`; usare assert per queste precondizioni interne, lasciando gli errori di input ai controlli già presenti negli adapter. `create` non applica default di filtro. Il documento non crea thread né cache e non possiede il provider GUI: `StageSequence` continua a ricevere il `PageSelectionAccessor` esistente. Non introdurre interfacce astratte per un singolo codec o riferimenti da `core` a `app`/`app_cli`.


`resolveSelection` riproduce esatta → sinistra → destra → singola → fallback; su sequenza vuota restituisce `PageId()` senza indicizzare la prima pagina. Il fallback appartiene alla sequenza ed è la prima pagina visualizzata fornita dall'adapter, non necessariamente la prima naturale. Il metodo è una query: solo `selectPage` cambia la selezione usando `SelectedPage::set`. Calcolare i quattro candidati su stack e cercarne la presenza in una sola scansione, senza costruire altre sequenze o contenitori dinamici.

`insertImage` riutilizza `ProjectPages::insertImage`, registra i percorsi solo per risultati effettivamente inseriti e restituisce le pagine senza riordinarle. `setLayoutDirection` mantiene allineati pagine e generatore. `performRelinking` richiama nell'ordine pagine, stadi e generatore. `save` è l'unico punto produttivo di assemblaggio di `ProjectWriter`, con pagine, selezione e nomi del documento più tutti i suoi filtri.

### 2. Migrare la GUI e il salvataggio d'emergenza

In `MainWindow.{h,cpp}` eliminare `m_pages`, `m_stages`, `m_outFileNameGen`, `m_selectedPage` e `updateDisambiguationRecords`; conservare `m_document` come unico proprietario. Inizializzarlo con `createEmpty(newPageSelectionAccessor())` prima di usare gli stadi. Tutti gli accessi ai vecchi campi nello stesso file diventano accessi al documento, compresi dipendenze `StageChain`, propagatori, DPI, enumerazione dei percorsi, rimozione e cancellazione dei file di output. Non creare nuovi alias persistenti.

Sostituire il metodo privato a quattro argomenti con:

```cpp
void switchToProject(std::shared_ptr<ProjectDocument> document,
                     const QString& projectFilePath = QString());
```

Migrare i tre consumatori letti: `newProjectCreated` usa `create` con pagine nuove; `projectOpened` usa `fromProject` dopo la validazione di `ProjectOpeningContext`; `closeProjectWithoutSaving` usa `createEmpty`. Il nuovo metodo interrompe batch e coda interattiva prima di sostituire il proprietario e mantiene l'ordine attuale di cache, vista degli stadi, propagatori, miniatura e aggiornamenti UI. Non conservare il vecchio metodo come wrapper.

Preservare anche la selezione ricordata dalla GUI nei passaggi senza reader: prima di installare il documento nuovo o vuoto, `newProjectCreated` e `closeProjectWithoutSaving` chiamano `document->selectPage(m_document->selectedPage().get(PAGE_VIEW), PAGE_VIEW)`. Questa copia esplicita conserva il comportamento corrente; non trasformare il refactor in una correzione della selezione persistente fra chiusura e riapertura degli stessi input. Il documento iniziale resta invece privo di selezione. Il percorso del file rimane proprietà dell'adapter, non del documento.


In `resetThumbSequence`, conservare in una variabile la sola `PageSequence` già necessaria a `reset`, riutilizzarla per `resolveSelection` e passare `m_thumbSequence->firstPage().id()` come fallback dopo l'ordinamento. Chiamare una sola `setSelection` col risultato; mantenere invariato il ramo `KEEP_SELECTION`. `currentPageChanged` e l'aggiornamento finale di `performRelinking` usano `selectPage`; il confronto in `filterResult` usa `selectedPage().get(getCurrentView())`. La scelta della selezione dopo eliminazione resta nella UI, perché dipende dall'ordine visualizzato e dalla selezione multipla.

`toggleTwoPageSpreadReadingOrder` richiama `document->setLayoutDirection`. `insertImage` chiama l'operazione del documento, poi mantiene l'inversione `BEFORE` e l'inserimento delle miniature, senza registrare nomi nella UI. `performRelinking` delega la mutazione coerente al documento e mantiene nella finestra cache, reset delle miniature e reload.

`closeProjectInteractive` conserva la politica `Backup.<nome>`, confronto file, prompt e rinomina, ma scrive il backup con `m_document->save(backupFilePath)`. `isProjectLoaded` delega al predicato del documento.

Cambiare `OutOfMemoryDialog::setParams` in:

```cpp
void setParams(const QString& projectFile,
               std::shared_ptr<ProjectDocument> document);
```

Eliminare i quattro campi documentali del dialogo. `MainWindow::handleOutOfMemorySituation` consegna lo stesso `shared_ptr` prima di `closeProjectWithoutSaving`; il dialogo deve poter salvare il vecchio documento anche dopo distruzione della finestra. Non fare un clone, non ricostruire gli stadi, non leggere la selezione attraverso il provider GUI durante `save`.

### 3. Eliminare anche la duplicazione di Salva con nome

Creare `src/app/ProjectSaveInteraction.{h,cpp}`: non c'è un equivalente nei flussi letti; le due routine esistenti ripetono scelta percorso, estensione, scrittura, QSettings e progetti recenti. Usare due funzioni nel namespace `project_save`:

```cpp
bool saveWithFeedback(QWidget* parent, const ProjectDocument& document,
                      const QString& filePath);
bool saveAs(QWidget* parent, const ProjectDocument& document,
            QString& currentProjectFile);
```

`saveWithFeedback` chiama `document.save` e mostra lo stesso warning in caso di `false`. `saveAs` usa come directory iniziale il padre del file corrente o `QSettings["project/lastDir"]`, invoca `QFileDialog::getSaveFileName`, restituisce `false` su cancellazione, aggiunge `.ScanTailor` solo se manca con confronto case-insensitive, poi salva. Solo dopo successo aggiorna `currentProjectFile`, `project/lastDir` e `RecentProjects`. Cancellazione o errore non cambiano percorso né impostazioni. Non catturare nuove eccezioni e non cambiare il comportamento del writer.

Conservare le traduzioni esistenti tramite `QCoreApplication::translate("MainWindow", ...)` per `"Scan Tailor Projects"`, `"Error"` e `"Error saving the project file!"`; comporre lo stesso filtro `" (*.ScanTailor)"`.

`MainWindow::saveProjectAsTriggered` e `OutOfMemoryDialog::saveProjectAs` diventano chiamate alla funzione comune più la rispettiva presentazione del successo (`updateWindowTitle` o `showSaveSuccessScreen`). I salvataggi normali/autosave usano `saveWithFeedback`; eliminare i due metodi privati `saveProjectWithFeedback` e i commenti `XXX` sulle duplicazioni. Il backup di chiusura continua a chiamare direttamente il documento per mantenere la sua gestione errori specifica.

### 4. Migrare la CLI conservandone la politica

In `PipelineSession.{h,cpp}` sostituire `m_pages`, `m_stages`, `m_disambiguator`, `m_outFileNameGen`, `m_outputDir` con `m_document`; restano request, fotografia `m_pageSequence`, cache e directory temporanea delle miniature.

`loadImages` conserva lettura metadati, ordine degli input e scelta `ONE_PAGE`/`TWO_PAGES`/`AUTO_PAGES`; costruisce pagine e `CliPageSelectionProvider` locali, quindi `ProjectDocument::create`. Continua ad applicare `loadDefaultSettings` a tutte le pagine e tutti gli stadi soltanto per immagini nuove.

`loadProject` conserva errori e controlli attuali prima di pubblicare il documento: file/XML/versione, immagini mancanti, directory effettiva nell'ordine output esplicito → `reader.outputDirectory()` → padre del progetto. Costruisce il provider sulle `reader.pages()` e usa `fromProject`; non rilegge direttamente le impostazioni dei filtri.

`open` usa la fotografia `PAGE_VIEW` per rifiutare una corsa senza pagine con l'attuale errore `"No pages to process."`, poi imposta una sola volta `document->selectPage(m_pageSequence.pageAt(0).id(), PAGE_VIEW)` per preservare la selezione che la CLI salva oggi. Conserva creazione/errore della directory di output e durata della cache, ma elimina il ciclo di registrazione dei nomi duplicato con la GUI. `configure`, `process`, `pages()` e `stages()` usano i modelli del documento senza cambiare la fotografia della corsa.

`saveProject` chiama solo `m_document->save(path)` e mantiene `ExitCode::OUTPUT_ERROR` e `"Failed to save project file: %1"`; non costruisce più `SelectedPage` o `ProjectWriter`. La CLI continua a dipendere solo da `core`, non dalla libreria GUI.

### 5. Rendere collegabile lo strato app

Il minimo dichiarato è CMake 3.9: non introdurre collegamento diretto a OBJECT library che richieda CMake 3.12, né alzare il minimo per questo refactor.

Definire `app_resource_files` una sola volta in `src/CMakeLists.txt`, prima di `add_subdirectory(app)`, con i tre percorsi assoluti già in uso sotto `${SCANTAILOR_RESOURCES_DIR}`: `resources.qrc`, `dark_scheme/dark_scheme.qrc`, `light_scheme/light_scheme.qrc`. Eliminare la lista locale equivalente da `src/app/CMakeLists.txt`.

Definire `app_lib STATIC` con tutte le sorgenti GUI e i `.ui` attuali, inclusa `ProjectSaveInteraction`, escludendo solo `main.cpp` e i `.qrc`. Collegare `core` e `${EXTRA_LIBS}` come dipendenze `PUBLIC`; mantenere l'include TIFF corrente. Pubblicare `src/app` e i due percorsi `${CMAKE_CURRENT_BINARY_DIR}/app_lib_autogen/include` e `${CMAKE_CURRENT_BINARY_DIR}/app_lib_autogen/include_$<CONFIG>` perché le intestazioni app includono `ui_*.h`. Abilitare `AUTOUIC` e `AUTOMOC` per la libreria.

L'eseguibile `scantailor-advanced` conserva `main.cpp`, `${app_resource_files}`, risorse Windows, installazione e `windeployqt`; collega `app_lib`. Non spostare le risorse Qt nell'archivio statico: i relativi registratori rischierebbero di non essere estratti dal linker. Conservare la scansione traduzioni includendo tutte le sorgenti prima presenti, anche `main.cpp`.

In `src/qt_tests/CMakeLists.txt` aggiungere un eseguibile distinto `app_tests` con i nuovi `app_main.cpp`, `tst_project_document_ui.{h,cpp}` e `${app_resource_files}`; collegarlo a `app_lib`, `${Qt_Test_lib}` e `${EXTRA_LIBS}`, abilitare `AUTORCC` e registrarlo con `add_test(NAME app_tests COMMAND app_tests)` e ambiente CTest `QT_QPA_PLATFORM=offscreen`. Tenere invariati runner e dipendenze di `qt_tests`: il processo separato evita contaminazioni del singleton delle impostazioni nei test esistenti. Non aggiungere getter di `MainWindow` o `PipelineSession` esclusivamente per i test.

Il runner `app_main.cpp` crea `QApplication`, nomi app/organizzazione `"scantailor-advanced-app-tests"`, una `QTemporaryDir` viva fino all'uscita, `ConfigPaths::setPortableRoot(temp.path())` e `configureQSettings()` prima di qualsiasi accesso a `ApplicationSettings`. Inizializza tema e icone usando lo stesso `ColorSchemeFactory`/`ColorSchemeManager` e `StyledIconPack::createDefault()` già usati da `src/app/main.cpp`; disabilita i dialoghi nativi nei soli test tramite `Qt::AA_DontUseNativeDialogs`. Esegue `TestProjectDocumentUi` con `QTest::qExec`.

Se una compilazione concorrente di un consumatore fallisce per `ui_*.h` non ancora generato, aggiungere la dipendenza esplicita di `scantailor-advanced` e `app_tests` da `app_lib_autogen`; non copiare intestazioni generate nel sorgente.

## Critical files & anchors

- `src/core/ProjectWriter.cpp:31-64,80-111,176-204`: snapshot e serializzazione esistenti, inclusa la selezione di sotto-pagine; riutilizzarli senza cambiare schema o politica di scrittura.
- `src/app/ThumbnailSequence.cpp:491-557,753-809`: `reset` conserva l'ordine visualizzato e il lookup di `setSelection` fallito non emette segnali; l'estrazione deve mantenere questa semantica.
- `src/app/MainWindow.cpp:348-356,1580-1589`: cancellazione/shutdown dei task e trasferimento al dialogo OOM definiscono la durata del documento.
- `src/app/main.cpp:35-58`: bootstrap di configurazione, tema e icone da riprodurre nel processo di test, senza spostare `main` nella libreria.


## Verification

### Contratti del documento

Aggiungere `src/core/tests/TestProjectDocument.cpp` a `core_tests`, suite `ProjectDocumentTestSuite`, usando il runner Boost già presente (senza QApplication), `QTemporaryDir`, metadati sintetici e un provider senza widget sul modello degli altri test di core. Verificare risultati osservabili, non numero di getter o dettagli dei campi privati:

1. Due file distinti `a/page.png`, `b/page.png` registrati in quell'ordine producono `page.tif`, `page(1).tif`; dopo salvataggio e riapertura i nomi restano identici. Inserire `c/page.png` produce `page(2).tif`; inserimento fallito rispetto a un'immagine assente non consuma un'etichetta.
2. Round trip con file multipagina, RTL e pagina destra selezionata: preservare ID, ordine delle pagine, nomi `_page0001`/`_page0002`, suffissi destra/sinistra e selezione. Applicare una rotazione non predefinita tramite `fixOrientationFilter()->getSettings()->applyRotation` e verificarla dopo `fromProject` tramite `getRotationFor`.
3. Salvare il progetto accanto a input/output sotto la stessa directory temporanea; spostare l'intero albero in una seconda directory temporanea e riaprire col nuovo percorso del file: input/output risolti nella nuova radice e stessi nomi generati. È una prova semantica, non un confronto byte-per-byte dell'XML.
4. Ripristino selezione: esatta, sola metà sinistra disponibile, sola destra, ritorno a singola, immagine scomparsa con fallback diverso dalla prima naturale, sequenza vuota. Passare dalla pagina destra a `IMAGE_VIEW` sulla stessa immagine non deve perdere la destra al ritorno a `PAGE_VIEW`.
5. Cambio LTR↔RTL e relinking di input/output con un relinker deterministico: controllare insieme ordine, nomi/percorso di output e impostazioni indicizzate con il nuovo ImageId; la tabella di disambiguazione non deve perdere le etichette.
6. `createEmpty` ha sequenza e selezione vuote; il predicato caricato resta legato alla directory secondo la scelta dell'utente. Salvataggio verso una directory padre inesistente restituisce `false`; nessuna eccezione o nuova categoria di errore.

### Integrazione app

In `TestProjectDocumentUi` usare immagini reali generate con `QImage::save`, metadati 300×300 DPI, input e directory di output esistenti dentro una `QTemporaryDir`; costruire il file iniziale tramite `ProjectDocument::save`. Così `MainWindow::openProject` non apre dialoghi DPI o relinking. Osservare `allPages()`, `selectedPages()`, azioni trovate con `findChild<QAction*>`, titolo e contenuto del file salvato; invocare i metodi privati solo attraverso i loro slot Qt esistenti con `QMetaObject::invokeMethod`, non con friend, macro `private` o getter di test.

- Aprire il progetto su una finestra inizialmente vuota: Save disabilitato prima, abilitato dopo, pagine corrette, selezione restaurata. Invocare `goLastPage` e `saveProjectTriggered`, rileggere il file e verificare l'ultima pagina selezionata e una rotazione non predefinita conservata. Chiudere senza ulteriori cambiamenti attraverso `closeProject`: sequenza vuota e azioni di salvataggio disabilitate, senza prompt perché il backup coincide.
- Esercitare `project_save::saveAs` con un `QTimer` che accetta/rifiuta il vero `QFileDialog` non nativo. Verificare estensione aggiunta, estensione maiuscola non duplicata, percorso corrente e progetti recenti aggiornati solo dopo successo. Su cancellazione verificare che percorso, `project/lastDir` e recenti non cambino. Esercitare l'errore di scrittura attraverso `project_save::saveWithFeedback` verso un padre inesistente, chiudere il warning atteso e verificare il risultato `false` e l'assenza del file. Non introdurre un mock del writer o un nuovo injection point nel prodotto.
- Salvare da un `OutOfMemoryDialog` con il documento condiviso dopo che ogni altro proprietario è stato rilasciato: il file deve contenere nomi, selezione e impostazioni precedenti e il dialogo deve mostrare `saveSuccessPage`.
- Eseguire anche il passaggio reale dalla finestra: istanziare `MainWindow` sull'heap, aprire il progetto, scegliere l'ultima pagina, invocare direttamente lo slot `handleOutOfMemorySituation`, attendere la distruzione tramite `QPointer` ed eventi `DeferredDelete`, trovare il dialogo fra `QApplication::topLevelWidgets()` e premere `saveProjectBtn`. Rileggere il file dopo la distruzione della finestra e verificare selezione e impostazioni. Non provocare un vero esaurimento memoria né chiamare `OutOfMemoryHandler`, il cui latch è globale; non rendere opzionale questa prova di durata.

### Smoke sui binari reali

Prerequisiti verificati nella macchina di pianificazione: `cmake`, `ctest`, `python3` e ImageMagick `magick` disponibili; `build-tests` è Debug/Qt5/Unix Makefiles con `BUILD_TESTS=ON`, `build` è Release con `BUILD_TESTS=OFF`. Tutti i comandi seguenti si eseguono dalla radice del repository dopo approvazione, non durante la pianificazione.

```sh
cmake -S . -B build-tests -DBUILD_TESTS=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build-tests --parallel 4
./build-tests/core_tests --run_test=ProjectDocumentTestSuite --log_level=message
QT_QPA_PLATFORM=offscreen ./build-tests/app_tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build-tests --output-on-failure
cmake -S . -B build -DBUILD_TESTS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build --parallel 4 --target scantailor-advanced scantailor-advanced-cli
```

Risultati richiesti: suite del documento senza QApplication, nuova suite app e suite esistenti verdi; i due eseguibili compilano anche con test disabilitati. Usare la build completa per ricostruire ogni consumatore del nuovo header, non soltanto `core_tests`. Il layout degli include AUTOUIC single/multi-config è documentato da [CMake](https://github.com/Kitware/CMake/blob/master/Help/prop_tgt/AUTOUIC.rst); il multi-config non è stato esercitato in pianificazione. Se è disponibile un generatore multi-config, configurare una directory distinta con `cmake -S . -B build-document-multiconfig -G "Ninja Multi-Config" -DBUILD_TESTS=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE`, poi `cmake --build build-document-multiconfig --config Debug --parallel 4 --target scantailor-advanced app_tests`; altrimenti riportare quel canale come non verificato, senza cambiare generatore di una directory esistente.

Smoke CLI con fixture temporanea indipendente dai test:

```sh
work=$(mktemp -d /tmp/st-project-document.XXXXXX)
export ST_DOCUMENT_SMOKE="$work"
mkdir -p "$work/a" "$work/b" "$work/out-a" "$work/out-b" "$work/config"
magick -size 600x800 xc:white -fill black -draw 'rectangle 100,100 500,150' \
  -units PixelsPerInch -density 300 "$work/a/page.png"
magick -size 600x800 xc:white -fill black -draw 'rectangle 100,200 500,250' \
  -units PixelsPerInch -density 300 "$work/b/page.png"
QT_QPA_PLATFORM=offscreen XDG_CONFIG_HOME="$work/config" \
  ./build-tests/scantailor-advanced-cli --output=json --layout=1 \
  --orientation=90 --output-dpi=300 --output-project="$work/project.ScanTailor" \
  "$work/a/page.png" "$work/b/page.png" "$work/out-a"
QT_QPA_PLATFORM=offscreen XDG_CONFIG_HOME="$work/config" \
  ./build-tests/scantailor-advanced-cli --output=json \
  --output-project="$work/reloaded.ScanTailor" "$work/project.ScanTailor" "$work/out-b"
```

Entrambe le corse devono uscire con 0 e JSON `status="ok"`, `pages_processed=2`, `pages_failed=0`; `output_files[*].output_file` deve indicare esattamente `page.tif` e `page(1).tif` nella rispettiva directory. Eseguire `magick identify "$work/out-a/page.tif" "$work/out-a/page(1).tif" "$work/out-b/page.tif" "$work/out-b/page(1).tif"` e richiedere decodifica riuscita. Per ciascun file `.ScanTailor` leggere con Python `xml.etree.ElementTree`: in `pages/page` deve essere selezionata solo la prima pagina; entrambi gli elementi `filters/fix-orientation/image/rotation` devono avere `degrees="90"`; `outputDirectory`, risolto rispetto al padre del progetto se relativo, deve essere rispettivamente `out-a` e `out-b`. Il secondo comando prova realmente caricamento → esecuzione → nuovo salvataggio, non solo creazione del file.

Avviare inoltre il vero `build-tests/scantailor-advanced` sul primo progetto con configurazione isolata, usando il desktop disponibile. Verificare con screenshot fresco miniature, icone/tema, titolo e selezione; scegliere l'altra pagina, salvare, chiudere e riaprire per verificare che la selezione e i parametri persistano. Questa osservazione GUI si aggiunge ai test offscreen. Se manca un desktop autorizzato, riportare esattamente questa verifica visiva come non eseguita, senza sostituirla con un'affermazione basata sul solo linking.

Per controllare il cutover usare `grep` sulle superfici migrate: nessuna costruzione di `ProjectWriter` in `src/app`/`src/app_cli`; nessun `registerFile` negli adapter; nessun vecchio campo documentale nelle tre classi. `ProjectWriter` resta in `core` perché i filtri lo usano come codec. Le ricerche LSP effettuate in pianificazione sottostimavano i riferimenti C++: verificare anche i callsite testuali elencati, non dedurre completezza dal solo indice.

## Assumptions & contingencies

- Decisione esplicita dell'utente: preservare il comportamento attuale di «progetto aperto». `ProjectDocument::isLoaded()` centralizza `!outputFileNameGenerator().outDir().isEmpty()`; non introdurre uno stato esplicito né un nuovo flusso GUI per directory vuota.
- Conservare formato `.ScanTailor`, precedenze della CLI, politica di selezione CLI e messaggi esistenti. Non aggiungere atomicità del salvataggio, migrazioni o interfacce astratte del codec.

