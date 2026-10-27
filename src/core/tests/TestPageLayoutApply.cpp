// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/page_layout/Alignment.h>
#include <filters/page_layout/Params.h>
#include <filters/page_layout/Settings.h>

#include <ImageId.h>
#include <Margins.h>
#include <PageId.h>
#include <QSizeF>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace page_layout;

namespace {

PageId pageIdOf(const char* path) {
  return PageId(ImageId(QString::fromLatin1(path)));
}

Margins marginsOf(double left, double top, double right, double bottom) {
  return Margins(left, top, right, bottom);
}

void requireMargins(const Margins& margins, double left, double top, double right, double bottom) {
  BOOST_CHECK_CLOSE(margins.left(), left, 1e-9);
  BOOST_CHECK_CLOSE(margins.top(), top, 1e-9);
  BOOST_CHECK_CLOSE(margins.right(), right, 1e-9);
  BOOST_CHECK_CLOSE(margins.bottom(), bottom, 1e-9);
}

}  // namespace

BOOST_AUTO_TEST_SUITE(PageLayoutApplyTestSuite)

BOOST_AUTO_TEST_CASE(copy_margins_replaces_only_the_selected_sides) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setHardMarginsMM(source, marginsOf(10, 20, 30, 40));
  settings.setHardMarginsMM(target, marginsOf(1, 2, 3, 4));

  Settings::MarginValues selected;
  selected.left = 10.0;
  selected.right = 30.0;
  settings.copyMargins(std::set<PageId>{target}, source, selected);

  requireMargins(settings.getHardMarginsMM(target), 10, 2, 30, 4);
}

BOOST_AUTO_TEST_CASE(copy_margins_from_an_auto_source_invalidates_the_content_size) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageAutoMarginsEnabled(source, true);
  settings.setHardMarginsMM(target, marginsOf(1, 2, 3, 4));
  settings.setContentSizeMM(target, QSizeF(50, 70));

  Settings::MarginValues selected;
  selected.left = 10.0;
  selected.top = 20.0;
  selected.right = 30.0;
  selected.bottom = 40.0;
  settings.copyMargins(std::set<PageId>{target}, source, selected);

  BOOST_CHECK(settings.isPageAutoMarginsEnabled(target));
  requireMargins(settings.getHardMarginsMM(target), 1, 2, 3, 4);
  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK(!stored->contentSizeMM().isValid());
}

BOOST_AUTO_TEST_CASE(copy_margins_without_a_selected_side_still_copies_the_flag) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setHardMarginsMM(source, marginsOf(10, 20, 30, 40));
  settings.setHardMarginsMM(target, marginsOf(1, 2, 3, 4));
  settings.setPageAutoMarginsEnabled(target, true);

  settings.copyMargins(std::set<PageId>{target}, source, Settings::MarginValues());

  BOOST_CHECK(!settings.isPageAutoMarginsEnabled(target));
  requireMargins(settings.getHardMarginsMM(target), 1, 2, 3, 4);
}

BOOST_AUTO_TEST_CASE(override_margins_keeps_the_target_auto_flag) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setHardMarginsMM(target, marginsOf(1, 2, 3, 4));
  settings.setPageAutoMarginsEnabled(target, true);

  settings.overrideMargins(std::set<PageId>{target}, marginsOf(10, 20, 30, 40));

  BOOST_CHECK(settings.isPageAutoMarginsEnabled(target));
  requireMargins(settings.getHardMarginsMM(target), 10, 20, 30, 40);
}

BOOST_AUTO_TEST_CASE(copy_alignment_skips_the_source_page) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  const Alignment center(Alignment::VCENTER, Alignment::HCENTER);
  const Alignment topLeft(Alignment::TOP, Alignment::LEFT);
  settings.setPageAlignment(source, topLeft);

  settings.copyAlignment(std::set<PageId>{source, target}, source, center);

  BOOST_CHECK(settings.getPageAlignment(target) == center);
  BOOST_CHECK(settings.getPageAlignment(source) == topLeft);
}

BOOST_AUTO_TEST_CASE(apply_alignment_includes_the_source_page) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  const Alignment center(Alignment::VCENTER, Alignment::HCENTER);
  settings.setPageAlignment(source, Alignment(Alignment::TOP, Alignment::LEFT));

  settings.applyAlignment(std::set<PageId>{source, target}, center);

  BOOST_CHECK(settings.getPageAlignment(source) == center);
  BOOST_CHECK(settings.getPageAlignment(target) == center);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
