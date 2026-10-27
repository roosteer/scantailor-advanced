// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CLIUSAGE_H_
#define SCANTAILOR_APP_CLI_CLIUSAGE_H_

#include <QString>
#include <vector>

#include "CliRequest.h"
#include "ExitCode.h"

namespace cli {

/**
 * Applies one flag's value to the request. Returns INVALID_INPUT and fills
 * errorMessage when the value is not acceptable.
 */
using FlagHandler = ExitCode (*)(const QString& value, CliRequest& out, QString& errorMessage);

struct FlagSpec {
  const char* name;       // "--output-dpi"
  const char* valueHint;  // "N"; nullptr marks a boolean flag, which must not carry "=value"
  const char* group;      // "General", "Filter selection", or a filter name
  const char* description;
  FlagHandler handler;
};

/** The flag table: the single source of truth for both parsing and the help text. */
const std::vector<FlagSpec>& flagSpecs();

/** Null when the name (with its leading dashes) is not a known flag. */
const FlagSpec* findFlagSpec(const QString& name);

/** Usage, filter indices, exit codes, examples, and the flag table, on stdout. */
void printHelp();

/** Program and project versions, on stdout. */
void printVersion();

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CLIUSAGE_H_
