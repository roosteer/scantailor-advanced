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
#include "PageSequence.h"

class BackgroundTask;
class PageInfo;
class ProjectDocument;
class ThumbnailPixmapCache;

namespace cli {

class ConsoleLog;

/**
 * \brief One headless run, wired the way MainWindow wires a new project.
 *
 * The document owns the pages, the stages, the output naming and the selection; the
 * session keeps only the page order the request was made against, which must not change
 * while the filters are being configured.
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

  /** Pages as the user sees them (PAGE_VIEW), in request order. */
  const PageSequence& pages() const { return m_pageSequence; }

  const std::shared_ptr<ProjectDocument>& document() const { return m_document; }

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
  std::shared_ptr<ProjectDocument> m_document;
  PageSequence m_pageSequence;
  std::shared_ptr<ThumbnailPixmapCache> m_thumbnailCache;
  QString m_outputDir;
  QString m_thumbnailDir;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_PIPELINESESSION_H_
