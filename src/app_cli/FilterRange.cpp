// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "FilterRange.h"

namespace cli {

std::optional<FilterRange> resolveFilterRange(const CliRequest& request, int availableFilters, CliError& error) {
  FilterRange range;
  range.start = request.startFilter.value_or(0);
  range.end = request.endFilter.value_or(availableFilters - 1);

  if (range.start > range.end) {
    error = {ExitCode::INVALID_INPUT,
             QString("--start-filter (%1) must not be greater than --end-filter (%2)")
                 .arg(range.start)
                 .arg(range.end)};
    return std::nullopt;
  }
  if (range.end >= availableFilters) {
    error = {ExitCode::INVALID_INPUT,
             QString("--end-filter (%1) out of range (0..%2)").arg(range.end).arg(availableFilters - 1)};
    return std::nullopt;
  }
  return range;
}

}  // namespace cli
