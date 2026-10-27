// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_BATCHREPORT_H_
#define SCANTAILOR_APP_CLI_BATCHREPORT_H_

#include <QString>
#include <vector>

#include "ExitCode.h"

namespace cli {

struct PageOutcome {
  int index = 0;  // 0-based position in the processed sequence
  QString inputFile;
  QString outputFile;  // empty when the output stage did not run
  bool ok = false;
  QString errorMessage;
};

struct BatchReport {
  std::vector<PageOutcome> pages;

  int processed() const;
  int failed() const;
};

/** The JSON document promised by --output=json, trailing newline included. */
QString renderJsonReport(const BatchReport& report);

/** FILTER_ERROR when any page failed, SUCCESS otherwise. */
ExitCode exitCodeFor(const BatchReport& report);

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_BATCHREPORT_H_
