// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CONSOLELOG_H_
#define SCANTAILOR_APP_CLI_CONSOLELOG_H_

#include <QString>

#include "CliRequest.h"

namespace cli {

/**
 * Timestamped stderr output with one verbosity policy. Progress survives
 * --quiet, detail needs --verbose, and errors always get through.
 */
class ConsoleLog {
 public:
  explicit ConsoleLog(const CliRequest& request);

  void progress(const QString& operation, const QString& message) const;

  void detail(const QString& operation, const QString& message) const;

  void error(const QString& operation, const QString& message) const;

  /** For failures before the request exists, so before verbosity is known. */
  static void writeError(const QString& message);

 private:
  bool m_quiet;
  bool m_verbose;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CONSOLELOG_H_
