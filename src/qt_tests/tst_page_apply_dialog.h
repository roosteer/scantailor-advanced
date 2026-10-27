// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef TST_PAGE_APPLY_DIALOG_H
#define TST_PAGE_APPLY_DIALOG_H

#include <QObject>

class TestPageApplyDialog : public QObject {
  Q_OBJECT
 private slots:
  void defaultsToThisPageOnly();
  void allPagesScopeResolvesEveryPage();
  void optionsReportTheirCheckedState();
  void selectedPagesScopesResolveFromTheAccessor();
  void singleSelectionDisablesSelectedScopes();
  void okEmitsSubmittedThenAccepts();
};

#endif  // TST_PAGE_APPLY_DIALOG_H
