// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/fix_orientation/ImageTrim.h>
#include <filters/fix_orientation/Settings.h>

#include <ImageId.h>
#include <PageId.h>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace fix_orientation;

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

ImageTrim trimOf(int left, int top, int right, int bottom) {
  ImageTrim trim;
  trim.enabled = true;
  trim.left = left;
  trim.top = top;
  trim.right = right;
  trim.bottom = bottom;
  return trim;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(OrientationApplyTestSuite)

/** Both split siblings are the same image, so a bulk apply reaches one rotation. */
BOOST_AUTO_TEST_CASE(split_siblings_share_rotation_and_trim) {
  Settings settings;
  const PageId left = pageIdOf("/scan/spread.png", PageId::LEFT_PAGE);
  const PageId right = pageIdOf("/scan/spread.png", PageId::RIGHT_PAGE);

  settings.applyRotation(std::set<PageId>{left}, rotationOf(1));
  settings.applyTrim(std::set<PageId>{left}, trimOf(10, 20, 30, 40));

  BOOST_CHECK(settings.getRotationFor(right.imageId()) == rotationOf(1));
  const ImageTrim shared = settings.getTrim(right.imageId());
  BOOST_REQUIRE(shared.enabled);
  BOOST_CHECK_EQUAL(shared.left, 10);
  BOOST_CHECK_EQUAL(shared.bottom, 40);

  settings.applyTrim(std::set<PageId>{right}, trimOf(1, 2, 3, 4));
  const ImageTrim rewritten = settings.getTrim(left.imageId());
  BOOST_REQUIRE(rewritten.enabled);
  BOOST_CHECK_EQUAL(rewritten.left, 1);
  BOOST_CHECK_EQUAL(rewritten.bottom, 4);
}

/** The CLI applies rotation only; an existing trim has to survive that. */
BOOST_AUTO_TEST_CASE(rotation_only_keeps_trim) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.applyTrim(std::set<PageId>{page}, trimOf(5, 6, 7, 8));

  settings.applyRotation(std::set<PageId>{page}, rotationOf(3));

  BOOST_CHECK(settings.getRotationFor(page.imageId()) == rotationOf(3));
  const ImageTrim kept = settings.getTrim(page.imageId());
  BOOST_REQUIRE(kept.enabled);
  BOOST_CHECK_EQUAL(kept.left, 5);
  BOOST_CHECK_EQUAL(kept.bottom, 8);
}

BOOST_AUTO_TEST_CASE(disabled_trim_erases_the_shared_trim) {
  Settings settings;
  const PageId left = pageIdOf("/scan/spread.png", PageId::LEFT_PAGE);
  const PageId right = pageIdOf("/scan/spread.png", PageId::RIGHT_PAGE);
  settings.applyTrim(std::set<PageId>{left}, trimOf(10, 20, 30, 40));

  ImageTrim disabled = trimOf(10, 20, 30, 40);
  disabled.enabled = false;
  settings.applyTrim(std::set<PageId>{right}, disabled);

  BOOST_CHECK(!settings.getTrim(left.imageId()).enabled);
  BOOST_CHECK(!settings.getTrim(right.imageId()).enabled);
}

BOOST_AUTO_TEST_CASE(unrelated_images_keep_their_state) {
  Settings settings;
  const PageId applied = pageIdOf("/scan/a.png");
  const PageId untouched = pageIdOf("/scan/b.png");
  settings.applyRotation(std::set<PageId>{untouched}, rotationOf(2));
  settings.applyTrim(std::set<PageId>{untouched}, trimOf(1, 1, 1, 1));

  settings.applyRotation(std::set<PageId>{applied}, rotationOf(1));
  settings.applyTrim(std::set<PageId>{applied}, trimOf(9, 9, 9, 9));

  BOOST_CHECK(settings.getRotationFor(untouched.imageId()) == rotationOf(2));
  const ImageTrim kept = settings.getTrim(untouched.imageId());
  BOOST_REQUIRE(kept.enabled);
  BOOST_CHECK_EQUAL(kept.left, 1);
  BOOST_CHECK(!settings.isRotationNull(applied.imageId()));
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
