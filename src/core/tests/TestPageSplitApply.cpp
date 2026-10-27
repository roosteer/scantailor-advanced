// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/page_split/Dependencies.h>
#include <filters/page_split/PageLayout.h>
#include <filters/page_split/Params.h>
#include <filters/page_split/Settings.h>

#include <ImageId.h>
#include <OrthogonalRotation.h>
#include <PageId.h>
#include <QLineF>
#include <QRectF>
#include <QSize>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace page_split;

namespace {

PageId pageIdOf(const char* path, const PageId::SubPage subPage = PageId::SINGLE_PAGE) {
  return PageId(ImageId(QString::fromLatin1(path)), subPage);
}

OrthogonalRotation rotationOf(int quarterTurns) {
  OrthogonalRotation rotation;
  for (int i = 0; i < quarterTurns; ++i) {
    rotation.nextClockwiseDirection();
  }
  return rotation;
}

PageLayout sourceLayout() {
  return PageLayout(QRectF(0, 0, 100, 200), QLineF(50, 0, 50, 200));
}

PageLayout targetLayout() {
  return PageLayout(QRectF(0, 0, 200, 400), QLineF(100, 0, 100, 400));
}

Params sourceParams() {
  return Params(sourceLayout(), Dependencies(QSize(100, 200), rotationOf(0), TWO_PAGES), MODE_MANUAL);
}

Params targetParams() {
  return Params(targetLayout(), Dependencies(QSize(200, 400), rotationOf(1), TWO_PAGES), MODE_AUTO);
}

void seedPage(Settings& settings, const PageId& page, const Params& params) {
  Settings::UpdateAction action;
  action.setLayoutType(params.pageLayout().toLayoutType());
  action.setParams(params);
  settings.updatePage(page.imageId(), action);
}

}  // namespace

BOOST_AUTO_TEST_SUITE(PageSplitApplyTestSuite)

/** The copied cutters are re-intersected with the target outline, not scaled. */
BOOST_AUTO_TEST_CASE(copy_source_adapts_the_cutter_to_the_target_outline) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  seedPage(settings, target, targetParams());

  settings.applyLayout(std::set<PageId>{target}, TWO_PAGES, sourceParams(), Settings::CutTransfer::CopySource);

  const Settings::Record record = settings.getPageRecord(target.imageId());
  const Params* stored = record.params();
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->pageLayout().type() == PageLayout::TWO_PAGES);
  BOOST_CHECK(stored->pageLayout().uncutOutline().boundingRect() == QRectF(0, 0, 200, 400));

  const QLineF cutter = stored->pageLayout().cutterLine(0);
  BOOST_CHECK_EQUAL(cutter.x1(), 50.0);
  BOOST_CHECK_EQUAL(cutter.y1(), 0.0);
  BOOST_CHECK_EQUAL(cutter.x2(), 50.0);
  BOOST_CHECK_EQUAL(cutter.y2(), 400.0);

  BOOST_CHECK_EQUAL(stored->dependencies().orientation().toDegrees(), 90);
  BOOST_CHECK(stored->splitLineMode() == MODE_MANUAL);
  BOOST_CHECK(record.combinedLayoutType() == TWO_PAGES);
}

BOOST_AUTO_TEST_CASE(keep_target_drops_the_params_that_conflict) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  seedPage(settings, target, targetParams());

  settings.applyLayout(std::set<PageId>{target}, SINGLE_PAGE_UNCUT, sourceParams(),
                       Settings::CutTransfer::KeepTarget);

  const Settings::Record record = settings.getPageRecord(target.imageId());
  BOOST_CHECK(record.params() == nullptr);
  BOOST_CHECK(record.combinedLayoutType() == SINGLE_PAGE_UNCUT);
}

BOOST_AUTO_TEST_CASE(auto_layout_keeps_the_target_params) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  seedPage(settings, target, targetParams());

  settings.applyLayout(std::set<PageId>{target}, AUTO_LAYOUT_TYPE, sourceParams(), Settings::CutTransfer::CopySource);

  const Settings::Record record = settings.getPageRecord(target.imageId());
  const Params* stored = record.params();
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->pageLayout().uncutOutline().boundingRect() == QRectF(0, 0, 200, 400));
  BOOST_CHECK(record.combinedLayoutType() == AUTO_LAYOUT_TYPE);
}

BOOST_AUTO_TEST_CASE(missing_target_copies_the_source_layout_unchanged) {
  Settings settings;
  const PageId missing = pageIdOf("/scan/missing.png");

  settings.applyLayout(std::set<PageId>{missing}, TWO_PAGES, sourceParams(), Settings::CutTransfer::CopySource);

  const Settings::Record record = settings.getPageRecord(missing.imageId());
  const Params* stored = record.params();
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->pageLayout().uncutOutline().boundingRect() == QRectF(0, 0, 100, 200));
  const QLineF cutter = stored->pageLayout().cutterLine(0);
  BOOST_CHECK_EQUAL(cutter.x1(), 50.0);
  BOOST_CHECK_EQUAL(cutter.y2(), 200.0);
  BOOST_CHECK(stored->dependencies().orientation().toDegrees() == 0);
}

/** Both split siblings name the same image, so one record carries the result. */
BOOST_AUTO_TEST_CASE(split_siblings_share_the_copied_layout) {
  Settings settings;
  const PageId left = pageIdOf("/scan/spread.png", PageId::LEFT_PAGE);
  const PageId right = pageIdOf("/scan/spread.png", PageId::RIGHT_PAGE);
  seedPage(settings, left, targetParams());

  settings.applyLayout(std::set<PageId>{left, right}, TWO_PAGES, sourceParams(),
                       Settings::CutTransfer::CopySource);

  const Settings::Record record = settings.getPageRecord(right.imageId());
  const Params* stored = record.params();
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(stored->pageLayout().uncutOutline().boundingRect() == QRectF(0, 0, 200, 400));
  BOOST_CHECK(settings.getPageRecord(left.imageId()).params() != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
