// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_SELECT_CONTENT_SETTINGS_H_
#define SCANTAILOR_SELECT_CONTENT_SETTINGS_H_

#include <DeviationProvider.h>

#include <QMutex>
#include <memory>
#include <set>
#include <unordered_map>

#include "NonCopyable.h"
#include "PageId.h"
#include "Params.h"

class AbstractRelinker;

namespace select_content {
class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  Settings();

  virtual ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  void setPageParams(const PageId& pageId, const Params& params);

  void clearPageParams(const PageId& pageId);

  std::unique_ptr<Params> getPageParams(const PageId& pageId) const;

  bool isParamsNull(const PageId& pageId) const;

  /** Which of the two boxes a bulk copy touches. */
  enum class BoxSelection { Neither, Content, Page, Both };

  /**
   * \brief Copies the selected boxes and their modes from \p source onto \p pages.
   *
   * \p sourcePage itself is never a target. Every copied record starts with
   * empty dependencies, so the target box is recomputed on the next run.
   */
  void copyBoxes(const std::set<PageId>& pages,
                 const PageId& sourcePage,
                 const Params& source,
                 BoxSelection selection);

  /** Sets a fixed manual page and content box on every page of \p pages. */
  void setManualBox(const std::set<PageId>& pages, const QRectF& rect);

  /**
   * \brief Disables content detection for every page at once.
   *
   * Expressed as a detection box big enough to always contain the page and a
   * zero tolerance, which leaves the per-page detection modes untouched.
   */
  void useUnboundedPageDetection();

  QSizeF pageDetectionBox() const;

  void setPageDetectionBox(QSizeF size);

  double pageDetectionTolerance() const;

  void setPageDetectionTolerance(double tolerance);

  const DeviationProvider<PageId>& deviationProvider() const;

 private:
  using PageParams = std::unordered_map<PageId, Params>;

  mutable QMutex m_mutex;
  PageParams m_pageParams;
  QSizeF m_pageDetectionBox;
  double m_pageDetectionTolerance;
  DeviationProvider<PageId> m_deviationProvider;
};
}  // namespace select_content
#endif  // ifndef SCANTAILOR_SELECT_CONTENT_SETTINGS_H_
