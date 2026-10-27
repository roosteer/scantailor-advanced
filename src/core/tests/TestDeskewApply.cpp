// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/deskew/Dependencies.h>
#include <filters/deskew/Params.h>
#include <filters/deskew/Settings.h>

#include <ImageId.h>
#include <OrthogonalRotation.h>
#include <PageId.h>
#include <QPointF>
#include <QPolygonF>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace deskew;

namespace {

PageId pageIdOf(const char* path) {
  return PageId(ImageId(QString::fromLatin1(path)));
}

OrthogonalRotation rotationOf(int quarterTurns) {
  OrthogonalRotation rotation;
  for (int i = 0; i < quarterTurns; ++i) {
    rotation.nextClockwiseDirection();
  }
  return rotation;
}

QPolygonF outline(int width, int height) {
  QPolygonF polygon;
  polygon << QPointF(0, 0) << QPointF(width, 0) << QPointF(width, height) << QPointF(0, height);
  return polygon;
}

Dependencies sourceDependencies() {
  return Dependencies(outline(120, 240), rotationOf(1));
}

Dependencies targetDependencies() {
  return Dependencies(outline(100, 200), rotationOf(0));
}

}  // namespace

BOOST_AUTO_TEST_SUITE(DeskewApplyTestSuite)

BOOST_AUTO_TEST_CASE(deskew_only_keeps_the_target_oblique_angle) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, Params(1.0, 3.0, targetDependencies(), MODE_AUTO, MODE_MANUAL));

  settings.applyAngles(std::set<PageId>{target}, Params(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_AUTO),
                       Settings::AngleSelection::Deskew);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK_CLOSE(stored->deskewAngle(), 2.0, 1e-9);
  BOOST_CHECK(stored->mode() == MODE_MANUAL);
  BOOST_CHECK_CLOSE(stored->obliqueAngle(), 3.0, 1e-9);
  BOOST_CHECK(stored->obliqueMode() == MODE_MANUAL);
  BOOST_CHECK(stored->dependencies().matches(sourceDependencies()));
}

BOOST_AUTO_TEST_CASE(oblique_only_keeps_the_target_deskew_angle) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, Params(1.0, 3.0, targetDependencies(), MODE_AUTO, MODE_MANUAL));

  settings.applyAngles(std::set<PageId>{target}, Params(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_AUTO),
                       Settings::AngleSelection::Oblique);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK_CLOSE(stored->deskewAngle(), 1.0, 1e-9);
  BOOST_CHECK(stored->mode() == MODE_AUTO);
  BOOST_CHECK_CLOSE(stored->obliqueAngle(), 4.0, 1e-9);
  BOOST_CHECK(stored->obliqueMode() == MODE_AUTO);
  BOOST_CHECK(stored->dependencies().matches(sourceDependencies()));
}

BOOST_AUTO_TEST_CASE(both_copies_both_axes) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, Params(1.0, 3.0, targetDependencies(), MODE_AUTO, MODE_MANUAL));

  settings.applyAngles(std::set<PageId>{target}, Params(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_AUTO),
                       Settings::AngleSelection::Both);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK_CLOSE(stored->deskewAngle(), 2.0, 1e-9);
  BOOST_CHECK(stored->mode() == MODE_MANUAL);
  BOOST_CHECK_CLOSE(stored->obliqueAngle(), 4.0, 1e-9);
  BOOST_CHECK(stored->obliqueMode() == MODE_AUTO);
}

BOOST_AUTO_TEST_CASE(neither_leaves_the_target_untouched) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, Params(1.0, 3.0, targetDependencies(), MODE_AUTO, MODE_MANUAL));

  settings.applyAngles(std::set<PageId>{target}, Params(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_AUTO),
                       Settings::AngleSelection::Neither);

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK_CLOSE(stored->deskewAngle(), 1.0, 1e-9);
  BOOST_CHECK_CLOSE(stored->obliqueAngle(), 3.0, 1e-9);
  BOOST_CHECK(stored->dependencies().matches(targetDependencies()));
}

BOOST_AUTO_TEST_CASE(neither_creates_no_record) {
  Settings settings;
  const PageId missing = pageIdOf("/scan/missing.png");

  settings.applyAngles(std::set<PageId>{missing}, Params(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_AUTO),
                       Settings::AngleSelection::Neither);

  BOOST_CHECK(settings.isParamsNull(missing));
}

BOOST_AUTO_TEST_CASE(missing_target_uses_auto_for_the_unselected_axis) {
  Settings settings;
  const PageId deskewOnly = pageIdOf("/scan/a.png");
  const PageId obliqueOnly = pageIdOf("/scan/b.png");
  const Params source(2.0, 4.0, sourceDependencies(), MODE_MANUAL, MODE_MANUAL);

  settings.applyAngles(std::set<PageId>{deskewOnly}, source, Settings::AngleSelection::Deskew);
  settings.applyAngles(std::set<PageId>{obliqueOnly}, source, Settings::AngleSelection::Oblique);

  const std::unique_ptr<Params> deskewed = settings.getPageParams(deskewOnly);
  BOOST_REQUIRE(deskewed != nullptr);
  BOOST_CHECK_CLOSE(deskewed->deskewAngle(), 2.0, 1e-9);
  BOOST_CHECK(deskewed->mode() == MODE_MANUAL);
  BOOST_CHECK_CLOSE(deskewed->obliqueAngle(), 0.0, 1e-9);
  BOOST_CHECK(deskewed->obliqueMode() == MODE_AUTO);

  const std::unique_ptr<Params> obliqued = settings.getPageParams(obliqueOnly);
  BOOST_REQUIRE(obliqued != nullptr);
  BOOST_CHECK_CLOSE(obliqued->deskewAngle(), 0.0, 1e-9);
  BOOST_CHECK(obliqued->mode() == MODE_AUTO);
  BOOST_CHECK_CLOSE(obliqued->obliqueAngle(), 4.0, 1e-9);
  BOOST_CHECK(obliqued->obliqueMode() == MODE_MANUAL);
}

/** The command line replaces the whole record instead of merging selectively. */
BOOST_AUTO_TEST_CASE(cli_manual_angle_replaces_the_oblique_field) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setPageParams(target, Params(1.0, 3.0, targetDependencies(), MODE_AUTO, MODE_MANUAL));

  settings.setDegrees(std::set<PageId>{target}, Params(1.5, Dependencies(), MODE_MANUAL));

  const std::unique_ptr<Params> stored = settings.getPageParams(target);
  BOOST_REQUIRE(stored != nullptr);
  BOOST_CHECK_CLOSE(stored->deskewAngle(), 1.5, 1e-9);
  BOOST_CHECK(stored->mode() == MODE_MANUAL);
  BOOST_CHECK_CLOSE(stored->obliqueAngle(), 0.0, 1e-9);
  BOOST_CHECK(stored->obliqueMode() == MODE_AUTO);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
