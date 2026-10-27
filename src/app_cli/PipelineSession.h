// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_PIPELINESESSION_H_
#define SCANTAILOR_APP_CLI_PIPELINESESSION_H_

#include <QString>
#include <memory>

#include "BatchReport.h"
#include "CliRequest.h"
#include "ExitCode.h"
#include "FilterRange.h"
#include "NonCopyable.h"
#include "OutputFileNameGenerator.h"
#include "PageSequence.h"

class BackgroundTask;
class FileNameDisambiguator;
class PageInfo;
class ProjectPages;
class StageSequence;
class ThumbnailPixmapCache;

namespace cli {

class ConsoleLog;

/**
 * One headless run: the pages, the filter stages, the output naming and the
 * thumbnail cache, wired the way MainWindow wires a new project.
 */
class PipelineSession {
  DECLARE_NON_COPYABLE(PipelineSession)

 public:
  /**
   * Loads the images or the project file named by the request and prepares the
   * stages. Returns nullptr and fills error when that fails.
   */
  static std::unique_ptr<PipelineSession> open(const CliRequest& request, CliError& error);

  ~PipelineSession();

  /** Pages as the user sees them (PAGE_VIEW). */
  const PageSequence& pages() const { return m_pageSequence; }

  const std::shared_ptr<StageSequence>& stages() const { return m_stages; }

  /** Applies the request's filter overrides for the range. */
  void configure(const FilterRange& range);

  /** Runs every page through the chain and reports what happened. */
  BatchReport process(const FilterRange& range, const ConsoleLog& log);

  bool saveProject(const QString& path, CliError& error);

 private:
  explicit PipelineSession(const CliRequest& request);

  bool loadImages(CliError& error);

  bool loadProject(CliError& error);

  CliRequest m_request;
  std::shared_ptr<ProjectPages> m_pages;
  PageSequence m_pageSequence;
  std::shared_ptr<StageSequence> m_stages;
  std::shared_ptr<FileNameDisambiguator> m_disambiguator;
  OutputFileNameGenerator m_outFileNameGen;
  std::shared_ptr<ThumbnailPixmapCache> m_thumbnailCache;
  QString m_outputDir;
  QString m_thumbnailDir;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_PIPELINESESSION_H_
