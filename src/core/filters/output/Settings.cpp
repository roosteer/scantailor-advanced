// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include "../../Utils.h"
#include "AbstractRelinker.h"
#include "FillColorProperty.h"
#include "PictureLayerProperty.h"
#include "RelinkablePath.h"

using namespace core;

namespace output {
Settings::Settings()
    : m_defaultPictureZoneProps(initialPictureZoneProps()), m_defaultFillZoneProps(initialFillZoneProps()) {}

Settings::~Settings() = default;

void Settings::clear() {
  const QMutexLocker locker(&m_mutex);

  initialPictureZoneProps().swap(m_defaultPictureZoneProps);
  initialFillZoneProps().swap(m_defaultFillZoneProps);
  m_perPageParams.clear();
  m_perPageOutputParams.clear();
  m_perPagePictureZones.clear();
  m_perPageFillZones.clear();
  m_perPageOutputProcessingParams.clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  const QMutexLocker locker(&m_mutex);

  PerPageParams newParams;
  PerPageOutputParams newOutputParams;
  PerPageZones newPictureZones;
  PerPageZones newFillZones;
  PerPageOutputProcessingParams newOutputProcessingParams;

  for (const PerPageParams::value_type& kv : m_perPageParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newParams.insert(PerPageParams::value_type(newPageId, kv.second));
  }

  for (const PerPageOutputParams::value_type& kv : m_perPageOutputParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newOutputParams.insert(PerPageOutputParams::value_type(newPageId, kv.second));
  }

  for (const PerPageZones::value_type& kv : m_perPagePictureZones) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newPictureZones.insert(PerPageZones::value_type(newPageId, kv.second));
  }

  for (const PerPageZones::value_type& kv : m_perPageFillZones) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newFillZones.insert(PerPageZones::value_type(newPageId, kv.second));
  }

  for (const PerPageOutputProcessingParams::value_type& kv : m_perPageOutputProcessingParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newOutputProcessingParams.insert(PerPageOutputProcessingParams::value_type(newPageId, kv.second));
  }

  m_perPageParams.swap(newParams);
  m_perPageOutputParams.swap(newOutputParams);
  m_perPagePictureZones.swap(newPictureZones);
  m_perPageFillZones.swap(newFillZones);
  m_perPageOutputProcessingParams.swap(newOutputProcessingParams);
}  // Settings::performRelinking

Params Settings::getParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it != m_perPageParams.end()) {
    return it->second;
  } else {
    return Params();
  }
}

void Settings::setParams(const PageId& pageId, const Params& params) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageParams, pageId, params);
}

void Settings::setColorParams(const PageId& pageId, const ColorParams& prms) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setColorParams(prms);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setColorParams(prms);
  }
}

void Settings::setPictureShapeOptions(const PageId& pageId, PictureShapeOptions pictureShapeOptions) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setPictureShapeOptions(pictureShapeOptions);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setPictureShapeOptions(pictureShapeOptions);
  }
}

void Settings::setDpi(const PageId& pageId, const Dpi& dpi) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setOutputDpi(dpi);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setOutputDpi(dpi);
  }
}

void Settings::setDewarpingOptions(const PageId& pageId, const DewarpingOptions& opt) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDewarpingOptions(opt);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDewarpingOptions(opt);
  }
}

void Settings::setSplittingOptions(const PageId& pageId, const SplittingOptions& opt) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setSplittingOptions(opt);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setSplittingOptions(opt);
  }
}

void Settings::setDistortionModel(const PageId& pageId, const dewarping::DistortionModel& model) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDistortionModel(model);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDistortionModel(model);
  }
}

void Settings::setDepthPerception(const PageId& pageId, const DepthPerception& depthPerception) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDepthPerception(depthPerception);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDepthPerception(depthPerception);
  }
}

void Settings::setDespeckleLevel(const PageId& pageId, double level) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDespeckleLevel(level);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDespeckleLevel(level);
  }
}

std::unique_ptr<OutputParams> Settings::getOutputParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageOutputParams.find(pageId));
  if (it != m_perPageOutputParams.end()) {
    return std::make_unique<OutputParams>(it->second);
  } else {
    return nullptr;
  }
}

void Settings::removeOutputParams(const PageId& pageId) {
  const QMutexLocker locker(&m_mutex);
  m_perPageOutputParams.erase(pageId);
}

void Settings::setOutputParams(const PageId& pageId, const OutputParams& params) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageOutputParams, pageId, params);
}

ZoneSet Settings::pictureZonesForPage(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPagePictureZones.find(pageId));
  if (it != m_perPagePictureZones.end()) {
    return it->second;
  } else {
    return ZoneSet();
  }
}

ZoneSet Settings::fillZonesForPage(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageFillZones.find(pageId));
  if (it != m_perPageFillZones.end()) {
    return it->second;
  } else {
    return ZoneSet();
  }
}

void Settings::setPictureZones(const PageId& pageId, const ZoneSet& zones) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPagePictureZones, pageId, zones);
}

void Settings::setFillZones(const PageId& pageId, const ZoneSet& zones) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageFillZones, pageId, zones);
}

PropertySet Settings::defaultPictureZoneProperties() const {
  const QMutexLocker locker(&m_mutex);
  return m_defaultPictureZoneProps;
}

PropertySet Settings::defaultFillZoneProperties() const {
  const QMutexLocker locker(&m_mutex);
  return m_defaultFillZoneProps;
}

void Settings::setDefaultPictureZoneProperties(const PropertySet& props) {
  const QMutexLocker locker(&m_mutex);
  m_defaultPictureZoneProps = props;
}

void Settings::setDefaultFillZoneProperties(const PropertySet& props) {
  const QMutexLocker locker(&m_mutex);
  m_defaultFillZoneProps = props;
}

PropertySet Settings::initialPictureZoneProps() {
  PropertySet props;
  props.locateOrCreate<PictureLayerProperty>()->setLayer(PictureLayerProperty::ZONEPAINTER2);
  return props;
}

PropertySet Settings::initialFillZoneProps() {
  PropertySet props;
  props.locateOrCreate<FillColorProperty>()->setColor(Qt::white);
  return props;
}

OutputProcessingParams Settings::getOutputProcessingParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageOutputProcessingParams.find(pageId));
  if (it != m_perPageOutputProcessingParams.end()) {
    return it->second;
  } else {
    return OutputProcessingParams();
  }
}

void Settings::setOutputProcessingParams(const PageId& pageId, const OutputProcessingParams& outputProcessingParams) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageOutputProcessingParams, pageId, outputProcessingParams);
}

bool Settings::isParamsNull(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);
  return m_perPageParams.find(pageId) == m_perPageParams.end();
}

void Settings::setBlackOnWhite(const PageId& pageId, const bool blackOnWhite) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setBlackOnWhite(blackOnWhite);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setBlackOnWhite(blackOnWhite);
  }
}

void Settings::applyDpi(const std::set<PageId>& pages, const Dpi& dpi) {
  for (const PageId& pageId : pages) {
    const Params params(getParams(pageId));
    if (params.dewarpingOptions().dewarpingMode() == AUTO) {
      DewarpingOptions opt(params.dewarpingOptions());
      opt.setDewarpingMode(MANUAL);
      setDewarpingOptions(pageId, opt);
    }
    setDpi(pageId, dpi);
  }
}

void Settings::applyColors(const std::set<PageId>& pages,
                           const ColorParams& colors,
                           const PictureShapeOptions& pictureShape) {
  for (const PageId& pageId : pages) {
    setColorParams(pageId, colors);
    setPictureShapeOptions(pageId, pictureShape);
  }
}

void Settings::applySplitting(const std::set<PageId>& pages, const SplittingOptions& options) {
  for (const PageId& pageId : pages) {
    setSplittingOptions(pageId, options);
  }
}

void Settings::applyDespeckleLevel(const std::set<PageId>& pages, const double level) {
  for (const PageId& pageId : pages) {
    setDespeckleLevel(pageId, level);
  }
}

void Settings::applyDewarping(const std::set<PageId>& pages, const DewarpingOptions& options) {
  for (const PageId& pageId : pages) {
    setDewarpingOptions(pageId, options);
  }
}

void Settings::applyDepthPerception(const std::set<PageId>& pages, const DepthPerception& depth) {
  for (const PageId& pageId : pages) {
    setDepthPerception(pageId, depth);
  }
}

void Settings::copyProcessing(const std::set<PageId>& pages, const PageId& sourcePage) {
  if (pages.empty()) {
    return;
  }

  const bool blackOnWhite = getParams(sourcePage).isBlackOnWhite();
  for (const PageId& pageId : pages) {
    setBlackOnWhite(pageId, blackOnWhite);

    OutputProcessingParams processingParams = getOutputProcessingParams(pageId);
    processingParams.setBlackOnWhiteSetManually(true);
    setOutputProcessingParams(pageId, processingParams);
  }
}

bool Settings::Overrides::empty() const {
  return !dpi && !colorMode && !thresholdAdjustment && !despeckleLevel && !dewarpingMode && !depthPerception
         && (illumination == IlluminationUpdate::Preserve);
}

namespace {
/** Patches the colour-related overrides into one copy; reports whether anything changed. */
bool patchColorParams(const Settings::Overrides& overrides, ColorParams& colorParams) {
  bool changed = false;

  if (overrides.colorMode) {
    colorParams.setColorMode(*overrides.colorMode);
    changed = true;
  }

  if (overrides.thresholdAdjustment) {
    BlackWhiteOptions bwOptions = colorParams.blackWhiteOptions();
    bwOptions.setThresholdAdjustment(*overrides.thresholdAdjustment);
    colorParams.setBlackWhiteOptions(bwOptions);
    changed = true;
  }

  if (overrides.illumination == Settings::IlluminationUpdate::Normalize) {
    BlackWhiteOptions bwOptions = colorParams.blackWhiteOptions();
    bwOptions.setNormalizeIllumination(true);
    colorParams.setBlackWhiteOptions(bwOptions);

    ColorCommonOptions commonOptions = colorParams.colorCommonOptions();
    commonOptions.setNormalizeIllumination(true);
    colorParams.setColorCommonOptions(commonOptions);
    changed = true;
  }

  return changed;
}
}  // namespace

void Settings::applyOverrides(const std::set<PageId>& pages, const Overrides& overrides) {
  if (overrides.empty()) {
    return;
  }

  for (const PageId& pageId : pages) {
    Params params(getParams(pageId));

    if (overrides.dpi) {
      params.setOutputDpi(*overrides.dpi);
    }

    ColorParams colorParams(params.colorParams());
    if (patchColorParams(overrides, colorParams)) {
      params.setColorParams(colorParams);
    }

    if (overrides.despeckleLevel) {
      params.setDespeckleLevel(static_cast<double>(*overrides.despeckleLevel));
    }

    if (overrides.dewarpingMode) {
      DewarpingOptions dewarpingOptions(params.dewarpingOptions());
      dewarpingOptions.setDewarpingMode(*overrides.dewarpingMode);
      params.setDewarpingOptions(dewarpingOptions);
    }

    if (overrides.depthPerception) {
      params.setDepthPerception(DepthPerception(*overrides.depthPerception));
    }

    setParams(pageId, params);
  }
}
}  // namespace output