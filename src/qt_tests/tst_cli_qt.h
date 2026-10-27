// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef TST_CLI_QT_H
#define TST_CLI_QT_H

#include <QObject>

class TestCliQt : public QObject {
  Q_OBJECT
 private slots:
  void parsesDefaults();
  void parsesValueFlags();
  void acceptsLegacyColorModeAliases();
  void rejectsUnknownFlag();
  void rejectsMalformedValues();
  void rejectsConflictingOptions();
  void resolvesPositionals();
  void flagTableIsWellFormed();
  void resolvesFilterRange();
  void rendersReport();
  void mapsExitCode();
};

#endif  // TST_CLI_QT_H
