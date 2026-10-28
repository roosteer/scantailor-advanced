// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_project_document_ui.h"

#include <ApplicationSettings.h>
#include <Dpi.h>
#include <ImageId.h>
#include <ImageInfo.h>
#include <ImageMetadata.h>
#include <PageId.h>
#include <PageInfo.h>
#include <PageRange.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <PageSequence.h>
#include <PageView.h>
#include <ProjectDocument.h>
#include <ProjectPages.h>
#include <ProjectReader.h>

#include <QAbstractButton>
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "MainWindow.h"
#include "OutOfMemoryDialog.h"

namespace {

/** Selection provider with no widgets: the dialog only needs the pages to be there. */
class AllPagesProvider : public PageSelectionProvider {
 public:
  explicit AllPagesProvider(std::shared_ptr<ProjectPages> pages) : m_pages(std::move(pages)) {}

  PageSequence allPages() const override { return m_pages->toPageSequence(PAGE_VIEW); }

  std::set<PageId> selectedPages() const override {
    std::set<PageId> all;
    const PageSequence sequence = allPages();
    for (size_t i = 0; i < sequence.numPages(); ++i) {
      all.insert(sequence.pageAt(i).id());
    }
    return all;
  }

  std::vector<PageRange> selectedRanges() const override { return std::vector<PageRange>(); }

 private:
  std::shared_ptr<ProjectPages> m_pages;
};

ImageInfo imageInfoFor(const QString& filePath) {
  const ImageId imageId(filePath);
  const ImageMetadata metadata(QSize(600, 800), Dpi(300, 300));
  return ImageInfo(imageId, metadata, 1, false, false);
}

void touchFile(const QString& filePath) {
  QDir().mkpath(QFileInfo(filePath).absolutePath());
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("not really an image");
}

/** A two-page project, ready to be handed to a widget. */
std::shared_ptr<ProjectDocument> twoPageDocument(const QString& firstPage,
                                                 const QString& secondPage,
                                                 const QString& outputDir) {
  const std::vector<ImageInfo> images{imageInfoFor(firstPage), imageInfoFor(secondPage)};
  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);
  return ProjectDocument::create(pages, outputDir, PageSelectionAccessor(std::make_shared<AllPagesProvider>(pages)));
}

/** A loadable page, so the stages compute on it instead of failing to open it. */
QString writePageImage(const QString& filePath) {
  QDir().mkpath(QFileInfo(filePath).absolutePath());
  QImage image(60, 80, QImage::Format_RGB32);
  image.fill(Qt::white);
  if (!image.save(filePath)) {
    return QString();
  }
  return filePath;
}

/** A saved project the editor opens without asking anything: real pages, 300 dpi on file. */
struct TestProject {
  QString projectPath;
  QStringList pageFiles;
};

TestProject writeProjectFile(const QString& projectPath, int pageCount) {
  TestProject project;
  const QString projectDir = QFileInfo(projectPath).absolutePath();

  std::vector<ImageInfo> images;
  for (int i = 0; i < pageCount; ++i) {
    const QString pageFile
        = projectDir + QStringLiteral("/in/page%1.png").arg(i + 1, 4, 10, QLatin1Char('0'));
    if (writePageImage(pageFile).isEmpty()) {
      return project;
    }
    project.pageFiles << pageFile;
    images.push_back(imageInfoFor(pageFile));
  }

  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);
  const std::shared_ptr<ProjectDocument> document
      = ProjectDocument::create(pages, projectDir + QStringLiteral("/out"),
                                PageSelectionAccessor(std::make_shared<AllPagesProvider>(pages)));
  if (!document->save(projectPath)) {
    return project;
  }

  project.projectPath = projectPath;
  return project;
}

QByteArray readAll(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QByteArray();
  }
  return file.readAll();
}

/** The page a saved file remembers, so a write can be observed from outside the window. */
PageId savedSelection(const QString& projectPath) {
  QFile file(projectPath);
  if (!file.open(QIODevice::ReadOnly)) {
    return PageId();
  }
  QDomDocument xml;
  if (!xml.setContent(&file)) {
    return PageId();
  }
  file.close();

  const ProjectReader reader(xml, projectPath);
  if (!reader.success()) {
    return PageId();
  }
  return reader.selectedPage().get(PAGE_VIEW);
}

/** A real window on \p projectPath, with the opening work given time to finish. */
std::unique_ptr<MainWindow> openWindow(const QString& projectPath) {
  auto window = std::make_unique<MainWindow>();
  window->show();
  window->openProject(projectPath);
  // The stages compute for every page right after opening. A checkpoint taken before that
  // would make the test measure their writes instead of the user's change.
  QTest::qWait(2000);
  return window;
}

bool invokeSlot(QObject* object, const char* name) {
  return QMetaObject::invokeMethod(object, name, Qt::DirectConnection);
}

/** Answers whatever modal box appears, so an offscreen run can never block on one. */
class DialogAnswerer : public QObject {
 public:
  explicit DialogAnswerer(QMessageBox::StandardButton answer) : m_answer(answer) {
    connect(&m_timer, &QTimer::timeout, this, &DialogAnswerer::answerModal);
    m_timer.start(20);
  }

  int dialogsSeen() const { return m_dialogsSeen; }

 private:
  void answerModal() {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box) {
      return;
    }
    ++m_dialogsSeen;

    // The save prompt gets the configured answer; a plain warning only has an acknowledgement.
    QAbstractButton* button = box->button(m_answer);
    if (!button) {
      button = box->defaultButton();
    }
    if (!button) {
      button = box->button(QMessageBox::Ok);
    }
    if (button) {
      button->click();
    }
  }

  QMessageBox::StandardButton m_answer;
  QTimer m_timer;
  int m_dialogsSeen{0};
};

/** Waits for the delayed close to run and for any dialog it puts up to be answered. */
void waitForClosed(MainWindow& window) {
  for (int i = 0; (i < 100) && window.isVisible(); ++i) {
    QTest::qWait(50);
  }
}

/** The editor's autosave deadline plus a margin for the timer to fire. */
constexpr int kAutosaveWaitMs = 62000;

/** Half of the deadline: an edit made here must not move it. */
constexpr int kAutosaveHalfWaitMs = 30000;

}  // namespace

/** The dialog saves in place only when a project file, and therefore a path, is known. */
void TestProjectDocumentUi::saveButtonFollowsTheProjectFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const std::shared_ptr<ProjectDocument> document
      = twoPageDocument(dir.filePath("in/page0001.png"), dir.filePath("in/page0002.png"), dir.filePath("out"));

  OutOfMemoryDialog dialog;
  auto* saveButton = dialog.findChild<QPushButton*>(QStringLiteral("saveProjectBtn"));
  QVERIFY(saveButton);

  // A project that was never saved has no in-place target; only "Save Project As" applies.
  dialog.setParams(QString(), document);
  QVERIFY(!saveButton->isVisibleTo(&dialog));

  dialog.setParams(dir.filePath("project.ScanTailor"), document);
  QVERIFY(saveButton->isVisibleTo(&dialog));
}

/**
 * The dialog is the last holder of the document: the window that owned it may already be
 * gone when the user presses Save, and the file must still hold both pages and the page the
 * editor was on.
 */
void TestProjectDocumentUi::savesTheDocumentAfterTheEditorIsGone() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString firstPage = dir.filePath("in/page0001.png");
  const QString secondPage = dir.filePath("in/page0002.png");
  touchFile(firstPage);
  touchFile(secondPage);

  const QString projectPath = dir.filePath("project.ScanTailor");

  OutOfMemoryDialog dialog;
  {
    const std::shared_ptr<ProjectDocument> document = twoPageDocument(firstPage, secondPage, dir.filePath("out"));
    const PageSequence sequence = document->pages()->toPageSequence(PAGE_VIEW);
    QCOMPARE(static_cast<int>(sequence.numPages()), 2);
    document->selectPage(sequence.pageAt(1).id(), PAGE_VIEW);

    dialog.setParams(projectPath, document);
  }  // The editor is gone; only the dialog keeps the document alive.

  QVERIFY(QMetaObject::invokeMethod(&dialog, "saveProject", Qt::DirectConnection));
  QVERIFY(QFileInfo::exists(projectPath));

  auto* stack = dialog.findChild<QStackedWidget*>(QStringLiteral("topLevelStack"));
  QVERIFY(stack);
  QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("saveSuccessPage"));

  // What was written is a project the editor can reopen, not just a file that exists.
  QFile file(projectPath);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QDomDocument xml;
  QVERIFY(xml.setContent(&file));
  file.close();

  const ProjectReader reader(xml, projectPath);
  QVERIFY(reader.success());

  const PageSequence reopened = reader.pages()->toPageSequence(PAGE_VIEW);
  QCOMPARE(static_cast<int>(reopened.numPages()), 2);
  QVERIFY(reader.selectedPage().get(PAGE_VIEW) == PageId(ImageId(secondPage), PageId::SINGLE_PAGE));

  // Saving again from the same dialog keeps working: the document survived the first write.
  QVERIFY(QMetaObject::invokeMethod(&dialog, "saveProject", Qt::DirectConnection));
  QVERIFY(QFileInfo::exists(projectPath));
}

/**
 * The deadline is not a trigger to rewrite: the slot it calls writes only when the model
 * differs from the file, and a write makes the file the new clean state.
 *
 * The deadline itself is exercised once, in the test below; here the slot is called directly
 * so the decision it makes is what is measured.
 */
void TestProjectDocumentUi::autosaveWritesOnlyWhatTheFileDoesNotHave() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const TestProject project = writeProjectFile(dir.filePath("project.ScanTailor"), 2);
  QVERIFY(!project.projectPath.isEmpty());
  QCOMPARE(project.pageFiles.size(), 2);

  ApplicationSettings::getInstance().setAutoSaveProjectEnabled(true);

  const std::unique_ptr<MainWindow> window = openWindow(project.projectPath);
  QVERIFY(window);

  // Put the file in the state the window ended up in, so what follows measures this test.
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);
  const QByteArray savedBytes = readAll(project.projectPath);
  QVERIFY(!savedBytes.isEmpty());

  // A change that stands is written by the next autosave, and only then.
  QVERIFY(invokeSlot(window.get(), "goLastPage"));
  QTest::qWait(200);
  QCOMPARE(readAll(project.projectPath), savedBytes);

  QVERIFY(invokeSlot(window.get(), "autoSaveProject"));
  const QByteArray writtenBytes = readAll(project.projectPath);
  QVERIFY(writtenBytes != savedBytes);
  QVERIFY(savedSelection(project.projectPath).imageId() == ImageId(project.pageFiles.at(1)));

  // The stage workers may still be landing the params of the page just visited, and each of
  // those is a real change. Let the slot drain them until an invocation finds nothing to
  // write, then hold it to silence: neither the bytes nor the modification time may move.
  QByteArray quietBytes = writtenBytes;
  QDateTime quietTime = QFileInfo(project.projectPath).lastModified();
  bool quiet = false;
  for (int attempt = 0; attempt < 10 && !quiet; ++attempt) {
    QTest::qWait(100);
    QVERIFY(invokeSlot(window.get(), "autoSaveProject"));
    const QByteArray bytesNow = readAll(project.projectPath);
    const QDateTime timeNow = QFileInfo(project.projectPath).lastModified();
    quiet = (bytesNow == quietBytes) && (timeNow == quietTime);
    quietBytes = bytesNow;
    quietTime = timeNow;
  }
  QVERIFY(quiet);

  QVERIFY(invokeSlot(window.get(), "autoSaveProject"));
  QCOMPARE(readAll(project.projectPath), quietBytes);
  QCOMPARE(QFileInfo(project.projectPath).lastModified(), quietTime);

  // And the window it leaves behind is clean enough to close without asking.
  DialogAnswerer answerer(QMessageBox::Cancel);
  window->close();
  waitForClosed(*window);
  QCOMPARE(answerer.dialogsSeen(), 0);
  QVERIFY(!window->isVisible());
}

/**
 * The first change starts the clock. A second change before the deadline is written by that
 * same deadline, so nothing waits for the user to stop editing.
 */
void TestProjectDocumentUi::laterChangesDoNotPostponeTheAutosaveDeadline() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const TestProject project = writeProjectFile(dir.filePath("project.ScanTailor"), 3);
  QVERIFY(!project.projectPath.isEmpty());
  QCOMPARE(project.pageFiles.size(), 3);

  ApplicationSettings::getInstance().setAutoSaveProjectEnabled(true);

  const std::unique_ptr<MainWindow> window = openWindow(project.projectPath);
  QVERIFY(window);
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);
  const QByteArray savedBytes = readAll(project.projectPath);

  // The first change arms the deadline.
  QVERIFY(invokeSlot(window.get(), "goNextPage"));
  QTest::qWait(kAutosaveHalfWaitMs);

  // The second change lands halfway through it.
  QVERIFY(invokeSlot(window.get(), "goNextPage"));
  QTest::qWait(kAutosaveWaitMs - kAutosaveHalfWaitMs + 3000);

  // Past the first deadline and well before a postponed one: the file already holds the
  // second change.
  QVERIFY(readAll(project.projectPath) != savedBytes);
  QVERIFY(savedSelection(project.projectPath).imageId() == ImageId(project.pageFiles.at(2)));

  DialogAnswerer answerer(QMessageBox::Cancel);
  window->close();
  waitForClosed(*window);
  QCOMPARE(answerer.dialogsSeen(), 0);
}

/** Closing a document that matches its file asks nothing and leaves no backup behind. */
void TestProjectDocumentUi::cleanCloseAsksNothingAndLeavesNoBackup() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const TestProject project = writeProjectFile(dir.filePath("project.ScanTailor"), 2);
  QVERIFY(!project.projectPath.isEmpty());

  ApplicationSettings::getInstance().setAutoSaveProjectEnabled(false);

  const std::unique_ptr<MainWindow> window = openWindow(project.projectPath);
  QVERIFY(window);
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);
  const QByteArray savedBytes = readAll(project.projectPath);

  DialogAnswerer answerer(QMessageBox::Cancel);
  window->close();
  waitForClosed(*window);

  QCOMPARE(answerer.dialogsSeen(), 0);
  QVERIFY(!window->isVisible());
  QCOMPARE(readAll(project.projectPath), savedBytes);

  // The old comparison wrote a Backup.* file next to the project to diff it. Nothing like
  // that is written now, so nothing may be left over.
  const QStringList leftovers
      = QDir(QFileInfo(project.projectPath).absolutePath()).entryList(QStringList(QStringLiteral("Backup.*")), QDir::Files);
  QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(QStringLiteral(", "))));
}

/**
 * A dirty document survives both a refused prompt and a save that cannot be written, and
 * closes once the file really holds what the editor shows.
 */
void TestProjectDocumentUi::dirtyCloseKeepsTheDocumentUnlessTheSaveSucceeds() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const TestProject project = writeProjectFile(dir.filePath("project.ScanTailor"), 2);
  QVERIFY(!project.projectPath.isEmpty());

  ApplicationSettings::getInstance().setAutoSaveProjectEnabled(false);

  const std::unique_ptr<MainWindow> window = openWindow(project.projectPath);
  QVERIFY(window);
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);
  const QByteArray savedBytes = readAll(project.projectPath);

  // Cancel: the window stays open and the file is untouched.
  QVERIFY(invokeSlot(window.get(), "goLastPage"));
  {
    DialogAnswerer answerer(QMessageBox::Cancel);
    window->close();
    waitForClosed(*window);
    QCOMPARE(answerer.dialogsSeen(), 1);
  }
  QVERIFY(window->isVisible());
  QCOMPARE(readAll(project.projectPath), savedBytes);

  // The destination cannot take the file any more: the save fails and the document is kept.
  const QString parkedProject = dir.filePath("parked.ScanTailor");
  QVERIFY(QFile::rename(project.projectPath, parkedProject));
  QVERIFY(QDir().mkpath(project.projectPath));  // a directory where the file used to be
  {
    DialogAnswerer answerer(QMessageBox::Yes);
    window->close();
    waitForClosed(*window);
    QVERIFY2(answerer.dialogsSeen() >= 2, "the prompt and the write failure both asked");
  }
  QVERIFY(window->isVisible());

  // With the destination back, Save closes the window and the file holds the moved selection.
  QVERIFY(QDir(project.projectPath).removeRecursively());
  QVERIFY(QFile::rename(parkedProject, project.projectPath));
  {
    DialogAnswerer answerer(QMessageBox::Yes);
    window->close();
    waitForClosed(*window);
    QCOMPARE(answerer.dialogsSeen(), 1);
  }
  QVERIFY(!window->isVisible());
  QVERIFY(savedSelection(project.projectPath) == PageId(ImageId(project.pageFiles.at(1)), PageId::SINGLE_PAGE));
}

/**
 * A change still waiting for its deadline must not follow the user into the next project: the
 * new document closes silently and its file is not rewritten.
 */
void TestProjectDocumentUi::switchingProjectsKeepsThePendingChangeOutOfTheNewFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const TestProject first = writeProjectFile(dir.filePath("first/project.ScanTailor"), 2);
  const TestProject second = writeProjectFile(dir.filePath("second/project.ScanTailor"), 2);
  QVERIFY(!first.projectPath.isEmpty());
  QVERIFY(!second.projectPath.isEmpty());

  ApplicationSettings::getInstance().setAutoSaveProjectEnabled(true);

  const std::unique_ptr<MainWindow> window = openWindow(first.projectPath);
  QVERIFY(window);
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);

  // A change whose deadline is already running.
  QVERIFY(invokeSlot(window.get(), "goLastPage"));

  window->openProject(second.projectPath);
  QTest::qWait(2000);
  QVERIFY(invokeSlot(window.get(), "saveProjectTriggered"));
  QTest::qWait(200);

  const QByteArray secondBytes = readAll(second.projectPath);
  const QDateTime secondTime = QFileInfo(second.projectPath).lastModified();

  // Deliver what the previous document left queued. It may only look at the new document,
  // which is still exactly what its file holds, so it can neither dirty it nor write it.
  QVERIFY(invokeSlot(window.get(), "projectDocumentChanged"));
  QTest::qWait(200);

  DialogAnswerer answerer(QMessageBox::Cancel);
  window->close();
  waitForClosed(*window);
  QCOMPARE(answerer.dialogsSeen(), 0);
  QVERIFY(!window->isVisible());
  QCOMPARE(readAll(second.projectPath), secondBytes);
  QCOMPARE(QFileInfo(second.projectPath).lastModified(), secondTime);
}
