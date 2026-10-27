// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_page_apply_dialog.h"

#include <ImageFileInfo.h>
#include <ImageMetadata.h>
#include <PageApplyDialog.h>
#include <PageId.h>
#include <PageRange.h>
#include <PageSelectionAccessor.h>
#include <PageSelectionProvider.h>
#include <PageSequence.h>
#include <PageView.h>
#include <ProjectPages.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <memory>
#include <set>
#include <vector>

namespace {

/** Snapshot of the page selection, which is what the real provider hands the dialog too. */
class FixedSelectionProvider : public PageSelectionProvider {
 public:
  FixedSelectionProvider(std::shared_ptr<ProjectPages> pages, std::set<PageId> selected)
      : m_pages(std::move(pages)), m_selected(std::move(selected)) {}

  PageSequence allPages() const override { return m_pages->toPageSequence(PAGE_VIEW); }

  std::set<PageId> selectedPages() const override { return m_selected; }

  std::vector<PageRange> selectedRanges() const override { return std::vector<PageRange>(); }

 private:
  std::shared_ptr<ProjectPages> m_pages;
  std::set<PageId> m_selected;
};

std::shared_ptr<ProjectPages> makePages(const int count) {
  std::vector<ImageFileInfo> files;
  for (int i = 0; i < count; ++i) {
    files.emplace_back(QFileInfo(QStringLiteral("/nonexistent/page%1.png").arg(i)),
                       std::vector<ImageMetadata>{ImageMetadata(QSize(300, 400), Dpi(300, 300))});
  }
  return std::make_shared<ProjectPages>(files, ProjectPages::ONE_PAGE, Qt::LeftToRight);
}

QRadioButton* scopeButton(PageApplyDialog& dialog, const char* name) {
  return dialog.findChild<QRadioButton*>(QString::fromLatin1(name));
}

}  // namespace

void TestPageApplyDialog::defaultsToThisPageOnly() {
  const std::shared_ptr<ProjectPages> pages = makePages(2);
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, std::set<PageId>());
  const PageSelectionAccessor accessor(provider);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"));

  QVERIFY(!dialog.allPages());
  QVERIFY(dialog.pages().empty());

  QRadioButton* const thisPageOnly = scopeButton(dialog, "thisPageOnly");
  QVERIFY(thisPageOnly != nullptr);
  QVERIFY(thisPageOnly->isChecked());
}

void TestPageApplyDialog::allPagesScopeResolvesEveryPage() {
  const std::shared_ptr<ProjectPages> pages = makePages(2);
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, std::set<PageId>());
  const PageSelectionAccessor accessor(provider);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"));

  QRadioButton* const allPages = scopeButton(dialog, "allPages");
  QVERIFY(allPages != nullptr);
  allPages->click();

  QVERIFY(dialog.allPages());

  const std::set<PageId> selected = dialog.pages();
  QCOMPARE(static_cast<int>(selected.size()), 2);
  QCOMPARE(static_cast<int>(selected.count(all.pageAt(0).id())), 1);
  QCOMPARE(static_cast<int>(selected.count(all.pageAt(1).id())), 1);
}

void TestPageApplyDialog::optionsReportTheirCheckedState() {
  const std::shared_ptr<ProjectPages> pages = makePages(2);
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, std::set<PageId>());
  const PageSelectionAccessor accessor(provider);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"),
                         {{QStringLiteral("First"), true}, {QStringLiteral("Second"), false}});

  QVERIFY(dialog.isOptionChecked(0));
  QVERIFY(!dialog.isOptionChecked(1));
  QVERIFY(!dialog.isOptionChecked(-1));
  QVERIFY(!dialog.isOptionChecked(2));

  auto* const second = dialog.findChild<QCheckBox*>(QStringLiteral("option1"));
  QVERIFY(second != nullptr);
  second->click();
  QVERIFY(dialog.isOptionChecked(1));
}

void TestPageApplyDialog::selectedPagesScopesResolveFromTheAccessor() {
  const std::shared_ptr<ProjectPages> pages = makePages(4);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);

  std::set<PageId> selection;
  for (int i = 0; i < 3; ++i) {
    selection.insert(all.pageAt(i).id());
  }
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, selection);
  const PageSelectionAccessor accessor(provider);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"));

  QRadioButton* const selectedPages = scopeButton(dialog, "selectedPages");
  QVERIFY(selectedPages != nullptr);
  selectedPages->click();
  QCOMPARE(static_cast<int>(dialog.pages().size()), 3);

  QRadioButton* const everyOtherSelected = scopeButton(dialog, "everyOtherSelected");
  QVERIFY(everyOtherSelected != nullptr);
  everyOtherSelected->click();

  // The current page and every second page after it within the selection: rows 0 and 2.
  const std::set<PageId> everyOther = dialog.pages();
  QCOMPARE(static_cast<int>(everyOther.size()), 2);
  QCOMPARE(static_cast<int>(everyOther.count(all.pageAt(0).id())), 1);
  QCOMPARE(static_cast<int>(everyOther.count(all.pageAt(1).id())), 0);
  QCOMPARE(static_cast<int>(everyOther.count(all.pageAt(2).id())), 1);
}

void TestPageApplyDialog::singleSelectionDisablesSelectedScopes() {
  const std::shared_ptr<ProjectPages> pages = makePages(3);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, std::set<PageId>{all.pageAt(0).id()});
  const PageSelectionAccessor accessor(provider);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"));

  QRadioButton* const selectedPages = scopeButton(dialog, "selectedPages");
  QRadioButton* const everyOtherSelected = scopeButton(dialog, "everyOtherSelected");
  QVERIFY(selectedPages != nullptr);
  QVERIFY(everyOtherSelected != nullptr);
  QVERIFY(!selectedPages->isEnabled());
  QVERIFY(!everyOtherSelected->isEnabled());
}

void TestPageApplyDialog::okEmitsSubmittedThenAccepts() {
  const std::shared_ptr<ProjectPages> pages = makePages(2);
  const auto provider = std::make_shared<FixedSelectionProvider>(pages, std::set<PageId>());
  const PageSelectionAccessor accessor(provider);
  const PageSequence all = pages->toPageSequence(PAGE_VIEW);

  PageApplyDialog dialog(nullptr, all.pageAt(0).id(), accessor, QStringLiteral("Test"));
  QSignalSpy submitted(&dialog, &PageApplyDialog::submitted);
  QVERIFY(submitted.isValid());

  auto* const buttonBox = dialog.findChild<QDialogButtonBox*>();
  QVERIFY(buttonBox != nullptr);
  QPushButton* const ok = buttonBox->button(QDialogButtonBox::Ok);
  QVERIFY(ok != nullptr);

  ok->click();

  QCOMPARE(submitted.count(), 1);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
}
