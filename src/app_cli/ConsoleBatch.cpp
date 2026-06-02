// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ConsoleBatch.h"

#include <BackgroundTask.h>
#include <FileNameDisambiguator.h>
#include <FilterResult.h>
#include <LoadFileTask.h>
#include <OutputFileNameGenerator.h>
#include <PageInfo.h>
#include <PageSequence.h>
#include <ProjectPages.h>
#include <ProjectWriter.h>
#include <StageSequence.h>
#include <ThumbnailPixmapCache.h>
#include <filters/deskew/Task.h>
#include <filters/fix_orientation/Task.h>
#include <filters/output/Task.h>
#include <filters/page_layout/Task.h>
#include <filters/page_split/Task.h>
#include <filters/select_content/Task.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>
#include <QTextStream>
#include <QSize>

#include "FilterConfigAdapter.h"

namespace cli {

ConsoleBatch::ConsoleBatch(const CliRequest& request,
                           std::shared_ptr<ProjectPages> pages,
                           std::shared_ptr<StageSequence> stages)
    : m_request(request),
      m_pages(std::move(pages)),
      m_stages(std::move(stages)),
      m_thumbnailCache(std::make_shared<ThumbnailPixmapCache>(
          QDir::tempPath() + "/scantailor-cli-thumbs", QSize(100, 100), 100, 50)) {
  auto disambiguator = std::make_shared<FileNameDisambiguator>();
  QString outDir = request.outputDir;
  if (outDir.isEmpty() && !request.projectFile.isEmpty()) {
    QFileInfo fi(request.projectFile);
    outDir = fi.absolutePath();
  }
  m_outFileNameGen = std::make_shared<OutputFileNameGenerator>(
      disambiguator, outDir, m_pages->layoutDirection());
}

std::shared_ptr<BackgroundTask> ConsoleBatch::createCompositeTask(const PageInfo& page) {
  const int lastFilterIdx = m_request.endFilter >= 0 ? m_request.endFilter : 5;
  const bool batch = true;
  const bool debug = false;

  std::shared_ptr<fix_orientation::Task> fixOrientationTask;
  std::shared_ptr<page_split::Task> pageSplitTask;
  std::shared_ptr<deskew::Task> deskewTask;
  std::shared_ptr<select_content::Task> selectContentTask;
  std::shared_ptr<page_layout::Task> pageLayoutTask;
  std::shared_ptr<output::Task> outputTask;

  if (lastFilterIdx >= m_stages->outputFilterIdx()) {
    outputTask = m_stages->outputFilter()->createTask(page.id(), m_thumbnailCache, *m_outFileNameGen, batch, debug);
  }
  if (lastFilterIdx >= m_stages->pageLayoutFilterIdx()) {
    pageLayoutTask = m_stages->pageLayoutFilter()->createTask(page.id(), outputTask, batch, debug);
  }
  if (lastFilterIdx >= m_stages->selectContentFilterIdx()) {
    selectContentTask = m_stages->selectContentFilter()->createTask(page.id(), pageLayoutTask, batch, debug);
  }
  if (lastFilterIdx >= m_stages->deskewFilterIdx()) {
    deskewTask = m_stages->deskewFilter()->createTask(page.id(), selectContentTask, batch, debug);
  }
  if (lastFilterIdx >= m_stages->pageSplitFilterIdx()) {
    pageSplitTask = m_stages->pageSplitFilter()->createTask(page, deskewTask, batch, debug);
  }
  if (lastFilterIdx >= m_stages->fixOrientationFilterIdx()) {
    fixOrientationTask = m_stages->fixOrientationFilter()->createTask(page.id(), pageSplitTask, batch);
  }
  assert(fixOrientationTask);
  return std::make_shared<LoadFileTask>(BackgroundTask::BATCH, page, m_thumbnailCache, m_pages, fixOrientationTask);
}

static void logStderr(const QString& msg) {
  QTextStream err(stderr);
  err << QDateTime::currentDateTime().toUTC().toString(Qt::ISODate)
      << " " << msg << "\n";
}

ExitCode ConsoleBatch::process() {
  m_results.clear();

  // Create output directory if needed.
  if (!m_request.outputDir.isEmpty()) {
    QDir dir(m_request.outputDir);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        logStderr(QString("[error]: Failed to create output directory: %1").arg(m_request.outputDir));
        return ExitCode::OUTPUT_ERROR;
      }
    }
  }

  // Get the page sequence.
  const PageSequence pageSequence = m_pages->toPageSequence(PAGE_VIEW);
  const int numPages = static_cast<int>(pageSequence.numPages());

  if (numPages == 0) {
    logStderr("[error]: No pages to process.");
    return ExitCode::INVALID_INPUT;
  }

  // Configure filters from CLI flags.
  {
    QString errorMsg;
    FilterConfigAdapter adapter(m_request, m_stages, pageSequence);
    const int startFilter = m_request.startFilter >= 0 ? m_request.startFilter : 0;
    const int endFilter = m_request.endFilter >= 0 ? m_request.endFilter : 5;
    adapter.apply(startFilter, endFilter, errorMsg);
    if (!errorMsg.isEmpty()) {
      logStderr(QString("[error]: %1").arg(errorMsg));
      return ExitCode::INVALID_INPUT;
    }
  }

  if (!m_request.quiet) {
    logStderr(QString("[process]: Processing %1 page(s)").arg(numPages));
  }

  int successCount = 0;
  int failCount = 0;
  QJsonArray outputFiles;
  QJsonArray errors;

  for (size_t i = 0; i < pageSequence.numPages(); ++i) {
    const PageInfo& page = pageSequence.pageAt(i);

    if (!m_request.quiet) {
      logStderr(QString("[process]: Page %1/%2: %3")
                    .arg(i + 1)
                    .arg(numPages)
                    .arg(page.id().imageId().filePath()));
    }

    auto task = createCompositeTask(page);

    PageResult result;
    result.inputFile = page.id().imageId().filePath();

    try {
      FilterResultPtr filterResult = (*task)();
      result.success = true;

      if (m_outFileNameGen) {
        result.outputFile = m_outFileNameGen->filePathFor(page.id());
      }
      successCount++;

      if (m_request.verbose) {
        logStderr(QString("[ok]: %1 -> %2").arg(result.inputFile, result.outputFile));
      }
    } catch (const std::exception& e) {
      result.success = false;
      result.errorMessage = QString::fromUtf8(e.what());
      failCount++;

      if (!m_request.quiet) {
        logStderr(QString("[error]: Page %1 failed: %2").arg(result.inputFile, result.errorMessage));
      }
    }

    m_results.push_back(result);

    if (result.success) {
      QJsonObject fileObj;
      fileObj.insert("page_id", QString::number(static_cast<qlonglong>(i)));
      fileObj.insert("input_file", result.inputFile);
      fileObj.insert("output_file", result.outputFile);
      outputFiles.append(fileObj);
    } else {
      QJsonObject errObj;
      errObj.insert("page_id", QString::number(static_cast<qlonglong>(i)));
      errObj.insert("input_file", result.inputFile);
      errObj.insert("message", result.errorMessage);
      errors.append(errObj);
    }
  }

  // Emit JSON output if requested.
  if (m_request.outputMode == "json") {
    QJsonObject json;
    if (failCount == 0) {
      json.insert("status", QString("ok"));
    } else if (successCount > 0) {
      json.insert("status", QString("partial"));
    } else {
      json.insert("status", QString("error"));
    }
    json.insert("pages_processed", successCount);
    json.insert("pages_failed", failCount);
    json.insert("output_files", outputFiles);
    json.insert("errors", errors);

    QJsonDocument doc(json);
    QTextStream out(stdout);
    out << doc.toJson(QJsonDocument::Indented) << "\n";
  }

  if (!m_request.quiet) {
    logStderr(QString("[process]: Complete. %1 succeeded, %2 failed.").arg(successCount).arg(failCount));
  }

  if (failCount > 0 && successCount == 0) {
    return ExitCode::FILTER_ERROR;
  }
  if (failCount > 0) {
    return ExitCode::FILTER_ERROR;
  }
  return ExitCode::SUCCESS;
}

}  // namespace cli
