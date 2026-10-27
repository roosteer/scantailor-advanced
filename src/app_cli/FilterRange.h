// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_FILTERRANGE_H_
#define SCANTAILOR_APP_CLI_FILTERRANGE_H_

#include <optional>

#include "CliRequest.h"
#include "ExitCode.h"

namespace cli {

/**
 * Stages of the pipeline, in processing order. The indices are the ones
 * --start-filter and --end-filter refer to.
 */
constexpr int kFilterCount = 6;

/** Inclusive, zero-based filter index range. */
struct FilterRange {
  int start = 0;
  int end = kFilterCount - 1;
};

/**
 * Validates the filter indices of the request against the stages actually
 * available. On failure fills error with INVALID_INPUT and returns nullopt.
 */
std::optional<FilterRange> resolveFilterRange(const CliRequest& request, int availableFilters, CliError& error);

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_FILTERRANGE_H_
