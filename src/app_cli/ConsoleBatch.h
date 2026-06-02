// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CONSOLEBATCH_H_
#define SCANTAILOR_APP_CLI_CONSOLEBATCH_H_

#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "CliRequest.h"
#include "ExitCode.h"
#include "NonCopyable.h"

class BackgroundTask;
class OutputFileNameGenerator;
class PageInfo;
class PageSequence;
class ProjectPages;
class StageSequence;
class ThumbnailPixmapCache;

namespace cli {

/**
 * Per-page processing result.
 */
struct PageResult {
  QString inputFile;
  QString outputFile;
  bool success = false;
  QString errorFilter;
  QString errorMessage;
};

/**
 * Use case orchestrator.
 * Configures the filter pipeline from a CliRequest and executes batch processing.
 */
class ConsoleBatch {
  DECLARE_NON_COPYABLE(ConsoleBatch)

 public:
  /**
   * Constructor for processing images directly.
   * @param request Parsed CLI arguments.
   * @param pages Project pages (already populated with images).
   * @param stages Filter stage sequence.
   */
  ConsoleBatch(const CliRequest& request,
               std::shared_ptr<ProjectPages> pages,
               std::shared_ptr<StageSequence> stages);

  /**
   * Process all pages through the filter pipeline.
   * @return Exit code (0 on success).
   */
  ExitCode process();

  /** Results from the last process() call. */
  const std::vector<PageResult>& results() const { return m_results; }

 private:
  /**
   * Build the composite task chain for a page, mirroring MainWindow::createCompositeTask.
   */
  std::shared_ptr<BackgroundTask> createCompositeTask(const PageInfo& page);

  const CliRequest& m_request;
  std::shared_ptr<ProjectPages> m_pages;
  std::shared_ptr<StageSequence> m_stages;
  std::shared_ptr<ThumbnailPixmapCache> m_thumbnailCache;
  std::shared_ptr<OutputFileNameGenerator> m_outFileNameGen;
  std::vector<PageResult> m_results;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CONSOLEBATCH_H_
