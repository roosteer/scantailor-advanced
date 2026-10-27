// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_project_document_ui.h"

#include <QApplication>
#include <QtTest/QtTest>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  TestProjectDocumentUi test;
  return QTest::qExec(&test, argc, argv);
}
