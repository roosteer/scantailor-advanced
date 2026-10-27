// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/select_content/Dependencies.h>
#include <filters/select_content/Params.h>
#include <filters/select_content/Settings.h>

#include <ImageId.h>
#include <PageId.h>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace select_content;

namespace {

PageId pageIdOf(const char* path) {
  return PageId(ImageId(QString::fromLatin1(path)));
}

QPolygonF pageOutline(int width, int height) {
  QPolygonF outline;
  outline << QPointF(0, 0) << QPointF(width, 0) << QPointF(width, height) << QPointF(0, height);
  return outline;
}

Params manualParams(const QRectF& pageRect, const QRectF& contentRect) {
  return Params(contentRect, contentRect.size(), pageRect, Dependencies(), MODE_MANUAL, MODE_MANUAL, false);
}

Params automaticParams(const QRectF& pageRect, const QRectF& contentRect) {
  return Params(contentRect, contentRect.size(), pageRect, Dependencies(), MODE_AUTO, MODE_AUTO, false);
}

void requirePageParams(const std::unique_ptr<Params>& params, const QRectF& pageRect, const QRectF& contentRect) {
  BOOST_REQUIRE(params != nullptr);
  BOOST_CHECK(params->pageRect() == pageRect);
  BOOST_CHECK(params->contentRect() == contentRect);
}

}  // namespace

BOOST_AUTO_TEST_SUITE(SelectContentApplyTestSuite)

BOOST_AUTO_TEST_CASE(apply_content_box_auto_forces_recompute) {
  Dependencies currentDeps(pageOutline(100, 150), MODE_AUTO, MODE_DISABLED, false);

  Dependencies storedDepsWhenApplyingAuto(QPolygonF(), MODE_DISABLED, MODE_DISABLED, false);

  bool needUpdateContentBox = false;
  bool needUpdatePageBox = false;
  currentDeps.compatibleWith(storedDepsWhenApplyingAuto, &needUpdateContentBox, &needUpdatePageBox);

  BOOST_REQUIRE_MESSAGE(needUpdateContentBox,
                        "When applying 'Content Box: Auto' we store deps with MODE_DISABLED so that "
                        "Task::process recomputes content box for each target page.");
  BOOST_CHECK(needUpdatePageBox);
}

BOOST_AUTO_TEST_CASE(same_mode_auto_no_content_box_update) {
  Dependencies deps1(pageOutline(100, 150), MODE_AUTO, MODE_DISABLED, false);
  Dependencies deps2(pageOutline(100, 150), MODE_AUTO, MODE_DISABLED, false);

  bool needUpdateContentBox = false;
  bool needUpdatePageBox = false;
  deps1.compatibleWith(deps2, &needUpdateContentBox, &needUpdatePageBox);

  BOOST_CHECK(!needUpdateContentBox);
  BOOST_CHECK(!needUpdatePageBox);
}

BOOST_AUTO_TEST_CASE(different_outlines_force_content_box_update) {
  Dependencies currentDeps(pageOutline(100, 150), MODE_AUTO, MODE_DISABLED, false);
  Dependencies storedDeps(pageOutline(120, 150), MODE_AUTO, MODE_DISABLED, false);

  bool needUpdateContentBox = false;
  bool needUpdatePageBox = false;
  currentDeps.compatibleWith(storedDeps, &needUpdateContentBox, &needUpdatePageBox);

  BOOST_REQUIRE_MESSAGE(needUpdateContentBox,
                        "When outlines differ, needUpdateContentBox must be true (PR #108 review).");
  BOOST_CHECK(needUpdatePageBox);
}

/** Copying both boxes replaces the target layout and skips the source page itself. */
BOOST_AUTO_TEST_CASE(copy_both_boxes_replaces_the_target_layout) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, manualParams(QRectF(0, 0, 50, 50), QRectF(0, 0, 10, 10)));
  const Params sourceParams = manualParams(QRectF(10, 20, 100, 150), QRectF(30, 40, 60, 70));

  settings.copyBoxes(std::set<PageId>{source, target}, source, sourceParams, Settings::BoxSelection::Both);

  requirePageParams(settings.getPageParams(target), QRectF(10, 20, 100, 150), QRectF(30, 40, 60, 70));
  BOOST_CHECK(settings.isParamsNull(source));
}

BOOST_AUTO_TEST_CASE(copy_content_only_keeps_the_target_page_rect) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, manualParams(QRectF(0, 0, 50, 50), QRectF(0, 0, 10, 10)));
  const Params sourceParams = manualParams(QRectF(10, 20, 100, 150), QRectF(30, 40, 60, 70));

  settings.copyBoxes(std::set<PageId>{target}, source, sourceParams, Settings::BoxSelection::Content);

  // The content box follows the target page box, so it shifts by the page rect delta.
  requirePageParams(settings.getPageParams(target), QRectF(0, 0, 50, 50), QRectF(20, 20, 60, 70));
}

BOOST_AUTO_TEST_CASE(copy_page_only_keeps_the_target_content_rect) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, manualParams(QRectF(0, 0, 50, 50), QRectF(5, 5, 10, 10)));
  const Params sourceParams = manualParams(QRectF(10, 20, 100, 150), QRectF(30, 40, 60, 70));

  settings.copyBoxes(std::set<PageId>{target}, source, sourceParams, Settings::BoxSelection::Page);

  requirePageParams(settings.getPageParams(target), QRectF(10, 20, 100, 150), QRectF(5, 5, 10, 10));
}

/** Storing empty dependencies is what makes Task::process recompute an automatic box. */
BOOST_AUTO_TEST_CASE(copy_an_auto_content_box_forces_a_recompute) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, manualParams(QRectF(0, 0, 50, 50), QRectF(5, 5, 10, 10)));
  const Params sourceParams(QRectF(), QSizeF(), QRectF(10, 20, 100, 150), Dependencies(), MODE_AUTO, MODE_MANUAL,
                            false);

  settings.copyBoxes(std::set<PageId>{target}, source, sourceParams, Settings::BoxSelection::Content);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->contentDetectionMode() == MODE_AUTO);

  bool needUpdateContentBox = false;
  bool needUpdatePageBox = false;
  Dependencies(pageOutline(100, 150), MODE_AUTO, MODE_MANUAL, false)
      .compatibleWith(stored->dependencies(), &needUpdateContentBox, &needUpdatePageBox);
  BOOST_CHECK(needUpdateContentBox);
}

BOOST_AUTO_TEST_CASE(neither_writes_the_modes_without_touching_the_boxes) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, automaticParams(QRectF(0, 0, 50, 50), QRectF(5, 5, 10, 10)));
  const Params sourceParams = manualParams(QRectF(10, 20, 100, 150), QRectF(30, 40, 60, 70));

  settings.copyBoxes(std::set<PageId>{target}, source, sourceParams, Settings::BoxSelection::Neither);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->pageRect() == QRectF(0, 0, 50, 50));
  BOOST_CHECK(stored->contentRect() == QRectF(5, 5, 10, 10));
  BOOST_CHECK(stored->contentDetectionMode() == MODE_MANUAL);
  BOOST_CHECK(stored->pageDetectionMode() == MODE_MANUAL);
}

BOOST_AUTO_TEST_CASE(set_manual_box_overwrites_every_page) {
  Settings settings;
  const PageId first = pageIdOf("/scan/first.png");
  const PageId second = pageIdOf("/scan/second.png");
  const QRectF box(1, 2, 30, 40);

  settings.setManualBox(std::set<PageId>{first, second}, box);

  for (const PageId& page : {first, second}) {
    const std::unique_ptr<Params> stored = settings.getPageParams(page);
    BOOST_REQUIRE(stored != nullptr);
    BOOST_CHECK(stored->pageRect() == box);
    BOOST_CHECK(stored->contentRect() == box);
    BOOST_CHECK(stored->contentDetectionMode() == MODE_MANUAL);
    BOOST_CHECK(stored->pageDetectionMode() == MODE_MANUAL);
  }
}

BOOST_AUTO_TEST_CASE(unbounded_page_detection_makes_the_page_box_the_whole_image) {
  Settings settings;

  settings.useUnboundedPageDetection();

  BOOST_CHECK(settings.pageDetectionBox() == QSizeF(100000.0, 100000.0));
  BOOST_CHECK_SMALL(settings.pageDetectionTolerance(), 1e-9);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
