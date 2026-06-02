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

void FilterConfigAdapter::apply(int startFilter, int endFilter, QString& errorMsg) {
  const int total = m_stages->count();
  if (startFilter < 0) {
    startFilter = 0;
  }
  if (endFilter < 0) {
    endFilter = total - 1;
  }
  if (startFilter > endFilter || startFilter >= total) {
    errorMsg = QString("Invalid filter range: %1..%2 (available: 0..%3)").arg(startFilter).arg(endFilter).arg(total - 1);
    return;
  }

  const int fixOrientationIdx = m_stages->fixOrientationFilterIdx();
  const int pageSplitIdx = m_stages->pageSplitFilterIdx();
  const int deskewIdx = m_stages->deskewFilterIdx();
  const int selectContentIdx = m_stages->selectContentFilterIdx();
  const int pageLayoutIdx = m_stages->pageLayoutFilterIdx();
  const int outputIdx = m_stages->outputFilterIdx();

  if (fixOrientationIdx >= startFilter && fixOrientationIdx <= endFilter) {
    applyFixOrientation(errorMsg);
  }
  if (pageSplitIdx >= startFilter && pageSplitIdx <= endFilter) {
    applyPageSplit(errorMsg);
  }
  if (deskewIdx >= startFilter && deskewIdx <= endFilter) {
    applyDeskew(errorMsg);
  }
  if (selectContentIdx >= startFilter && selectContentIdx <= endFilter) {
    applySelectContent(errorMsg);
  }
  if (pageLayoutIdx >= startFilter && pageLayoutIdx <= endFilter) {
    applyPageLayout(errorMsg);
  }
  if (outputIdx >= startFilter && outputIdx <= endFilter) {
    applyOutput(errorMsg);
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

void FilterConfigAdapter::applyFixOrientation(QString& /*errorMsg*/) {
  if (m_request.orientation < 0) {
    return;
  }
  auto* filter = m_stages->fixOrientationFilter().get();
  auto settings = filter->getSettings();
  const OrthogonalRotation rot = degreesToOrthogonalRotation(m_request.orientation);

  std::set<PageId> pageIds;
  for (const PageInfo& page : m_pages) {
    pageIds.insert(page.id());
  }
  settings->applyRotation(pageIds, rot);
}

void FilterConfigAdapter::applyPageSplit(QString& /*errorMsg*/) {
  if (!m_request.layoutTypeSet) {
    return;
  }
  auto* filter = m_stages->pageSplitFilter().get();
  auto settings = filter->getSettings();
  settings->setLayoutTypeForAllPages(m_request.layoutType);
}

void FilterConfigAdapter::applyDeskew(QString& /*errorMsg*/) {
  if (!m_request.deskewModeSet && !m_request.deskewAngleSet) {
    return;
  }
  auto* filter = m_stages->deskewFilter().get();
  auto settings = filter->getSettings();

  if (m_request.deskewModeSet) {
    settings->setAlgoContentBased(m_request.deskewMode == CliRequest::DESKEW_AUTO);
  }

  // If a specific angle is set and we're in manual mode, apply per-page.
  if (m_request.deskewAngleSet && m_request.deskewMode == CliRequest::DESKEW_MANUAL) {
    for (const PageInfo& page : m_pages) {
      deskew::Params params(m_request.deskewAngle, deskew::Dependencies(), MODE_MANUAL);
      settings->setPageParams(page.id(), params);
    }
  }
}

void FilterConfigAdapter::applySelectContent(QString& /*errorMsg*/) {
  auto* filter = m_stages->selectContentFilter().get();
  auto settings = filter->getSettings();

  if (m_request.contentDetectionSet) {
    // Content detection mode is per-page, but we can set a global default
    // by configuring the detection box size.
    if (m_request.contentDetection == CliRequest::CONTENT_OFF) {
      // Setting a very large detection box effectively disables content detection
      settings->setPageDetectionBox(QSizeF(100000, 100000));
      settings->setPageDetectionTolerance(0.0);
    }
  }

  // Manual content box: set per-page params with the specific rect.
  if (m_request.contentBox[0] >= 0) {
    for (const PageInfo& page : m_pages) {
      QRectF rect(m_request.contentBox[0], m_request.contentBox[1],
                  m_request.contentBox[2] - m_request.contentBox[0],
                  m_request.contentBox[3] - m_request.contentBox[1]);
      QSizeF sizeMm(rect.width(), rect.height());
      QRectF pageRect = rect;  // Same as content for manual
      select_content::Params params(rect, sizeMm, pageRect, select_content::Dependencies(),
                                    MODE_MANUAL, MODE_MANUAL, false);
      settings->setPageParams(page.id(), params);
    }
  }
}

void FilterConfigAdapter::applyPageLayout(QString& /*errorMsg*/) {
  auto* filter = m_stages->pageLayoutFilter().get();
  auto settings = filter->getSettings();

  // Apply margins if set.
  if (m_request.margins[0] >= 0) {
    Margins margins(m_request.margins[0], m_request.margins[1], m_request.margins[2], m_request.margins[3]);
    for (const PageInfo& page : m_pages) {
      settings->setHardMarginsMM(page.id(), margins);
    }
  }

  // Apply alignment if set.
  if (m_request.alignmentSet) {
    page_layout::Alignment alignment;
    switch (m_request.alignment) {
      case CliRequest::ALIGN_CENTER:
        alignment = page_layout::Alignment(page_layout::Alignment::VCENTER, page_layout::Alignment::HCENTER);
        break;
      case CliRequest::ALIGN_ORIGINAL:
        alignment = page_layout::Alignment(page_layout::Alignment::VORIGINAL, page_layout::Alignment::HORIGINAL);
        break;
      case CliRequest::ALIGN_AUTO:
        alignment = page_layout::Alignment(page_layout::Alignment::VAUTO, page_layout::Alignment::HAUTO);
        break;
    }
    for (const PageInfo& page : m_pages) {
      settings->setPageAlignment(page.id(), alignment);
    }
  }
}

void FilterConfigAdapter::applyOutput(QString& /*errorMsg*/) {
  auto* filter = m_stages->outputFilter().get();
  auto settings = filter->getSettings();

  for (const PageInfo& page : m_pages) {
    const PageId& pageId = page.id();

    // Get current params (or defaults) and modify.
    output::Params params = settings->getParams(pageId);

    if (m_request.outputDpi > 0) {
      params.setOutputDpi(Dpi(m_request.outputDpi, m_request.outputDpi));
    }

    if (m_request.colorModeSet) {
      output::ColorParams cp = params.colorParams();
      cp.setColorMode(m_request.colorMode);
      params.setColorParams(cp);
    }

    if (m_request.threshold >= 0) {
      output::ColorParams cp = params.colorParams();
      auto bwOpts = cp.blackWhiteOptions();
      bwOpts.setThresholdAdjustment(m_request.threshold);
      cp.setBlackWhiteOptions(bwOpts);
      params.setColorParams(cp);
    }

    if (m_request.despeckleLevelSet) {
      params.setDespeckleLevel(static_cast<double>(m_request.despeckleLevel));
    }

    if (m_request.dewarpingModeSet) {
      output::DewarpingOptions dewarpOpts = params.dewarpingOptions();
      dewarpOpts.setDewarpingMode(m_request.dewarpingMode);
      params.setDewarpingOptions(dewarpOpts);
    }

    if (m_request.depthPerception > 0.0) {
      params.setDepthPerception(output::DepthPerception(m_request.depthPerception));
    }

    if (m_request.whiteMarginsSet) {
      output::ColorParams cp = params.colorParams();
      auto bwOpts = cp.blackWhiteOptions();
      bwOpts.setNormalizeIllumination(m_request.whiteMargins);
      cp.setBlackWhiteOptions(bwOpts);
      params.setColorParams(cp);
    }

    settings->setParams(pageId, params);
  }
}

}  // namespace cli
