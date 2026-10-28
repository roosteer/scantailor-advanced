// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <iostream>

#include "../../Utils.h"
#include "AbstractRelinker.h"
#include "DocumentChangeNotifier.h"
#include "RelinkablePath.h"

using namespace core;

namespace select_content {
namespace {
constexpr double kUnboundedPageDetectionBoxMm = 100000.0;
}  // namespace

Settings::Settings(std::shared_ptr<DocumentChangeNotifier> changeNotifier)
    : m_changeNotifier(std::move(changeNotifier)), m_pageDetectionBox(0.0, 0.0), m_pageDetectionTolerance(0.1) {
  m_deviationProvider.setComputeValueByKey([this](const PageId& pageId) -> double {
    auto it(m_pageParams.find(pageId));
    if (it != m_pageParams.end()) {
      const Params& params = it->second;
      const QSizeF& contentSizeMM = params.contentSizeMM();
      if (!contentSizeMM.toSize().isEmpty())
        return std::sqrt(std::pow(contentSizeMM.width(), 2) + std::pow(contentSizeMM.height(), 2));
    }
    return NAN;
  });
}

Settings::~Settings() = default;

void Settings::clear() {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  m_pageParams.clear();
  m_deviationProvider.clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  PageParams newParams;

  for (const PageParams::value_type& kv : m_pageParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newParams.insert(PageParams::value_type(newPageId, kv.second));
  }

  m_pageParams.swap(newParams);

  m_deviationProvider.clear();
  for (const PageParams::value_type& kv : m_pageParams) {
    m_deviationProvider.addOrUpdate(kv.first);
  }
}

void Settings::setPageParams(const PageId& pageId, const Params& params) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_pageParams, pageId, params);
  m_deviationProvider.addOrUpdate(pageId);
}

void Settings::clearPageParams(const PageId& pageId) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  m_pageParams.erase(pageId);
  m_deviationProvider.remove(pageId);
}

std::unique_ptr<Params> Settings::getPageParams(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);

  const auto it(m_pageParams.find(pageId));
  if (it != m_pageParams.end()) {
    return std::make_unique<Params>(it->second);
  } else {
    return nullptr;
  }
}

bool Settings::isParamsNull(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);
  return (m_pageParams.find(pageId) == m_pageParams.end());
}

void Settings::copyBoxes(const std::set<PageId>& pages,
                         const PageId& sourcePage,
                         const Params& source,
                         const BoxSelection selection) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  const bool applyContentBox = (selection == BoxSelection::Content) || (selection == BoxSelection::Both);
  const bool applyPageBox = (selection == BoxSelection::Page) || (selection == BoxSelection::Both);

  const QMutexLocker locker(&m_mutex);
  for (const PageId& pageId : pages) {
    if (pageId == sourcePage) {
      continue;
    }

    Params newParams(source);
    newParams.setDependencies(Dependencies());

    const auto it(m_pageParams.find(pageId));
    if (it != m_pageParams.end()) {
      const Params& oldParams = it->second;

      if ((newParams.pageDetectionMode() == MODE_MANUAL) && !applyPageBox) {
        newParams.setPageRect(oldParams.pageRect());
      }

      if (newParams.contentDetectionMode() == MODE_MANUAL) {
        if (!applyContentBox) {
          newParams.setContentRect(oldParams.contentRect());
        } else if (!newParams.contentRect().isEmpty()) {
          QRectF correctedContentRect = newParams.contentRect();
          const QRectF& sourcePageRect = source.pageRect();
          const QRectF& newPageRect = newParams.pageRect();
          correctedContentRect.translate(newPageRect.x() - sourcePageRect.x(), newPageRect.y() - sourcePageRect.y());
          newParams.setContentRect(correctedContentRect);
        }
      }
    }

    if (applyContentBox && (newParams.contentDetectionMode() == MODE_AUTO)) {
      newParams.setDependencies(
          Dependencies(QPolygonF(), MODE_DISABLED, newParams.pageDetectionMode(), newParams.isFineTuningEnabled()));
    }

    Utils::mapSetValue(m_pageParams, pageId, newParams);
    m_deviationProvider.addOrUpdate(pageId);
  }
}

void Settings::setManualBox(const std::set<PageId>& pages, const QRectF& rect) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  const QSizeF sizeMm(rect.width(), rect.height());
  const Params params(rect, sizeMm, rect, Dependencies(), MODE_MANUAL, MODE_MANUAL, false);

  const QMutexLocker locker(&m_mutex);
  for (const PageId& pageId : pages) {
    Utils::mapSetValue(m_pageParams, pageId, params);
    m_deviationProvider.addOrUpdate(pageId);
  }
}

void Settings::useUnboundedPageDetection() {
  setPageDetectionBox(QSizeF(kUnboundedPageDetectionBoxMm, kUnboundedPageDetectionBoxMm));
  setPageDetectionTolerance(0.0);
}

QSizeF Settings::pageDetectionBox() const {
  QMutexLocker locker(&m_mutex);
  return m_pageDetectionBox;
}

void Settings::setPageDetectionBox(QSizeF size) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  m_pageDetectionBox = size;
}

double Settings::pageDetectionTolerance() const {
  QMutexLocker locker(&m_mutex);
  return m_pageDetectionTolerance;
}

void Settings::setPageDetectionTolerance(double tolerance) {
  DocumentChangeGuard guard(m_changeNotifier.get());

  QMutexLocker locker(&m_mutex);
  m_pageDetectionTolerance = tolerance;
}

const DeviationProvider<PageId>& Settings::deviationProvider() const {
  return m_deviationProvider;
}

}  // namespace select_content
