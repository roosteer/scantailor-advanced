// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_EXITCODE_H_
#define SCANTAILOR_APP_CLI_EXITCODE_H_

namespace cli {

/**
 * Distinct exit codes for different error classes.
 * Code 1 is reserved for unhandled exceptions.
 */
enum class ExitCode {
  SUCCESS = 0,
  INVALID_INPUT = 2,
  FILE_NOT_FOUND = 3,
  FILTER_ERROR = 4,
  OUTPUT_ERROR = 5,
  INTERNAL_ERROR = 6
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_EXITCODE_H_
