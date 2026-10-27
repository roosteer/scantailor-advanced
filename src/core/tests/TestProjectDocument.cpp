// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <Dpi.h>
#include <ProjectDocument.h>
#include <ProjectPages.h>
#include <ProjectReader.h>

#include <AbstractRelinker.h>
#include <ImageId.h>
#include <ImageInfo.h>
#include <ImageMetadata.h>
#include <OrthogonalRotation.h>
#include <PageId.h>
#include <PageInfo.h>
#include <PageRange.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <PageSequence.h>
#include <PageView.h>
#include <RelinkablePath.h>
#include <StageSequence.h>
#include <boost/test/unit_test.hpp>
#include <filters/fix_orientation/Filter.h>
#include <filters/fix_orientation/Settings.h>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSize>
#include <QTemporaryDir>

#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace Tests {

namespace {

/** Selection provider with no widgets: everything the stages may ask is in the pages. */
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

/** Rewrites every path under \p from to the same path under \p to, as the relinking dialog does. */
class PrefixRelinker : public AbstractRelinker {
 public:
  PrefixRelinker(QString from, QString to) : m_from(std::move(from)), m_to(std::move(to)) {}

  QString substitutionPathFor(const RelinkablePath& origPath) const override {
    const QString& path = origPath.normalizedPath();
    if (path.startsWith(m_from)) {
      return m_to + path.mid(m_from.size());
    }
    return path;
  }

 private:
  QString m_from;
  QString m_to;
};

PageSelectionAccessor accessorFor(const std::shared_ptr<ProjectPages>& pages) {
  return PageSelectionAccessor(std::make_shared<AllPagesProvider>(pages));
}

ImageInfo imageInfoFor(const QString& filePath, int numSubPages = 1, int page = 0) {
  const ImageId imageId(filePath, page);
  const ImageMetadata metadata(QSize(600, 800), Dpi(300, 300));
  return ImageInfo(imageId, metadata, numSubPages, false, false);
}

/** A single logical page that keeps only one half of a spread. */
ImageInfo halfPageFor(const QString& filePath, bool leftRemoved, bool rightRemoved) {
  const ImageId imageId(filePath);
  const ImageMetadata metadata(QSize(600, 800), Dpi(300, 300));
  return ImageInfo(imageId, metadata, 1, leftRemoved, rightRemoved);
}

void touchFile(const QString& filePath) {
  QDir().mkpath(QFileInfo(filePath).absolutePath());
  QFile file(filePath);
  BOOST_REQUIRE(file.open(QIODevice::WriteOnly));
  file.write("not really an image");
}

OrthogonalRotation quarterTurns(int count) {
  OrthogonalRotation rotation;
  for (int i = 0; i < count; ++i) {
    rotation.nextClockwiseDirection();
  }
  return rotation;
}

/** Reopens \p projectFilePath the way the editor does once the opening checks passed. */
std::shared_ptr<ProjectDocument> openDocument(const QString& projectFilePath) {
  QFile file(projectFilePath);
  BOOST_REQUIRE(file.open(QIODevice::ReadOnly));

  QDomDocument xml;
  BOOST_REQUIRE(xml.setContent(&file));
  file.close();

  const ProjectReader reader(xml, projectFilePath);
  BOOST_REQUIRE(reader.success());

  return ProjectDocument::fromProject(reader, reader.outputDirectory(), accessorFor(reader.pages()));
}

QStringList pageNames(const ProjectDocument& project) {
  QStringList names;
  for (const PageInfo& page : project.pages()->toPageSequence(PAGE_VIEW)) {
    names << project.outputFileNameGenerator().fileNameFor(page.id());
  }
  return names;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(ProjectDocumentTestSuite)

/**
 * Two inputs sharing a basename must keep distinct output names across a save and a reopen:
 * one page silently overwriting another's output is the failure this guards against.
 */
BOOST_AUTO_TEST_CASE(duplicate_basenames_keep_their_labels) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());

  const QString firstPage = dir.filePath("a/page.png");
  const QString secondPage = dir.filePath("b/page.png");
  touchFile(firstPage);
  touchFile(secondPage);

  const std::vector<ImageInfo> images{imageInfoFor(firstPage), imageInfoFor(secondPage)};
  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);
  auto document = ProjectDocument::create(pages, dir.filePath("out"), accessorFor(pages));

  const PageSequence sequence = pages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(sequence.numPages(), 2);
  BOOST_CHECK_EQUAL(document->outputFileNameGenerator().fileNameFor(sequence.pageAt(0).id()).toStdString(),
                    "page.tif");
  BOOST_CHECK_EQUAL(document->outputFileNameGenerator().fileNameFor(sequence.pageAt(1).id()).toStdString(),
                    "page(1).tif");

  const QString projectPath = dir.filePath("project.ScanTailor");
  BOOST_REQUIRE(document->save(projectPath));

  const auto reopened = openDocument(projectPath);
  const QStringList reopenedNames = pageNames(*reopened);
  BOOST_REQUIRE_EQUAL(reopenedNames.size(), 2);
  BOOST_CHECK_EQUAL(reopenedNames.at(0).toStdString(), "page.tif");
  BOOST_CHECK_EQUAL(reopenedNames.at(1).toStdString(), "page(1).tif");

  // A third input joins the same name group.
  const std::vector<PageInfo> inserted = document->insertImage(
      imageInfoFor(dir.filePath("c/page.png")), AFTER, sequence.pageAt(0).id().imageId(), PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(inserted.size(), 1);
  BOOST_CHECK_EQUAL(document->outputFileNameGenerator().fileNameFor(inserted[0].id()).toStdString(), "page(2).tif");

  // Inserting next to an image the project does not have inserts nothing and takes no label.
  const std::vector<PageInfo> rejected = document->insertImage(
      imageInfoFor(dir.filePath("d/page.png")), AFTER, ImageId(dir.filePath("absent/page.png")), PAGE_VIEW);
  BOOST_CHECK(rejected.empty());

  const std::vector<PageInfo> afterRejected = document->insertImage(
      imageInfoFor(dir.filePath("e/page.png")), AFTER, sequence.pageAt(1).id().imageId(), PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(afterRejected.size(), 1);
  BOOST_CHECK_EQUAL(document->outputFileNameGenerator().fileNameFor(afterRejected[0].id()).toStdString(),
                    "page(3).tif");
}

/**
 * A multi-page spread saved right-to-left keeps its pages, its suffixes, its selection and the
 * rotation applied in the editor.
 */
BOOST_AUTO_TEST_CASE(multipage_rtl_round_trip_keeps_selection_and_rotation) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());

  const QString filePath = dir.filePath("in/scan.tif");
  touchFile(filePath);

  // Pages of a multi-page file are numbered from one.
  const ImageInfo spread(ImageId(filePath, 1), ImageMetadata(QSize(600, 800), Dpi(300, 300)), 2, false, false);
  const ImageInfo back(ImageId(filePath, 2), ImageMetadata(QSize(600, 800), Dpi(300, 300)), 1, false, false);
  const std::vector<ImageInfo> images{spread, back};

  auto pages = std::make_shared<ProjectPages>(images, Qt::RightToLeft);
  auto document = ProjectDocument::create(pages, dir.filePath("out"), accessorFor(pages));

  const PageId spreadRight(spread.id(), PageId::RIGHT_PAGE);
  document->selectPage(spreadRight, PAGE_VIEW);
  document->stages()->fixOrientationFilter()->getSettings()->applyRotation(spread.id(), quarterTurns(2));

  const PageSequence beforeSave = pages->toPageSequence(PAGE_VIEW);
  const QStringList namesBeforeSave = pageNames(*document);
  BOOST_REQUIRE_EQUAL(beforeSave.numPages(), 3);
  BOOST_CHECK(beforeSave.pageAt(0).id() == PageId(spread.id(), PageId::RIGHT_PAGE));
  BOOST_CHECK_EQUAL(namesBeforeSave.at(0).toStdString(), "scan_page0001_1R.tif");
  BOOST_CHECK_EQUAL(namesBeforeSave.at(1).toStdString(), "scan_page0001_2L.tif");
  BOOST_CHECK_EQUAL(namesBeforeSave.at(2).toStdString(), "scan_page0002.tif");

  const QString projectPath = dir.filePath("project.ScanTailor");
  BOOST_REQUIRE(document->save(projectPath));

  const auto reopened = openDocument(projectPath);
  BOOST_CHECK(reopened->pages()->layoutDirection() == Qt::RightToLeft);

  const PageSequence afterLoad = reopened->pages()->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(afterLoad.numPages(), beforeSave.numPages());
  for (size_t i = 0; i < afterLoad.numPages(); ++i) {
    BOOST_CHECK(afterLoad.pageAt(i).id() == beforeSave.pageAt(i).id());
  }
  BOOST_CHECK(pageNames(*reopened) == namesBeforeSave);

  BOOST_CHECK(reopened->selectedPage().get(PAGE_VIEW) == spreadRight);
  BOOST_CHECK(reopened->stages()->fixOrientationFilter()->getSettings()->getRotationFor(spread.id())
              == quarterTurns(2));
}

/** A project keeps working after the whole scan tree is moved to another root. */
BOOST_AUTO_TEST_CASE(moved_tree_resolves_input_and_output_in_the_new_root) {
  QTemporaryDir dir;
  QTemporaryDir otherRoot;
  BOOST_REQUIRE(dir.isValid());
  BOOST_REQUIRE(otherRoot.isValid());

  const QString workDir = dir.filePath("work");
  touchFile(workDir + "/in/page.png");
  QDir().mkpath(workDir + "/out");

  const std::vector<ImageInfo> images{imageInfoFor(workDir + "/in/page.png")};
  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);
  auto document = ProjectDocument::create(pages, workDir + "/out", accessorFor(pages));

  const QString projectPath = workDir + "/project.ScanTailor";
  BOOST_REQUIRE(document->save(projectPath));

  const QString movedDir = otherRoot.filePath("moved");
  BOOST_REQUIRE(QDir().rename(workDir, movedDir));

  const auto reopened = openDocument(movedDir + "/project.ScanTailor");

  const PageSequence sequence = reopened->pages()->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(sequence.numPages(), 1);
  BOOST_CHECK_EQUAL(QDir::cleanPath(sequence.pageAt(0).imageId().filePath()).toStdString(),
                    QDir::cleanPath(movedDir + "/in/page.png").toStdString());
  BOOST_CHECK_EQUAL(QDir::cleanPath(reopened->outputFileNameGenerator().outDir()).toStdString(),
                    QDir::cleanPath(movedDir + "/out").toStdString());
  BOOST_CHECK_EQUAL(reopened->outputFileNameGenerator().fileNameFor(sequence.pageAt(0).id()).toStdString(),
                    "page.tif");
  BOOST_CHECK(QDir(movedDir + "/out").exists());
}

/**
 * Selection restore: the remembered sub-page first, then the surviving half, then the single
 * page, and the caller's fallback last. It is a query, so it must not change the selection.
 */
BOOST_AUTO_TEST_CASE(selection_falls_back_through_halves_and_single_page) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());

  // A spread offers both halves.
  const std::vector<ImageInfo> spreadImages{imageInfoFor(dir.filePath("spread.png"), 2)};
  auto spreadPages = std::make_shared<ProjectPages>(spreadImages, Qt::LeftToRight);
  auto spreadDocument = ProjectDocument::create(spreadPages, dir.filePath("out"), accessorFor(spreadPages));

  const PageSequence spreadSequence = spreadPages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(spreadSequence.numPages(), 2);
  const PageId leftPage(spreadImages[0].id(), PageId::LEFT_PAGE);
  const PageId rightPage(spreadImages[0].id(), PageId::RIGHT_PAGE);

  spreadDocument->selectPage(leftPage, PAGE_VIEW);
  BOOST_CHECK(spreadDocument->resolveSelection(spreadSequence, PAGE_VIEW, rightPage) == leftPage);

  // Leaving PAGE_VIEW for the same image must not lose the remembered half.
  spreadDocument->selectPage(rightPage, PAGE_VIEW);
  const PageId imageView = spreadDocument->selectedPage().get(IMAGE_VIEW);
  BOOST_CHECK(imageView == PageId(spreadImages[0].id(), PageId::SINGLE_PAGE));
  spreadDocument->selectPage(imageView, IMAGE_VIEW);
  BOOST_CHECK(spreadDocument->selectedPage().get(PAGE_VIEW) == rightPage);
  BOOST_CHECK(spreadDocument->resolveSelection(spreadSequence, PAGE_VIEW, leftPage) == rightPage);
  BOOST_CHECK(spreadDocument->selectedPage().get(PAGE_VIEW) == rightPage);

  // Only the left half survives.
  const std::vector<ImageInfo> leftOnly{halfPageFor(dir.filePath("left.png"), false, true)};
  auto leftPages = std::make_shared<ProjectPages>(leftOnly, Qt::LeftToRight);
  auto leftDocument = ProjectDocument::create(leftPages, dir.filePath("out"), accessorFor(leftPages));
  const PageSequence leftSequence = leftPages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(leftSequence.numPages(), 1);
  leftDocument->selectPage(PageId(leftOnly[0].id(), PageId::RIGHT_PAGE), PAGE_VIEW);
  BOOST_CHECK(leftDocument->resolveSelection(leftSequence, PAGE_VIEW, PageId()) == leftSequence.pageAt(0).id());

  // Only the right half survives.
  const std::vector<ImageInfo> rightOnly{halfPageFor(dir.filePath("right.png"), true, false)};
  auto rightPages = std::make_shared<ProjectPages>(rightOnly, Qt::LeftToRight);
  auto rightDocument = ProjectDocument::create(rightPages, dir.filePath("out"), accessorFor(rightPages));
  const PageSequence rightSequence = rightPages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(rightSequence.numPages(), 1);
  rightDocument->selectPage(PageId(rightOnly[0].id(), PageId::LEFT_PAGE), PAGE_VIEW);
  BOOST_CHECK(rightDocument->resolveSelection(rightSequence, PAGE_VIEW, PageId()) == rightSequence.pageAt(0).id());

  // A single page has neither half.
  const std::vector<ImageInfo> singleImage{imageInfoFor(dir.filePath("single.png"))};
  auto singlePages = std::make_shared<ProjectPages>(singleImage, Qt::LeftToRight);
  auto singleDocument = ProjectDocument::create(singlePages, dir.filePath("out"), accessorFor(singlePages));
  const PageSequence singleSequence = singlePages->toPageSequence(PAGE_VIEW);
  singleDocument->selectPage(PageId(singleImage[0].id(), PageId::LEFT_PAGE), PAGE_VIEW);
  BOOST_CHECK(singleDocument->resolveSelection(singleSequence, PAGE_VIEW, PageId())
              == PageId(singleImage[0].id(), PageId::SINGLE_PAGE));

  // The remembered image is gone: the caller's fallback wins, not the first page in the sequence.
  const std::vector<ImageInfo> twoPages{imageInfoFor(dir.filePath("p1.png")), imageInfoFor(dir.filePath("p2.png"))};
  auto twoPagesPages = std::make_shared<ProjectPages>(twoPages, Qt::LeftToRight);
  auto twoPagesDocument = ProjectDocument::create(twoPagesPages, dir.filePath("out"), accessorFor(twoPagesPages));
  const PageSequence twoPagesSequence = twoPagesPages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(twoPagesSequence.numPages(), 2);
  twoPagesDocument->selectPage(PageId(ImageId(dir.filePath("absent.png")), PageId::SINGLE_PAGE), PAGE_VIEW);
  BOOST_CHECK(twoPagesDocument->resolveSelection(twoPagesSequence, PAGE_VIEW, twoPagesSequence.pageAt(1).id())
              == twoPagesSequence.pageAt(1).id());

  // Nothing to select.
  BOOST_CHECK(spreadDocument->resolveSelection(PageSequence(), PAGE_VIEW, leftPage).isNull());
}

/**
 * Changing the reading direction and relinking the whole tree must move pages, output directory,
 * filter settings and disambiguation labels together.
 */
BOOST_AUTO_TEST_CASE(reading_direction_and_relinking_keep_order_names_and_settings) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());

  const QString firstPath = dir.filePath("a/spread.png");
  const QString secondPath = dir.filePath("b/spread.png");
  touchFile(firstPath);
  touchFile(secondPath);

  const std::vector<ImageInfo> images{imageInfoFor(firstPath, 2), imageInfoFor(secondPath)};
  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);
  auto document = ProjectDocument::create(pages, dir.filePath("out"), accessorFor(pages));

  document->stages()->fixOrientationFilter()->getSettings()->applyRotation(images[0].id(), quarterTurns(1));

  const PageSequence leftToRightSequence = pages->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(leftToRightSequence.numPages(), 3);
  const QStringList leftToRightNames = pageNames(*document);
  BOOST_CHECK_EQUAL(leftToRightNames.at(0).toStdString(), "spread_1L.tif");
  BOOST_CHECK_EQUAL(leftToRightNames.at(1).toStdString(), "spread_2R.tif");
  BOOST_CHECK_EQUAL(leftToRightNames.at(2).toStdString(), "spread(1).tif");

  document->setLayoutDirection(Qt::RightToLeft);
  BOOST_CHECK(document->pages()->layoutDirection() == Qt::RightToLeft);
  BOOST_CHECK(document->outputFileNameGenerator().layoutDirection() == Qt::RightToLeft);

  const PageSequence rightToLeftSequence = document->pages()->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(rightToLeftSequence.numPages(), 3);
  BOOST_CHECK(rightToLeftSequence.pageAt(0).id() == PageId(images[0].id(), PageId::RIGHT_PAGE));
  BOOST_CHECK(rightToLeftSequence.pageAt(1).id() == PageId(images[0].id(), PageId::LEFT_PAGE));
  BOOST_CHECK(rightToLeftSequence.pageAt(2).id() == PageId(images[1].id(), PageId::SINGLE_PAGE));

  const QStringList rightToLeftNames = pageNames(*document);
  BOOST_CHECK_EQUAL(rightToLeftNames.at(0).toStdString(), "spread_1R.tif");
  BOOST_CHECK_EQUAL(rightToLeftNames.at(1).toStdString(), "spread_2L.tif");
  BOOST_CHECK_EQUAL(rightToLeftNames.at(2).toStdString(), "spread(1).tif");

  const QString movedRoot = dir.filePath("moved");
  const PrefixRelinker relinker(dir.path(), movedRoot);
  document->performRelinking(relinker);

  const PageSequence relinkedSequence = document->pages()->toPageSequence(PAGE_VIEW);
  BOOST_REQUIRE_EQUAL(relinkedSequence.numPages(), rightToLeftSequence.numPages());
  for (size_t i = 0; i < relinkedSequence.numPages(); ++i) {
    BOOST_CHECK(relinkedSequence.pageAt(i).id().subPage() == rightToLeftSequence.pageAt(i).id().subPage());
  }
  BOOST_CHECK_EQUAL(QDir::cleanPath(relinkedSequence.pageAt(0).imageId().filePath()).toStdString(),
                    QDir::cleanPath(movedRoot + "/a/spread.png").toStdString());
  BOOST_CHECK_EQUAL(QDir::cleanPath(document->outputFileNameGenerator().outDir()).toStdString(),
                    QDir::cleanPath(movedRoot + "/out").toStdString());
  BOOST_CHECK(pageNames(*document) == rightToLeftNames);
  BOOST_CHECK(document->stages()->fixOrientationFilter()->getSettings()->getRotationFor(
                  ImageId(movedRoot + "/a/spread.png"))
              == quarterTurns(1));
}

/** An empty document, the loaded predicate and the write failure the caller has to handle. */
BOOST_AUTO_TEST_CASE(empty_document_and_failed_save) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());

  const std::vector<ImageInfo> noImages;
  auto emptyPages = std::make_shared<ProjectPages>(noImages, Qt::LeftToRight);
  auto emptyDocument = ProjectDocument::createEmpty(accessorFor(emptyPages));

  BOOST_CHECK_EQUAL(emptyDocument->pages()->toPageSequence(PAGE_VIEW).numPages(), 0);
  BOOST_CHECK(emptyDocument->selectedPage().isNull());
  BOOST_CHECK(!emptyDocument->isLoaded());
  BOOST_CHECK(emptyDocument->pages()->numImages() == 0);

  const std::vector<ImageInfo> images{imageInfoFor(dir.filePath("in/page.png"))};
  auto pages = std::make_shared<ProjectPages>(images, Qt::LeftToRight);

  auto noOutput = ProjectDocument::create(pages, QString(), accessorFor(pages));
  BOOST_CHECK(!noOutput->isLoaded());

  auto withOutput = ProjectDocument::create(pages, dir.filePath("out"), accessorFor(pages));
  BOOST_CHECK(withOutput->isLoaded());

  const QString unreachable = dir.filePath("missing/project.ScanTailor");
  BOOST_CHECK(!withOutput->save(unreachable));
  BOOST_CHECK(!QFile::exists(unreachable));
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
