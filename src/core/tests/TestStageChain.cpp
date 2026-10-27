// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <ImageFileInfo.h>
#include <ImageId.h>
#include <LoadFileTask.h>
#include <PageId.h>
#include <PageInfo.h>
#include <PageRange.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <PageSequence.h>
#include <PageView.h>
#include <ProjectPages.h>
#include <StageChain.h>
#include <StageSequence.h>

#include <boost/test/unit_test.hpp>
#include <QFileInfo>
#include <memory>
#include <set>
#include <vector>

namespace Tests {

namespace {

class NoSelectionProvider : public PageSelectionProvider {
 public:
  explicit NoSelectionProvider(std::shared_ptr<ProjectPages> pages) : m_pages(std::move(pages)) {}

  PageSequence allPages() const override { return m_pages->toPageSequence(PAGE_VIEW); }

  std::set<PageId> selectedPages() const override { return std::set<PageId>(); }

  std::vector<PageRange> selectedRanges() const override { return std::vector<PageRange>(); }

 private:
  std::shared_ptr<ProjectPages> m_pages;
};

}  // namespace

BOOST_AUTO_TEST_SUITE(StageChainTestSuite)

/**
 * This suite has no QApplication, so a passing run also proves that building
 * the stage graph never touches a widget.
 */
BOOST_AUTO_TEST_CASE(chain_builds_without_widgets) {
  // The file-based constructor expands its input by metadata entry, so the
  // fixture needs one entry for the image to exist at all.
  std::vector<ImageFileInfo> files;
  files.emplace_back(QFileInfo(QStringLiteral("/nonexistent/page.png")),
                     std::vector<ImageMetadata>{ImageMetadata(QSize(300, 400), Dpi(300, 300))});

  auto pages = std::make_shared<ProjectPages>(files, ProjectPages::ONE_PAGE, Qt::LeftToRight);
  auto provider = std::make_shared<NoSelectionProvider>(pages);
  auto stages = std::make_shared<StageSequence>(pages, PageSelectionAccessor(provider));

  const StageChain chain(stages, StageChain::Deps{nullptr, pages, OutputFileNameGenerator(), false, false});

  const PageInfo page = pages->toPageSequence(PAGE_VIEW).pageAt(0);
  const int lastStage = stages->count() - 1;

  BOOST_REQUIRE_EQUAL(stages->count(), 6);

  const std::shared_ptr<BackgroundTask> fullChain = chain.taskFor(page, lastStage);
  BOOST_REQUIRE(fullChain != nullptr);
  BOOST_CHECK(dynamic_cast<LoadFileTask*>(fullChain.get()) != nullptr);

  BOOST_CHECK(chain.taskFor(page, 3) != nullptr);
  BOOST_CHECK(chain.cacheDrivenTaskFor(lastStage) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
