// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_DESKEW_SETTINGS_H_
#define SCANTAILOR_DESKEW_SETTINGS_H_

#include <DeviationProvider.h>

#include <QMutex>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>

#include "NonCopyable.h"
#include "PageId.h"
#include "Params.h"

class AbstractRelinker;
class DocumentChangeNotifier;

namespace deskew {
class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  explicit Settings(std::shared_ptr<DocumentChangeNotifier> changeNotifier = nullptr);

  virtual ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  void setPageParams(const PageId& pageId, const Params& params);

  void clearPageParams(const PageId& pageId);

  std::unique_ptr<Params> getPageParams(const PageId& pageId) const;

  bool isParamsNull(const PageId& pageId) const;

  void setDegrees(const std::set<PageId>& pages, const Params& params);

  /** Which of the two independently applied angles a bulk copy touches. */
  enum class AngleSelection { Neither, Deskew, Oblique, Both };

  /**
   * \brief Copies the selected angles and modes from \p source onto \p pages.
   *
   * An unselected axis keeps the target's own angle and mode, or falls back to
   * angle zero in automatic mode when the target has no parameters yet. Whenever
   * at least one axis is selected, the source dependencies replace the target's.
   */
  void applyAngles(const std::set<PageId>& pages, const Params& source, AngleSelection selection);

  const DeviationProvider<PageId>& deviationProvider() const;

  void setAlgoContentBased(bool contentBased);

  bool algoContentBased() const;

  void setPendingAutoOblique(const PageId& pageId, bool enabled);

  std::optional<bool> takePendingAutoOblique(const PageId& pageId);

 private:
  using PerPageParams = std::unordered_map<PageId, Params>;

  std::shared_ptr<DocumentChangeNotifier> m_changeNotifier;
  mutable QMutex m_mutex;
  PerPageParams m_perPageParams;
  DeviationProvider<PageId> m_deviationProvider;
  bool m_algoContentBased;
  std::unordered_map<PageId, bool> m_pendingAutoOblique;
};
}  // namespace deskew
#endif  // ifndef SCANTAILOR_DESKEW_SETTINGS_H_
