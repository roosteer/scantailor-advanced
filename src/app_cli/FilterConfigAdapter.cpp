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

std::set<PageId> FilterConfigAdapter::selectedPageIds() const {
  std::set<PageId> pageIds;
  for (const PageInfo& page : m_pages) {
    pageIds.insert(page.id());
  }
  return pageIds;
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
    for (const PageInfo& page : m_pages) {
      deskew::Params params(*m_request.deskewAngle, deskew::Dependencies(), MODE_MANUAL);
      settings->setPageParams(page.id(), params);
    }
  }
}

void FilterConfigAdapter::applySelectContent() {
  auto* filter = m_stages->selectContentFilter().get();
  auto settings = filter->getSettings();

  // Detection is per-page in the GUI. Turning it off globally is expressed as a
  // detection box big enough to always contain the page.
  if (m_request.contentDetection == ContentDetection::OFF) {
    settings->setPageDetectionBox(QSizeF(100000, 100000));
    settings->setPageDetectionTolerance(0.0);
  }

  if (m_request.contentBox) {
    const MmRect& box = *m_request.contentBox;
    const QRectF rect(box.left, box.top, box.right - box.left, box.bottom - box.top);
    const QSizeF sizeMm(rect.width(), rect.height());
    const QRectF pageRect = rect;  // Same as content for manual
    for (const PageId& pageId : selectedPageIds()) {
      select_content::Params params(rect, sizeMm, pageRect, select_content::Dependencies(), MODE_MANUAL,
                                    MODE_MANUAL, false);
      settings->setPageParams(pageId, params);
    }
  }
}

void FilterConfigAdapter::applyPageLayout() {
  auto* filter = m_stages->pageLayoutFilter().get();
  auto settings = filter->getSettings();

  if (m_request.margins) {
    const MmRect& box = *m_request.margins;
    const Margins margins(box.left, box.top, box.right, box.bottom);
    for (const PageInfo& page : m_pages) {
      settings->setHardMarginsMM(page.id(), margins);
    }
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
    for (const PageInfo& page : m_pages) {
      settings->setPageAlignment(page.id(), alignment);
    }
  }
}

void FilterConfigAdapter::applyOutput() {
  if (!m_request.outputDpi && !m_request.colorMode && !m_request.thresholdAdjustment
      && !m_request.equalizeIllumination && !m_request.despeckleLevel && !m_request.dewarpingMode
      && !m_request.depthPerception) {
    return;
  }

  auto* filter = m_stages->outputFilter().get();
  auto settings = filter->getSettings();

  for (const PageInfo& page : m_pages) {
    const PageId& pageId = page.id();

    output::Params params = settings->getParams(pageId);

    if (m_request.outputDpi) {
      params.setOutputDpi(Dpi(*m_request.outputDpi, *m_request.outputDpi));
    }

    // The three illumination-and-colour knobs all land in ColorParams, so they
    // share one copy rather than reading and writing it back three times.
    output::ColorParams colorParams = params.colorParams();
    bool colorParamsChanged = false;

    if (m_request.colorMode) {
      colorParams.setColorMode(*m_request.colorMode);
      colorParamsChanged = true;
    }

    if (m_request.thresholdAdjustment) {
      auto bwOptions = colorParams.blackWhiteOptions();
      bwOptions.setThresholdAdjustment(*m_request.thresholdAdjustment);
      colorParams.setBlackWhiteOptions(bwOptions);
      colorParamsChanged = true;
    }

    if (m_request.equalizeIllumination) {
      auto bwOptions = colorParams.blackWhiteOptions();
      bwOptions.setNormalizeIllumination(true);
      colorParams.setBlackWhiteOptions(bwOptions);

      auto commonOptions = colorParams.colorCommonOptions();
      commonOptions.setNormalizeIllumination(true);
      colorParams.setColorCommonOptions(commonOptions);
      colorParamsChanged = true;
    }

    if (colorParamsChanged) {
      params.setColorParams(colorParams);
    }

    if (m_request.despeckleLevel) {
      params.setDespeckleLevel(static_cast<double>(*m_request.despeckleLevel));
    }

    if (m_request.dewarpingMode) {
      output::DewarpingOptions dewarpOpts = params.dewarpingOptions();
      dewarpOpts.setDewarpingMode(*m_request.dewarpingMode);
      params.setDewarpingOptions(dewarpOpts);
    }

    if (m_request.depthPerception) {
      params.setDepthPerception(output::DepthPerception(*m_request.depthPerception));
    }

    settings->setParams(pageId, params);
  }
}

}  // namespace cli
