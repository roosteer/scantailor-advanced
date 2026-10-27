// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PageApplyDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "PageSelectionAccessor.h"

namespace {

// Scope hints sit under their radio button, indented like the .ui files did.
constexpr int HINT_INDENT = 30;

QLabel* makeHint(const QString& text) {
  auto* label = new QLabel(text);
  QFont font = label->font();
  font.setPointSize(7);
  label->setFont(font);
  return label;
}

QLabel* addHintRow(QVBoxLayout* layout, const QString& text) {
  auto* row = new QHBoxLayout;
  row->setSpacing(6);
  row->addSpacerItem(new QSpacerItem(HINT_INDENT, 0, QSizePolicy::Fixed, QSizePolicy::Minimum));
  QLabel* const label = makeHint(text);
  row->addWidget(label);
  layout->addLayout(row);
  return label;
}

QRadioButton* addScope(QVBoxLayout* layout, const char* name, const QString& label) {
  auto* button = new QRadioButton(label);
  button->setObjectName(QString::fromLatin1(name));
  layout->addWidget(button);
  return button;
}

}  // namespace

PageApplyDialog::PageApplyDialog(QWidget* parent, const PageId& curPage,
                                 const PageSelectionAccessor& pageSelectionAccessor, const QString& title,
                                 const std::vector<Option>& options, const QString& hint)
    : QDialog(parent),
      m_pages(pageSelectionAccessor.allPages()),
      m_curPage(curPage),
      m_selectedPages(pageSelectionAccessor.selectedPages()) {
  setWindowTitle(title);
  setWindowModality(Qt::WindowModal);

  auto* layout = new QVBoxLayout(this);

  auto* scopeBox = new QGroupBox(tr("Apply to"), this);
  auto* scopeLayout = new QVBoxLayout(scopeBox);
  m_scopeButtons.assign(SCOPE_COUNT, nullptr);

  m_scopeButtons[THIS_PAGE_ONLY] = addScope(scopeLayout, "thisPageOnly", tr("This page only (already applied)"));
  m_scopeButtons[THIS_PAGE_ONLY]->setChecked(true);
  m_scopeButtons[ALL_PAGES] = addScope(scopeLayout, "allPages", tr("All pages"));
  m_scopeButtons[THIS_AND_FOLLOWING_PAGES] =
      addScope(scopeLayout, "thisPageAndFollowers", tr("This page and the following ones"));
  m_scopeButtons[THIS_AND_FOLLOWING_EVERY_OTHER] =
      addScope(scopeLayout, "thisAndFollowingEveryOther", tr("This page and the following every other page"));
  m_scopeButtons[EVERY_OTHER] = addScope(scopeLayout, "everyOther", tr("Every other page"));
  m_scopeButtons[SELECTED_PAGES] = addScope(scopeLayout, "selectedPages", tr("Selected pages"));
  QLabel* const selectedPagesHint =
      addHintRow(scopeLayout, tr("Use Ctrl+Click / Shift+Click to select multiple pages."));
  m_scopeButtons[EVERY_OTHER_SELECTED] = addScope(scopeLayout, "everyOtherSelected", tr("Every other selected page"));
  QLabel* const everyOtherSelectedHint = addHintRow(scopeLayout, tr("The current page will be included."));

  if (m_selectedPages.size() <= 1) {
    for (QWidget* widget : {static_cast<QWidget*>(m_scopeButtons[SELECTED_PAGES]),
                            static_cast<QWidget*>(selectedPagesHint),
                            static_cast<QWidget*>(m_scopeButtons[EVERY_OTHER_SELECTED]),
                            static_cast<QWidget*>(everyOtherSelectedHint)}) {
      widget->setEnabled(false);
    }
  }

  layout->addWidget(scopeBox);

  if (!options.empty()) {
    auto* optionsBox = new QGroupBox(tr("Options"), this);
    auto* optionsLayout = new QVBoxLayout(optionsBox);
    for (size_t i = 0; i < options.size(); ++i) {
      auto* box = new QCheckBox(options[i].label, optionsBox);
      box->setObjectName(QStringLiteral("option%1").arg(i));
      box->setChecked(options[i].checked);
      optionsLayout->addWidget(box);
      m_optionButtons.push_back(box);
    }
    layout->addWidget(optionsBox);
  }

  if (!hint.isEmpty()) {
    auto* hintLabel = new QLabel(hint, this);
    hintLabel->setWordWrap(true);
    QFont hintFont = hintLabel->font();
    hintFont.setPointSize(8);
    hintLabel->setFont(hintFont);
    layout->addWidget(hintLabel);
  }

  layout->addStretch();

  auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  layout->addWidget(buttonBox);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &PageApplyDialog::onSubmit);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &PageApplyDialog::reject);
}

std::set<PageId> PageApplyDialog::pages() const {
  switch (checkedScope()) {
    case ALL_PAGES:
      return m_pages.selectAll();
    case THIS_AND_FOLLOWING_PAGES:
      return m_pages.selectPagePlusFollowers(m_curPage);
    case THIS_AND_FOLLOWING_EVERY_OTHER:
      return m_pages.selectThisPageAndFollowingEveryOther(m_curPage);
    case EVERY_OTHER:
      return m_pages.selectEveryOther(m_curPage);
    case SELECTED_PAGES:
      return m_selectedPages;
    case EVERY_OTHER_SELECTED:
      // Resolves over the selection in document order, so it stays defined for a
      // non-contiguous selection where a single PageRange would not exist.
      return m_pages.selectEveryOtherInSubsetFromPage(m_curPage, m_selectedPages);
    case THIS_PAGE_ONLY:
    default:
      return std::set<PageId>();
  }
}

bool PageApplyDialog::isOptionChecked(const int index) const {
  return (index >= 0) && (index < static_cast<int>(m_optionButtons.size())) && m_optionButtons[index]->isChecked();
}

bool PageApplyDialog::allPages() const {
  return checkedScope() == ALL_PAGES;
}

int PageApplyDialog::checkedScope() const {
  for (int i = 0; i < static_cast<int>(m_scopeButtons.size()); ++i) {
    if (m_scopeButtons[i] && m_scopeButtons[i]->isChecked()) {
      return i;
    }
  }
  return THIS_PAGE_ONLY;
}

void PageApplyDialog::onSubmit() {
  emit submitted();
  accept();
}
