// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_OUTOFMEMORYDIALOG_H_
#define SCANTAILOR_APP_OUTOFMEMORYDIALOG_H_

#include <QDialog>
#include <QString>
#include <memory>

#include "ui_OutOfMemoryDialog.h"

class ProjectDocument;

/**
 * \brief Last chance to save a project that is already being torn down.
 *
 * The dialog owns the document it is given, so the window it belongs to may be gone by the
 * time the user presses Save.
 */
class OutOfMemoryDialog : public QDialog {
  Q_OBJECT
 public:
  explicit OutOfMemoryDialog(QWidget* parent = nullptr);

  void setParams(const QString& projectFile, std::shared_ptr<ProjectDocument> document);

 private slots:

  void saveProject();

  void saveProjectAs();

 private:
  void showSaveSuccessScreen();

  Ui::OutOfMemoryDialog ui;
  QString m_projectFile;
  std::shared_ptr<ProjectDocument> m_document;
};


#endif  // ifndef SCANTAILOR_APP_OUTOFMEMORYDIALOG_H_
