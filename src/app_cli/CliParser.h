// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CLIPARSER_H_
#define SCANTAILOR_APP_CLI_CLIPARSER_H_

#include <QStringList>

#include "CliRequest.h"
#include "ExitCode.h"

namespace cli {

struct ParseResult {
  CliRequest request;
  CliError error;
};

/**
 * Turns argv (without the program name) into a request. On failure the request
 * holds whatever was parsed so far and error says why.
 */
ParseResult parseCommandLine(const QStringList& args);

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CLIPARSER_H_
