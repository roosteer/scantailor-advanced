// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OutOfMemoryDialog.h"

#include <utility>

#include "ProjectDocument.h"
#include "ProjectSaveInteraction.h"

OutOfMemoryDialog::OutOfMemoryDialog(QWidget* parent) : QDialog(parent) {
  ui.setupUi(this);
  if (sizeof(void*) > 4) {
    ui.only_32bit_1->hide();
    ui.only_32bit_2->hide();
  }

  ui.topLevelStack->setCurrentWidget(ui.mainPage);

  connect(ui.saveProjectBtn, SIGNAL(clicked()), SLOT(saveProject()));
  connect(ui.saveProjectAsBtn, SIGNAL(clicked()), SLOT(saveProjectAs()));
  connect(ui.dontSaveBtn, SIGNAL(clicked()), SLOT(reject()));
}

void OutOfMemoryDialog::setParams(const QString& projectFile, std::shared_ptr<ProjectDocument> document) {
  m_projectFile = projectFile;
  m_document = std::move(document);

  ui.saveProjectBtn->setVisible(!projectFile.isEmpty());
}

void OutOfMemoryDialog::saveProject() {
  if (m_projectFile.isEmpty()) {
    saveProjectAs();
  } else if (project_save::saveWithFeedback(this, *m_document, m_projectFile)) {
    showSaveSuccessScreen();
  }
}

void OutOfMemoryDialog::saveProjectAs() {
  if (project_save::saveAs(this, *m_document, m_projectFile)) {
    showSaveSuccessScreen();
  }
}

void OutOfMemoryDialog::showSaveSuccessScreen() {
  ui.topLevelStack->setCurrentWidget(ui.saveSuccessPage);
}
