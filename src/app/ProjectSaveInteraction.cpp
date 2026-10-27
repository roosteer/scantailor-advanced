// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ProjectSaveInteraction.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QString>

#include "ProjectDocument.h"
#include "RecentProjects.h"

namespace project_save {

namespace {

QString translated(const char* text) {
  return QCoreApplication::translate("MainWindow", text);
}

QString projectFilter() {
  return translated("Scan Tailor Projects") + QStringLiteral(" (*.ScanTailor)");
}

}  // namespace

bool saveWithFeedback(QWidget* parent, const ProjectDocument& document, const QString& filePath) {
  if (document.save(filePath)) {
    return true;
  }

  QMessageBox::warning(parent, translated("Error"), translated("Error saving the project file!"));
  return false;
}

bool saveAs(QWidget* parent, const ProjectDocument& document, QString& currentProjectFile) {
  QString projectDir;
  if (!currentProjectFile.isEmpty()) {
    projectDir = QFileInfo(currentProjectFile).absolutePath();
  } else {
    QSettings settings;
    projectDir = settings.value("project/lastDir").toString();
  }

  QString projectFile(QFileDialog::getSaveFileName(parent, QString(), projectDir, projectFilter()));
  if (projectFile.isEmpty()) {
    // Cancelled by the user: no path, no settings, no recent projects.
    return false;
  }

  if (!projectFile.endsWith(QStringLiteral(".ScanTailor"), Qt::CaseInsensitive)) {
    projectFile += QStringLiteral(".ScanTailor");
  }

  if (!saveWithFeedback(parent, document, projectFile)) {
    return false;
  }

  currentProjectFile = projectFile;

  QSettings settings;
  settings.setValue("project/lastDir", QFileInfo(currentProjectFile).absolutePath());

  RecentProjects rp;
  rp.read();
  rp.setMostRecent(currentProjectFile);
  rp.write();

  return true;
}

}  // namespace project_save
