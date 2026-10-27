// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/output/Params.h>
#include <filters/output/Settings.h>

#include <ImageId.h>
#include <PageId.h>
#include <boost/test/unit_test.hpp>
#include <set>

namespace Tests {
using namespace output;

namespace {

PageId pageIdOf(const char* path) {
  return PageId(ImageId(QString::fromLatin1(path)));
}

DewarpingOptions dewarpingOf(DewarpingMode mode, bool needPostDeskew = true) {
  DewarpingOptions options(mode, needPostDeskew);
  return options;
}

SplittingOptions splittingOf(bool enabled) {
  SplittingOptions options;
  options.setSplitOutput(enabled);
  return options;
}

ColorParams colorsOf(ColorMode mode, int threshold) {
  BlackWhiteOptions bwOptions;
  bwOptions.setThresholdAdjustment(threshold);
  ColorParams colors;
  colors.setColorMode(mode);
  colors.setBlackWhiteOptions(bwOptions);
  return colors;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(OutputApplyTestSuite)

/** An interactive DPI edit decides the dewarping mode, so AUTO must not survive it. */
BOOST_AUTO_TEST_CASE(apply_dpi_freezes_auto_dewarping) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(150, 150));
  settings.setDewarpingOptions(page, dewarpingOf(AUTO, false));

  settings.applyDpi(std::set<PageId>{page}, Dpi(300, 300));

  const Params stored = settings.getParams(page);
  BOOST_CHECK(stored.outputDpi() == Dpi(300, 300));
  BOOST_CHECK(stored.dewarpingOptions().dewarpingMode() == MANUAL);
  BOOST_CHECK(!stored.dewarpingOptions().needPostDeskew());
}

BOOST_AUTO_TEST_CASE(apply_dpi_freezes_auto_even_when_the_dpi_is_unchanged) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(300, 300));
  settings.setDewarpingOptions(page, dewarpingOf(AUTO));

  settings.applyDpi(std::set<PageId>{page}, Dpi(300, 300));

  BOOST_CHECK(settings.getParams(page).dewarpingOptions().dewarpingMode() == MANUAL);
}

BOOST_AUTO_TEST_CASE(apply_dpi_leaves_decided_dewarping_modes_alone) {
  Settings settings;
  const PageId marginal = pageIdOf("/scan/marginal.png");
  const PageId off = pageIdOf("/scan/off.png");
  settings.setDewarpingOptions(marginal, dewarpingOf(MARGINAL));
  settings.setDewarpingOptions(off, dewarpingOf(OFF));

  settings.applyDpi(std::set<PageId>{marginal, off}, Dpi(200, 200));

  BOOST_CHECK(settings.getParams(marginal).dewarpingOptions().dewarpingMode() == MARGINAL);
  BOOST_CHECK(settings.getParams(off).dewarpingOptions().dewarpingMode() == OFF);
  BOOST_CHECK(settings.getParams(off).outputDpi() == Dpi(200, 200));
}

BOOST_AUTO_TEST_CASE(apply_colors_replaces_colors_and_picture_shape_only) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(400, 400));
  settings.setSplittingOptions(page, splittingOf(true));
  settings.setDespeckleLevel(page, 0.5);
  settings.setColorParams(page, colorsOf(BLACK_AND_WHITE, 7));

  PictureShapeOptions shape;
  shape.setPictureShape(FREE_SHAPE);
  shape.setSensitivity(9);
  settings.applyColors(std::set<PageId>{page}, colorsOf(MIXED, 3), shape);

  const Params stored = settings.getParams(page);
  BOOST_CHECK(stored.colorParams().colorMode() == MIXED);
  BOOST_CHECK_EQUAL(stored.colorParams().blackWhiteOptions().thresholdAdjustment(), 3);
  BOOST_CHECK(stored.pictureShapeOptions().getPictureShape() == FREE_SHAPE);
  BOOST_CHECK_EQUAL(stored.pictureShapeOptions().getSensitivity(), 9);
  BOOST_CHECK(stored.outputDpi() == Dpi(400, 400));
  BOOST_CHECK(stored.splittingOptions().isSplitOutput());
  BOOST_CHECK_CLOSE(stored.despeckleLevel(), 0.5, 1e-9);
}

BOOST_AUTO_TEST_CASE(each_single_value_command_touches_only_its_field) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(400, 400));
  settings.setDespeckleLevel(page, 0.5);
  settings.setDewarpingOptions(page, dewarpingOf(MANUAL));
  settings.setDepthPerception(page, DepthPerception(2.5));
  settings.setSplittingOptions(page, splittingOf(false));

  settings.applySplitting(std::set<PageId>{page}, splittingOf(true));
  Params stored = settings.getParams(page);
  BOOST_CHECK(stored.splittingOptions().isSplitOutput());
  BOOST_CHECK_CLOSE(stored.despeckleLevel(), 0.5, 1e-9);
  BOOST_CHECK(stored.dewarpingOptions().dewarpingMode() == MANUAL);
  BOOST_CHECK_CLOSE(stored.depthPerception().value(), 2.5, 1e-9);

  settings.applyDespeckleLevel(std::set<PageId>{page}, 0.0);
  stored = settings.getParams(page);
  BOOST_CHECK_SMALL(stored.despeckleLevel(), 1e-9);
  BOOST_CHECK_CLOSE(stored.depthPerception().value(), 2.5, 1e-9);
  BOOST_CHECK(stored.outputDpi() == Dpi(400, 400));

  settings.applyDewarping(std::set<PageId>{page}, dewarpingOf(OFF, false));
  stored = settings.getParams(page);
  BOOST_CHECK(stored.dewarpingOptions().dewarpingMode() == OFF);
  BOOST_CHECK(!stored.dewarpingOptions().needPostDeskew());
  BOOST_CHECK(stored.splittingOptions().isSplitOutput());

  settings.applyDepthPerception(std::set<PageId>{page}, DepthPerception(1.5));
  stored = settings.getParams(page);
  BOOST_CHECK_CLOSE(stored.depthPerception().value(), 1.5, 1e-9);
  BOOST_CHECK_SMALL(stored.despeckleLevel(), 1e-9);
}

BOOST_AUTO_TEST_CASE(copy_processing_marks_the_choice_manual) {
  Settings settings;
  const PageId source = pageIdOf("/scan/source.png");
  const PageId target = pageIdOf("/scan/target.png");
  settings.setBlackOnWhite(source, false);
  settings.setBlackOnWhite(target, true);
  OutputProcessingParams processing;
  processing.setAutoZonesFound(true);
  settings.setOutputProcessingParams(target, processing);

  settings.copyProcessing(std::set<PageId>{target}, source);

  BOOST_CHECK(!settings.getParams(target).isBlackOnWhite());
  const OutputProcessingParams stored = settings.getOutputProcessingParams(target);
  BOOST_CHECK(stored.isBlackOnWhiteSetManually());
  BOOST_CHECK(stored.isAutoZonesFound());
}

BOOST_AUTO_TEST_CASE(copy_processing_uses_the_default_for_an_unknown_source) {
  Settings settings;
  const PageId target = pageIdOf("/scan/target.png");
  settings.setBlackOnWhite(target, false);

  settings.copyProcessing(std::set<PageId>{target}, pageIdOf("/scan/unknown.png"));

  BOOST_CHECK(settings.getParams(target).isBlackOnWhite());
  BOOST_CHECK(settings.getOutputProcessingParams(target).isBlackOnWhiteSetManually());
}

BOOST_AUTO_TEST_CASE(copy_processing_with_no_pages_creates_no_record) {
  Settings settings;
  const PageId untouched = pageIdOf("/scan/untouched.png");

  settings.copyProcessing(std::set<PageId>(), pageIdOf("/scan/source.png"));

  BOOST_CHECK(settings.isParamsNull(untouched));
  BOOST_CHECK(!settings.getOutputProcessingParams(untouched).isBlackOnWhiteSetManually());
}

BOOST_AUTO_TEST_CASE(empty_overrides_are_recognized) {
  Settings::Overrides overrides;
  BOOST_CHECK(overrides.empty());

  overrides.thresholdAdjustment = 0;
  BOOST_CHECK(!overrides.empty());

  Settings::Overrides despeckle;
  despeckle.despeckleLevel = DESPECKLE_OFF;
  BOOST_CHECK(!despeckle.empty());

  Settings::Overrides illumination;
  illumination.illumination = Settings::IlluminationUpdate::Normalize;
  BOOST_CHECK(!illumination.empty());
}

BOOST_AUTO_TEST_CASE(empty_overrides_change_nothing) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(400, 400));

  settings.applyOverrides(std::set<PageId>{page}, Settings::Overrides());

  BOOST_CHECK(settings.getParams(page).outputDpi() == Dpi(400, 400));
}

BOOST_AUTO_TEST_CASE(overrides_engage_a_zero_threshold_and_the_off_level) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setColorParams(page, colorsOf(BLACK_AND_WHITE, 7));
  settings.setDespeckleLevel(page, 0.5);

  Settings::Overrides overrides;
  overrides.thresholdAdjustment = 0;
  overrides.despeckleLevel = DESPECKLE_OFF;
  settings.applyOverrides(std::set<PageId>{page}, overrides);

  const Params stored = settings.getParams(page);
  BOOST_CHECK_EQUAL(stored.colorParams().blackWhiteOptions().thresholdAdjustment(), 0);
  BOOST_CHECK_SMALL(stored.despeckleLevel(), 1e-9);
}

/** A file-level DPI override is not an interactive edit and keeps AUTO alive. */
BOOST_AUTO_TEST_CASE(overrides_dpi_keeps_auto_dewarping) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(150, 150));
  settings.setDewarpingOptions(page, dewarpingOf(AUTO, false));

  Settings::Overrides overrides;
  overrides.dpi = Dpi(300, 300);
  settings.applyOverrides(std::set<PageId>{page}, overrides);

  const Params stored = settings.getParams(page);
  BOOST_CHECK(stored.outputDpi() == Dpi(300, 300));
  BOOST_CHECK(stored.dewarpingOptions().dewarpingMode() == AUTO);
  BOOST_CHECK(!stored.dewarpingOptions().needPostDeskew());
}

BOOST_AUTO_TEST_CASE(overrides_patch_only_the_engaged_fields) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setDpi(page, Dpi(400, 400));
  settings.setDespeckleLevel(page, 0.5);
  settings.setDewarpingOptions(page, dewarpingOf(MARGINAL));
  settings.setSplittingOptions(page, splittingOf(true));
  settings.setColorParams(page, colorsOf(BLACK_AND_WHITE, 7));

  Settings::Overrides overrides;
  overrides.colorMode = MIXED;
  overrides.depthPerception = 1.5;
  settings.applyOverrides(std::set<PageId>{page}, overrides);

  const Params stored = settings.getParams(page);
  BOOST_CHECK(stored.colorParams().colorMode() == MIXED);
  BOOST_CHECK_EQUAL(stored.colorParams().blackWhiteOptions().thresholdAdjustment(), 7);
  BOOST_CHECK_CLOSE(stored.depthPerception().value(), 1.5, 1e-9);
  BOOST_CHECK(stored.outputDpi() == Dpi(400, 400));
  BOOST_CHECK_CLOSE(stored.despeckleLevel(), 0.5, 1e-9);
  BOOST_CHECK(stored.dewarpingOptions().dewarpingMode() == MARGINAL);
  BOOST_CHECK(stored.splittingOptions().isSplitOutput());
}

BOOST_AUTO_TEST_CASE(overrides_normalize_sets_both_illumination_flags) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  settings.setColorParams(page, colorsOf(BLACK_AND_WHITE, 7));

  Settings::Overrides overrides;
  overrides.illumination = Settings::IlluminationUpdate::Normalize;
  settings.applyOverrides(std::set<PageId>{page}, overrides);

  const ColorParams stored = settings.getParams(page).colorParams();
  BOOST_CHECK(stored.blackWhiteOptions().normalizeIllumination());
  BOOST_CHECK(stored.colorCommonOptions().normalizeIllumination());
  BOOST_CHECK_EQUAL(stored.blackWhiteOptions().thresholdAdjustment(), 7);
}

BOOST_AUTO_TEST_CASE(overrides_create_a_record_for_a_page_without_params) {
  Settings settings;
  const PageId page = pageIdOf("/scan/page.png");
  BOOST_REQUIRE(settings.isParamsNull(page));

  Settings::Overrides overrides;
  overrides.dpi = Dpi(200, 200);
  settings.applyOverrides(std::set<PageId>{page}, overrides);

  BOOST_CHECK(!settings.isParamsNull(page));
  BOOST_CHECK(settings.getParams(page).outputDpi() == Dpi(200, 200));
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Tests
