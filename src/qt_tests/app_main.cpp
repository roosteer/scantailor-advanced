// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "tst_project_document_ui.h"

#include <config.h>
#include <core/ConfigPaths.h>
#include <core/IconProvider.h>
#include <core/StyledIconPack.h>

#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

int main(int argc, char* argv[]) {
  // These tests enable application-wide autosave and open real projects. Both the settings and
  // the recent-projects list go to a throwaway config home instead of the user's.
  QTemporaryDir configHome;
  if (configHome.isValid()) {
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
  }

  QApplication app(argc, argv);

  // QSettings finds the application's own file through these names, exactly as main.cpp does.
  QApplication::setApplicationName(APPLICATION_NAME);
  QApplication::setOrganizationName(ORGANIZATION_NAME);
  ConfigPaths::configureQSettings();
  IconProvider::getInstance().setIconPack(StyledIconPack::createDefault());

  // A test closes windows on purpose; that must not end the test run.
  QApplication::setQuitOnLastWindowClosed(false);

  TestProjectDocumentUi test;
  return QTest::qExec(&test, argc, argv);
}
