// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PROJECTSAVEINTERACTION_H_
#define SCANTAILOR_APP_PROJECTSAVEINTERACTION_H_

#include <QString>

class ProjectDocument;
class QWidget;

/**
 * \brief Saving a project the way the user sees it: file dialog, warning, recent projects.
 *
 * The main window and the out-of-memory dialog both save, and both have to leave the current
 * file path and the settings alone when the user cancels or the write fails.
 */
namespace project_save {

/** Saves \p document to \p filePath, warning on failure. */
bool saveWithFeedback(QWidget* parent, const ProjectDocument& document, const QString& filePath);

/**
 * Asks for a file and saves \p document there.
 *
 * On success \p currentProjectFile, the last-used directory and the recent projects list are
 * updated; on cancellation or failure none of them change.
 */
bool saveAs(QWidget* parent, const ProjectDocument& document, QString& currentProjectFile);

}  // namespace project_save

#endif  // ifndef SCANTAILOR_APP_PROJECTSAVEINTERACTION_H_
