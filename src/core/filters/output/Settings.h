// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_SETTINGS_H_
#define SCANTAILOR_OUTPUT_SETTINGS_H_

#include <dewarping/DistortionModel.h>

#include <QMutex>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>

#include "ColorParams.h"
#include "DespeckleLevel.h"
#include "DewarpingOptions.h"
#include "Dpi.h"
#include "NonCopyable.h"
#include "OutputParams.h"
#include "OutputProcessingParams.h"
#include "PageId.h"
#include "Params.h"
#include "PropertySet.h"
#include "ZoneSet.h"

class AbstractRelinker;
class DocumentChangeNotifier;

namespace output {
class Params;

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  explicit Settings(std::shared_ptr<DocumentChangeNotifier> changeNotifier = nullptr);

  virtual ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  Params getParams(const PageId& pageId) const;

  void setParams(const PageId& pageId, const Params& params);

  bool isParamsNull(const PageId& pageId) const;

  void setColorParams(const PageId& pageId, const ColorParams& prms);

  void setPictureShapeOptions(const PageId& pageId, PictureShapeOptions pictureShapeOptions);

  void setDpi(const PageId& pageId, const Dpi& dpi);

  void setDewarpingOptions(const PageId& pageId, const DewarpingOptions& opt);

  void setSplittingOptions(const PageId& pageId, const SplittingOptions& opt);

  void setDistortionModel(const PageId& pageId, const dewarping::DistortionModel& model);

  void setDepthPerception(const PageId& pageId, const DepthPerception& depthPerception);

  void setDespeckleLevel(const PageId& pageId, double level);

  std::unique_ptr<OutputParams> getOutputParams(const PageId& pageId) const;

  void removeOutputParams(const PageId& pageId);

  void setOutputParams(const PageId& pageId, const OutputParams& params);

  ZoneSet pictureZonesForPage(const PageId& pageId) const;

  ZoneSet fillZonesForPage(const PageId& pageId) const;

  void setPictureZones(const PageId& pageId, const ZoneSet& zones);

  void setFillZones(const PageId& pageId, const ZoneSet& zones);

  /**
   * For now, default zone properties are not persistent.
   * They may become persistent later though.
   */
  PropertySet defaultPictureZoneProperties() const;

  PropertySet defaultFillZoneProperties() const;

  void setDefaultPictureZoneProperties(const PropertySet& props);

  void setDefaultFillZoneProperties(const PropertySet& props);

  OutputProcessingParams getOutputProcessingParams(const PageId& pageId) const;

  void setOutputProcessingParams(const PageId& pageId, const OutputProcessingParams& outputProcessingParams);

  void setBlackOnWhite(const PageId& pageId, bool blackOnWhite);

  /** Bulk variants of the per-page setters, used by the "Apply to…" actions. */
  void applyDpi(const std::set<PageId>& pages, const Dpi& dpi);

  void applyColors(const std::set<PageId>& pages, const ColorParams& colors, const PictureShapeOptions& pictureShape);

  void applySplitting(const std::set<PageId>& pages, const SplittingOptions& options);

  void applyDespeckleLevel(const std::set<PageId>& pages, double level);

  void applyDewarping(const std::set<PageId>& pages, const DewarpingOptions& options);

  void applyDepthPerception(const std::set<PageId>& pages, const DepthPerception& depth);

  /** Copies the source's black-on-white choice, marking it manual on every target. */
  void copyProcessing(const std::set<PageId>& pages, const PageId& sourcePage);

  enum class IlluminationUpdate { Preserve, Normalize };

  /**
   * \brief The output settings a command line asks for.
   *
   * Every field is optional so that an absent flag leaves the stored value
   * alone; a free function or a zero threshold still counts as engaged.
   */
  struct Overrides {
    std::optional<Dpi> dpi;
    std::optional<ColorMode> colorMode;
    std::optional<int> thresholdAdjustment;
    std::optional<DespeckleLevel> despeckleLevel;
    std::optional<DewarpingMode> dewarpingMode;
    std::optional<double> depthPerception;
    IlluminationUpdate illumination = IlluminationUpdate::Preserve;

    bool empty() const;
  };

  /**
   * \brief Patches only the engaged overrides into each page's parameters.
   *
   * Unlike applyDpi(), the DPI here never touches the dewarping mode: the
   * caller asked for a file-level override, not for an interactive edit.
   */
  void applyOverrides(const std::set<PageId>& pages, const Overrides& overrides);

 private:
  using PerPageParams = std::unordered_map<PageId, Params>;
  using PerPageOutputParams = std::unordered_map<PageId, OutputParams>;
  using PerPageZones = std::unordered_map<PageId, ZoneSet>;
  using PerPageOutputProcessingParams = std::unordered_map<PageId, OutputProcessingParams>;

  static PropertySet initialPictureZoneProps();

  static PropertySet initialFillZoneProps();

  std::shared_ptr<DocumentChangeNotifier> m_changeNotifier;
  mutable QMutex m_mutex;
  PerPageParams m_perPageParams;
  PerPageOutputParams m_perPageOutputParams;
  PerPageZones m_perPagePictureZones;
  PerPageZones m_perPageFillZones;
  PropertySet m_defaultPictureZoneProps;
  PropertySet m_defaultFillZoneProps;
  PerPageOutputProcessingParams m_perPageOutputProcessingParams;
};
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_SETTINGS_H_
