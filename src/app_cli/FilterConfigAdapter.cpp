// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "FilterConfigAdapter.h"

#include <StageSequence.h>
#include <AutoManualMode.h>
#include <Margins.h>
#include <OrthogonalRotation.h>
#include <PageInfo.h>
#include <PageSequence.h>
#include <imageproc/Dpi.h>

#include <set>

#include "filters/deskew/Params.h"
#include "filters/output/ColorParams.h"
#include "filters/output/DepthPerception.h"
#include "filters/output/DespeckleLevel.h"
#include "filters/output/DewarpingOptions.h"
#include "filters/output/Params.h"
#include "filters/page_layout/Alignment.h"
#include "filters/fix_orientation/Settings.h"
#include "filters/output/Settings.h"
#include "filters/page_layout/Settings.h"
#include "filters/page_split/Settings.h"

namespace cli {

FilterConfigAdapter::FilterConfigAdapter(const CliRequest& request,
                                         std::shared_ptr<StageSequence> stages,
                                         const PageSequence& pages)
    : m_request(request), m_stages(std::move(stages)), m_pages(pages) {}

void FilterConfigAdapter::apply(const FilterRange& range) {
  // A reused adapter must never hand a previous run's page list to a stage.
  m_selectedPageIds.reset();

  const int fixOrientationIdx = m_stages->fixOrientationFilterIdx();
  const int pageSplitIdx = m_stages->pageSplitFilterIdx();
  const int deskewIdx = m_stages->deskewFilterIdx();
  const int selectContentIdx = m_stages->selectContentFilterIdx();
  const int pageLayoutIdx = m_stages->pageLayoutFilterIdx();
  const int outputIdx = m_stages->outputFilterIdx();

  if (fixOrientationIdx >= range.start && fixOrientationIdx <= range.end) {
    applyFixOrientation();
  }
  if (pageSplitIdx >= range.start && pageSplitIdx <= range.end) {
    applyPageSplit();
  }
  if (deskewIdx >= range.start && deskewIdx <= range.end) {
    applyDeskew();
  }
  if (selectContentIdx >= range.start && selectContentIdx <= range.end) {
    applySelectContent();
  }
  if (pageLayoutIdx >= range.start && pageLayoutIdx <= range.end) {
    applyPageLayout();
  }
  if (outputIdx >= range.start && outputIdx <= range.end) {
    applyOutput();
  }
}

static OrthogonalRotation degreesToOrthogonalRotation(int degrees) {
  OrthogonalRotation rot;  // 0 degrees
  if (degrees == 90) {
    rot.nextClockwiseDirection();
  } else if (degrees == 180) {
    rot.nextClockwiseDirection();
    rot.nextClockwiseDirection();
  } else if (degrees == 270) {
    rot.nextClockwiseDirection();
    rot.nextClockwiseDirection();
    rot.nextClockwiseDirection();
  }
  return rot;
}

const std::set<PageId>& FilterConfigAdapter::selectedPageIds() {
  if (!m_selectedPageIds) {
    std::set<PageId>& pageIds = m_selectedPageIds.emplace();
    for (const PageInfo& page : m_pages) {
      pageIds.insert(page.id());
    }
  }
  return *m_selectedPageIds;
}

void FilterConfigAdapter::applyFixOrientation() {
  if (!m_request.orientation) {
    return;
  }
  auto* filter = m_stages->fixOrientationFilter().get();
  auto settings = filter->getSettings();
  const OrthogonalRotation rot = degreesToOrthogonalRotation(*m_request.orientation);
  settings->applyRotation(selectedPageIds(), rot);
}

void FilterConfigAdapter::applyPageSplit() {
  if (!m_request.layoutType) {
    return;
  }
  auto* filter = m_stages->pageSplitFilter().get();
  auto settings = filter->getSettings();
  settings->setLayoutTypeForAllPages(*m_request.layoutType);
}

void FilterConfigAdapter::applyDeskew() {
  if (!m_request.deskewMode && !m_request.deskewAngle) {
    return;
  }
  auto* filter = m_stages->deskewFilter().get();
  auto settings = filter->getSettings();

  if (m_request.deskewMode) {
    settings->setAlgoContentBased(*m_request.deskewMode == DeskewMode::AUTO);
  }

  // A manual angle only means something in manual mode; the parser rejects the
  // combination outright, so reaching here with an angle implies manual mode.
  if (m_request.deskewAngle) {
    // Unlike the GUI's selective copy, this replaces the whole record: the
    // oblique angle and its mode fall back to the parser's defaults.
    settings->setDegrees(selectedPageIds(), deskew::Params(*m_request.deskewAngle, deskew::Dependencies(), MODE_MANUAL));
  }
}

void FilterConfigAdapter::applySelectContent() {
  auto* filter = m_stages->selectContentFilter().get();
  auto settings = filter->getSettings();

  // Detection is per-page in the GUI. Turning it off globally is expressed as a
  // detection box big enough to always contain the page.
  if (m_request.contentDetection == ContentDetection::OFF) {
    settings->useUnboundedPageDetection();
  }

  if (m_request.contentBox) {
    const MmRect& box = *m_request.contentBox;
    const QRectF rect(box.left, box.top, box.right - box.left, box.bottom - box.top);
    settings->setManualBox(selectedPageIds(), rect);
  }
}

void FilterConfigAdapter::applyPageLayout() {
  auto* filter = m_stages->pageLayoutFilter().get();
  auto settings = filter->getSettings();

  if (m_request.margins) {
    const MmRect& box = *m_request.margins;
    settings->overrideMargins(selectedPageIds(), Margins(box.left, box.top, box.right, box.bottom));
  }

  if (m_request.alignment) {
    page_layout::Alignment alignment;
    switch (*m_request.alignment) {
      case Alignment::CENTER:
        alignment = page_layout::Alignment(page_layout::Alignment::VCENTER, page_layout::Alignment::HCENTER);
        break;
      case Alignment::ORIGINAL:
        alignment = page_layout::Alignment(page_layout::Alignment::VORIGINAL, page_layout::Alignment::HORIGINAL);
        break;
      case Alignment::AUTO:
        alignment = page_layout::Alignment(page_layout::Alignment::VAUTO, page_layout::Alignment::HAUTO);
        break;
    }
    settings->applyAlignment(selectedPageIds(), alignment);
  }
}

void FilterConfigAdapter::applyOutput() {
  output::Settings::Overrides overrides;
  if (m_request.outputDpi) {
    overrides.dpi = Dpi(*m_request.outputDpi, *m_request.outputDpi);
  }
  overrides.colorMode = m_request.colorMode;
  overrides.thresholdAdjustment = m_request.thresholdAdjustment;
  overrides.despeckleLevel = m_request.despeckleLevel;
  overrides.dewarpingMode = m_request.dewarpingMode;
  overrides.depthPerception = m_request.depthPerception;
  if (m_request.equalizeIllumination) {
    overrides.illumination = output::Settings::IlluminationUpdate::Normalize;
  }

  if (overrides.empty()) {
    return;
  }

  auto* filter = m_stages->outputFilter().get();
  auto settings = filter->getSettings();
  settings->applyOverrides(selectedPageIds(), overrides);
}

}  // namespace cli
