// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_
#define SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_

#include <memory>
#include <set>

#include "CliRequest.h"
#include "FilterRange.h"
#include "NonCopyable.h"

class PageId;
class PageSequence;
class StageSequence;

namespace cli {

/**
 * Translates CliRequest values into filter Settings calls.
 * This is the ONLY place where CLI-origin data touches filter-internal types.
 */
class FilterConfigAdapter {
  DECLARE_NON_COPYABLE(FilterConfigAdapter)

 public:
  FilterConfigAdapter(const CliRequest& request,
                      std::shared_ptr<StageSequence> stages,
                      const PageSequence& pages);

  /** Apply the request's settings to every filter the range covers. */
  void apply(const FilterRange& range);

 private:
  void applyFixOrientation();
  void applyPageSplit();
  void applyDeskew();
  void applySelectContent();
  void applyPageLayout();
  void applyOutput();

  /** Every page of the run: the CLI has no page selection, so all of them are configured. */
  std::set<PageId> selectedPageIds() const;

  const CliRequest& m_request;
  std::shared_ptr<StageSequence> m_stages;
  const PageSequence& m_pages;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_
