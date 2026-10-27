// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_project_document_ui.h"

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

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <memory>
#include <set>
#include <utility>
#include <vector>

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
