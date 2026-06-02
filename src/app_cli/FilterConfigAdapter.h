// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_
#define SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_

#include <QString>
#include <memory>

#include "CliRequest.h"
#include "NonCopyable.h"

class PageSequence;
class StageSequence;

namespace page_split {
class Filter;
}
namespace output {
class Filter;
}

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

  /** Apply CLI settings to all filters in the range [startFilter, endFilter]. */
  void apply(int startFilter, int endFilter, QString& errorMsg);

 private:
  void applyFixOrientation(QString& errorMsg);
  void applyPageSplit(QString& errorMsg);
  void applyDeskew(QString& errorMsg);
  void applySelectContent(QString& errorMsg);
  void applyPageLayout(QString& errorMsg);
  void applyOutput(QString& errorMsg);

  const CliRequest& m_request;
  std::shared_ptr<StageSequence> m_stages;
  const PageSequence& m_pages;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_FILTERCONFIGADAPTER_H_
