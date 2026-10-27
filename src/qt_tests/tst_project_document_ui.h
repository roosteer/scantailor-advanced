// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef TST_PROJECT_DOCUMENT_UI_H
#define TST_PROJECT_DOCUMENT_UI_H

#include <QObject>

/**
 * \brief The seam where a ProjectDocument meets the editor's widgets.
 *
 * The main window is gone by the time the out-of-memory dialog saves, so what the dialog
 * holds and writes is exactly what these cases check.
 */
class TestProjectDocumentUi : public QObject {
  Q_OBJECT
 private slots:
  void saveButtonFollowsTheProjectFile();

  void savesTheDocumentAfterTheEditorIsGone();
};

#endif  // TST_PROJECT_DOCUMENT_UI_H
