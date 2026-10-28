// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_SPLIT_SETTINGS_H_
#define SCANTAILOR_PAGE_SPLIT_SETTINGS_H_

#include <QMutex>
#include <memory>
#include <set>
#include <unordered_map>

#include "ImageId.h"
#include "LayoutType.h"
#include "NonCopyable.h"
#include "PageId.h"
#include "PageLayout.h"
#include "Params.h"

class AbstractRelinker;
class DocumentChangeNotifier;

namespace page_split {
class Settings {
  DECLARE_NON_COPYABLE(Settings)

 private:
  class BaseRecord {
    // Member-wise copying is OK.
    friend class Settings;

   public:
    BaseRecord();

    const LayoutType* layoutType() const;

    const Params* params() const;

    /**
     * \brief A record is considered null of it doesn't carry any
     *        information.
     */
    bool isNull() const;

   protected:
    void setParams(const Params& params);

    void setLayoutType(LayoutType layoutType);

    void clearParams();

    void clearLayoutType();

    /**
     * \brief Checks if a particular layout type conflicts with PageLayout
     *        that is part of Params.
     */
    bool hasLayoutTypeConflict(LayoutType layoutType) const;

    Params m_params;
    LayoutType m_layoutType;
    bool m_paramsValid;
    bool m_layoutTypeValid;
  };


 public:
  class UpdateAction;

  class Record : public BaseRecord {
    // Member-wise copying is OK.
   public:
    explicit Record(LayoutType defaultLayoutType);

    Record(const BaseRecord& baseRecord, LayoutType defaultLayoutType);

    LayoutType combinedLayoutType() const;

    void update(const UpdateAction& action);

    bool hasLayoutTypeConflict() const;

   private:
    LayoutType m_defaultLayoutType;
  };


  class UpdateAction {
    friend class Settings::Record;

   public:
    UpdateAction();

    void setLayoutType(LayoutType layoutType);

    void clearLayoutType();

    void setParams(const Params& params);

    void clearParams();

   private:
    enum Action { DONT_TOUCH, SET, CLEAR };

    Params m_params;
    LayoutType m_layoutType;
    Action m_paramsAction;
    Action m_layoutTypeAction;
  };


  explicit Settings(std::shared_ptr<DocumentChangeNotifier> changeNotifier = nullptr);

  virtual ~Settings();

  /**
   * \brief Reset all settings to their initial state.
   */
  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  LayoutType defaultLayoutType() const;

  /**
   * Sets layout type for all pages, removing other page
   * parameters where they conflict with the new layout type.
   */
  void setLayoutTypeForAllPages(LayoutType layoutType);

  /**
   * \brief Sets layout type for specified pages, removing other page
   * parameters where they conflict with the new layout type.
   */
  void setLayoutTypeFor(LayoutType layoutType, const std::set<PageId>& pages);

  /** Whether a bulk layout change keeps the target's cutters or adopts the source's. */
  enum class CutTransfer { KeepTarget, CopySource };

  /**
   * \brief Sets \p layoutType on every page of \p pages.
   *
   * With CutTransfer::CopySource and a concrete layout type, the source
   * parameters are copied onto each target, with the source cutters adapted to
   * that target's own outline. Targets that already carry parameters keep their
   * dependencies apart from the layout type.
   */
  void applyLayout(const std::set<PageId>& pages, LayoutType layoutType, const Params& source, CutTransfer transfer);

  /**
   * \brief Returns all data related to a page as a single object.
   */
  Record getPageRecord(const ImageId& imageId) const;

  /**
   * \brief Performs the requested update on the page.
   *
   * If the update would lead to a conflict between the layout
   * type and page parameters, the page parameters will be
   * cleared.
   */
  void updatePage(const ImageId& imageId, const UpdateAction& action);

  /**
   * \brief Performs a conditional update on the page.
   *
   * If the update would lead to a conflict between the layout
   * type and page parameters, the update won't take place.
   * Whether the update took place or not, the current page record
   * (updated or not) will be returned.
   */
  Record conditionalUpdate(const ImageId& imageId, const UpdateAction& action, bool* conflict = nullptr);

 private:
  using PerPageRecords = std::unordered_map<ImageId, BaseRecord>;

  Record getPageRecordLocked(const ImageId& imageId) const;

  void updatePageLocked(const ImageId& imageId, const UpdateAction& action);

  void updatePageLocked(PerPageRecords::iterator it, const UpdateAction& action);

  std::shared_ptr<DocumentChangeNotifier> m_changeNotifier;
  mutable QMutex m_mutex;
  PerPageRecords m_perPageRecords;
  LayoutType m_defaultLayoutType;
};
}  // namespace page_split
#endif  // ifndef SCANTAILOR_PAGE_SPLIT_SETTINGS_H_
