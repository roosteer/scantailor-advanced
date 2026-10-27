// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PAGEAPPLYDIALOG_H_
#define SCANTAILOR_CORE_PAGEAPPLYDIALOG_H_

#include <QDialog>
#include <QString>
#include <set>
#include <vector>

#include "PageId.h"
#include "PageSequence.h"

class PageSelectionAccessor;
class QCheckBox;
class QRadioButton;

/**
 * Asks which pages a set of option changes applies to, and which options to apply.
 *
 * Every filter that copies the current page's settings elsewhere (deskew, fix orientation,
 * page layout, select content) shows this dialog instead of its own private one.
 */
class PageApplyDialog : public QDialog {
  Q_OBJECT
 public:
  struct Option {
    QString label;
    bool checked = true;
  };

  PageApplyDialog(QWidget* parent, const PageId& curPage, const PageSelectionAccessor& pageSelectionAccessor,
                  const QString& title, const std::vector<Option>& options = std::vector<Option>(),
                  const QString& hint = QString());

  /** Pages the change applies to, resolved from the checked scope. Empty for "this page only". */
  std::set<PageId> pages() const;

  bool isOptionChecked(int index) const;

  /** True when the "All pages" scope is checked. */
  bool allPages() const;

 signals:
  /** Emitted when Ok is pressed, just before the dialog accepts itself. */
  void submitted();

 private:
  enum Scope {
    THIS_PAGE_ONLY,
    ALL_PAGES,
    THIS_AND_FOLLOWING_PAGES,
    THIS_AND_FOLLOWING_EVERY_OTHER,
    EVERY_OTHER,
    SELECTED_PAGES,
    EVERY_OTHER_SELECTED,
    SCOPE_COUNT
  };

  void onSubmit();

  int checkedScope() const;

  PageSequence m_pages;
  PageId m_curPage;
  std::set<PageId> m_selectedPages;
  std::vector<QRadioButton*> m_scopeButtons;
  std::vector<QCheckBox*> m_optionButtons;
};

#endif  // SCANTAILOR_CORE_PAGEAPPLYDIALOG_H_
